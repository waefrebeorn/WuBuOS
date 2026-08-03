# AV1: The New Internet Standard And Where It Stands In 2026

Source: https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026
Ingested: 2026-08-03 (Media avenue, Kevin-Bacon growth wave)

---

# AV1: The New Internet Standard And Where It Stands In 2026

![Nikolay Sapunov](https://cdn.prod.website-files.com/64e8910adc5a63966a68acea/6a0c72a04c29b22a8ddb991b_6a0c728d356dff40621f0967_nikolay_sapunov.jpeg)

By

[Nikolay Sapunov](https://www.forasoft.com/team/nikolay-sapunov)

·

Published

Jul 2, 2026

·

31

min read

TL;DR

**AV1** is the royalty-free video codec published by the **Alliance for Open Media (AOMedia)** in March 2018 — a single piece of software, jointly built by Google, Netflix, Amazon, Microsoft, Mozilla, Apple, Cisco, Intel, Meta, Samsung, NVIDIA, and others, that compresses video roughly **30%** smaller than H.265/HEVC and **50%** smaller than H.264/AVC at the same perceptual quality. In May 2026 it has crossed every threshold that matters for a codec to be called "the new default": **YouTube** encodes over **75%** of its catalogue in AV1, **Netflix** delivers **30%** of all streaming hours in AV1 and expects it to overtake H.264 inside 2026, **88%** of large-screen consumer devices submitted to Netflix for certification between 2021 and 2025 ship native AV1 hardware decode, and every major browser — Chrome, Firefox, Edge, and Safari (on M3/A17-Pro silicon and later) — plays AV1 out of the box. The remaining frictions are real but bounded: **encoder cost** still runs 5–20× higher than x264 even on the fastest **SVT-AV1** preset, **Apple's** AV1 hardware decode requires recent silicon, and the **Sisvel** patent pool continues to claim royalties that **Netflix and Google refuse to pay** — a legal posture that has held since 2020 and is the practical reason "royalty-free" is still the honest descriptor. For any 2026 deployment that ships at internet scale, the question has shifted from "should I do AV1" to "what fraction of my ladder is AV1 today, and how fast can the rest move".

## Why This Matters

If you run a streaming service, a video-conferencing product, an OTT platform, an e-learning catalogue, a video surveillance archive, or any product that pushes more than a terabyte of video a month over the public internet, AV1 is the single biggest lever you have on your CDN bill, your customer's data plan, and your video-quality scores. A **30%** bitrate cut at constant quality translates directly into **30%** less egress, **30%** less origin storage, **30%** longer mobile-data sessions, and measurably better quality on the same network. Netflix has published numbers attaching AV1 to **45%** fewer rebuffer events worldwide; for a streaming product, that is not a rounding error — that is the difference between a five-star review and a churn event.[2](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:netflix-blog)

This article is the definitive map of what AV1 actually is, where it came from, why every browser and chip vendor agreed to ship it, how its compression engine differs from H.264 / HEVC / VP9 / VVC, what it costs to encode in 2026, who has hardware decode in their pocket today, what the licensing fight with the Sisvel patent pool means in practice, and how to deploy AV1 inside a 2026 video pipeline without falling into the four or five common production traps. You do not need prior codec knowledge to follow it — every term is defined in plain language before it appears. If you have not yet read [VP8 And VP9: Google's Open Alternative](https://www.forasoft.com/learn/video-encoding/articles/vp8-vp9-explained) and [H.265 / HEVC: +50% Over H.264 And The Patent Nightmare](https://www.forasoft.com/learn/video-encoding/articles/h265-hevc-explained), those two articles are useful prerequisites — AV1 inherits VP9's licensing playbook and was designed to retire HEVC and VP9 simultaneously.

## What AV1 Actually Is

A **codec** is a pair of software pieces stitched together: an **encoder** that takes raw pictures coming out of a camera, an editor, or a screen capture and packs them into a small file; and a **decoder** that reads the small file and reconstructs the pictures for playback. The name is a portmanteau of _co_ der and _dec_ oder. AV1 is a codec in this sense, just like H.264, H.265, VP9, and the brand-new H.266/VVC. All five compete to do the same job: shrink video as small as possible without making the result look noticeably worse.

The full name is **AOMedia Video 1**, abbreviated **AV1**. The "AOMedia" part refers to the **Alliance for Open Media**, a consortium founded in **September 2015** by Amazon, Cisco, Google, Intel, Microsoft, Mozilla, and Netflix, and joined later by Apple, Samsung, NVIDIA, AMD, ARM, Meta, IBM, Tencent, ByteDance, Hulu, and over **fifty** other companies.[1](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:aom-license) The "1" denotes that this is AOMedia's first published codec; **AV2** is in development and we cover it separately in [AV2: What To Expect In The Next 5–10 Years](https://www.forasoft.com/learn/video-encoding/articles/av2-what-to-expect-2026-2031).

AV1 was published in **March 2018** as an open specification, with three reference implementations released under the open-source **BSD-style** licence: **libaom** (the AOMedia reference), **rav1e** (a pure-Rust encoder maintained by Mozilla and Xiph.Org), and **SVT-AV1** (a multi-threaded production encoder built by Intel and Netflix). On the decode side, **dav1d** (also by VideoLAN and FFmpeg developers) is the universal software decoder and is what every browser uses when no hardware path is available.[9](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:netflix-svt)[10](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:dav1d)

The licence model is the headline. Every member company of AOMedia commits to a **non-sublicensable, perpetual, worldwide, no-charge, royalty-free, irrevocable** patent grant for any patent claim necessary to implement AV1, conditional on the implementer not asserting their own patents against other AV1 implementers.[1](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:aom-license) In plain language: if you ship AV1, you do not pay AOMedia members for the right. The implications, the patent-pool fights this licence triggered, and where things stand in 2026 are the subject of a long section below.

The compression target was equally explicit. AV1 was designed to deliver roughly **30% better compression** than HEVC and **50% better compression** than H.264 at matched objective quality, while keeping decoder complexity under **2×** that of HEVC — high enough to need careful engineering on phones, low enough to run in software on any device built after 2015.[5](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:aom-overview) Independent academic measurements have largely confirmed those targets: AV1 averages **30–40%** below HEVC at matched VMAF and **45–55%** below H.264 at matched VMAF on streaming-grade content.[7](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:mdpi-codec-comparison)[8](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:subjective-study)

## A Short History: How AV1 Got Built

The reason AV1 exists is the same reason VP8 and VP9 existed: somebody had to give the open web a top-tier video codec that nobody pays royalties to ship. That sentence sounds obvious in 2026, but in 2015 it was a five-alarm fire.

The fire was lit by **HEVC's licensing collapse**. HEVC was published in 2013 as the successor to H.264 with roughly 50% better compression, and the industry expected the same simple MPEG LA patent pool that had made H.264 deployable. Instead, three things happened in quick succession. **MPEG LA** announced an HEVC pool in 2013 at H.264-comparable rates. In **March 2015**, **HEVC Advance** — a second pool with different terms, different members, and explicit per-stream fees on internet video — launched and refused to coordinate with MPEG LA. By mid-2015, **Technicolor** and several others had refused both pools and announced direct bilateral licensing. The cumulative cost projection for a large streaming service was on the order of **$25 million per year**, and nobody could promise a single counterparty to write the cheque to.[17](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:ipeurope)

The streaming industry's response was to build a new codec from scratch. On **1 September 2015**, **Amazon, Cisco, Google, Intel, Microsoft, Mozilla,** and **Netflix** announced the **Alliance for Open Media**.[1](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:aom-license) The plan was to merge three already-in-progress royalty-free codec efforts — **Google's VP10** (the next VPx after VP9), **Cisco's Thor** (a low-complexity codec designed for WebRTC), and **Mozilla / Xiph's Daala** (a perceptual-coding research codec) — into a single specification that every member would commit to using.

The development cadence was unusually fast for a video codec. The first **AV1 experimental bitstream** went into the libaom repository in late 2016. The bitstream was frozen in **June 2018** and the **1.0 specification** published in **March 2018**, less than three years after AOMedia formed.[5](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:aom-overview) For comparison: H.264 took eight years from kickoff to publication, HEVC took five, and VVC took six.

Adoption then followed the script the founders wrote. **Facebook (Meta)** announced internal AV1 deployment for video uploads in **April 2018**, reporting roughly **34%** better compression than libvpx-vp9 in their production tests.[11](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:meta-blog) **YouTube** began AV1 encoding for popular videos in **September 2018** and expanded gradually as encoder cost dropped. **Netflix** added AV1 on Android in **February 2020**, on TVs in **November 2021**, and reached **30% of all streaming hours** in **December 2025**.[2](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:netflix-blog) **Twitch** added AV1 ingest support for partner streamers in **2024**. **Vimeo, Disney+, Apple TV+**, and **Amazon Prime Video** all added AV1 ladders between 2022 and 2025.

On the device side the rollout was slower but steady. **Intel** added AV1 hardware decode to its 11th-gen "Tiger Lake" laptop CPUs in **September 2020**, and AV1 hardware encode to **Arc Alchemist** discrete GPUs in **early 2022**.[12](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:intel-tigerlake) **NVIDIA** added AV1 decode to **RTX 30-series** GPUs in **September 2020** and encode to **RTX 40-series** (Ada Lovelace) in **October 2022**.[13](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:nvidia-ada) **AMD** added decode in **RDNA 2** (RX 6000, late 2020) and encode in **RDNA 3** (RX 7000, December 2022).[14](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:amd-rdna3) **Apple** — historically the longest holdout — added AV1 hardware decode in the **A17 Pro** SoC (iPhone 15 Pro, September 2023) and the **M3** family (October 2023), expanded to every iPhone 16 and 17 model in 2024–2025, and then in **March 2026** released the **M5 Pro / M5 Max** chips with AV1 hardware _encode_, completing the round-trip.[15](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:apple-av1)

By the end of **2025**, Netflix's device-certification programme reported that **88%** of large-screen consumer devices submitted between 2021 and 2025 had AV1 hardware decode, and effectively every smart TV manufactured since **2023** ships AV1 hardware decode at **4K60**.[2](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:netflix-blog) On the mobile side, the **Pixel 6** (October 2021), **Galaxy S22** (February 2022), every flagship Android since, and every iPhone 15 Pro / iPhone 16 / iPhone 17 device decodes AV1 in hardware up to 4K60. Mid-range Android phones decode AV1 in software through **dav1d** at 1080p comfortably.

![Horizontal timeline of AV1 milestones from 2013 (HEVC published triggering the licensing crisis), through 2015 (Alliance for Open Media founded by 7 companies), 2016 (first AV1 experimental bitstream), March 2018 (AV1 1.0 specification published), April 2018 (Meta first production deployment), September 2018 (YouTube begins AV1 encoding), September 2020 (Intel Tiger Lake adds hardware decode, NVIDIA RTX 30 adds hardware decode), late 2020 (AMD RDNA 2 adds hardware decode), February 2020 (Netflix Android AV1), early 2022 (Intel Arc adds hardware encode), October 2022 (NVIDIA RTX 40 adds hardware encode), December 2022 (AMD RDNA 3 adds hardware encode), September 2023 (Apple A17 Pro and M3 add hardware decode), 2025 (YouTube 75% AV1, Netflix 30% AV1), March 2026 (Apple M5 Pro and M5 Max add hardware encode completing the major-vendor encoder rollout)](https://cdn.prod.website-files.com/64e8910adc5a63966a68acea/6a451567106f171e95cdf673_6a451562e51eff650d54be15_03_7-av1-state-2026__01-av1-timeline.svg)_Figure 1. The AV1 adoption timeline. The cadence is unusually fast for a video codec — 3 years from consortium founding to 1.0 spec, 5 more years to mass-market hardware decode, 8 years to "default on every major platform"._

## How AV1 Compresses: The Eight Things That Make It Work

AV1 is a **hybrid block-based codec** in the same broad architecture as every modern codec since H.264. We unpack the hybrid framework in [Hybrid Video Codec Architecture](https://www.forasoft.com/learn/video-encoding/articles/hybrid-codec-architecture); here the key idea is that AV1 splits each picture into rectangular blocks, predicts each block from neighbouring pixels (intra-prediction) or from earlier and later pictures (inter-prediction), encodes only the small leftover (the **residual**), and uses an entropy coder to pack the residual bits efficiently into the bitstream.

AV1 keeps that framework and rebuilds eight stages of it. The compression win against HEVC and VP9 is the sum of those eight redesigns.

### 1\. Bigger blocks — 128×128 superblocks

Every modern codec starts by cutting the picture into squares and then recursively splitting each square as needed to match the content. H.264 used **16×16 macroblocks** with no subdivision. HEVC introduced **64×64 Coding Tree Units (CTUs)** that subdivide to **4×4**. VP9 used **64×64 superblocks** that subdivide to **4×4**.

AV1 doubles the ceiling to **128×128 superblocks**, still recursively splittable down to **4×4**.[6](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:av1-tech-overview) On flat areas like sky, walls, snow, gradients, sea — content where one big block compresses dramatically better than many small ones — the 128×128 super-block saves bits that HEVC and VP9 simply cannot save. On detailed areas, AV1 splits as fine as anyone else.

Where AV1 really differs from earlier codecs is in **how** a block subdivides. HEVC's quadtree was restricted to four equal quadrants. VP9 added rectangular two-way splits (a 64×64 can become two 64×32 or 32×64 halves). AV1 adds **T-shaped 3-way splits** and **4:1 / 1:4 rectangular strip splits** on top of the quadtree, for a total of **ten** partition options per node.[6](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:av1-tech-overview) On real content with strong horizontal or vertical edges (think on-screen text, fence posts, building facades, panning shots), the T-shapes match the actual edge geometry better than any pure-square or pure-rectangular split could.

The mental picture is the same chessboard-of-subdividable-squares from HEVC, with the extra freedom that any cell can be cut into three regions of different shape — a horizontal stripe on top with two squares below, for instance — instead of being forced into halves or quarters.

### 2\. More intra-prediction modes — and three new modes nobody had before

When the encoder finishes splitting the picture into blocks, it has to predict each block's pixels. **Intra-prediction** predicts a block using only the already-decoded pixels above and to the left of the block. H.264 had 9 directional intra modes for 4×4 blocks. HEVC had 35. VP9 had 10. **AV1 has 56** directional modes plus three new non-directional modes.[6](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:av1-tech-overview)

The three new non-directional modes earn most of their keep:

**Smooth predictors** — three variants (smooth, smooth\_v, smooth\_h) that linearly interpolate from the left and top edge pixels across the block. Smooth predictors are the right model for gradients, soft skin, soft shadows, gradual sky transitions.

**Recursive filter-based intra modes** — five variants where each pixel is predicted as a weighted combination of pixels above, to the left, and _above-and-to-the-left_. These match small-scale texture (cloth, hair, sand) better than any pure-direction line.

**Chroma from Luma (CfL)** — the chroma (colour) channels are predicted directly from the already-reconstructed luma (brightness) channel using a simple linear model. Most of the time the colour channel is a smooth re-scaling of the brightness channel, and CfL lets AV1 spend almost no bits on colour signal for those blocks.[6](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:av1-tech-overview)

Counter-intuitively, the directional-mode count gap (AV1 56, HEVC 35, VP9 10) doesn't matter as much as the three new non-directional modes. The smooth and CfL modes account for a measurable fraction of AV1's chroma-signal compression win over HEVC.

### 3\. Smarter inter-prediction — warped motion, global motion, compound prediction

When intra-prediction is not enough, the encoder predicts a block from blocks in earlier or later pictures. This is **inter-prediction**, and it is where the biggest compression wins live in any codec — temporal redundancy between adjacent frames is far larger than spatial redundancy inside one frame. We cover the full inter-prediction picture in [Inter-Frame Coding And Motion Estimation](https://www.forasoft.com/learn/video-encoding/articles/inter-frame-coding-motion-estimation).

AV1 adds three meaningful tools on top of the H.264 / HEVC inter-prediction toolkit.

**Warped motion** lets the encoder describe the motion of a block as a 2D **affine transformation** (scaling, rotation, shearing) rather than only as a translation (move left, move right). Real camera motion — handheld walking, pans, zooms, drone shots — almost never reduces to pure translation, and warped motion captures it with one set of parameters per block instead of forcing the rate-control to spend bits on a dense motion-vector field.[6](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:av1-tech-overview)

**Global motion** generalises warped motion to a whole picture: the encoder describes camera pan / zoom / rotation as a single affine transformation applied to all blocks, with per-block residuals handling whatever the global model misses. On a panning landscape shot, global motion can drop the bitrate to fractions of what H.264 would charge for the same frame.

**Compound prediction** lets a block be predicted as a weighted average of two reference frames (one past, one future, or two from the same direction) instead of forcing the encoder to pick one. AV1 supports up to **seven reference frames** active simultaneously per frame (against H.264's two and HEVC's four), which dramatically extends the range over which compound prediction is useful.

The result is that AV1 routinely beats VP9 by **15–25%** on motion-heavy content (sports, action movies, music video) and beats H.264 by **40–50%** on the same content.

### 4\. New transforms — DCT, ADST, flipped ADST, identity

After prediction, AV1 transforms each block's residual signal from pixel space into frequency space so that the entropy coder can compress the result. Different content prefers different transform basis functions. AV1 supports the **Discrete Cosine Transform (DCT)**, the **Asymmetric Discrete Sine Transform (ADST)** (already in VP9), the new **Flipped ADST**, and the **Identity transform (IDTX)**, in combinations along the horizontal and vertical axes independently.[6](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:av1-tech-overview)

For example, the same 8×16 block can be transformed with DCT horizontally and ADST vertically, or Identity horizontally and DCT vertically. This per-axis choice lets the encoder match the actual signal geometry of the residual much better than HEVC's fixed DCT could. We unpack the transform-coding family in [Transform Coding: DCT, ADST, Integer Transforms](https://www.forasoft.com/learn/video-encoding/articles/transform-coding-dct-adst-integer-transforms).

### 5\. A new entropy coder — multi-symbol arithmetic

Where H.264 used CABAC (a binary arithmetic coder), HEVC kept CABAC, and VP9 used a simpler boolean arithmetic coder, AV1 introduces a **non-binary multi-symbol arithmetic coder**.[6](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:av1-tech-overview) The mathematical detail matters less than the practical effect: AV1's entropy coder codes more bits per cycle than CABAC on the same hardware, which is one of the reasons AV1 decoders can keep pace with HEVC decoders despite doing more total work per block.

### 6\. New in-loop filters — CDEF, loop restoration

After decoding a block, the codec runs **in-loop filters** that smooth the boundaries between blocks and reduce visible compression artefacts. AV1 keeps HEVC's deblocking filter and adds two new ones.

**Constrained Directional Enhancement Filter (CDEF)** detects the direction of edges in each block and applies a filter that sharpens edges while suppressing ringing artefacts perpendicular to those edges.[6](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:av1-tech-overview) The visual effect is most obvious on sharp text, hair, leaves — AV1's CDEF keeps edges crisp at low bitrates where HEVC's deblocking smears them.

**Loop Restoration** applies one of two restoration filters (a separable symmetric Wiener filter or a self-guided restoration filter) to recover detail lost in compression. Loop Restoration is the single feature most responsible for AV1's quality lead at very low bitrates — the bitrates where streaming services hit the device-tail of cheap Android phones on weak networks.

### 7\. Film grain synthesis — a parametric trick

Film grain is the small random noise that movies shoot on film and that filmmakers like enough that digital productions often add it back synthetically. Compressed video traditionally smears film grain into something that looks like JPEG mosquitoes; preserving grain requires very high bitrates because compression algorithms see grain as random high-entropy signal.

AV1 introduces a **film grain synthesis** path: the encoder strips the grain out before compression, encodes the clean image at a low bitrate, and stores a tiny parametric model of the grain. The decoder reconstructs the clean image and then synthesises grain on top using the parametric model.[6](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:av1-tech-overview) The result is that a film-grain-heavy movie (Christopher Nolan, Wes Anderson, Coen Brothers) ships at the same bitrate as a clean digital image while preserving the grain texture viewers expect. Netflix uses film grain synthesis for most of its movie catalogue.

### 8\. Tile-based parallel decoding — saturates modern CPUs and GPUs

VP9 introduced tile-based parallel decoding; AV1 expands it. A picture is divided into rectangular **tiles** that decode independently, and AV1 specifies **tile groups** (a flexible aggregation of tiles into bitstream-units) that map cleanly onto modern multi-core CPUs and GPU compute units. **dav1d**, the universal software decoder, routinely saturates 8–16 CPU cores at 4K60.

The cost of all eight redesigns is **encoder complexity**. The reference encoder **libaom** at its quality-balanced settings is roughly **50×** slower than **x264** at the same VMAF target, which is why every production AV1 deployment in 2026 runs the **SVT-AV1** encoder (about **5×** slower than x264 at preset 6) or a hardware encoder (real-time at 1080p60 on any 2023+ Intel / NVIDIA / AMD GPU).

![Pipeline diagram of the AV1 encoder showing eight stages — input frame entering a 128x128 superblock partitioner with quadtree plus T-shaped plus 4:1 strip subdivision options, followed by intra-prediction with 56 directional modes plus smooth predictors plus chroma-from-luma plus filter-intra, then inter-prediction with translational motion plus warped motion plus global motion plus compound prediction across up to 7 reference frames, then transform coding with DCT plus ADST plus flipped ADST plus identity transform selectable per axis, then quantization, then multi-symbol arithmetic entropy coder writing to the output bitstream, in-loop filters deblocking plus CDEF plus loop restoration feeding reconstructed pixels back to the reference buffer, film grain analysis stripping grain before compression and storing a parametric model, and tile-based parallel decoding labelled as the mechanism that lets dav1d saturate 8 to 16 CPU cores at 4K60](https://cdn.prod.website-files.com/64e8910adc5a63966a68acea/6a451567106f171e95cdf66e_6a451565744deaa8e2216209_03_7-av1-state-2026__02-av1-pipeline.svg)_Figure 2. The AV1 encoder pipeline. Eight stages redesigned over VP9 / HEVC; the cumulative effect is roughly 30% lower bitrate than HEVC at the same perceptual quality._

## AV1 Vs HEVC Vs VP9 Vs H.264 Vs VVC: The Compression Numbers

The honest comparison is the one independent academic measurements support, with the year of the study and the test set called out. The 2024 MDPI study by Bonatto et al. measured all five codecs on common test sequences at HD, UHD, and 8K resolutions using Bjøntegaard Delta Bitrate (the standard codec-comparison metric, which reports how much bitrate codec X needs to match codec Y at the same PSNR).[7](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:mdpi-codec-comparison)

| Comparison | AV1 vs H.264 | AV1 vs HEVC | AV1 vs VP9 | VVC vs AV1 |
| --- | --- | --- | --- | --- |
| Bitrate saving at FHD (1080p) | ~33% lower | ~3–10% lower | ~13–25% lower | ~1–5% lower |
| Bitrate saving at UHD (4K) | ~50% lower | ~25–35% lower | ~25–35% lower | ~10–20% lower |
| Bitrate saving at 8K | ~81% lower | ~44% lower | ~50–60% lower | ~30–70% lower |
| Subjective MOS vs HEVC | clearly better at low bitrate | ~equal at high bitrate | n/a | ~equal to AV1 at HD, better at 8K |

_Table 1. Codec compression efficiency in 2026. AV1 sits roughly_ _30%_ _below HEVC on average across modern content (UHD streaming-grade). VVC outperforms AV1, but the gap closes at HD and only opens at 8K. AV1 dominates over VP9 by_ _15–30%_ _depending on resolution._

Three notes on how to read this table for product decisions.

First, the **30%-below-HEVC** number is the right one to plan a streaming deployment around. On the kind of content modern services ship — 1080p–4K HDR live action — AV1 ladders typically save **25–35%** of the bandwidth of an HEVC ladder at matched VMAF. Netflix's published number on its production traffic is the cleanest data point: AV1 streams have shown roughly **one-third less bandwidth** than equivalent AVC and HEVC streams while scoring **4.3** VMAF points above AVC and **0.9** VMAF points above HEVC.[2](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:netflix-blog)

Second, the **VVC** column matters less in 2026 than it looks. VVC is technically the strongest codec in the table, but its 2026 hardware decode footprint is **roughly 5%** of consumer devices (essentially: a few high-end TVs, a few set-top boxes, no shipping mobile silicon, no browser support), against AV1's **80%+**. A compression win you cannot ship is a marketing slide, not a deployment plan. We cover the VVC question in [H.266 / VVC: Technically Excellent, Market-Weak](https://www.forasoft.com/learn/video-encoding/articles/h266-vvc-explained).

Third, the **AV1 vs VP9** gap is what made the upgrade case for the codecs Google, Netflix, and YouTube already ran. VP9 hardware decode is broader than AV1 today (about **85%** of devices vs **80%**), but at matched quality VP9 needs **25–30%** more bitrate than AV1 — enough that a streaming service paying real CDN bills will gladly pay the encoder cost to switch its biggest assets.

## AV1 Vs The Field At A Glance: Browsers, Hardware, Licence

The codec landscape your product team will face in 2026 fits into one comparison table.

| Criterion | **H.264** | **VP9** | **HEVC** | **AV1** | **VVC** |
| --- | --- | --- | --- | --- | --- |
| Year published | 2003 | 2013 | 2013 | 2018 | 2020 |
| Compression vs H.264 | baseline | ~40% better | ~50% better | ~65% better | ~70% better |
| Hardware decode share 2026 | ~100% | ~85% | ~95% | ~80% | ~5% |
| Browser support 2026 | universal | universal | partial (Safari, Edge, FF 134+) | universal | none |
| Royalty model | MPEG LA pool + free-to-EU internet | royalty-free (Google grant) | 2 pools + residual | royalty-free (AOMedia) + Sisvel claim | 3 pools, expensive |
| WebRTC mandatory | yes | no | no | no | no |
| Reference encoder | x264 / OpenH264 | libvpx | x265 | libaom / SVT-AV1 / rav1e | VVenC |
| Encoder CPU vs x264 | 1× | 3–8× | 2–10× | 5–20× (SVT-AV1) | 30–80× |
| Best-in-class hardware encoder 2026 | every GPU | most GPUs | most GPUs | Intel Arc, NVIDIA RTX 40+, AMD RDNA3+, Apple M5 Pro/Max | none |

_Table 2. The codec landscape in 2026. AV1 is the only codec that combines top-quartile compression with universal browser support and a royalty-free licence. HEVC has broader hardware footprint but legal cost. VVC has the best compression but the worst deployability. H.264 remains the floor codec for the legacy device tail._

The 2026 product-team reading of this table: **H.264 is the floor codec you ship to the legacy tail, AV1 is the default codec you ship to everything new, HEVC is the bridge you maintain while AV1 hardware decode crosses the last 20% of devices, VP9 is the codec you keep as the royalty-free fallback inside WebRTC and on older Android, and VVC is a watch-list item for 2028+.**

## The Licensing Story: "Royalty-Free" With An Asterisk

The simple version is the one the AOMedia FAQ uses: AV1 is royalty-free under the AOMedia Patent License 1.0, which grants every implementer a perpetual, worldwide, no-charge licence to every patent claim that AOMedia members hold against the AV1 specification.[1](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:aom-license) If you are a streaming service, a browser vendor, a chip vendor, or a software developer who ships AV1, you do not pay AOMedia members to do so. That part of the story is the same in 2026 as it was in 2018.

The longer version is the one your General Counsel will eventually surface. In **March 2020**, the **Sisvel** patent pool — a separate organisation from AOMedia and from MPEG LA — announced an **"AV1 patent pool"** that claims a small set of patents allegedly essential to AV1 and asks implementers to pay per-device royalties (initially **$0.32 per device** for AV1).[16](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:sisvel-pool) The Sisvel pool's patents are not contributed by AOMedia members (AOMedia members cross-licensed to each other and to all AV1 implementers). They are held by a different set of patent owners — JVCKenwood, NTT DOCOMO, Orange, Philips, and others — who are not AOMedia members and who therefore did not sign the royalty-free grant.

The **Avanci** pool added a separate **video streaming services** programme in **October 2023** targeting streaming operators rather than device vendors, again with AV1 in scope alongside HEVC, VP9, and VVC.

The practical state of play in 2026 is that **Netflix and Google have publicly refused to pay Sisvel** and have not been sued.[17](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:ipeurope) Most other large implementers have followed the same posture. Sisvel has not yet brought a successful AV1 lawsuit against a major implementer. The legal calculus is that the Sisvel claims have not been tested in court for AV1; the patents in the pool would have to be individually proven essential and non-invalid to extract royalties; and the cost of fighting is high enough that nobody has volunteered. In **March 2026**, **Dolby** sued **Snapchat** over video-codec patents — a case that touches HEVC and AV1 indirectly — and the outcome of that case will likely shift expectations for the whole codec-licensing landscape.

For a 2026 streaming or video product, the honest summary is the one Netflix offered in its public communications: **AV1 is royalty-free in practice from every party that has signed the AOMedia patent grant**, which covers the patents AOMedia members hold. **AV1 is not royalty-free in theory from parties outside AOMedia who claim patents on the standard**, but those claims have not produced enforceable royalties in the seven years since the AV1 specification was frozen. A risk-tolerant deployment treats AV1 as royalty-free; a risk-averse deployment buys legal insurance and budgets for a possible per-device royalty if Sisvel ever wins a case.

We unpack the broader codec-royalty story in [Codec Patent Pools And The Cost Of Royalties](https://www.forasoft.com/learn/video-encoding/articles/codec-comparison-table).

## Encoder Cost In 2026: libaom, SVT-AV1, rav1e, And Hardware

The compression-quality side of AV1 is settled by the spec. The deployment-cost side is settled by which encoder you run. Three software encoders and a fast-growing set of hardware encoders cover the field.

### libaom — the reference, slow and accurate

**libaom** is the AOMedia reference encoder. Its `cpu-used` parameter scales from **0** (slowest, highest quality) to **9** (fastest, lowest quality). At `cpu-used 0` libaom is roughly **50–100×** slower than x264 at the same VMAF and produces the cleanest output of any current AV1 encoder, especially at very low bitrates. At `cpu-used 4` libaom is **10–20×** slower than x264. libaom is the right choice when encoder time is free — overnight VOD batches, archive transcodes, codec-comparison benchmarks. It is the wrong choice for live or for cost-sensitive cloud transcoding.[18](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:av1-bench)

### SVT-AV1 — the production encoder

**SVT-AV1** is the encoder Intel and Netflix built for production. Its `preset` parameter scales from **0** (slowest, highest quality, close to libaom at cpu-used 0) to **13** (fastest, real-time on a single CPU core at 1080p). Preset **4–6** is the production sweet spot for VOD: roughly **5–10×** slower than x264, **3–5×** faster than libaom at matched quality, and within **5–10%** of libaom's compression efficiency.[9](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:netflix-svt)[19](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:ottverse-presets) Preset **8** runs real-time at 1080p on a modern desktop CPU. Preset **10–12** are real-time choices on cloud transcoding instances and integrate cleanly with FFmpeg.

Version **4.0.0** of SVT-AV1, released in **January 2026**, added meaningful speed improvements (up to **40%** faster encoding on Intel CPUs and modern AMD Ryzen at matched quality) and is the production default for every cloud-transcoding vendor in 2026.[20](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:svt-av1-3)

### rav1e — the pure-Rust alternative

**rav1e** is a Rust implementation maintained by Mozilla, Xiph.Org, and contributors. Its `speed` parameter scales from **0** to **10**. rav1e is memory-safe, easy to integrate into Rust pipelines, and slightly slower than SVT-AV1 at matched quality. Its niche is browser-shipping (Mozilla's plan was always to bundle rav1e in Firefox for client-side encoding) and embedded deployments where the memory safety matters.

### Hardware encoders — real-time at 4K60

Every major GPU vendor shipped AV1 hardware encode between 2022 and 2026:

- **Intel Arc** (Alchemist 2022, Battlemage 2024) — discrete GPU AV1 encode at 8K60.
- **NVIDIA RTX 40 / 50 series** (October 2022, January 2025) — AV1 encode at 8K60.
- **AMD RDNA 3 / 4** (December 2022, January 2025) — AV1 encode at 4K60.
- **Apple M5 Pro / M5 Max** (March 2026) — AV1 encode at 4K60.[15](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:apple-av1)

Hardware encoders are the right choice for **live** workloads (streaming concerts, sports, lectures, conferences) and for **cost-sensitive cloud transcoding** at scale. The compression efficiency of hardware AV1 encoders is roughly comparable to **SVT-AV1 preset 8** — somewhat below libaom and below SVT-AV1 preset 4, but at fractions of the CPU cost.

| Encoder | Type | Speed vs x264 (matched VMAF) | Best for |
| --- | --- | --- | --- |
| libaom (cpu-used 0–2) | software | 50–100× slower | research, codec comparisons, overnight VOD |
| libaom (cpu-used 4–6) | software | 10–20× slower | quality-priority VOD batches |
| SVT-AV1 (preset 4–6) | software | 5–10× slower | production VOD default in 2026 |
| SVT-AV1 (preset 8–10) | software | 1–3× slower | cloud transcoding at scale |
| SVT-AV1 (preset 12–13) | software | real-time | live encoding on CPU |
| rav1e (speed 4) | software | 8–15× slower | Rust pipelines, embedded |
| NVENC AV1 (RTX 40/50) | hardware | real-time 4K60 | live streaming, transcoding farms |
| Intel Arc AV1 | hardware | real-time 8K60 | live, transcoding |
| AMD VCN 4 AV1 | hardware | real-time 4K60 | live, transcoding |
| Apple M5 Pro/Max AV1 | hardware | real-time 4K60 | macOS / iOS export pipelines |

_Table 3. AV1 encoder choices in 2026. SVT-AV1 is the production software default; hardware encoders cover live and high-throughput; libaom is reserved for quality benchmarks._

## AV1 In FFmpeg: The Commands You Will Actually Use

Three FFmpeg recipes cover roughly **90%** of the AV1 encoding work most video teams do in 2026. All three assume FFmpeg 7 or later with libsvtav1 enabled.

```bash
# 1) 2-pass VOD encode, 1080p60 from a master, 10-bit, perceptual quality target
ffmpeg -i master_1080p60.mxf -c:v libsvtav1 -preset 5 \
  -crf 28 -b:v 0 -pix_fmt yuv420p10le \
  -g 240 -svtav1-params "tune=0:film-grain=8:enable-overlays=1" \
  -c:a libopus -b:a 128k out_1080p60_av1.mkv
```

The recipe runs **SVT-AV1 preset 5** (the 2026 production sweet spot), constant-quality mode at **CRF 28** (perceptually transparent for most 1080p content), **10-bit** output (smaller than 8-bit at the same VMAF on this generation of encoders, even for SDR), a **4-second keyframe interval** (`-g 240` at 60 fps) appropriate for HLS / DASH segmenting, and a small dose of synthetic film grain to mask any residual banding. We cover rate-control modes in [Rate Control: CBR, VBR, CRF, ABR, Capped CRF](https://www.forasoft.com/learn/video-encoding/articles/rate-control-cbr-vbr-crf).

```bash
# 2) 4K HDR encode (BT.2020 PQ, 10-bit), constant quality, slow preset
ffmpeg -i master_4k.mxf -c:v libsvtav1 -preset 3 \
  -crf 22 -b:v 0 -pix_fmt yuv420p10le \
  -color_primaries bt2020 -color_trc smpte2084 -colorspace bt2020nc \
  -svtav1-params "tune=0:enable-hdr=1:mastering-display=G(8500,39850)B(6550,2300)R(35400,14600)WP(15635,16450)L(10000000,1):content-light=1000,400" \
  -c:a copy out_4k_hdr_av1.mkv
```

The HDR signalling (`-color_primaries bt2020 -color_trc smpte2084 -colorspace bt2020nc`) plus the SVT-AV1 mastering-display and content-light parameters are what mark the file as HDR10 in a way that every AV1 decoder will respect. Without those tags, the file decodes as a 10-bit SDR file and the HDR pipeline collapses. See [The Complete Guide To HDR: HDR10, HDR10+, Dolby Vision, HLG](https://www.forasoft.com/learn/video-encoding/articles/hdr-complete-guide-hdr10-dolby-vision-hlg) for the HDR side of the story.

```bash
# 3) Live ingest, 1080p30, hardware encode on NVIDIA RTX 40+
ffmpeg -hwaccel cuda -hwaccel_output_format cuda -i input \
  -c:v av1_nvenc -preset p4 -tune ll -cbr 1 -b:v 4M \
  -g 60 -keyint_min 60 -no-scenecut 1 \
  -c:a libopus -b:a 96k -f mpegts udp://distribution:9000
```

The hardware path (`-c:v av1_nvenc`) on a modern NVIDIA GPU runs real-time at 1080p60 inside a streaming pipeline. **Constant Bitrate (CBR)** mode keeps the bitrate flat at 4 Mbps for predictable distribution, the **2-second GOP** matches HLS-style segmenting, and the `-tune ll` (low-latency) flag minimises the encoder's lookahead. See [Streaming Protocols: The 8 That Matter In 2026](https://www.forasoft.com/learn/video-encoding/articles/bitrate-math-uncompressed-vs-compressed) for the protocol side.

## Where AV1 Hardware Decode Lives In 2026 (And Where It Does Not)

The 2026 AV1 hardware decode footprint is what makes the codec deployable at internet scale. The summary by platform is:

**Desktop CPU / iGPU.** Intel Tiger Lake (11th-gen, September 2020) and later all decode AV1 in hardware. AMD Ryzen 6000 and later integrated graphics decode AV1. Apple **M3** (October 2023) and later decode AV1 in hardware. Apple **M5 Pro / M5 Max** (March 2026) decode AND encode in hardware.

**Discrete GPU.** NVIDIA RTX 30-series (September 2020) and later decode AV1. RTX 40-series (October 2022) and later also encode AV1. AMD RDNA 2 (RX 6000, late 2020) and later decode; RDNA 3 (RX 7000, December 2022) and later encode. Intel Arc Alchemist (2022) and later decode and encode AV1.[12](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:intel-tigerlake)[13](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:nvidia-ada)[14](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:amd-rdna3)

**Mobile — Android.** Google Tensor (Pixel 6, October 2021) and later. Qualcomm Snapdragon 8 Gen 1 (Galaxy S22, February 2022) and later flagships. MediaTek Dimensity 9000 (early 2022) and later. Samsung Exynos 2200 and later. Effectively every Android flagship since 2022 ships AV1 hardware decode at 4K60. Mid-range Android phones (Dimensity 7000 series, Snapdragon 7-series) decode AV1 in software through dav1d at 1080p comfortably.

**Mobile — iOS.** Apple A17 Pro (iPhone 15 Pro, September 2023). Every iPhone 16 (2024) and iPhone 17 (2025) model. iPad Pro with M4 (May 2024) and later. iPad Air with M3 (March 2025) and later. iPad mini with A17 Pro (October 2024). The **gap** is older iPhones — every iPhone 14 and earlier, every iPhone 15 standard (non-Pro), and every iPad Air before M3 — none of those devices decode AV1 in hardware, and Apple has not shipped a software decoder. **Safari** on those older devices does not play AV1 at all.[4](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:caniuse)

**Smart TVs.** Samsung 2020+ QLEDs, LG 2020+ OLEDs, Sony 2020+ Bravia XR, Hisense / TCL / Vizio 2021+ models. Every smart TV shipped after **2023** decodes AV1 in hardware at 4K60. Roku, Apple TV 4K (2nd gen, 2021+), Fire TV Cube (2nd gen+, 2019+), Chromecast with Google TV (2020+) all decode AV1.

**Browser.** Chrome (70+, 2018), Firefox (67+, 2019), Edge (121+, 2024), Safari (17+ on M3 / A17 Pro silicon and later, 2023). Universal browser support modulo Apple's silicon gate.[4](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:caniuse)

The honest summary in **May 2026**: AV1 hardware decode covers **roughly 80%** of consumer devices that play streaming video. The **20%** gap is mid-range and older Android, iPhone 14 and earlier, iPad mini before A17 Pro, and the long tail of pre-2020 smart TVs and set-top boxes. Every greenfield streaming deployment in 2026 ships an AV1 ladder + an HEVC or H.264 fallback ladder for the 20% gap, and the gap shrinks year over year.

## Where AV1 Is The Wrong Choice — Even In 2026

A blunt list of situations where AV1 should not be your default in 2026.

**Real-time conferencing on the public WebRTC stack today.** AV1 has shipped in Chrome WebRTC since 2021 and is increasingly supported by SFUs (Janus, mediasoup, LiveKit, Daily) — but **VP8 is still the mandatory-to-implement WebRTC codec under RFC 7742**, and the H.264 / VP8 / VP9 baseline is what most cross-browser conferencing stacks negotiate against by default.[21](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fn:rfc7742) AV1 in WebRTC is a meaningful 2026 win when both peers support it; it is not yet the floor codec.


[... middle omitted — see footer ...]


03. Streaming Media. "The State of Streaming Codecs 2026." 2026. https://www.streamingmedia.com/Articles/Editorial/Featured-Articles/The-State-of-Streaming-Codecs-2026-173838.aspx [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:streaming-media "Jump back to footnote 3 in the text")

04. Can I Use. "AV1 video format." Browser support snapshot accessed 2026-05-16. https://caniuse.com/av1 [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:caniuse "Jump back to footnote 4 in the text") [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref2:caniuse "Jump back to footnote 4 in the text")

05. AOMedia. "An Overview of Core Coding Tools in the AV1 Video Codec." PCS 2018. https://norkin.org/pdf/PCS\_2018\_AV1\_tools\_overview.pdf [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:aom-overview "Jump back to footnote 5 in the text") [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref2:aom-overview "Jump back to footnote 5 in the text")

06. Chen, Y., et al. "A Technical Overview of AV1." arXiv preprint. https://arxiv.org/pdf/2008.06091 [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:av1-tech-overview "Jump back to footnote 6 in the text") [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref2:av1-tech-overview "Jump back to footnote 6 in the text") [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref3:av1-tech-overview "Jump back to footnote 6 in the text") [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref4:av1-tech-overview "Jump back to footnote 6 in the text") [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref5:av1-tech-overview "Jump back to footnote 6 in the text") [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref6:av1-tech-overview "Jump back to footnote 6 in the text") [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref7:av1-tech-overview "Jump back to footnote 6 in the text") [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref8:av1-tech-overview "Jump back to footnote 6 in the text") [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref9:av1-tech-overview "Jump back to footnote 6 in the text")

07. Bonatto, R., et al. "Performance Comparison of VVC, AV1, HEVC, and AVC for High Resolutions." Electronics, MDPI, 2024. https://www.mdpi.com/2079-9292/13/5/953 [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:mdpi-codec-comparison "Jump back to footnote 7 in the text") [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref2:mdpi-codec-comparison "Jump back to footnote 7 in the text")

08. Grois, D., et al. "Comparison of Compression Efficiency between HEVC/H.265, VP9 and AV1 based on Subjective Quality Assessments." IEEE PCS 2018. https://ieeexplore.ieee.org/document/8463294 [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:subjective-study "Jump back to footnote 8 in the text")

09. Netflix Technology Blog. "SVT-AV1: open-source AV1 encoder and decoder." 2020. https://netflixtechblog.com/svt-av1-an-open-source-av1-encoder-and-decoder-ad295d9b5ca2 [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:netflix-svt "Jump back to footnote 9 in the text") [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref2:netflix-svt "Jump back to footnote 9 in the text")

10. VideoLAN / dav1d project. Open-source AV1 decoder. Accessed 2026-05-16. https://code.videolan.org/videolan/dav1d [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:dav1d "Jump back to footnote 10 in the text")

11. Engineering at Meta. "AV1 beats x264 and libvpx-vp9 in practical use case." 2018-04-10. https://engineering.fb.com/2018/04/10/video-engineering/av1-beats-x264-and-libvpx-vp9-in-practical-use-case/ [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:meta-blog "Jump back to footnote 11 in the text")

12. Intel. Tiger Lake (11th-gen Intel Core) launch materials, September 2020. AV1 hardware decode in Xe iGPU. https://www.intel.com/content/www/us/en/products/docs/processors/core/11th-gen-processors.html [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:intel-tigerlake "Jump back to footnote 12 in the text") [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref2:intel-tigerlake "Jump back to footnote 12 in the text")

13. NVIDIA. Ada Lovelace architecture whitepaper, October 2022. AV1 hardware encoder in RTX 40-series. https://images.nvidia.com/aem-dam/Solutions/geforce/ada/nvidia-ada-gpu-architecture.pdf [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:nvidia-ada "Jump back to footnote 13 in the text") [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref2:nvidia-ada "Jump back to footnote 13 in the text")

14. AMD. Radeon RX 7000 (RDNA 3) launch materials, December 2022. AV1 hardware encode. https://www.amd.com/en/graphics/radeon-rx-7000 [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:amd-rdna3 "Jump back to footnote 14 in the text") [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref2:amd-rdna3 "Jump back to footnote 14 in the text")

15. Apple. M5 / iPhone 17 keynote materials, March 2026. AV1 hardware encode in M5 Pro / M5 Max. https://www.apple.com/newsroom/ [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:apple-av1 "Jump back to footnote 15 in the text") [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref2:apple-av1 "Jump back to footnote 15 in the text")

16. Sisvel. "AV1 Video Coding Platform." Patent pool description. https://www.sisvel.com/licensing-programmes/audio-and-video-coding-decoding/video-coding-platform-av1/ [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:sisvel-pool "Jump back to footnote 16 in the text")

17. IP Europe. "'Royalty-free' standards are not free of costs: AV1 as a case study." 2024. https://ipeurope.org/blog/royalty-free-standards-are-not-free-of-costs-av1-as-a-case-study/ [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:ipeurope "Jump back to footnote 17 in the text") [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref2:ipeurope "Jump back to footnote 17 in the text")

18. Kagami. "AV1 encoders benchmarks." GitHub. https://github.com/Kagami/av1-bench [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:av1-bench "Jump back to footnote 18 in the text")

19. OTTVerse. "Comparing SVT-AV1 Presets: Size, Quality, and Speed with CRF Variations." 2024. https://ottverse.com/analysis-of-svt-av1-presets-and-crf-values/ [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:ottverse-presets "Jump back to footnote 19 in the text") [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref2:ottverse-presets "Jump back to footnote 19 in the text")

20. Phoronix. "SVT-AV1 3.0 Released With Faster CPU-Based AV1 Encoding." 2025. https://www.phoronix.com/news/SVT-AV1-3.0-Released [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:svt-av1-3 "Jump back to footnote 20 in the text")

21. IETF. "RFC 7742: WebRTC Video Processing and Codec Requirements." March 2016. https://datatracker.ietf.org/doc/html/rfc7742 [↩](https://www.forasoft.com/learn/video-encoding/articles/av1-state-2026#fnref:rfc7742 "Jump back to footnote 21 in the text")


## Call to action

- **Talk to a video engineer** — [book a 30-minute scoping call](https://calendly.com/vadim-fora-soft/30min) to talk through your av1 vs hevc plan.
- **See our case studies** — [250+ shipped projects across video streaming, WebRTC, OTT, telemedicine, e-learning, surveillance, and AR/VR](https://www.forasoft.com/projects).
- **Download the AV1 Deployment Checklist 2026** — [One-page A4 reference with SVT-AV1 FFmpeg recipes, the 2026 hardware decode footprint, the CRF cheat sheet, and the four-stage deployment plan](https://www.forasoft.com/learn/downloads/av1-deployment-checklist).

Key takeaways

- AV1 is the royalty-free codec built by AOMedia (founded 2015) and published in March 2018; in 2026 it is the default modern codec.
- Compression: ~30% below HEVC, ~50% below H.264, ~25% below VP9 at matched quality on modern streaming content.
- Hardware decode covers ~80% of consumer devices in 2026 — universal on post-2022 Android flagships and on Apple silicon from A17 Pro / M3.
- The reference encoder libaom is slow; SVT-AV1 preset 4–6 is the production sweet spot in 2026 with real-time at preset 12.
- The Sisvel patent pool claims AV1 royalties; Netflix and Google have publicly refused to pay since 2020 with no successful lawsuit yet.
- Ship AV1 + HEVC dual ladders today, encode at 10-bit, use SVT-AV1 with CRF 22–28, and budget for an HEVC fallback to cover the 20% AV1-less device tail.

## Related glossary terms

[**Banding** \\
Visible stepping on smooth gradients (sky, walls) caused by insufficient bit depth. Mitigated by 10-bit encoding or dithering.](https://www.forasoft.com/learn/video-encoding/glossary/terms/banding)

[**Bitrate** \\
Average number of bits transmitted per second of video. Higher bitrate = more data = (usually) better quality; the chief lever in delivery.](https://www.forasoft.com/learn/video-encoding/glossary/terms/bitrate)

[**Block** \\
Generic term for a rectangular region of pixels processed jointly by an encoder — sizes range from 4×4 up to 128×128 in modern codecs.](https://www.forasoft.com/learn/video-encoding/glossary/terms/block)

[**BT.2020** \\
The 4K/HDR-era wide-gamut color space. Much larger gamut than BT.709; used by UHD broadcast, HDR10, Dolby Vision, modern OTT.](https://www.forasoft.com/learn/video-encoding/glossary/terms/bt2020)

[**CABAC** \\
Context-adaptive binary arithmetic coder used by H.264 Main/High and HEVC. ~10–20 % better than CAVLC; the dominant entropy coder of the AVC family.](https://www.forasoft.com/learn/video-encoding/glossary/terms/cabac)

[**Chroma (Cb, Cr)** \\
The color components of a YCbCr pixel, encoding blue-difference (Cb) and red-difference (Cr). Lower-resolution than luma in 4:2:0.](https://www.forasoft.com/learn/video-encoding/glossary/terms/chroma)

![Nikolay Sapunov, CEO at Fora Soft](https://cdn.prod.website-files.com/64e8910adc5a63966a68acea/6a0c72a04c29b22a8ddb991b_6a0c728d356dff40621f0967_nikolay_sapunov.jpeg)

Nikolay Sapunov

CEO at Fora Soft

Nikolay Sapunov is the CEO of Fora Soft, a software studio specialising in video-centric products — streaming platforms, WebRTC apps, video conferencing, and AI-driven video tools. He writes the Video Encoding knowledge base to give product and engineering teams a clear, no-jargon way to reason about codecs, compression, HDR, and the trade-offs behind every video architecture decision.

[View profile →](https://www.forasoft.com/team/nikolay-sapunov)

[← Previous article\\
\\
VP8 And VP9: Google's Open Alternative](https://www.forasoft.com/learn/video-encoding/articles/vp8-vp9-explained)

[Next article →\\
\\
H.266 / VVC: Technically Excellent, Market-Weak](https://www.forasoft.com/learn/video-encoding/articles/h266-vvc-explained)

[![Fora Soft](https://cdn.prod.website-files.com/64e8910adc5a63966a68acc1/64e8910adc5a63966a68acca_ForaSoft_logo_white.svg)](https://www.forasoft.com/)

Specialist software house for video, real-time and AI products. Founded 2005. 50 in-house engineers.

Knowledge base

[Blog](https://www.forasoft.com/blog) [Guides](https://www.forasoft.com/learn#guides) [Courses](https://www.forasoft.com/learn#courses) [Glossary](https://www.forasoft.com/learn#glossary-ref) [Downloads](https://www.forasoft.com/learn/downloads)

Company

[Services](https://www.forasoft.com/services) [Projects](https://www.forasoft.com/projects) [Demos](https://www.forasoft.com/lab) [Calculator](https://www.forasoft.com/calculator) [Contacts](https://www.forasoft.com/#Contacts)

[+852-8193-2621](tel:+85281932621)

Hong Kong

[+1 (914) 775-5855](tel:+19147755855)

New York · USA

[eager2develop@forasoft.com](mailto:eager2develop@forasoft.com)

Email

© Fora Soft, 2005–2026

[Privacy Policy](https://www.forasoft.com/legal)

[![Instagram](https://cdn.prod.website-files.com/64e8910adc5a63966a68acc1/6572e49c2e345879335a916d_social-inst.svg)](https://www.instagram.com/eager2develop)[![Youtube](https://cdn.prod.website-files.com/64e8910adc5a63966a68acc1/6572e49cb0629b88aca1ac72_social-youtube.svg)](https://www.youtube.com/@eager2develop)[![Github](https://cdn.prod.website-files.com/64e8910adc5a63966a68acc1/6572e49c10e3dc944fabd72d_social-github.svg)](https://github.com/forasoft)[![Behance](https://cdn.prod.website-files.com/64e8910adc5a63966a68acc1/6572e49ce35e8d399a2c2a91_social-behance.svg)](https://www.behance.net/Fora-Soft?tracking_source=search_users|fora%20soft)[![Dribbble](https://cdn.prod.website-files.com/64e8910adc5a63966a68acc1/6572e49cb20bde1e8f6ff3ca_social-dribbble.svg)](https://dribbble.com/ForaSoft)[![LinkedIn](https://cdn.prod.website-files.com/64e8910adc5a63966a68acc1/6572e49c720cb8a64ce75980_social-linkedin.svg)](https://linkedin.com/company/fora-soft-llc-)[![Medium](https://cdn.prod.website-files.com/64e8910adc5a63966a68acc1/6572e49c0155f1acbea2b259_social-medium.svg)](https://forasoft.medium.com/)

![](https://cdn.prod.website-files.com/64e8910adc5a63966a68acc1/64ff15bebc7380d05ba1b049_Rectangle-3815.avif)

Describe your project and we will get in touch

Enter your message

Enter your email

Enter your name

reCAPTCHA

Recaptcha requires verification.

I'm not a robot

reCAPTCHA

By submitting data in this form, you agree with the [Personal Data Processing Policy.](https://www.forasoft.com/legal)

[![WhatsApp](https://cdn.prod.website-files.com/64e8910adc5a63966a68acc1/664479e0486f0a12e9428679_Whatsapp%20logo.avif)\\
\\
WhatsApp us](https://wa.me/971585396367) [![Calendly](https://cdn.prod.website-files.com/64e8910adc5a63966a68acc1/664479e07b108eff3e0a1125_Calendly%20logo.avif)\\
\\
Book a call](https://calendly.com/vadim-fora-soft/30min)

![ ](https://cdn.prod.website-files.com/64e8910adc5a63966a68acc1/64e8910adc5a63966a68acdc_emoji_succsess.avif)

Your message has been sent successfully

We will contact you soon

Message not sent. Please try again.

✕

reCAPTCHA

──────── [TRUNCATED] ────────
Showing 44,995 chars (head) + 14,688 chars (tail) of 66,128 total clean characters.
Full text saved to: /home/wubu/.hermes/profiles/mind-palace/cache/web/www.forasoft.com-ecb5a13343.md
To read the omitted middle: read_file path="/home/wubu/.hermes/profiles/mind-palace/cache/web/www.forasoft.com-ecb5a13343.md" offset=295 limit=200  (the file is the complete page; raise/lower offset to page through it).
─────────────────────────────