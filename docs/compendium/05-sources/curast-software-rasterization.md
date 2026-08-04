---
source_url: https://arxiv.org/html/2604.21749v2
title: CuRast: Cuda-Based Software Rasterization for Billions of Triangles
ingested: 2026-08-03
avenue: Games
---

# CuRast: Cuda-Based Software Rasterization for Billions of Triangles

Title:

Content selection saved. Describe the issue below:

Description:

![](https://arxiv.org/static/base/1.0.1/images/icons/smileybones-small.svg)arXiv is now an independent nonprofit! [Learn more](https://info.arxiv.org/about) ×

[License: CC BY-SA 4.0](https://info.arxiv.org/help/license/index.html#licenses-available)

arXiv:2604.21749v2 \[cs.GR\] 24 Apr 2026

\\biberVersion\\BibtexOrBiblatex\\electronicVersion\\PrintedOrElectronic

\\teaser![[Uncaptioned image]](https://arxiv.org/html/2604.21749v2/images/teaser.png)

Brute-force rendering the Zorah data set on an RTX 5090. 38.8GB of geometry loaded from an SSD, compressed to 21.7 GB, transferred to GPU and ready to render in 6.6 seconds. 18.9 billion triangles total; 13.5 billion triangles visible after frustum culling. Rendered in 67.3 milliseconds into a 3840×\\times2160 framebuffer with screen space ambient occlusion and eye-dome lighting enabled.

# CuRast: Cuda-Based Software Rasterization for Billions of Triangles

Markus Schütz, Lukas Lipp, Elias Kristmann, Michael Wimmer

TU Wien

###### Abstract

Previous work shows that small triangles can be rasterized efficiently with compute shaders. Building on this insight, we explore how far this can be pushed for massive triangle datasets without the need to construct acceleration structures in advance.

Method: A 3-stage rasterization pipeline first rasterizes small triangles directly in stage 1, using atomicMin to store the closest fragments. Larger triangles are forwarded to stages 2 and 3.

Results: CuRast can render models with hundreds of millions of triangles up to 2-5x (unique) or up to 12x (instanced) faster than Vulkan. Vulkan remains an order of magnitude faster for low-poly meshes.

Limitations: We currently focus on dense, opaque meshes that you would typically obtain from photogrammetry/3D reconstruction. Blending/Transparency is not yet supported, and scenes with thousands of low-poly meshes are not implemented efficiently.

Future Work: To make it suitable for games and a wider range of use cases, future work will need to (1) optimize handling of scenes with tens of thousands of nodes/meshes, (2) add support for hierarchical clustered LODs such as those produced by Meshoptimizer, (3) add support for transparency, likely in its own stage so as to keep opaque rasterization untouched and fast.

Source Code: https://github.com/m-schuetz/CuRast

## 1 Introduction

Dedicated hardware rendering pipelines have been the dominant approach for visualizing three-dimensional triangle meshes for decades. However, prior work on micropolygon and triangle cluster rendering, such as Nanite \[ [KSW21](https://arxiv.org/html/2604.21749v2#bib.bibx21 "")\], have demonstrated that hardware rasterization can be outperformed in certain scenarios, particularly when rendering highly detailed geometry composed of pixel-sized triangles. On average, Nanite’s software rasterizer achieves roughly three times the throughput of the hardware pipeline, with even larger gains for pure micropolygon geometry. For larger triangles or triangle clusters, Nanite falls back to traditional hardware rasterization.

In this paper, we further explore the potential of GPGPU-accelerated software rasterization and investigate the limits achievable through brute-force processing of unstructured triangle meshes. By _unstructured_, we mean that no spatial acceleration structures or hierarchical levels of detail (LOD) are precomputed. We deliberately avoid such preprocessing for two reasons: first, constructing these structures requires a costly preprocessing step, and second, they typically must be recomputed whenever the underlying mesh is modified.

GPU-based software rasterizers typically also aim to support a wide range of features, including blending and support for transparent geometry. These features can be a defining aspect in the architecture of a rasterization pipeline and potentially slow down the processing of opaque geometry. In this work, we omit blending and focus on maximizing performance for opaque geometry. Transparent geometry could, in theory, be implemented in subsequent passes that execute after our pipeline.

Our contributions to the state of the art are as follows:

- •


A software rasterizer capable of rendering up to a billion (unique geometry) to 4 billion (instanced geometry) of triangles in real time, without the need to create spatial acceleration structures or hierarchical levels of detail in advance.

- •


A 3-stage pipeline that is highly optimized for small, but also handles medium and large triangles.

- •


Adapting visibility buffer indexing for an arbitrary mix of nodes/instances/triangles, rather than supporting a fixed number of nodes and a fixed maximum of triangles per node.


To manage expectations about the scope of this work, following limitations and observations apply:

- •


We specifically focus on dense and opaque geometry. Blending is not supported.

- •


Our implementation currently focuses on triangle-dense meshes and scales poorly with tens of thousands of low-density meshes.

- •


Likewise, instancing is targeted towards meshes with several hundreds of thousands to millions of triangles.

- •


Larger triangles are handled in a "good enough" fashion with glaring potential for improvement.


Given these benefits and limitations, we believe the results of this work are particularly relevant for applications involving dense and editable or animated meshes, as our approach does not rely on acceleration structures that require recomputation after modifications. Representative use cases include photogrammetry reconstruction and processing pipelines that generate and curate models containing tens of millions to billions of triangles. Additional examples include content creation tools such as Blender, where users perform arbitrary editing operations on complex geometry. It is not yet suitable for games as these work largely with non-editable geometry that benefits from pre-constructed LODs, and because they typically comprise scenes with tens of thousands of low-density meshes, which our implementation is currently not made for.

## 2 Related Work

### 2.1 Software Rasterization

Software rasterization has a long history that predates dedicated GPU hardware, and it continues to be of interest as modern GPUs provide powerful and fast compute pipelines for custom rendering algorithms. One of the most fundamental concepts of efficient rasterization pipelines – the depth buffer – dates all the way back to 1974, long before GPUs became commonplace \[ [Cat74](https://arxiv.org/html/2604.21749v2#bib.bibx4 "")\]. Since GPUs became widely available, software rasterization was primarily motivated by education and research, but in the recent decade the interest has shifted towards targeting specialized use cases in which custom programs can outperform the more generalized hardware pipeline that has to support a wide range of scenarios.

### 2.2 Triangles

FreePipe \[ [LHLW10](https://arxiv.org/html/2604.21749v2#bib.bibx23 "")\] introduces the concept of atomic-min to efficiently store the closest fragment inside pixels without the need for inter-thread communication and sorting stages. Since CUDA was limited to 32 bit atomics back then, which did not allow atomically writing depth and color values with a single atomic operation, they suggested performing two 32 bit atomic-min operations with the same 20 bit depth value but different 12 bits of the color value into the same pixel that is separated into two buffers. Afterwards, they extract the 12 bit parts located in different buffers, and fuse them back into a 24 bit color value. Brunhaver et al. \[ [BFH10](https://arxiv.org/html/2604.21749v2#bib.bibx1 "")\] design a hardware system targeted towards micropolygons that is able to process triangles at the rate of an GTX 480, but with less than 1% of the die size and power usage. CudaRaster \[ [LK11](https://arxiv.org/html/2604.21749v2#bib.bibx24 "")\] employs a hierarchical approach where triangles are first queued into bins, then tiles, and finally rasterized in pixels. They also structure their pipeline to honor the order of triangles during rendering via a sort-middle \[ [MCEF94](https://arxiv.org/html/2604.21749v2#bib.bibx26 "")\] approach, enabling order-dependent algorithms such as front-to-back blending operations of transparent triangles. Weber built a software rasterization approach for micropolygons, where they first lock a pixel sample, update the color value, then unlock it \[ [Web15](https://arxiv.org/html/2604.21749v2#bib.bibx35 "")\]. Kenzel et al. \[ [KKSS18](https://arxiv.org/html/2604.21749v2#bib.bibx18 "")\] propose cuRE, a streaming architecture that performs multiple rasterization stages in parallel rather than one after another. This approach avoids amassing large workloads in queues between stages, as the queues are quickly processed in a timely manner. They also aim for a rich set of features comparable to DirectX 9, and evaluate with a large set of more than 100 test scenes captured from video games, with about 0.3 to 8 million triangles. cuRE compares to Piko \[ [PTSO15](https://arxiv.org/html/2604.21749v2#bib.bibx31 "")\], FreePipe, CUDARaster and OpenGL, and finds that CUDARaster generally performs fastest within a factor of 4-6 to OpenGL, followed by cuRE. cuRE, on the other hand, scales better with multiple draw calls and higher screen resolutions. FreePipe is slower than either for most scenes due to utilizing only one thread per triangle, which becomes an issue for scenes with large triangles.

### 2.3 Points and Splats

Software rasterization has seen particular success in the field of point-based rendering, which poses fewer challenges as points are typically always small, often pixel-sized. Due to this, we rarely run into load-balancing issues caused by processing different-sized primitives (with the notable exception of Gaussian Splats). Günther et al. \[ [GKLR13](https://arxiv.org/html/2604.21749v2#bib.bibx9 "")\] proposed a compute-based rendering pipeline based on spin loops: Each thread trying to draw a point to a pixel first attempts to lock it with atomic-CAS, then updates and subsequently unlocks it. Marrs et al. \[ [MWH18](https://arxiv.org/html/2604.21749v2#bib.bibx27 "")\] use 32 bit atomic-min operations to efficiently construct multiple depth maps. Schütz et al. \[ [SKW21](https://arxiv.org/html/2604.21749v2#bib.bibx34 "")\] use 64 bit atomic-min operations to efficiently write interleaved depth and color values into a framebuffer, then extract the color values for display on screen. Neural point-based renderers ADOP \[ [RFS22](https://arxiv.org/html/2604.21749v2#bib.bibx32 "")\] and NePO \[ [LRSF](https://arxiv.org/html/2604.21749v2#bib.bibx25 "")\] use atomic-min-based rasterization to quickly construct multi-resolution framebuffers that are then fed to a neural renderer that fills holes and shades the final image. One of the biggest success stories of software rasterization and point-based rendering might be 3D Gaussian Splatting (3DGS) \[ [KKLD23](https://arxiv.org/html/2604.21749v2#bib.bibx17 "")\]. 3DGS proposes using translucent gaussian primitives for scene reconstruction from photos, resulting in high-quality models of the real world. The 3-dimensional gaussians are then rendered by approximating them through 2-dimensional gaussians in screen space, sorting them by depth, and blending them from front to back. To efficiently process splats with wildly different sizes, they are partitioned into 16×\\times16 pixel tiles, and they then rasterize all splats in a tile with one CUDA thread block. Since then, a plethora of research has proposed optimizations to various aspects of the training and rasterization pipeline. We refer to Hahlbohm et al. \[ [HFEM26](https://arxiv.org/html/2604.21749v2#bib.bibx11 "")\] as a recent summary of a variety of these optimizations.

Besides 3DGS, Dreams (PS4) \[ [Eva15](https://arxiv.org/html/2604.21749v2#bib.bibx7 "")\] and Nanite \[ [KSW21](https://arxiv.org/html/2604.21749v2#bib.bibx21 "")\] are two notable software rasterizers that demonstrate the practical use of custom rendering pipelines. The developers of Dreams discovered that they could rasterize small splats faster with a custom atomics-based renderer than the standard PS4 hardware pipeline. Nanite is primarily an LOD system for massive meshes that renders clusters of 128 triangles with a desired level of detail. However, they discovered that rasterizing these fine-grained clusters with near pixel-sized triangles is often faster through custom compute shaders, than through the hardware rasterizer. For large triangles, they fall back to hardware rasterization.

### 2.4 Visibility Buffers

Visibility buffers were initially introduced under the term _item buffers_ in the context of ray tracing \[ [WHG84](https://arxiv.org/html/2604.21749v2#bib.bibx36 "")\]. For each pixel, they store the id of the element that corresponds to the first hit of a ray. They were later re-introduced and formalized for triangle-primitives under the term _visibility buffer_, as an alternative to deferred shading \[ [BH13](https://arxiv.org/html/2604.21749v2#bib.bibx2 "")\]. Instead of constructing feature-rich G-Buffers, visibility buffers store triangle and model/instance indices of the visible triangle for each pixel, which simplifies the rasterization stage and allows us to look up all necessary data in the shading stage. Alternatively, visibility buffers could also store references to shading values \[ [SD15](https://arxiv.org/html/2604.21749v2#bib.bibx33 "")\], but in the context of this paper, we assume they reference individual triangles. Visibility buffers have gained popularity for software rasterization as they allow us to address all attributes of a visible triangle, despite the limited amount of information we can write to a framebuffer with 64 bit atomic-min-based rasterizers.

### 2.5 Geometry Optimization

The order of vertices and indices has a substantial impact on rendering performance due to factors like access to coalesced data in memory and caching, as well as potential vertex reuse mechanisms in GPUs. Kenzel et al. \[ [KKT\*18](https://arxiv.org/html/2604.21749v2#bib.bibx19 "")\] and Kerbl et al. \[ [KKI\*18](https://arxiv.org/html/2604.21749v2#bib.bibx16 "")\] studied the behaviour of vertex caching and reuse on various modern GPUs, and propose strategies to optimize the meshes for better reuse, as well as strategies to implement reuse in software rasterization pipelines. Schütz et al. \[ [SKW21](https://arxiv.org/html/2604.21749v2#bib.bibx34 "")\] rearrange point clouds such that some spatially close points are also close in memory to promote coalesced memory access, but avoid too much locality that leads to contention when rendering thousands of overlapping points to the same pixel. Bene and Valasek \[ [BV26](https://arxiv.org/html/2604.21749v2#bib.bibx3 "")\] investigate various triangulation strategies for polygons to find those that render the fastest.

Meshoptimizer is an open source project that implements strategies that promote locality and vertex reuse \[ [Kap26](https://arxiv.org/html/2604.21749v2#bib.bibx13 "")\]. In this paper, we use it to obtain fair comparisons with a Vulkan-based renderer that greatly benefits from it, and also observe its impact on our own software rasterization pipeline.

## 3 Preliminaries

In this section, we recap important basics/prior work we build on in more detail, particularly atomic-min-based software rasterization and visibility buffer rendering pipelines.

### 3.1 Atomics-Based Depth Testing

In 2010, FreePipe \[ [LHLW10](https://arxiv.org/html/2604.21749v2#bib.bibx23 "")\] proposed using atomic min operations to efficiently write the closest depth value to each pixel in a massively parallel rasterizer. Because global atomics implicitly synchronize concurrent writes, this approach eliminates the need for inter-thread communication or fragment queuing and sorting. At the time, however, the method was limited to 32-bit atomics, which made it difficult to attach information in addition to the mandatory depth value. With the introduction of 64-bit atomic min/max operations, it became possible to attach additional payload such as color values or primitive IDs, enabling atomic updates to an interleaved depth+payload framebuffer:

[⬇](data:text/plain;base64,dTMyIHVkZXB0aCA9IF9fZmxvYXRfYXNfdWludChkZXB0aCk7CnUzMiBwYXlsb2FkID0gLi4uOyAvLyBjb2xvciBvciBwcmltaXRpdmUgSUQKdTY0IGZyYWdtZW50ID0gdTY0KHVkZXB0aCkgPDwgMzIgfCB1NjQocGF5bG9hZCk7CmF0b21pY01pbihmcmFtZWJ1ZmZlcltwaXhlbElEXSwgZnJhZ21lbnQpOw==)

1u32udepth=\_\_float\_as\_uint(depth);

2u32payload=...;//colororprimitiveID

3u64fragment=u64(udepth)<<32\|u64(payload);

4atomicMin(framebuffer\[pixelID\],fragment);

The PS4 game "Dreams" utilized that additional payload to create a highly efficient brush-stroke/splat based rasterizer that was already able to outperform the hardware rasterization pipeline of the PlayStation 4 \[ [Eva15](https://arxiv.org/html/2604.21749v2#bib.bibx7 "")\]. While 64 bit atomics are fast, the still fairly small payload is not suitable for rendering algorithms that rely on multiple render targets, such as deferred rendering with rich G-Buffers. To work around this limitation, we can either (A) compute each fragment’s final shaded color value directly during triangle rasterization, which is prohibitively expensive, or (B) we can store primitive indices instead to access all of the visible triangle’s attributes in a deferred resolve pass. The second approach is what is commonly referred to as "visibility buffers" and popularized by Nanite \[ [KSW21](https://arxiv.org/html/2604.21749v2#bib.bibx21 "")\].

### 3.2 Visibility Buffers

Visibility buffer rendering and deferred shading with G-Buffers are closely related. Both approaches aim to rasterize all geometry first, and defer the shading to a full-screen resolve pass in order to apply expensive shading operations only to visible fragments. The difference is that deferred rendering creates multiple (or packed) render targets that contain attributes that are necessary for shading (e.g., albedo, normals, material id/flags, roughness, etc.), while visibility buffers only store the ID of the visible mesh and triangle in each pixel. With these IDs, we are then able to fetch all of the data that we need for shading in the full-screen resolve pass.

![Refer to caption](https://arxiv.org/html/2604.21749v2/images/deferred_vs_visbuffer.png)Figure 1: Deferred shading creates G-Buffers with various attributes that may be needed for the shading pass. Visibility buffers only store triangle indices.

Nanite \[ [KSW21](https://arxiv.org/html/2604.21749v2#bib.bibx21 "")\] demonstrated the efficiency and viability of software-rasterization with visibility buffers in a practical and widely deployed gaming engine. It assigns 30 bit for depth values and a payload of 34 bit for the triangle ID. The triangle ID itself is composed of 27 bit for the cluster index and 7 bit to address the triangle within the cluster. With this assignment of bits, they are able to address 134 million clusters and 128 triangles per cluster.

A major advantage of visibility buffers is that they significantly reduce memory bandwidth pressure during geometry rasterization, since we only need to load vertex indices and positions. Other attributes, such as uv-coordinates, normals, textures, etc., are not needed, unless they affect vertex positions (e.g. bump maps or height maps). This further accelerates the geometry processing stage compared to deferred rendering, which still fetches data for numerous fragments that will not be visible in the final image. Another advantage of visibility buffers is that we get object picking for free.

A disadvantage of visibility buffers is that the resolve pass is significantly more expensive: We now need to load the triangle geometry, project it again, compute interpolated uv coordinates, etc. Visibility buffers may also suffer from caching issues because if triangles are roughly pixel-sized, adjacent pixels load vertex data from different and quasi-random regions in memory \[ [Hab21](https://arxiv.org/html/2604.21749v2#bib.bibx10 "")\]. Another disadvantage of visibility buffers is that computing the mip map level can be tricky for large triangles that intersect with the near plane. If one or two vertices are behind the near plane, we can not inter- or extrapolate uv-coordinates in adjacent pixels in screen space, which would allow us to cheaply compute the pixel’s footprint in texture space. We therefore shade triangles in world space (see Section [4.4](https://arxiv.org/html/2604.21749v2#S4.SS4 "4.4 Screen-Space Resolve/Shading Pass ‣ 4 Method ‣ CuRast: Cuda-Based Software Rasterization for Billions of Triangles")).

## 4 Method

Our method draws massive triangle models entirely in CUDA and supports following functionality: Indexed meshes, instancing, textures or vertex colors, mip mapping, and massive amounts of small but also large triangles.
We furthermore experiment with compressed/quantized indices between 8 to 32 bit and 16-bit fixed-precision coordinates.

![Refer to caption](https://arxiv.org/html/2604.21749v2/images/stages.png)Figure 2: Rasterization Stages. Pixels colored by iteration counter. Stage 1: A single thread iterates over all sample positions inside the triangle’s bounding box. Stage 2: A warp (32 threads) iterates over all samples inside the bounding box. Stage 3: A workgroup (64 threads) iterates over all samples in a 64x64 tile to rasterize a portion of the triangle.

As we primarily focus on massive models with hundreds of millions to billions of small triangles, our method first launches a CUDA kernel with one thread per triangle, under the assumption that triangles can easily be rasterized by a single thread, and multiple threads in a warp have balanced workload as they process similar-sized triangles. This assumption will, of course, be frequently violated so we implement two additional stages that handle medium-sized and large triangles:

- •


Stage 1: Launch 1 thread per triangle, rasterizing it directly if it is small, or adding it to a global queue for the next stage if it is either too large or intersects the near plane.

- •


Stage 2: Launch a warp (32 threads) per triangle that was queued by stage 1, rasterizing it directly if it is medium-sized, or splitting it into parts and adding each part to a queue if it is either very large or intersects the near plane.

- •


Stage 3: Launch 64 threads for each part of a triangle that was queued by stage 2.


_Small_ triangles were experimentally determined as those whose screen-space bounding box covers less than 128 pixels, and _medium_ triangles as those covering less than 4096 pixels.

On the host side, before launching the rasterization stages, we first perform frustum culling and assemble a list of meshes, each comprising transformation matrices and pointers to vertex and index buffers. Crucially, we also compute a cumulative triangle count for each mesh and instance (i.e. the exclusive prefix sum), which we will use to store the absolute triangle ID over all visible meshes in the visibility buffer. This data is then copied to the GPU, and we are ready to launch the 3 stages of our rasterization kernels.

Experienced readers will notice that we perform naïve boundary constraints by processing all fragments inside the triangle’s bounding box, despite well-known approaches that only traverse fragments within the triangle \[ [Pin88](https://arxiv.org/html/2604.21749v2#bib.bibx30 "")\]. We initially tried to do so but found that the additional effort that was required to avoid wasted work is often more expensive than unnecessary but cheap work, especially for massive, dense data sets that produce pixel-sized triangles.

### 4.1 Stage 1: Rasterizing Small Triangles

The premise of this stage is that each triangle is small enough such that a single thread can render it in a timely manner. If they are too large, we instead put them in a global queue for the next stage.

Looping Through Batches of 256 Triangles: We perform a persistent-kernel launch using cooperative groups since we are rendering numerous meshes with varying numbers of triangles, and therefore cannot easily map a global thread index to the corresponding triangle. A limited set of workgroups with 256 threads keeps looping until all triangles are rendered. Each specific workgroup obtains the next set of triangles it renders by atomically incrementing a global work counter: _atomicAdd(numTrianglesRendered, 256)_. If the returned triangle index is outside of the current mesh, the workgroup advances through the list of meshes until it encounters the mesh that contains the corresponding triangles.

Preparing a Batch of 256 Triangles for Rasterization: Each thread loads the vertex data of its triangle and transforms it to view space. Unlike classical graphics APIs, we do not use a projection matrix. Instead, we perform perspective projection through an element-wise multiplication of the view-space coordinates with following projection-vector:

|     |     |     |     |
| --- | --- | --- | --- |
|  | f=1tan⁡(fovy2)f=\\frac{1}{\\tan\\left(\\frac{\\text{fovy}}{2}\\right)} |  | (1) |

|     |     |     |     |
| --- | --- | --- | --- |
|  | 𝐏=\[faspectf−1\]\\mathbf{P}=\\begin{bmatrix}\\frac{f}{\\text{aspect}}&f&-1\\end{bmatrix} |  | (2) |

Afterwards, the x and y components are divided by the depth (z component). The result is a normalized-device-coordinate with x and y between -1 and 1, and z storing the positive, linear and non-normalized depth value, starting from the camera position.

We continue to perform frustum culling, discarding/skipping any triangle that is entirely outside the visible volume. For the remaining triangles, we compute the screen-space bounding box, or mark it as _nontrivial_ if it intersects the near plane. The latter is done because triangles that intersect the near plane require clipping to obtain accurate screen-space bounding boxes, but we want to keep this stage as simple as possible and avoid increased register pressure and expensive branches, which would slow down the processing of nicely behaved triangles. To optimize for scenarios with massive amounts of micro-polys, we also discard/skip triangles whose bounding box does not intersect with the center of any pixel, followed by performing backface culling. Finally, we put all triangles that either intersect with the near plane or whose bounding box covers more than 128 pixels into the queue for the next stage. The remaining triangles are then rasterized.

Rasterization: Rasterization follows a straight-forward bounding-box-rasterization scheme. Each thread loops over the y (outer loop) and x (inner loop) coordinates inside the bounding box, computes the barycentric coordinates of the triangle, and checks if the barycentric coordinates lie within the triangle boundary. If the fragment lies inside the triangle, we continue to perform a 64-bit atomicMin to update the framebuffer’s pixel with a combination of depth (most significant bits) and global triangle index (least significant bits). To support scenes with tens of billions of triangles, we assign 28 bit to the depth value, and 36 bit to the global triangle ID. Since depth values are always positive, one bit is taken from the sign, and three bits are taken away from the mantissa. As the 28 bit depth value is stored in the most significant bits, the fragment with the smallest depth will prevail in the resulting framebuffer. The encoded global triangle index is later used in a resolve/screen pass in order to find and process the triangle that occupies that pixel.

For efficient perspective-correct interpolation of depth values, we precompute the inverse depth 1z\\frac{1}{z} for each vertex outside of the loop, then interpolate them according to the barycentric coordinates s,t,vs,t,v and a fast division intrinsic inside the loop:

[⬇](data:text/plain;base64,ZmxvYXQgZGVwdGhJID0gdiAqIHowSSArIHMgKiB6MUkgKyB0ICogejJJOwpmbG9hdCBkZXB0aCA9IF9fZmRpdmlkZWYoMS4wZiwgZGVwdGhJKTs=)

1floatdepthI=v\*z0I+s\*z1I+t\*z2I;

2floatdepth=\_\_fdividef(1.0f,depthI);

Of note is that \_\_fdividef is not just a faster approximate division; it also significantly reduces the kernel’s register usage from 55 to 48, and therefore improves its occupancy.

For barycentric coordinates, we precompute their changes across the x and y directions outside of the loop, and then increment them as we loop over the pixels of the bounding box.

### 4.2 Stage 2: Rasterizing Medium Triangles

We now launch 32 threads per triangle that was queued by stage 1, so that we can process these more demanding ones with additional compute power. For any triangle whose bounding box covers up to 4096 pixels, the 32 threads perform a 1-dimensional loop over the number of pixels, incrementing the loop counter by 32 each iteration. The 1D index is then mapped to the 2D coordinate within the bounding box via x=imodwidthx=i\\bmod\\text{width} and y=iwidthy=\\frac{i}{\\text{width}}, but otherwise the rasterization logic remains the same as in stage 1.

A major difference from the first stage is that we now also perform an accurate and expensive screen-space bounding box calculation for each triangle by clipping them via the Sutherland-Hodgman algorithm. However, we do not actually split triangles that intersect the frustum into multiple smaller ones. Our intention is to (1) identify and measure huge triangles (which often intersect the near plane), so that we can partition them into a suitable number of tiles for stage 3, and (2) to obtain the bounding box of the visible part of the triangle, which may be a fraction of the bounding box of the entire triangle.


[... middle omitted — see footer ...]


This research has been funded by WWTF project _ICT22-055 - Instant Visualization and Interaction for Large Point Clouds_ and WWTF project _ICT25-084 - Instant Visualization and Editing of Arbitrarily Large 3D Data Sets_.

## References

- \[BFH10\]John S Brunhaver, Kayvon Fatahalian and Pat Hanrahan
“Hardware implementation of micropolygon rasterization with motion and defocus blur.”
In _High Performance Graphics_, 2010, pp. 1–9

- \[BH13\]Christopher A. Burns and Warren A. Hunt
“The Visibility Buffer: A Cache-Friendly Approach to Deferred Shading”
In _Journal of Computer Graphics Techniques (JCGT)_ 2.2, 2013, pp. 55–69
URL: [http://jcgt.org/published/0002/02/04/](http://jcgt.org/published/0002/02/04/ "")
- \[BV26\]Robert Bene and Gábor Valasek
“Helper-Lane Optimized Triangulation of Polygons” short paper
EUROGRAPHICS, 2026

- \[Cat74\]Edwin Earl Catmull
“A subdivision algorithm for computer display of curved surfaces”
The University of Utah, 1974

- \[CPS13\]Keenan Crane, Ulrich Pinkall and Peter Schröder
“Robust fairing via conformal curvature flow”
In _ACM Transactions on Graphics (TOG)_ 32.4ACM New York, NY, USA, 2013, pp. 1–10

- \[DMC\*26\]Marko Dabrovic et al.
“Sponza” Sponza has undergone several adjustments by different authors over the years. Originally created by Marko Dabrovic, then re-modelled by Frank Meinl at Crytek, Morgan McGuire, Hans-Kristian Arntzen and Ludicon., NVIDIA nvpro-samples, 2026
URL: [https://github.com/ludicon/sponza-gltf](https://github.com/ludicon/sponza-gltf "")
- \[Eva15\]Alex Evans
“Learning from failure: A Survey of Promising, Unconventional and Mostly Abandoned Renderers for ‘Dreams PS4’, a Geometrically Dense, Painterly UGC Game” [https://advances.realtimerendering.com/s2015/AlexEvans\_SIGGRAPH-2015-sml.pdf](https://advances.realtimerendering.com/s2015/AlexEvans_SIGGRAPH-2015-sml.pdf "") \[Accessed 23-April-2026\]
In _ACM SIGGRAPH 2015 Courses, Advances in Real-Time Rendering in Games_, 2015

- \[Gil20\]NRHK Gildas Sidobre
“Komainu Kobe Ikuta-jinja”, Distributed by Open Heritage 3D, 2020
DOI: [10.26301/1wv3-9775](https://dx.doi.org/10.26301/1wv3-9775 "")
- \[GKLR13\]Christian Günther, Thomas Kanzok, Lars Linsen and Paul Rosenthal
“A GPGPU-based Pipeline for Accelerated Rendering of Point Clouds”
In _J. WSCG_ 21, 2013, pp. 153–161

- \[Hab21\]John Hable
“Visibility Buffer Rendering with Material Graphs”, 2021
URL: [http://filmicworlds.com/blog/visibility-buffer-rendering-with-material-graphs/](http://filmicworlds.com/blog/visibility-buffer-rendering-with-material-graphs/ "")
- \[HFEM26\]Florian Hahlbohm, Linus Franke, Martin Eisemann and Marcus Magnor
“Faster-GS: Analyzing and Improving Gaussian Splatting Optimization”, 2026
arXiv: [https://arxiv.org/abs/2602.09999](https://arxiv.org/abs/2602.09999 "")
- \[ItF26\]undef Iconem and undef Fondazione Musei Civici di Venezia
“Venice”, 2026
URL: [https://iconem.com/](https://iconem.com/ "")
- \[Kap26\]Arseny Kapoulkine
“Meshoptimizer / gltfpack v1.1”, [https://github.com/zeux/meshoptimizer](https://github.com/zeux/meshoptimizer ""), 2026

- \[Kar26\]Brian Karis
“Nanite + Reyes”, 2026
URL: [https://graphicrants.blogspot.com/2026/02/nanite-reyes.html](https://graphicrants.blogspot.com/2026/02/nanite-reyes.html "")
- \[KKG\*26\]Christoph Kubisch et al.
“NVIDIA RTX Mega Geometry Now Available with New Vulkan Samples”, 2026
URL: [https://developer.nvidia.com/blog/nvidia-rtx-mega-geometry-now-available-with-new-vulkan-samples/](https://developer.nvidia.com/blog/nvidia-rtx-mega-geometry-now-available-with-new-vulkan-samples/ "")
- \[KKI\*18\]Bernhard Kerbl et al.
“Revisiting The Vertex Cache: Understanding and Optimizing Vertex Processing on the modern GPU”
In _Proc. ACM Comput. Graph. Interact. Tech._ 1.2New York, NY, USA: Association for Computing Machinery, 2018
DOI: [10.1145/3233302](https://dx.doi.org/10.1145/3233302 "")
- \[KKLD23\]Bernhard Kerbl, Georgios Kopanas, Thomas Leimkühler and George Drettakis
“3D Gaussian Splatting for Real-Time Radiance Field Rendering”
In _ACM Transactions on Graphics_ 42.4, 2023
URL: [https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/](https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/ "")
- \[KKSS18\]Michael Kenzel, Bernhard Kerbl, Dieter Schmalstieg and Markus Steinberger
“A High-Performance Software Graphics Pipeline Architecture for the GPU”
In _ACM Trans. Graph._ 37.4New York, NY, USA: ACM, 2018
DOI: [10.1145/3197517.3201374](https://dx.doi.org/10.1145/3197517.3201374 "")
- \[KKT\*18\]Michael Kenzel et al.
“On-the-fly Vertex Reuse for Massively-Parallel Software Geometry Processing”
In _Proc. ACM Comput. Graph. Interact. Tech._ 1.2New York, NY, USA: ACM, 2018
DOI: [10.1145/3233303](https://dx.doi.org/10.1145/3233303 "")
- \[KOK\*25\]Bastian Kuth et al.
“Real-time meshlet decompression”
In _Computers & Graphics_ 131, 2025, pp. 104292
DOI: [https://doi.org/10.1016/j.cag.2025.104292](https://dx.doi.org/https://doi.org/10.1016/j.cag.2025.104292 "")
- \[KSW21\]Brian Karis, Rune Stubbe and Graham Wihlidal
“A Deep Dive into Nanite Virtualized Geometry” Industry talk, SIGGRAPH 2021 Course: Advances in Real-Time Rendering in Games, 2021
URL: [https://advances.realtimerendering.com/s2021/Karis\_Nanite\_SIGGRAPH\_Advances\_2021\_final.pdf](https://advances.realtimerendering.com/s2021/Karis_Nanite_SIGGRAPH_Advances_2021_final.pdf "")
- \[KWS25\]Elias Kristmann, Michael Wimmer and Markus Schütz
“Variable-Rate Texture Compression: Real-Time Rendering with JPEG”, 2025
arXiv: [https://arxiv.org/abs/2510.08166](https://arxiv.org/abs/2510.08166 "")
- \[LHLW10\]Fang Liu, Meng-Cheng Huang, Xue-Hui Liu and En-Hua Wu
“FreePipe: a programmable parallel rendering architecture for efficient multi-fragment effects”
In _Proceedings of the 2010 ACM SIGGRAPH Symposium on Interactive 3D Graphics and Games_, I3D ’10
Washington, D.C.: Association for Computing Machinery, 2010, pp. 75–82
DOI: [10.1145/1730804.1730817](https://dx.doi.org/10.1145/1730804.1730817 "")
- \[LK11\]Samuli Laine and Tero Karras
“High-performance software rasterization on GPUs”
In _Proceedings of the ACM SIGGRAPH Symposium on High Performance Graphics_, HPG ’11
Vancouver, British Columbia, Canada: Association for Computing Machinery, 2011, pp. 79–88
DOI: [10.1145/2018323.2018337](https://dx.doi.org/10.1145/2018323.2018337 "")
- \[LRSF\]Noah Lewis, Darius Rückert, Marc Stamminger and Linus Franke
“NePO: Neural Point Octrees for Large-Scale Novel View Synthesis”
In _Computer Graphics Forum_, pp. e70287
DOI: [https://doi.org/10.1111/cgf.70287](https://dx.doi.org/https://doi.org/10.1111/cgf.70287 "")
- \[MCEF94\]Steven Molnar, Michael Cox, David Ellsworth and Henry Fuchs
“A Sorting Classification of Parallel Rendering”
In _IEEE Computer Graphics and Applications_ 14.4IEEE, 1994, pp. 23–32
DOI: [10.1109/38.291528](https://dx.doi.org/10.1109/38.291528 "")
- \[MWH18\]Adam Marrs, Benjamin Watson and Christopher Healey
“View-warped Multi-view Soft Shadows for Local Area Lights”
In _Journal of Computer Graphics Techniques (JCGT)_ 7.3, 2018, pp. 1–28

- \[NVI25\]undef NVIDIA
“Visualizing Next-Gen Games With RTX Neural Rendering and Unreal Engine 5”, GDC 2025, 2025
URL: [https://www.youtube.com/watch?v=udqApkIqZmQ](https://www.youtube.com/watch?v=udqApkIqZmQ "")
- \[NVI25a\]undef NVIDIA
“Zorah” Export of NVIDIA RTX Kit - Zorah Sample as presented in "NVIDIA RTX Advances with Neural Rendering and Digital Human Technologies at GDC 2025", NVIDIA nvpro-samples, 2025
URL: [https://github.com/nvpro-samples/vk\_lod\_clusters/blob/main/README.md#zorah-demo-scene](https://github.com/nvpro-samples/vk_lod_clusters/blob/main/README.md#zorah-demo-scene "")
- \[Pin88\]Juan Pineda
“A parallel algorithm for polygon rasterization”
In _Proceedings of the 15th Annual Conference on Computer Graphics and Interactive Techniques_, SIGGRAPH ’88
New York, NY, USA: Association for Computing Machinery, 1988, pp. 17–20
DOI: [10.1145/54852.378457](https://dx.doi.org/10.1145/54852.378457 "")
- \[PTSO15\]Anjul Patney, Stanley Tzeng, Jr. Seitz and John D. Owens
“Piko: a framework for authoring programmable graphics pipelines”
In _ACM Trans. Graph._ 34.4New York, NY, USA: Association for Computing Machinery, 2015
DOI: [10.1145/2766973](https://dx.doi.org/10.1145/2766973 "")
- \[RFS22\]Darius Rückert, Linus Franke and Marc Stamminger
“Adop: Approximate differentiable one-pixel point rendering”
In _ACM Transactions on Graphics (ToG)_ 41.4ACM New York, NY, USA, 2022, pp. 1–14

- \[SD15\]Christoph Schied and Carsten Dachsbacher
“Deferred attribute interpolation for memory-efficient deferred shading”
In _Proceedings of the 7th Conference on High-Performance Graphics_, 2015, pp. 43–49

- \[SKW21\]Markus Schütz, Bernhard Kerbl and Michael Wimmer
“Rendering Point Clouds with Compute Shaders and Vertex Order Optimization”
In _techreport_ 40.4Eurographics Association, 2021, pp. 115–126
DOI: [10.1111/cgf.14345](https://dx.doi.org/10.1111/cgf.14345 "")
- \[Web15\]Thomas Weber
“Micropolygon Rendering on the GPU”, 2015
URL: [https://www.cg.tuwien.ac.at/research/publications/2015/WEBER-2015-PRA1/](https://www.cg.tuwien.ac.at/research/publications/2015/WEBER-2015-PRA1/ "")
- \[WHG84\]Hank Weghorst, Gary Hooper and Donald P. Greenberg
“Improved Computational Methods for Ray Tracing”
In _ACM Trans. Graph._ 3.1New York, NY, USA: Association for Computing Machinery, 1984, pp. 52–69
DOI: [10.1145/357332.357335](https://dx.doi.org/10.1145/357332.357335 "")

──────── [TRUNCATED] ────────
Showing 29,929 chars (head) + 9,443 chars (tail) of 66,473 total clean characters.
Full text saved to: /home/wubu/.hermes/profiles/mind-palace/cache/web/arxiv.org-f90bc234d0.md
To read the omitted middle: read_file path="/home/wubu/.hermes/profiles/mind-palace/cache/web/arxiv.org-f90bc234d0.md" offset=219 limit=200  (the file is the complete page; raise/lower offset to page through it).
─────────────────────────────