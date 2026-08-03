# Ultra-Fast Neural Video Compression

Source: https://arxiv.org/html/2606.04410v1
Ingested: 2026-08-03 (Media avenue, Kevin-Bacon growth wave)

---

Title:

Content selection saved. Describe the issue below:

Description:

![](https://arxiv.org/static/base/1.0.1/images/icons/smileybones-small.svg)arXiv is now an independent nonprofit! [Learn more](https://info.arxiv.org/about) ×

[License: arXiv.org perpetual non-exclusive license](https://info.arxiv.org/help/license/index.html#licenses-available)

arXiv:2606.04410v1 \[cs.CV\] 03 Jun 2026

# Ultra-Fast Neural Video Compression

Jiahao Li1, Wenxuan Xie1, Zhaoyang Jia2,   Bin Li1, Zongyu Guo1, Xiaoyi Zhang1, Yan Lu1

1Microsoft Research Asia, 2 University of Science and Technology of China

{li.jiahao,wenxie,libin,zongyuguo,xiaoyizhang,yanlu}@microsoft.com,jzy\_ustc@mail.ustc.edu.cnDone during Zhaoyang Jia’s internship at Microsoft Research Asia.

###### Abstract

While neural video codecs (NVCs) have demonstrated superior compression ratio, their prohibitive computational complexity remains a critical barrier to real-world deployment. This paper introduces a chunk-based coding framework designed to significantly improve the rate-distortion-complexity trade-off. Instead of processing frames sequentially, our approach encodes a chunk of multiple frames into a single compact latent representation and decodes them simultaneously. This is enabled by cross-frame interaction modules for joint spatial-temporal modeling and frame-specific decoders for parallel reconstruction. This paradigm not only dramatically enhances coding throughput but also facilitates more effective modeling of long-term temporal correlations. To further boost speed, we propose a streamlined entropy coding mechanism that consolidates bit-stream interactions into a single step, substantially reducing decoding overhead. Building on these innovations, we present DCVC-UF (Ultra-Fast), a new NVC that sets a new SOTA in performance. Our experiments show that DCVC-UF can achieve ultra-fast encoding and decoding speeds, significantly outperforming previous leading codecs. DCVC-UF serves as a notable landmark in the journey of NVC evolution. The code is at [https://github.com/microsoft/DCVC](https://github.com/microsoft/DCVC "").

## 1 Introduction

Neural video codecs (NVCs) have emerged as transformative technologies, offering unprecedented capabilities in removing video redundancy.
Recent works \[ [35](https://arxiv.org/html/2606.04410v1#bib.bib28 "Neural Video Compression with Feature Modulation"), [50](https://arxiv.org/html/2606.04410v1#bib.bib39 "Long-term temporal context gathering for neural video compression"), [22](https://arxiv.org/html/2606.04410v1#bib.bib65 "Towards practical real-time neural video compression"), [11](https://arxiv.org/html/2606.04410v1#bib.bib22 "Neural Video Compression with Spatio-Temporal Cross-Covariance Transformers"), [28](https://arxiv.org/html/2606.04410v1#bib.bib48 "NVRC: neural video representation compression"), [15](https://arxiv.org/html/2606.04410v1#bib.bib47 "GIViC: generative implicit video compression"), [43](https://arxiv.org/html/2606.04410v1#bib.bib49 "Diffusion-based perceptual neural video compression with temporal diffusion information reuse"), [51](https://arxiv.org/html/2606.04410v1#bib.bib14 "Generative latent coding for ultra-low bitrate image and video compression"), [66](https://arxiv.org/html/2606.04410v1#bib.bib105 "Single-step diffusion-based video coding with semantic-temporal guidance")\] have driven rapid progress in improving compression ratios, enabling NVCs to surpass conventional codecs such as H.266/VTM \[ [60](https://arxiv.org/html/2606.04410v1#bib.bib3 "VTM")\]. Despite these advances, the practical deployment of NVCs still faces significant challenges due to their substantial complexity in encoding or decoding.
Consequently, achieving a better trade-off among rate, distortion, and complexity is a critical research direction.

![Refer to caption](https://arxiv.org/html/2606.04410v1/x1.png)Figure 1: Encoding and decoding speed with actual bit-stream writing and reading on 1920×10801920\\times 1080 videos across different GPUs. Our DCVC-UF models achieve unprecedented encoding and decoding speeds, demonstrating strong scalability and advanced rate-distortion-complexity trade-off on general-purpose GPUs.

In response, recent approaches explore implicit neural representation (INR) \[ [57](https://arxiv.org/html/2606.04410v1#bib.bib66 "Implicit neural representations with periodic activation functions")\] or
Gaussian Splatting \[ [24](https://arxiv.org/html/2606.04410v1#bib.bib74 "3D gaussian splatting for real-time radiance field rendering.")\], where each video is overfitted into implicit parameters \[ [6](https://arxiv.org/html/2606.04410v1#bib.bib46 "Nerv: neural representations for videos"), [25](https://arxiv.org/html/2606.04410v1#bib.bib64 "C3: high-performance and low-complexity neural compression from a single image or video")\] or explicit 2D Gaussians \[ [17](https://arxiv.org/html/2606.04410v1#bib.bib73 "Neural video compression using 2d gaussian splatting"), [10](https://arxiv.org/html/2606.04410v1#bib.bib72 "Versatile video tokenization with generative 2d gaussian splatting")\]. Both paradigms offer low decoding complexity. However, as they need extensive online optimization for each video, their encoding complexity is quite high. For instance, \[ [17](https://arxiv.org/html/2606.04410v1#bib.bib73 "Neural video compression using 2d gaussian splatting")\] reports that the encoding times for INRs are usually in the order of 10−310^{-3} FPS.

Another direction to improve the compression ratio within limited computational budgets is to explore more efficient spatial-temporal correlation modeling. For traditional codec H.266/VTM, the hierarchical-B coding can achieve an average of 33.8% bitrate saving over the low-delay coding by introducing the bidirectional temporal prediction in a GOP (group of pictures). So, recent NVCs \[ [68](https://arxiv.org/html/2606.04410v1#bib.bib79 "Learning for video compression with hierarchical quality and recurrent enhancement"), [9](https://arxiv.org/html/2606.04410v1#bib.bib76 "B-canf: adaptive b-frame coding with conditional augmented normalizing flows"), [56](https://arxiv.org/html/2606.04410v1#bib.bib77 "Bi-directional deep contextual video compression"), [23](https://arxiv.org/html/2606.04410v1#bib.bib78 "BiECVC: gated diversification of bidirectional contexts for learned video compression")\] also follow a similar design to traditional hierarchical-B coding. However, they still operate on a frame-by-frame basis, where each frame relies on explicit motion vector for temporal prediction. The motion vector only captures the pixel displacement between only two frames, and cannot represent temporal correlation across multiple frames. Using different reference frames necessitates different motion vectors.
In addition, motion vector struggles to handle the complex video dynamics or new contents \[ [16](https://arxiv.org/html/2606.04410v1#bib.bib81 "Implicit motion function")\], incurs non-trivial additional bitrate cost, and especially introduces the substantial system complexity. Hence, the capability of these NVCs’ to balance rate, distortion, and complexity efficiently remains limited.

![Refer to caption](https://arxiv.org/html/2606.04410v1/x2.png)Figure 2: Comparison of coding paradigms, illustrated with an 8-frame example. (a) The commonly-used hierarchical-B coding operates frame-by-frame, particularly relying on the complex motion coding. Here we show two reference frame example for B frames. If using more reference frames, more corresponding motion codings are needed, increasing non-trivial cost. Moreover, the coding order adheres to a rigid, pre-defined hierarchy, and is not learnable. (b) Our chunk coding processes all frames of a chunk in parallel to automatically learn the spatial-temporal correlation. It eliminates the explicit motion, significantly enhances the throughput, and yet enables more efficient long-term temporal modeling.

In this paper, motivated by the spatial-temporal autoencoder \[ [18](https://arxiv.org/html/2606.04410v1#bib.bib101 "Video compression with rate-distortion autoencoders")\] and the motion vector-free DCVC-RT \[ [22](https://arxiv.org/html/2606.04410v1#bib.bib65 "Towards practical real-time neural video compression")\], we propose a chunk-based coding framework to address the aforementioned challenges. As shown in Fig. [2](https://arxiv.org/html/2606.04410v1#S1.F2 "Figure 2 ‣ 1 Introduction ‣ Ultra-Fast Neural Video Compression"), instead of sequentially processing the video frame-by-frame, our approach divides the video into non-overlapping chunks of multiple frames.
All frames within each chunk are then encoded into compact latent representations and decoded back simultaneously, which is designed to maximize coding throughput.
Within this chunk-based framework, our architecture employs cross-frame interaction modules to jointly and implicitly model spatial-temporal correlations across all frames. Complementing this, a set of frame-specific decoders works in parallel to reconstruct each frame, adaptively tailoring the synthesis process to individual frame characteristics.
This paradigm facilitates a more holistic compression strategy and also can maximize coding throughput.
It amplifies the advancements of the motion vector-free design from DCVC-RT, removing the costly, iterative, and sequential motion estimation, motion entropy coding, and compensation processes between many frame pairs. Our approach significantly reduces the operational complexity, such as memory I/O and function call overhead, which are critical bottlenecks for practical coding speed.

Our chunk-based coding also enables more efficient modeling of long-term temporal context. One important reason why the previous SOTA DCVC series \[ [34](https://arxiv.org/html/2606.04410v1#bib.bib27 "Neural video compression with diverse contexts"), [35](https://arxiv.org/html/2606.04410v1#bib.bib28 "Neural Video Compression with Feature Modulation"), [22](https://arxiv.org/html/2606.04410v1#bib.bib65 "Towards practical real-time neural video compression")\] surpasses leading traditional codecs is the enabling of the feature propagation mechanism in the latent space, which implicitly captures temporal correlations across multiple frames through joint training. Notably, DCVC-FM \[ [35](https://arxiv.org/html/2606.04410v1#bib.bib28 "Neural Video Compression with Feature Modulation")\] shows that the compression ratio can be significantly boosted by increasing the training video length from 7 to 32 frames. However, extending the training to longer video sequences is challenging because each frame’s separate latent representation incurs substantial training costs. In contrast, our framework encodes all frames in a chunk into a single compact latent, significantly reducing the latent size for a video. It allows for training on much longer video sequences within limited computational budgets, thereby facilitating the exploration of long-term temporal correlations to improve the chunk latent generation and the corresponding distribution estimation.

To further accelerate practical coding speed during the conversion between chunk latents and bit-streams, we introduce a streamlined entropy coding mechanism. Early NVCs \[ [32](https://arxiv.org/html/2606.04410v1#bib.bib23 "Deep contextual video compression"), [42](https://arxiv.org/html/2606.04410v1#bib.bib53 "An end-to-end learning framework for video compression")\] typically employ auto-regressive decoding \[ [47](https://arxiv.org/html/2606.04410v1#bib.bib54 "Joint autoregressive and hierarchical priors for learned image compression")\], which is inherently slow due to its sequential nature. The recent quadtree partition-based method in \[ [34](https://arxiv.org/html/2606.04410v1#bib.bib27 "Neural video compression with diverse contexts")\] reduces decoding steps to four by leveraging decoded partition latents to estimate the means and scales for the next partition in parallel. However, even this four-step interaction with the bit-stream incurs notable operational overhead, especially in real-time scenarios. Our proposed method further simplifies this process by estimating the scales for all partitions in a single step, while retaining the four-step estimation for the means to keep the spatial-channel correlation modeling capability. Since bit-stream decoding depends only on the scales, this allows us to consolidate bit-stream interactions into a single step, substantially reducing operational cost and improving bit-stream decoding efficiency.

Together, these advancements culminate in our proposed NVC, named DCVC-UF (Ultra-Fast), which builds upon the DCVC series to deliver exceptional encoding and decoding speeds. To accommodate diverse application scenarios, we introduce several configurations of DCVC-UF. Fig. [1](https://arxiv.org/html/2606.04410v1#S1.F1 "Figure 1 ‣ 1 Introduction ‣ Ultra-Fast Neural Video Compression") shows the performance comparison, where the VTM (Low-Delay, LD) is as the anchor for BD-Rate calculation.
When the chunk has multiple frames, we can achieve High-Throughput (HT) coding yet with high compression ratio. Our large version DCVC-UF (HT-L) saves an average of 42.2% bitrate, with achieving 371.1 encoding and 273.6 decoding FPS for 1080p video with 4090 GPU. Our small version DCVC-UF (HT-S) achieves 31.6% bitrate saving. The encoding and decoding speeds are boosted to 655.9 and 453.3, respectively.
These two models will introduce the delay related to the chunk size, analogous to the hierarchical-B coding manner. So, to meet the low-delay requirement, we can also set the chunk size to 1 frame, i.e., DCVC-UF (LD), which can achieve 9.5% bitrate saving, yet achieve 313.6 encoding and 353.8 decoding FPS with 4090 GPU.
Unlike traditional codecs requiring bespoke hardware, our NVC framework is built on general-purpose GPUs, allowing it to automatically benefit from rapid advancements in AI accelerators without re-engineering. This inherent scalability is demonstrated as the speeds of our models consistently improve across GPU generations, from consumer cards to datacenter accelerators like the B200, where DCVC-UF (HT-S) sets a new throughput record of 1415.1 encoding and 945.8 decoding FPS for 1080p. As consumer GPUs advance in the future, their speeds will also rise automatically.

Our main contributions are summarized as follows:

- •


We propose a chunk-based coding framework. It encodes a chunk of frames into a single compact latent and decodes back simultaneously, leveraging cross-frame interaction and frame-specific decoders. This design significantly enhances coding throughput and enables more efficient modeling of long-term temporal context.

- •


We design a streamlined entropy coding mechanism that decouples the estimation of scales and means for latent partitions. This allows bit-stream interactions to be consolidated into a single step, substantially reducing operational overhead and accelerating practical decoding speed.

- •


Extensive experiments demonstrate that our model, DCVC-UF, establishes a new record of the rate-distortion-complexity performance, significantly outperforming previous SOTA codecs across various settings.


## 2 Related Work

### 2.1 Low-Delay Neural Video Compression

Low-delay coding constrains the coding of the current frame to reference only previously decoded frames in temporal order, suitable for real-time communication applications. Many methods \[ [41](https://arxiv.org/html/2606.04410v1#bib.bib82 "DVC: an end-to-end deep video compression framework"), [21](https://arxiv.org/html/2606.04410v1#bib.bib19 "Improving deep video compression by resolution-adaptive flow coding"), [53](https://arxiv.org/html/2606.04410v1#bib.bib83 "ELF-VC: efficient learned flexible-rate video coding"), [42](https://arxiv.org/html/2606.04410v1#bib.bib53 "An end-to-end learning framework for video compression"), [36](https://arxiv.org/html/2606.04410v1#bib.bib32 "M-LVC: Multiple frames prediction for learned video compression"), [1](https://arxiv.org/html/2606.04410v1#bib.bib33 "Scale-space flow for end-to-end optimized video compression"), [38](https://arxiv.org/html/2606.04410v1#bib.bib18 "Neural video coding using multiscale motion compensation and spatiotemporal context model"), [37](https://arxiv.org/html/2606.04410v1#bib.bib20 "MMVC: Learned Multi-Mode Video Compression with Block-based Prediction Mode Selection and Density-Adaptive Entropy Coding"), [44](https://arxiv.org/html/2606.04410v1#bib.bib88 "Uncertainty-Aware Deep Video Compression with Ensembles")\] adopt a residual coding inspired by traditional codecs, which requires a complex motion estimation, entropy coding, and compensation pipeline. The emerging conditional coding \[ [39](https://arxiv.org/html/2606.04410v1#bib.bib42 "Conditional entropy coding for efficient video compression"), [29](https://arxiv.org/html/2606.04410v1#bib.bib41 "Conditional Coding for Flexible Learned Video Compression"), [20](https://arxiv.org/html/2606.04410v1#bib.bib86 "CANF-vc: conditional augmented normalizing flows for video compression"), [45](https://arxiv.org/html/2606.04410v1#bib.bib36 "VCT: a video compression transformer"), [32](https://arxiv.org/html/2606.04410v1#bib.bib23 "Deep contextual video compression"), [55](https://arxiv.org/html/2606.04410v1#bib.bib84 "Temporal Context Mining for Learned Video Compression"), [33](https://arxiv.org/html/2606.04410v1#bib.bib85 "Hybrid spatial-temporal entropy modelling for neural video compression"), [52](https://arxiv.org/html/2606.04410v1#bib.bib26 "Motion information propagation for neural video compression"), [34](https://arxiv.org/html/2606.04410v1#bib.bib27 "Neural video compression with diverse contexts"), [35](https://arxiv.org/html/2606.04410v1#bib.bib28 "Neural Video Compression with Feature Modulation"), [50](https://arxiv.org/html/2606.04410v1#bib.bib39 "Long-term temporal context gathering for neural video compression")\] shows larger potential as the temporal context is not limited to pixel-domain prediction but can be any flexible feature. However, most of them still suffer from limited coding speed, as they often incorporate complex modules—particularly those related to motion processing. While some works \[ [59](https://arxiv.org/html/2606.04410v1#bib.bib37 "Mobilenvc: real-time 1080p neural video compression on a mobile device"), [30](https://arxiv.org/html/2606.04410v1#bib.bib38 "Mobilecodec: neural inter-frame video compression on mobile devices"), [58](https://arxiv.org/html/2606.04410v1#bib.bib75 "Towards real-time neural video codec for cross-platform application using calibration information")\] prioritize acceleration, their compression performance lags behind leading approaches.

### 2.2 Delay-Relaxed Neural Video Compression

Relaxing the frame reference constraint enables a larger design space, but increases delay. It is suited for delay-insensitive scenarios like offline storage and video streaming.

Hierarchical-B Coding. Drawing inspiration from the significant compression gains of hierarchical-B coding over low-delay configurations in traditional codecs, several recent NVCs \[ [12](https://arxiv.org/html/2606.04410v1#bib.bib80 "Neural inter-frame compression for video coding"), [68](https://arxiv.org/html/2606.04410v1#bib.bib79 "Learning for video compression with hierarchical quality and recurrent enhancement"), [9](https://arxiv.org/html/2606.04410v1#bib.bib76 "B-canf: adaptive b-frame coding with conditional augmented normalizing flows"), [56](https://arxiv.org/html/2606.04410v1#bib.bib77 "Bi-directional deep contextual video compression"), [23](https://arxiv.org/html/2606.04410v1#bib.bib78 "BiECVC: gated diversification of bidirectional contexts for learned video compression")\] have adopted analogous coding structures. This allows frames to reference both past and future frames for improved prediction. However, these methods still operate on a frame-by-frame basis, relying on explicit motion vectors to align two frames. To mitigate the bitrate overhead of motion vectors, some approaches \[ [9](https://arxiv.org/html/2606.04410v1#bib.bib76 "B-canf: adaptive b-frame coding with conditional augmented normalizing flows"), [56](https://arxiv.org/html/2606.04410v1#bib.bib77 "Bi-directional deep contextual video compression")\] introduce complex motion vector prediction modules, which in turn increase both computational and operational costs.

Online Optimization-based Coding. This paradigm trains a specialized model for each video instance. INR-based methods \[ [6](https://arxiv.org/html/2606.04410v1#bib.bib46 "Nerv: neural representations for videos"), [27](https://arxiv.org/html/2606.04410v1#bib.bib68 "Hinerv: video compression with hierarchical encoding-based neural representation"), [5](https://arxiv.org/html/2606.04410v1#bib.bib67 "Hnerv: a hybrid neural representation for videos"), [28](https://arxiv.org/html/2606.04410v1#bib.bib48 "NVRC: neural video representation compression"), [25](https://arxiv.org/html/2606.04410v1#bib.bib64 "C3: high-performance and low-complexity neural compression from a single image or video"), [14](https://arxiv.org/html/2606.04410v1#bib.bib45 "Pnvc: towards practical inr-based video compression")\] overfit a small neural network to represent a video, and decode frames by querying the network with coordinates. However, INRs are inefficient for high-resolution video \[ [17](https://arxiv.org/html/2606.04410v1#bib.bib73 "Neural video compression using 2d gaussian splatting")\]. Consequently, recent works \[ [40](https://arxiv.org/html/2606.04410v1#bib.bib71 "An exploration with entropy constrained 3d gaussians for 2d video compression"), [64](https://arxiv.org/html/2606.04410v1#bib.bib70 "GSVC: efficient video representation and compression through 2d gaussian splatting"), [17](https://arxiv.org/html/2606.04410v1#bib.bib73 "Neural video compression using 2d gaussian splatting"), [31](https://arxiv.org/html/2606.04410v1#bib.bib69 "GaussianVideo: efficient video representation and compression by gaussian splatting"), [10](https://arxiv.org/html/2606.04410v1#bib.bib72 "Versatile video tokenization with generative 2d gaussian splatting")\] explore explicit representations using Gaussian Splatting \[ [24](https://arxiv.org/html/2606.04410v1#bib.bib74 "3D gaussian splatting for real-time radiance field rendering.")\]. This method associates Gaussian parameters with video regions, enabling scalable representation and faster rendering \[ [17](https://arxiv.org/html/2606.04410v1#bib.bib73 "Neural video compression using 2d gaussian splatting")\]. While both approaches achieve high decoding speed, their per-video online optimization results in extremely high encoding complexity.

![Refer to caption](https://arxiv.org/html/2606.04410v1/x3.png)Figure 3: Framework overview of our DCVC-UF. DC Block, Q, AE and AD represent depth-wise convolution block, quantization, arithmetic encoder and decoder, respectively. After the patchify, the input chunk XiX\_{i} (comprising NN frames) directly encoded and decoded into feature FiF\_{i}, conditioned on the temporal chunk context CiC\_{i}. FiF\_{i} is then reconstructed into pixel domain using the frame-specific decoders. q​piqp\_{i} is the input quantization parameter. The number of DC Blocks for each module is detailed in the supplementary material. In DCVC-UF, all frames in the chunk are processed in parallel to enable the high-throughput coding.

Spatial-Temporal Autoencoders. In video generation, spatial-temporal autoencoders serve as powerful tokenizers, compressing raw pixels into a compact latent space to mitigate the prohibitive computational costs of generation in the pixel domain \[ [18](https://arxiv.org/html/2606.04410v1#bib.bib101 "Video compression with rate-distortion autoencoders")\]. Recent works \[ [71](https://arxiv.org/html/2606.04410v1#bib.bib94 "Open-sora: democratizing efficient video production for all"), [70](https://arxiv.org/html/2606.04410v1#bib.bib89 "Cv-vae: a compatible video vae for latent generative video models"), [8](https://arxiv.org/html/2606.04410v1#bib.bib95 "Od-vae: an omni-dimensional video compressor for improving latent video diffusion model"), [63](https://arxiv.org/html/2606.04410v1#bib.bib96 "Omnitokenizer: a joint image-video tokenizer for visual generation"), [69](https://arxiv.org/html/2606.04410v1#bib.bib97 "Cogvideox: text-to-video diffusion models with an expert transformer"), [26](https://arxiv.org/html/2606.04410v1#bib.bib98 "Hunyuanvideo: a systematic framework for large video generative models"), [65](https://arxiv.org/html/2606.04410v1#bib.bib99 "Improved video vae for latent video diffusion model"), [61](https://arxiv.org/html/2606.04410v1#bib.bib100 "Wan: open and advanced large-scale video generative models")\] commonly employ configurations with spatial (e.g., 8x) and temporal (e.g., 4x) compression \[ [7](https://arxiv.org/html/2606.04410v1#bib.bib93 "DC-videogen: efficient video generation with deep compression video autoencoder")\]. While primarily designed for generation, the underlying principle of converting raw video chunk into compact representations makes these autoencoders a promising foundation for developing efficient video codecs. Actually, early works \[ [18](https://arxiv.org/html/2606.04410v1#bib.bib101 "Video compression with rate-distortion autoencoders"), [49](https://arxiv.org/html/2606.04410v1#bib.bib102 "End-to-end learning of video compression using spatio-temporal autoencoders")\] explored NVCs based on spatial-temporal autoencoder. However, \[ [18](https://arxiv.org/html/2606.04410v1#bib.bib101 "Video compression with rate-distortion autoencoders"), [49](https://arxiv.org/html/2606.04410v1#bib.bib102 "End-to-end learning of video compression using spatio-temporal autoencoders")\] use a vanilla autoencoder to mainly learn the the inner correlation within a single chunk. The correlation across chunks is ignored, leading to their limited compression ratio.

Our work advances the spatial-temporal autoencoder paradigm for NVC. Within a chunk, unlike \[ [18](https://arxiv.org/html/2606.04410v1#bib.bib101 "Video compression with rate-distortion autoencoders"), [49](https://arxiv.org/html/2606.04410v1#bib.bib102 "End-to-end learning of video compression using spatio-temporal autoencoders")\], our autoencoder not only has cross-frame interaction modules for joint spatial-temporal modeling but also has frame-specific decoders tailoring the synthesis process to individual frame characteristics. Across different chunks, we build the efficient conditional coding, where temporal propagation is enabled to capture the implicit long-term correlation therein. In addition, we propose a streamlined entropy coding mechanism that consolidates bit-stream interactions into a single step, substantially accelerating the decoding. These make our NVC achieve significant rate-distortion-complexity trade-off advantage over \[ [18](https://arxiv.org/html/2606.04410v1#bib.bib101 "Video compression with rate-distortion autoencoders"), [49](https://arxiv.org/html/2606.04410v1#bib.bib102 "End-to-end learning of video compression using spatio-temporal autoencoders")\] and other previous SOTA codecs.

## 3 Proposed Method

### 3.1 Overview

As depicted in Fig. [3](https://arxiv.org/html/2606.04410v1#S2.F3 "Figure 3 ‣ 2.2 Delay-Relaxed Neural Video Compression ‣ 2 Related Work ‣ Ultra-Fast Neural Video Compression"), our DCVC-UF is architected around a chunk-coding paradigm, building upon the DCVC series \[ [34](https://arxiv.org/html/2606.04410v1#bib.bib27 "Neural video compression with diverse contexts"), [35](https://arxiv.org/html/2606.04410v1#bib.bib28 "Neural Video Compression with Feature Modulation"), [22](https://arxiv.org/html/2606.04410v1#bib.bib65 "Towards practical real-time neural video compression")\]. The input video is first segmented into non-overlapping chunks. For a given chunk Xi={xi,0,…,xi,N−1}X\_{i}=\\{x\_{i,0},\\dots,x\_{i,N-1}\\} containing NN frames, the process begins by transforming it to 1/8 resolution via patch embedding. It is then conditioned on the temporal chunk context CiC\_{i}, and fed into a chunk encoder. The role of the encoder is to distill the spatial-temporal information of the entire chunk into a compact latent representation yiy\_{i} efficiently. This latent is then quantized (y^i\\hat{y}\_{i}) and efficiently converted into a bit-stream. During decoding, the process is reversed. The latent representation y^i\\hat{y}\_{i} is parsed from the bit-stream and fed to the chunk decoder, which generates a rich feature FiF\_{i}.
This feature serves a dual purpose: it is used by a set of parallel, frame-specific decoders to reconstruct the individual frames {x^i,0,…,x^i,N−1}\\{\\hat{x}\_{i,0},\\dots,\\hat{x}\_{i,N-1}\\}, and it is also propagated to the next chunk to form the next temporal context.
DCVC-UF originates from the low-delay NVC DCVC-RT \[ [22](https://arxiv.org/html/2606.04410v1#bib.bib65 "Towards practical real-time neural video compression")\] which eliminates the explicit motion-related operations. DCVC-UF amplifies its advantage and enables high-throughput coding via our chunk-coding. DCVC-UF can boost the compression ratio of NVC to a new level with our frame-specific decoders (Sec. [3.2](https://arxiv.org/html/2606.04410v1#S3.SS2 "3.2 Frame-Specific Decoders ‣ 3 Proposed Method ‣ Ultra-Fast Neural Video Compression")) and efficient long-term correlation learning (Sec. [3.3](https://arxiv.org/html/2606.04410v1#S3.SS3 "3.3 Efficient Long-Term Correlation Learning ‣ 3 Proposed Method ‣ Ultra-Fast Neural Video Compression")). In particular, our DCVC-UF also achieves unprecedented coding speed with our streamlined entropy model (Sec. [3.4](https://arxiv.org/html/2606.04410v1#S3.SS4 "3.4 Streamlined Entropy Model ‣ 3 Proposed Method ‣ Ultra-Fast Neural Video Compression")).

### 3.2 Frame-Specific Decoders

Existing spatial-temporal autoencoders typically employ a unified decoder that applies identical reconstruction processes to all frames within a chunk. While this unified approach is straightforward to implement, it faces limitations when dealing with diverse contents. A single decoder must learn to handle all possible variations across different temporal positions, leading to a challenging optimization problem where the decoder needs to be a “jack of all trades”. This often results in suboptimal reconstruction quality, as the decoder cannot fully specialize for the distinct characteristics that may appear at different temporal positions within a chunk. To address these limitations, we additionally design frame-specific decoders in our chunk-based framework, where each frame index within the chunk is assigned its own dedicated decoder. As illustrated in Fig. [3](https://arxiv.org/html/2606.04410v1#S2.F3 "Figure 3 ‣ 2.2 Delay-Relaxed Neural Video Compression ‣ 2 Related Work ‣ Ultra-Fast Neural Video Compression"), after the chunk decoder generates the rich feature representation FiF\_{i} containing spatial-temporal information for all NN frames, we deploy NN distinct decoders operating in parallel. Each decoder specializes in reconstructing the frame at its corresponding position.

This design shares some similarities with Mixture of Experts (MoE) \[ [54](https://arxiv.org/html/2606.04410v1#bib.bib104 "Scaling vision with sparse mixture of experts")\] architecture, where specialized components handle different aspects of the content. In our case, each frame-specific decoder acts as an “expert” for its temporal position, allowing the model to better adapt to varying video content characteristics. By distributing the reconstruction task across multiple specialized decoders, we can achieve several advantages: (1) Each decoder can focus on learning patterns most relevant to its position, reducing the complexity of individual decoder optimization; (2) The parallel architecture naturally aligns with our chunk-based processing, enabling simultaneous reconstruction without sequential dependencies; (3) The specialization allows for more efficient parameter utilization, as each decoder can allocate its capacity to the specific challenges of its assigned position rather than attempting to generalize across all positions.

### 3.3 Efficient Long-Term Correlation Learning

One of the key advantages of our chunk-based coding framework is its ability to efficiently model long-term temporal correlations. Previous DCVC series have demonstrated that feature propagation mechanisms in the latent space can implicitly capture temporal correlations across multiple frames through joint training, which is a primary factor in their superiority over traditional codecs. Notably, DCVC-FM \[ [35](https://arxiv.org/html/2606.04410v1#bib.bib28 "Neural Video Compression with Feature Modulation")\] showed that compression ratio can be significantly improved by extending training video length from 7 to 32 frames, enabling the model to learn more comprehensive temporal dependencies. However, scaling to even longer sequence in frame-based approaches faces fundamental limitations: each frame requires its own latent representation, leading to large training cost. This constraint severely restricts the temporal context that can be practically leveraged during training.

Our chunk-based architecture fundamentally addresses this limitation by encoding all frames within a chunk into a single compact latent representation. This design dramatically reduces the total latent size for a video sequence. This compact representation enables training on much longer sequence (if the batch size is 1, it can be up to 1,024 frames at 512×512512\\times 512 spatial size, within 24GB GPU memory cost), letting the model capture long-term temporal correlation. The extended temporal context benefits both the chunk latent generation process and the entropy model’s distribution estimation. During training, the model learns to identify and exploit recurring patterns, scene structures, and dynamics that span across multiple chunks. The propagated chunk context CiC\_{i} carries forward essential information, helping subsequent chunks achieve higher compression efficiency.

### 3.4 Streamlined Entropy Model

![Refer to caption](https://arxiv.org/html/2606.04410v1/x4.png)Figure 4:  (a) A quadtree-like partition \[ [34](https://arxiv.org/html/2606.04410v1#bib.bib27 "Neural video compression with diverse contexts")\] for y^i\\hat{y}\_{i} is adopted. (b) Previous methods require interleaved entropy decoding and parameter estimation, which hinders practical decoding speed. (c) Our streamlined entropy model consolidates bit-stream manipulations into a single step, substantially accelerating decoding.Table 1: BD-Rate (%) comparison in YUV420 colorspace. All frames are tested.

|     |     |     |     |     |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Method | UVG | MCL-JCV | HEVC B | HEVC C | HEVC D | HEVC E | Average | Coding Speed |
| Enc. | Dec. |
| Low-Delay (LD) Codecs |  |  |  |  |  |  |  |  |  |
| VTM-17.0 (LD) | 0.00.0 | 0.00.0 | 0.00.0 | 0.00.0 | 0.00.0 | 0.00.0 | 0.00.0 | 0.010.01 FPS | 23.623.6 FPS |
| HM-16.25 (LD) | 40.140.1 | 48.648.6 | 47.647.6 | 41.041.0 | 34.534.5 | 42.842.8 | 42.442.4 | 0.050.05 FPS | 39.639.6 FPS |
| DCVC-DC | 6.56.5 | −4.4-4.4 | 13.113.1 | −3.4-3.4 | −14.8-14.8 | 90.290.2 | 14.514.5 | 2.32.3 FPS | 2.92.9 FPS |
| DCVC-FM | −16.8-16.8 | −8.0-8.0 | −15.4-15.4 | −30.2-30.2 | −37.5-37.5 | −20.2-20.2 | −21.3-21.3 | 3.73.7 FPS | 4.44.4 FPS |
| DCVC-RT | −24.0-24.0 | −14.8-14.8 | −16.6-16.6 | −21.0-21.0 | −27.3-27.3 | −22.4-22.4 | −21.0-21.0 | 118.8118.8 FPS | 105.3105.3 FPS |
| DCVC-UF (LD) | −15.3-15.3 | −0.3-0.3 | −3.3-3.3 | −6.5-6.5 | −16.6-16.6 | −15.0-15.0 | −9.5-9.5 | 313.6\\mathbf{313.6} FPS | 353.8\\mathbf{353.8} FPS |
| Delay-Relaxed Codecs |  |  |  |  |  |  |  |  |  |
| HM-16.25 (Hierarchical-B) | 4.94.9 | 17.317.3 | 12.612.6 | 11.311.3 | 3.23.2 | 1.61.6 | 8.58.5 | 0.060.06 FPS | 40.040.0 FPS |
| VTM-17.0 (Hierarchical-B) | −34.0-34.0 | −30.4-30.4 | −35.4-35.4 | −32.4-32.4 | −32.5-32.5 | −38.1-38.1 | −33.8-33.8 | 0.010.01 FPS | 23.123.1 FPS |
| DCVC-UF (HT-S) | −28.8-28.8 | −12.9-12.9 | −17.6-17.6 | −29.4-29.4 | −42.2-42.2 | −58.8-58.8 | −31.6-31.6 | 655.9\\mathbf{655.9} FPS | 453.3\\mathbf{453.3} FPS |
| DCVC-UF (HT-L) | −39.6-39.6 | −24.4-24.4 | −33.3-33.3 | −41.2-41.2 | −51.7-51.7 | −63.2-63.2 | −42.2-42.2 | 371.1\\mathbf{371.1} FPS | 273.6\\mathbf{273.6} FPS |

- •


Intra-period=–1 for all codecs and settings. The coding speeds of NVCs are tested on 1920×10801920\\times 1080 videos with 4090 GPU.


Efficient entropy coding is crucial for achieving high practical coding speed, as it directly determines how quickly latent representations can be converted to and from bit-streams. Recently, the quadtree partition-based entropy coding \[ [34](https://arxiv.org/html/2606.04410v1#bib.bib27 "Neural video compression with diverse contexts"), [35](https://arxiv.org/html/2606.04410v1#bib.bib28 "Neural Video Compression with Feature Modulation")\] was proposed to explore spatial-channel correlations efficiently, demonstrating significantly higher decoding speed than the famous auto-regressive model \[ [47](https://arxiv.org/html/2606.04410v1#bib.bib54 "Joint autoregressive and hierarchical priors for learned image compression")\]. As illustrated in Fig. [4](https://arxiv.org/html/2606.04410v1#S3.F4 "Figure 4 ‣ 3.4 Streamlined Entropy Model ‣ 3 Proposed Method ‣ Ultra-Fast Neural Video Compression") (a) and (b), the quantized latent y^i\\hat{y}\_{i} is divided into four partitions, where each partition’s decoding depends on previously decoded partitions to estimate its distribution parameters (mean μ\\mu and scale σ\\sigma). However, even this four-step process still incurs substantial operational overhead. As highlighted in Fig. [4](https://arxiv.org/html/2606.04410v1#S3.F4 "Figure 4 ‣ 3.4 Streamlined Entropy Model ‣ 3 Proposed Method ‣ Ultra-Fast Neural Video Compression") (b), the repeated bit-stream manipulations involve multiple arithmetic decoding calls, memory I/O operations, and costly synchronization between arithmetic decoding operations and neural network inference. If the arithmetic decoding is performed on CPU, the cross-device switching and synchronization between CPU and GPU further exacerbate the operational burden.

To address these bottlenecks, we first revisit the entropy coding process. During encoding, the entropy model estimates the mean μi\\mu\_{i} and scale σi\\sigma\_{i} for the latent yiy\_{i}, typically assuming a Gaussian distribution. After quantization via r^i=round​(yi−μi)\\hat{r}\_{i}=\\text{round}(y\_{i}-\\mu\_{i}), the result r^i\\hat{r}\_{i} is arithmetically encoded using scale σi\\sigma\_{i}. During decoding, r^i\\hat{r}\_{i} is recovered using only σi\\sigma\_{i}, and the final latent is reconstructed as y^i=r^i+μi\\hat{y}\_{i}=\\hat{r}\_{i}+\\mu\_{i}. The key insight is that bit-stream operations depend exclusively on scale parameters, which define the distribution width, while mean parameters merely shift the distribution center and can be applied post-decoding. This observation motivates us to decouple the estimation of means and scales, departing from the coupled approach in \[ [34](https://arxiv.org/html/2606.04410v1#bib.bib27 "Neural video compression with diverse contexts"), [35](https://arxiv.org/html/2606.04410v1#bib.bib28 "Neural Video Compression with Feature Modulation")\]. As shown in Fig. [4](https://arxiv.org/html/2606.04410v1#S3.F4 "Figure 4 ‣ 3.4 Streamlined Entropy Model ‣ 3 Proposed Method ‣ Ultra-Fast Neural Video Compression") (c), our parameter estimation network takes the prior input sis\_{i} (derived from hyper-prior z^i\\hat{z}\_{i} and temporal context CiC\_{i}) and simultaneously predicts the mean μi0\\mu\_{i}^{0} for the first partition and the scales σi\\sigma\_{i} for all four partitions in a single forward pass.

This architectural innovation enables a dramatic acceleration of the decoding pipeline. By eliminating sequential dependencies in scale estimation, we can perform arithmetic decoding for all partitions in one consolidated step, drastically reducing bit-stream manipulation overhead. We retain the four-step progressive estimation for means to preserve spatial-channel correlation modeling capacity, but since mean estimation requires no bit-stream interaction, it executes entirely on GPU without costly synchronization. This design minimizes memory transfer between processing units, eliminates multi-step decoding latency, and removes switching overhead between arithmetic operations and neural network inference. Our streamlined entropy model allows for better GPU utilization, combined with our chunk-based coding framework, enables DCVC-UF to achieve unprecedented decoding speed.

![Refer to caption](https://arxiv.org/html/2606.04410v1/x5.png)Figure 5: Rate-distortion curves on UVG dataset. BPP means bits per pixel. More curves are in the supplementary material.

## 4 Experimental Results

### 4.1 Experimental Settings

Implementation Details. For delay-relaxed scenario, our high-throughput (HT) codec DCVC-UF uses a chunk size of N=8N=8. We provide two network scales—DCVC-UF (HT-S) and DCVC-UF (HT-L)—denoting small and large models, respectively. To enable low-delay (LD) operation, the framework also supports N=1N=1 (single-frame chunk), i.e., DCVC-UF (LD).

Training Details. Following \[ [35](https://arxiv.org/html/2606.04410v1#bib.bib28 "Neural Video Compression with Feature Modulation")\], we first train DCVC-UF codecs on the ready-made 7-frame Vimeo-90k \[ [67](https://arxiv.org/html/2606.04410v1#bib.bib11 "Video enhancement with task-oriented flow")\] dataset, then fine-tune using longer sequences generated from original Vimeo videos \[ [48](https://arxiv.org/html/2606.04410v1#bib.bib5 "Original Vimeo links")\]. As discussed in Section [3.3](https://arxiv.org/html/2606.04410v1#S3.SS3 "3.3 Efficient Long-Term Correlation Learning ‣ 3 Proposed Method ‣ Ultra-Fast Neural Video Compression"), our chunk coding enables training on 512×512512\\times 512 videos with up to 1024 frames. However, assembling diverse, high-quality videos of such length is still challenging. Therefore, we currently fine-tune with 128-frame sequences and leave the exploration of longer training datasets for future work.

Testing Details. We evaluate on HEVC Class B∼\\simE \[ [13](https://arxiv.org/html/2606.04410v1#bib.bib12 "Common Test Conditions and Software Reference Configurations for HEVC Range Extensions, document JCTVC-N1006")\], UVG \[ [46](https://arxiv.org/html/2606.04410v1#bib.bib9 "UVG dataset: 50/120fps 4K sequences for video codec analysis and development")\], and MCL-JCV \[ [62](https://arxiv.org/html/2606.04410v1#bib.bib10 "MCL-JCV: a JND-based H. 264/AVC video quality assessment dataset")\]. For traditional codecs, we compare with HM \[ [19](https://arxiv.org/html/2606.04410v1#bib.bib2 "HM")\] and VTM \[ [60](https://arxiv.org/html/2606.04410v1#bib.bib3 "VTM")\], representing the best H.265 and H.266 encoders, respectively. Configuration details are in the supplementary material. For NVCs, we compare with previous SOTA DCVC series \[ [34](https://arxiv.org/html/2606.04410v1#bib.bib27 "Neural video compression with diverse contexts"), [35](https://arxiv.org/html/2606.04410v1#bib.bib28 "Neural Video Compression with Feature Modulation"), [22](https://arxiv.org/html/2606.04410v1#bib.bib65 "Towards practical real-time neural video compression")\], with all models tested using actual bit-stream writing and decoding. Compression ratio is measured by BD-Rate \[ [4](https://arxiv.org/html/2606.04410v1#bib.bib13 "Calculation of average PSNR differences between RD-curves")\], where positive values indicate a bitrate increase and negative values indicate savings. Video quality is reported using PSNR, with all frames evaluated in YUV420 colorspace. For both low-delay and delay-relaxed settings, the intra-period is set to –1 for all codecs to present their best compression ratios. We measure coding speed on a single GPU, using a sequential chunk-by-chunk coding process (chunk size N=8N=8 for HT and N=1N=1 for LD). We currently do not employ cross-chunk pipeline parallelism (e.g., overlapping the network inference and entropy coding of different chunks), indicating potential for further acceleration.

### 4.2 Comparisons with Previous SOTA Methods


[... middle omitted — see footer ...]


- \[61\]T. Wan, A. Wang, B. Ai, B. Wen, C. Mao, C. Xie, D. Chen, F. Yu, H. Zhao, J. Yang, et al. (2025)Wan: open and advanced large-scale video generative models.
arXiv preprint arXiv:2503.20314.
Cited by: [§2.2](https://arxiv.org/html/2606.04410v1#S2.SS2.p4.1 "2.2 Delay-Relaxed Neural Video Compression ‣ 2 Related Work ‣ Ultra-Fast Neural Video Compression").

- \[62\]H. Wang, W. Gan, S. Hu, J. Y. Lin, L. Jin, L. Song, P. Wang, I. Katsavounidis, A. Aaron, and C. J. Kuo (2016)MCL-JCV: a JND-based H. 264/AVC video quality assessment dataset.
In 2016 IEEE international conference on image processing (ICIP),
pp. 1509–1513.
Cited by: [§4.1](https://arxiv.org/html/2606.04410v1#S4.SS1.p1.6 "4.1 Experimental Settings ‣ 4 Experimental Results ‣ Ultra-Fast Neural Video Compression").

- \[63\]J. Wang, Y. Jiang, Z. Yuan, B. Peng, Z. Wu, and Y. Jiang (2024)Omnitokenizer: a joint image-video tokenizer for visual generation.
Advances in Neural Information Processing Systems37,  pp. 28281–28295.
Cited by: [§2.2](https://arxiv.org/html/2606.04410v1#S2.SS2.p4.1 "2.2 Delay-Relaxed Neural Video Compression ‣ 2 Related Work ‣ Ultra-Fast Neural Video Compression").

- \[64\]L. Wang, Y. Shi, and W. T. Ooi (2025)GSVC: efficient video representation and compression through 2d gaussian splatting.
In Proceedings of the 35th Workshop on Network and Operating System Support for Digital Audio and Video,
pp. 15–21.
Cited by: [§2.2](https://arxiv.org/html/2606.04410v1#S2.SS2.p3.1 "2.2 Delay-Relaxed Neural Video Compression ‣ 2 Related Work ‣ Ultra-Fast Neural Video Compression").

- \[65\]P. Wu, K. Zhu, Y. Liu, L. Zhao, W. Zhai, Y. Cao, and Z. Zha (2025)Improved video vae for latent video diffusion model.
In Proceedings of the Computer Vision and Pattern Recognition Conference,
pp. 18124–18133.
Cited by: [§2.2](https://arxiv.org/html/2606.04410v1#S2.SS2.p4.1 "2.2 Delay-Relaxed Neural Video Compression ‣ 2 Related Work ‣ Ultra-Fast Neural Video Compression").

- \[66\]N. Xue, Z. Jia, J. Li, B. Li, Z. Zheng, Y. Zhang, and Y. Lu (2026)Single-step diffusion-based video coding with semantic-temporal guidance.
In IEEE/CVF Conference on Computer Vision and Pattern Recognition, CVPR,
Cited by: [§1](https://arxiv.org/html/2606.04410v1#S1.p1.1 "1 Introduction ‣ Ultra-Fast Neural Video Compression").

- \[67\]T. Xue, B. Chen, J. Wu, D. Wei, and W. T. Freeman (2019)Video enhancement with task-oriented flow.
International Journal of Computer Vision127 (8),  pp. 1106–1125.
Cited by: [§4.1](https://arxiv.org/html/2606.04410v1#S4.SS1.p1.6 "4.1 Experimental Settings ‣ 4 Experimental Results ‣ Ultra-Fast Neural Video Compression").

- \[68\]R. Yang, F. Mentzer, L. V. Gool, and R. Timofte (2020)Learning for video compression with hierarchical quality and recurrent enhancement.
In Proceedings of the IEEE/CVF Conference on Computer Vision and Pattern Recognition,
pp. 6628–6637.
Cited by: [§1](https://arxiv.org/html/2606.04410v1#S1.p3.1 "1 Introduction ‣ Ultra-Fast Neural Video Compression"),
[§2.2](https://arxiv.org/html/2606.04410v1#S2.SS2.p2.1 "2.2 Delay-Relaxed Neural Video Compression ‣ 2 Related Work ‣ Ultra-Fast Neural Video Compression").

- \[69\]Z. Yang, J. Teng, W. Zheng, M. Ding, S. Huang, J. Xu, Y. Yang, W. Hong, X. Zhang, G. Feng, et al. (2024)Cogvideox: text-to-video diffusion models with an expert transformer.
arXiv preprint arXiv:2408.06072.
Cited by: [§2.2](https://arxiv.org/html/2606.04410v1#S2.SS2.p4.1 "2.2 Delay-Relaxed Neural Video Compression ‣ 2 Related Work ‣ Ultra-Fast Neural Video Compression").

- \[70\]S. Zhao, Y. Zhang, X. Cun, S. Yang, M. Niu, X. Li, W. Hu, and Y. Shan (2024)Cv-vae: a compatible video vae for latent generative video models.
Advances in Neural Information Processing Systems37,  pp. 12847–12871.
Cited by: [§2.2](https://arxiv.org/html/2606.04410v1#S2.SS2.p4.1 "2.2 Delay-Relaxed Neural Video Compression ‣ 2 Related Work ‣ Ultra-Fast Neural Video Compression").

- \[71\]Z. Zheng, X. Peng, T. Yang, C. Shen, S. Li, H. Liu, Y. Zhou, T. Li, and Y. You (2024)Open-sora: democratizing efficient video production for all.
arXiv preprint arXiv:2412.20404.
Cited by: [§2.2](https://arxiv.org/html/2606.04410v1#S2.SS2.p4.1 "2.2 Delay-Relaxed Neural Video Compression ‣ 2 Related Work ‣ Ultra-Fast Neural Video Compression").


\\thetitle

Supplementary Material

This document provides supplementary material for our paper, detailing the experimental settings and additional results for our proposed ultra-fast neural video codec (NVC), DCVC-UF (Ultra-Fast).

## Appendix A Test Settings

This section details the experimental configurations used for comparing our NVC with traditional codecs across both YUV420 and RGB (shown in Section [C](https://arxiv.org/html/2606.04410v1#A3 "Appendix C Results on RGB colorspace ‣ Ultra-Fast Neural Video Compression")) colorspaces.

YUV420 Colorspace. The YUV420 colorspace is the standard for most practical video applications and traditional codecs, which are highly optimized for this format. Therefore, benchmarking in YUV420 is crucial for assessing the practical performance of NVCs against established standards.
For traditional codecs, we use the reference software for H.265 (HM \[ [19](https://arxiv.org/html/2606.04410v1#bib.bib2 "HM")\]) and H.266 (VTM \[ [60](https://arxiv.org/html/2606.04410v1#bib.bib3 "VTM")\]). In the low-delay setting, we use the encoder\_lowdelay\_main10.cfg and encoder\_lowdelay\_vtm.cfg configurations for HM and VTM, respectively. For the delay-relaxed setting, we use encoder\_randomaccess\_main10.cfg and encoder\_randomaccess\_vtm.cfg. The parameters for each video are as follows:

- •


  -c {config file name}





–InputFile={input video name}





–InputBitDepth=8





–OutputBitDepth=8





–OutputBitDepthC=8





–FrameRate={frame rate}





–DecodingRefreshType=2





–FramesToBeEncoded={frame number}





–SourceWidth={width}





–SourceHeight={height}





–IntraPeriod={intra period}





–QP={qp}





–Level=6.2





–BitstreamFile={bitstream file name}


For both low-delay and delay-relaxed settings, it should be noted that we set the intra-period to –1 for HM and VTM to report their best compression ratio. Here we show a comparison for VTM under the delay-relaxed setting: if using VTM with intra-period=32 as the anchor, VTM with intra-period=–1 can achieve an average of 19.8% bitrate saving on the six test datasets (HEVC Class B∼\\simE, UVG, and MCL-JCV).

RGB Colorspace. Since our test datasets are originally in the YUV420 format, we convert them to RGB for this evaluation. Following the methodology of JPEG AI \[ [2](https://arxiv.org/html/2606.04410v1#bib.bib7 "[AHG 11] Brief information about JPEG AI CfP status"), [3](https://arxiv.org/html/2606.04410v1#bib.bib8 "Anchors · JPEG-AI MMSP Challenge")\] and \[ [34](https://arxiv.org/html/2606.04410v1#bib.bib27 "Neural video compression with diverse contexts")\], we use the BT.709 standard for the YUV-to-RGB conversion. This is because
using BT.709 obtains higher compression ratio under the similar visual quality when compared with the BT.601. As demonstrated in \[ [34](https://arxiv.org/html/2606.04410v1#bib.bib27 "Neural video compression with diverse contexts")\], traditional codecs achieve superior compression when encoding RGB content by using an internal 10-bit YUV444 pipeline, even though the final distortion is measured in RGB. We adopt this same best-practice configuration for our tests.
For HM and VTM, we use the encoder\_lowdelay\_main\_rext.cfg and encoder\_lowdelay\_vtm.cfg configurations for the low-delay scenario. The parameters for each video are as follows:

- •


  -c {config file name}





–InputFile={input file name}





–InputBitDepth=10





–OutputBitDepth=10





–OutputBitDepthC=10





–InputChromaFormat=444





–FrameRate={frame rate}





–DecodingRefreshType=2





–FramesToBeEncoded={frame number}





–SourceWidth={width}





–SourceHeight={height}





–IntraPeriod={intra period}





–QP={qp}





–Level=6.2





–BitstreamFile={bitstream file name}


For both YUV420 and RGB evaluations, all traditional codecs were configured with their best settings and reference structures to ensure rigorous comparison.

Table 4: BD-Rate (%) comparison in RGB colorspace. All frames are tested.

|     |     |     |     |     |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Method | UVG | MCL-JCV | HEVC B | HEVC C | HEVC D | HEVC E | Average | Coding Speed |
| Enc. | Dec. |
| Low-Delay (LD) Codecs |  |  |  |  |  |  |  |  |  |
| VTM-17.0 (LD) | 0.00.0 | 0.00.0 | 0.00.0 | 0.00.0 | 0.00.0 | 0.00.0 | 0.00.0 | 0.010.01 FPS | 23.623.6 FPS |
| HM-16.25 (LD) | 43.243.2 | 49.549.5 | 49.949.9 | 45.245.2 | 39.939.9 | 47.747.7 | 45.945.9 | 0.050.05 fps | 39.6 fps |
| DCVC-DC | 9.29.2 | 0.00.0 | 14.914.9 | 5.35.3 | −7.8-7.8 | 87.787.7 | 18.218.2 | 2.32.3 FPS | 2.92.9 FPS |
| DCVC-FM | −10.4-10.4 | −1.1-1.1 | −11.2-11.2 | −26.5-26.5 | −33.7-33.7 | −12.1-12.1 | −15.8-15.8 | 3.73.7 FPS | 4.44.4 FPS |
| DCVC-RT | −17.2-17.2 | −6.8-6.8 | −11.3-11.3 | −15.8-15.8 | −21.3-21.3 | −11.4-11.4 | −14.0-14.0 | 118.8118.8 FPS | 105.3105.3 FPS |
| DCVC-UF (LD) | −7.2-7.2 | 8.68.6 | 2.22.2 | −1.5-1.5 | −10.3-10.3 | −4.0-4.0 | −2.0-2.0 | 313.6\\mathbf{313.6} FPS | 353.8\\mathbf{353.8} FPS |
| Delay-Relaxed Codecs |  |  |  |  |  |  |  |  |  |
| DCVC-UF (HT-S) | −22.7-22.7 | −3.1-3.1 | −13.0-13.0 | −24.8-24.8 | −37.3-37.3 | −52.7-52.7 | −25.6-25.6 | 655.9\\mathbf{655.9} FPS | 453.3\\mathbf{453.3} FPS |
| DCVC-UF (HT-L) | −34.5-34.5 | −16.2-16.2 | −29.4-29.4 | −37.4-37.4 | −47.5-47.5 | −57.7-57.7 | −37.1-37.1 | 371.1\\mathbf{371.1} FPS | 273.6\\mathbf{273.6} FPS |

- •


Intra-period=–1 for all codecs and settings. The coding speeds of NVCs are tested on 1920×10801920\\times 1080 videos with 4090 GPU.


## Appendix B Depth-wise Convolution Block Details

Our DCVC-UF architecture is deliberately built from a single, lightweight primitive: depth-wise convolution (DC) blocks (as illustrated in Fig. [6](https://arxiv.org/html/2606.04410v1#A2.F6 "Figure 6 ‣ Appendix B Depth-wise Convolution Block Details ‣ Ultra-Fast Neural Video Compression")) \[ [22](https://arxiv.org/html/2606.04410v1#bib.bib65 "Towards practical real-time neural video compression")\], which serve as the fundamental building units across all modules. By avoiding complex operations or heterogeneous specialized blocks, the overall network remains conceptually simple and highly efficient to implement. For the delay-relaxed setting with chunk size N=8N=8, our high-throughput model DCVC-UF (HT-S) employs 6, 7, and 11 DC blocks in the chunk encoder, chunk decoder, and cross-chunk context generation modules, respectively. Each of the 8 frame-specific decoders (corresponding to the 8 frames in the chunk) contains 3 DC blocks, enabling all frames to be reconstructed in parallel. The larger configuration DCVC-UF (HT-L) increases the numbers of DC blocks to 7, 11, and 12 for the chunk encoder, chunk decoder, and cross-chunk context generation modules, respectively, and allocates 5 DC blocks to each of the 8 frame-specific decoders, providing stronger modeling capacity at modest additional complexity. For the low-delay setting with chunk size N=1N=1, DCVC-UF (LD) adopts a more compact design with 3, 3, and 9 DC blocks in the chunk encoder, chunk decoder, and cross-chunk context generation modules, respectively, and a single frame decoder comprising 3 DC blocks, yielding an efficient instantiation tailored to low-delay applications.

![Refer to caption](https://arxiv.org/html/2606.04410v1/x6.png)Figure 6:  The structure of DC Block (depth-wise convolution block) \[ [22](https://arxiv.org/html/2606.04410v1#bib.bib65 "Towards practical real-time neural video compression")\].

## Appendix C Results on RGB colorspace

Our DCVC-UF framework also employs a unified YUV444 colorspace that serves as a universal interface for both input and output. For both YUV420 and RGB videos, they will be converted into YUV444 for inference and converted back for loss calculation. The unified approach enhances practical deployment by allowing a single trained model to efficiently process multiple video formats without requiring specialized model variants for different formats, thereby reducing storage requirements and deployment complexity. The model is also trained exclusively on YUV444 colorspace, which greatly simplifies the training process. Table [4](https://arxiv.org/html/2606.04410v1#A1.T4 "Table 4 ‣ Appendix A Test Settings ‣ Ultra-Fast Neural Video Compression") presents the performance comparisons in the RGB colorspace. From this table, we can find that our DCVC-UF models achieve the advanced rate-distortion-complexity trade-off. These results validate the effectiveness of DCVC-UF for practical video compression applications.

![Refer to caption](https://arxiv.org/html/2606.04410v1/x7.png)Figure 7: Rate-distortion curves.

## Appendix D Rate-Distortion Curves

This section presents comprehensive rate-distortion (RD) curves across multiple datasets in the YUV420 colorspace, with all frames tested under the intra-period=–1 configuration. As illustrated in Fig. [7](https://arxiv.org/html/2606.04410v1#A3.F7 "Figure 7 ‣ Appendix C Results on RGB colorspace ‣ Ultra-Fast Neural Video Compression"), our analysis reveals different performance characteristics across different quality ranges and datasets. In the lower quality range, our proposed DCVC-UF (HT-L) demonstrates the best compression efficiency on all datasets, consistently outperforming both traditional codecs (H.265/HM, H.266/VTM) and previous state-of-the-art neural video codecs. This advantage is particularly pronounced in scenarios where bandwidth constraints or storage costs are critical, making our approach highly suitable for real-world applications.

In the higher quality range, the performance landscape becomes more heterogeneous. Our DCVC-UF (HT-L) achieves the best compression ratio on the HEVC E dataset, demonstrating its capability to handle specific content types effectively. However, on other datasets, VTM with Hierarchical-B configuration maintains its advantage in the very high quality range. This performance gap is expected, as it represents a deliberate design trade-off: our models prioritize computational efficiency while maintaining high compression ratio. The lightweight architecture that enables ultra-fast encoding and decoding speeds inherently limits the model capacity available for achieving maximum compression ratio at very high quality levels. These results underscore the practical applicability of our approach for scenarios requiring very fast processing with good compression ratio, while also identifying opportunities for future improvements in the high-quality domain through increased model capacity or more advanced design.

──────── [TRUNCATED] ────────
Showing 44,602 chars (head) + 14,859 chars (tail) of 94,822 total clean characters.
Full text saved to: /home/wubu/.hermes/profiles/mind-palace/cache/web/arxiv.org-4a381a48f9.md
To read the omitted middle: read_file path="/home/wubu/.hermes/profiles/mind-palace/cache/web/arxiv.org-4a381a48f9.md" offset=161 limit=200  (the file is the complete page; raise/lower offset to page through it).
─────────────────────────────