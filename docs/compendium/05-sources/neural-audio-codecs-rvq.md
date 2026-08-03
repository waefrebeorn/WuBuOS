# Neural Audio Codecs: Lyra, EnCodec, SoundStream and What Comes Next

Source: https://www.forasoft.com/learn/audio-for-video/articles-audio/neural-audio-codecs-lyra-encodec-soundstream
Ingested: 2026-08-03 (Media avenue, Kevin-Bacon growth wave)

---

# Neural Audio Codecs: Lyra, EnCodec, SoundStream and What Comes Next

![Nikolay Sapunov](https://cdn.prod.website-files.com/64e8910adc5a63966a68acea/6a0c72a04c29b22a8ddb991b_6a0c728d356dff40621f0967_nikolay_sapunov.jpeg)

By

Nikolay Sapunov

·

Published

June 8, 2026

·

10

min read

TL;DR

A neural audio codec is compression software where a neural network — not a hand-tuned algorithm — learns how to squeeze sound into a tiny stream of numbers and rebuild it convincingly, and since 2021 the best ones reach the quality of the old codecs at three to four times fewer bits. Three systems built the field: Google's SoundStream introduced the core trick (a learned encoder plus residual vector quantization), Google's Lyra v2 turned it into a phone-ready product that ships in real calls, and Meta's EnCodec pushed quality high enough to handle music, not just speech. The same compressed tokens that make a codec also turned out to be the way you feed sound into a large language model, which is why neural codecs are now the quiet engine under voice AI — and why "what comes next" is less about saving bandwidth and more about making sound something software can think in. This article explains how these codecs actually work in plain language, gives the real 2026 bitrate-versus-quality and processor-cost numbers, and is honest about why Opus, a fifteen-year-old codec, still wins almost every real-time deployment today.

## Why this matters

If you build anything that carries voice or sound — a conferencing app, a streaming service, a telemedicine platform, a voice assistant — you will keep hearing that "neural codecs" are about to change everything, and you need to know whether that is true for your product or just a conference-talk headline. The honest answer in 2026 has two halves. For raw real-time calling, the fifteen-year-old [Opus codec](https://www.forasoft.com/learn/audio-for-video/articles-audio/opus-codec-explained) is still the right default, and a neural codec is a research line item, not a shipping decision. But for anything that touches voice AI — a model that listens and talks back, automatic dubbing, on-device speech generation — neural codecs are already the foundation, because they are how sound gets into and out of a language model. This article is for the product manager, founder, or engineer who has to tell the difference: what these systems do, what they cost, where they actually ship today, and what the next three years hold. It is the deep-dive companion to our survey of [AI in audio for video](https://www.forasoft.com/learn/audio-for-video/articles-audio/ai-in-audio-cloning-dubbing-restoration-generative), which sketched neural codecs as one of four families; here we open the box.

* * *

## What a codec is, and what makes a neural one different

Start with the ordinary kind. A **codec** — short for coder-decoder — is the software that compresses sound into fewer bits to send or store, then unpacks it again at the other end. Every voice call and every streamed song uses one. For about forty years, codecs were designed by hand: engineers studied how human hearing works, noticed that we cannot hear a quiet tone right after a loud one, and wrote algorithms that throw away the sounds the ear will not notice. That is the family that gave us MP3, AAC, and Opus, and we explain the machinery in [how audio compression works](https://www.forasoft.com/learn/audio-for-video/articles-audio/how-audio-compression-works). It is brilliant engineering, and it is hand-built, brick by brick, by people.

A **neural audio codec** replaces the human designers with a learning process. Instead of an engineer deciding which parts of the sound to keep, you take a **neural network** — a mathematical model with millions of adjustable numbers — and show it enormous quantities of audio until it discovers, on its own, how to represent sound compactly and rebuild it. Nobody writes the rules; the model learns them from data. The result is a codec that can hit bitrates the hand-built ones cannot touch at the same quality, because the network finds patterns in real sound that no human thought to encode by hand.

The analogy worth keeping: a traditional codec is like a translator who learned grammar from a rulebook, and a neural codec is like one who grew up bilingual. The rulebook translator is fast, predictable, and cheap. The bilingual one is more fluent, catches nuances the rules miss — and is far more expensive to train and to run. That cost difference, as we will see, is the whole story of why neural codecs have not yet taken over.

* * *

## The one idea that made it work: residual vector quantization

Almost every neural codec you will read about — SoundStream, Lyra, EnCodec, the newest research systems — is built on the same three-part shape, and one of the three parts is the clever bit that made the whole field practical. Walk through it slowly, because once you have it, every codec in this article is a variation on it.

First, an **encoder**: a neural network that takes the raw sound wave and squeezes it down into a compact stream of numbers, called a _latent representation_. Think of it as the network's private shorthand for "what this sound is." Last, a **decoder**: a mirror-image network that takes the shorthand and reconstructs the sound wave you can actually play. Encoder and decoder are trained together, end to end, so the shorthand the encoder writes is exactly the shorthand the decoder knows how to read.

The middle part is where the difficulty — and the cleverness — lives. The encoder's shorthand is made of continuous numbers, like 0.3719 or −1.882, and you cannot send numbers of infinite precision over a network. You have to round them to a fixed menu of allowed values, a process called **quantization**. The catch is brutal: to represent rich, high-quality sound, you would need a menu of millions of possible values, and a menu that big is too expensive in memory and computation to use.

The breakthrough, introduced for audio in SoundStream in 2021, is called **residual vector quantization**, or RVQ, and it works like making change with coins. Suppose you owe \\$8.37 and your first "coin jar" only holds dollar coins. You pay \\$8, and you are left with a _residual_ of \\$0.37 that you could not represent. So you go to a second jar that holds dimes, pay three (\\$0.30), and your residual shrinks to \\$0.07. A third jar of pennies clears the rest. Each jar is small and cheap, but stacked in layers they represent any amount precisely. RVQ does exactly this with sound: the first quantizer captures the rough shape, the second captures the error the first one left behind, the third captures the error after that, and so on through a stack of layers. A handful of small menus, stacked, do the work of one impossibly large one.

![Left-to-right signal-flow diagram of a neural audio codec. On the left, a sound wave enters an encoder block labelled neural network, which outputs a compact latent representation. The latent feeds into a stack of residual vector quantization layers drawn as three stacked boxes: the first quantizer captures the coarse value, the second captures the residual error left by the first, the third captures the residual after that, each labelled with the coin-change analogy of dollars then dimes then pennies. The stacked quantizer indices form the compressed bitstream in the middle, drawn crossing a transmission or storage gap. On the right, a mirror-image decoder block labelled neural network reconstructs the sound wave. A caption notes that dropping the lower quantizer layers lowers the bitrate instantly, which is how one model serves many bitrates.](https://cdn.prod.website-files.com/64e8910adc5a63966a68acea/6a26c63958817d2f3f06f112_6a26c626d0146893ea96c410_06_11-neural-audio-codecs-lyra-encodec-soundstream__01-rvq-pipeline.svg)_Figure 1. The shared shape of every neural codec: a learned encoder, a stack of residual quantizers that refine the signal layer by layer, and a learned decoder. Using fewer quantizer layers lowers the bitrate on the spot._

RVQ has a second gift that turns out to matter enormously in practice. Because the layers refine the sound progressively, you can simply _stop early_. Use all the layers and you get the highest quality at the highest bitrate; drop the bottom few layers and you get lower quality at a lower bitrate — from the same model, decided on the fly. This is why a single neural codec can offer several bitrates without shipping several models. SoundStream's authors trained one model that runs anywhere from 3 to 18 kbps with almost no penalty versus models trained for each rate separately. Hold onto that property; it is the reason these codecs adapt so gracefully to bad networks.

One more piece, because it explains why the rebuilt sound is convincing rather than muffled. Early attempts to reconstruct audio from a tiny representation came out dull and blurry. The neural codecs fixed this by borrowing a technique from image generation called **adversarial training**: alongside the decoder, you train a second network — a _discriminator_ — whose only job is to tell real recordings apart from the decoder's reconstructions. The decoder is then pushed to fool the discriminator. The two networks chase each other, and the decoder learns to produce sound crisp enough to pass as real. That adversarial push is what separates a 2021-and-later neural codec from the muddy autoencoders that came before.

* * *

## The three codecs that built the field

### SoundStream — the one that started it

**SoundStream**, published by Google researchers in 2021, is the foundation everything else stands on. It was the first end-to-end neural codec to combine all three ideas above — a learned convolutional encoder-decoder, residual vector quantization, and adversarial training — into one system that clearly beat the hand-built codecs. The headline result is the number you will see quoted everywhere: in listening tests on 24 kHz audio, SoundStream at **3 kbps outperformed Opus at 12 kbps**, and approached the quality of the EVS speech codec at 9.6 kbps, using three to four times fewer bits for the same perceived quality. It also ran in real time on a smartphone processor and could fold noise suppression into the same pass, cleaning the audio for free while it compressed.

SoundStream did not ship as a consumer product under that name. Its importance is as the blueprint: RVQ-for-audio was its contribution, and every codec below inherits it.

### Lyra v2 — the one that actually ships

**Lyra v2** is Google's production speech codec, and it is SoundStream made practical. Released in September 2022, it is built directly on the SoundStream architecture, with the residual quantizer sitting on each side of the transmission link. It offers three bitrates — **3.2, 6, and 9.2 kbps** — and you can switch between them mid-call simply by changing how many quantizer layers you use, the RVQ property from the section above doing real work in a real product.

The quality numbers are specific and worth stating plainly. In Google's listening tests, Lyra v2 at 3.2, 6, and 9.2 kbps matched the quality of Opus at 10, 13, and 14 kbps respectively. Put differently, Lyra delivers Opus-grade speech using roughly half to sixty percent of the bandwidth. Against the telephony codecs it is meant to replace, it outperforms EVS and AMR-WB while using a fraction of their bits.

But the number that explains why Lyra matters is not about bitrate at all — it is about speed. On a Pixel 6 Pro phone, Lyra v2 encodes and decodes a 20-millisecond frame of audio in **0.57 milliseconds**, which is about 35 times faster than real time, and its algorithmic delay is 20 milliseconds, comparable to Opus. Google built Lyra specifically so a neural codec could run on an ordinary phone without draining the battery or adding lag. That engineering — not the research quality — is what let it ship in Google's calling products for users on weak networks. Lyra is the existence proof that a neural codec _can_ be a product. It is also, tellingly, still the exception rather than the rule.

### EnCodec — the one that handles music

**EnCodec**, from Meta in October 2022, took the SoundStream shape and pushed the quality ceiling up, especially for music. Where SoundStream and Lyra target speech, EnCodec was built to handle the full range: 24 kHz mono speech at the low end and 48 kHz stereo music at the high end, at bitrates from 1.5 up to 24 kbps. In MUSHRA listening tests — the standard method for [subjective audio quality testing](https://www.forasoft.com/learn/audio-for-video/articles-audio/subjective-testing-mushra-mos-ab-abx) — EnCodec scored higher than SoundStream at matched bitrates, and at 3 kbps it beat both Lyra v2 at 6 kbps and Opus at 12 kbps.

EnCodec added two refinements worth knowing. It used a single multi-scale spectrogram discriminator to make training simpler and faster, and a "loss balancer" that kept the training stable. And it showed that you could bolt a small Transformer model onto the codec's output to squeeze the bitstream a further 25 to 40 percent — turning a 3 kbps stream into roughly 1.9 kbps — while still running faster than real time. That last trick is a preview of where the field went next: treating the codec's output not as a final bitstream but as a _language_ that another model can compress and predict.

![Horizontal bar chart titled equal-quality bitrate, comparing how many kilobits per second each codec needs to reach roughly the same perceived speech quality. Opus is the baseline reference bar at 12 kbps in the warning colour. SoundStream reaches the same quality at 3 kbps in the positive green colour, annotated three to four times fewer bits. EnCodec sits at 3 kbps in the primary colour with a note that it also handles 48 kHz stereo music up to 24 kbps. Lyra v2 shows three stacked segments at 3.2, 6, and 9.2 kbps in the accent blue, annotated as matching Opus at 10, 13, and 14 kbps. Mimi sits at 1.1 kbps in the purple AI colour, annotated speech only, built for language models. The x-axis is bitrate in kbps and lower is better; a caption warns that lower bitrate buys nothing if the processor cost is too high to deploy.](https://cdn.prod.website-files.com/64e8910adc5a63966a68acea/6a26c63958817d2f3f06f102_6a26c629b128e54c1a1b33d1_06_11-neural-audio-codecs-lyra-encodec-soundstream__02-codec-comparison-bars.svg)_Figure 2. Where each codec lands on the bitrate-for-equal-quality axis. The neural codecs win on bits; the rest of the article is about what that win costs._

* * *

## The numbers that decide it: bitrate, quality, and the processor bill

The temptation with neural codecs is to read the bitrate savings and conclude they are obviously better. The discipline is to put all three costs on the table at once, because the bandwidth saving is real but it is paid for on a different axis.

Here is the comparison that matters for a real product decision, with the figures stated at the quality each codec is designed for.

| Codec | Year | Bitrate range | Equal-quality benchmark | Audio type | Processor cost | Ships in production? |
| --- | --- | --- | --- | --- | --- | --- |
| **Opus** | 2012 | 6–510 kbps | the baseline everyone compares to | speech + music | very low (microseconds) | yes — mandatory in WebRTC |
| **SoundStream** | 2021 | 3–18 kbps | 3 kbps ≈ Opus 12 kbps | speech + general | moderate (phone-capable) | no — research blueprint |
| **Lyra v2** | 2022 | 3.2 / 6 / 9.2 kbps | 9.2 kbps ≈ Opus 14 kbps | speech | low–moderate (0.57 ms/frame on a phone) | yes — Google calling |
| **EnCodec** | 2022 | 1.5–24 kbps | 3 kbps > Opus 12 kbps | speech + music | high | no — research / model fuel |
| **Mimi** | 2024 | ~1.1 kbps | speech-LLM tokenizer, not a hi-fi target | speech | high | yes — inside voice-AI models |

Read that table twice. The left columns are a triumph: neural codecs reach the same quality as Opus at a third to a quarter of the bits, and the newest speech-LLM codecs run near 1 kbps. The right columns are the catch. Opus decodes in microseconds on any device made this century; neural codecs need a neural network to run for every frame, which costs orders of magnitude more processor time, more battery, and — for the heavier ones — a capable phone or a GPU. Lyra is the only one engineered hard enough to escape this on a commodity phone, and even Lyra is a speech-only codec, not a general one.

Do the arithmetic on the saving so it is concrete. Suppose a 50-person all-hands streams audio and each speaker uses Opus at 12 kbps:

50 speakers × 12 kbps = 600 kbps of audio

Swap in a neural codec at equal quality and 3 kbps per speaker:

50 speakers × 3 kbps = 150 kbps of audio

That is a 4× cut in the audio bandwidth, and it lands exactly where bandwidth is scarce — a patient on rural mobile data in a telemedicine call, a student on a congested campus network. The saving is genuine. The reason you still cannot deploy it everywhere is the column on the far right: running the network on every client, every frame, costs more than the bandwidth it saves, unless your devices and your battery budget can absorb it.

> **Pitfall — "beats Opus" almost never means "replace Opus."** Nearly every neural-codec paper reports beating Opus at low bitrate, and it is true under the paper's conditions. It is also nearly irrelevant to a real-time product, because Opus's advantage is not its bitrate — it is that it decodes for free on every device, has zero licensing cost, and is built into the WebRTC standard. Before you take a "3 kbps beats Opus 12 kbps" result as a reason to switch, ask the three questions the paper did not: what does it cost to run on my worst client device, what does it do to battery, and what is the latency? For most live calling in 2026, those answers still favour Opus. Reach for a neural codec where the network is the hard constraint and the compute is available — not as a default.

* * *

## Where neural codecs really ship in 2026: as the mouth and ears of voice AI

Here is the turn that surprised the field, and it is the most important thing to understand about why neural codecs matter even though Opus still wins live calling.

A large language model predicts the next token in a sequence. For text that is easy — a tokenizer chops writing into a few thousand possible word-pieces, and the model predicts one at a time. Sound is far harder, because raw audio is tens of thousands of numbers per second; a model trying to predict it sample by sample drowns. What neural codecs gave the AI world, almost as a side effect, is a way to turn a second of sound into a short sequence of discrete **tokens** — the quantizer indices from the RVQ stack — that a language model can predict exactly the way it predicts words. The codec became the _tokenizer for audio_. The encoder is the model's ears; the decoder is its mouth.

This reframes everything. EnCodec was adopted as the audio tokenizer inside Meta's MusicGen. Descript's open **Descript Audio Codec** (2023), which compresses 44.1 kHz audio at a roughly 90× ratio, became a drop-in fuel for audio language models. And the clearest example of the new design is **Mimi**, the codec built by Kyutai for its Moshi speech model in 2024. Mimi runs at about **1.1 kbps and just 12.5 frames per second**, and it does something the earlier codecs did not: it splits its tokens into two kinds. The first token stream is **semantic** — distilled from a speech-understanding model, it carries _what is being said_, the way text does, stripped of voice. The remaining streams are **acoustic** — they carry _how it sounds_: the speaker's timbre, pitch, and emotion. By separating meaning from voice, Mimi lets a language model reason about content and delivery as two different things, which is exactly what a system needs to listen and speak naturally. Moshi, built on it, can listen and talk at the same time with about 200 milliseconds of latency.

So the production answer in 2026 is split cleanly. For moving a known voice across a network in real time, Opus is the default and a neural codec is rarely worth its compute. For _generating_ or _understanding_ voice with a model — voice assistants, [AI dubbing](https://www.forasoft.com/learn/audio-for-video/articles-audio/ai-in-audio-cloning-dubbing-restoration-generative), on-device text-to-speech, real-time translation — the neural codec is not optional; it is the thing that makes the model able to hear and speak at all. The same RVQ stack that compresses a call is what tokenizes sound for an LLM. One idea, two futures.

![Two-panel diagram contrasting the two jobs a neural codec does. The left panel is labelled transport and shows a microphone feeding an encoder, a compressed bitstream crossing a network cloud, and a decoder feeding a speaker, with a note that Opus usually wins this job today because it decodes almost for free. The right panel is labelled tokenizer for AI and shows a microphone feeding the same encoder, but its output is drawn as a row of discrete tokens split into a semantic token in one colour carrying what is said and acoustic tokens in another colour carrying how it sounds. The tokens feed into a large language model block, which emits new tokens that flow into the decoder and out to a speaker, with a note that this job is where neural codecs are already essential. A caption states the same residual vector quantization stack underlies both jobs.](https://cdn.prod.website-files.com/64e8910adc5a63966a68acea/6a26c63958817d2f3f06f0fa_6a26c628302590e17508753f_06_11-neural-audio-codecs-lyra-encodec-soundstream__03-codec-as-tokenizer.svg)_Figure 3. The same codec, two jobs. As transport (left), Opus still wins on cost. As a tokenizer for a language model (right), the neural codec is already the foundation — and the semantic/acoustic token split is why voice AI can separate what is said from how it sounds._

* * *

## What comes next

Three lines of work define the next few years, and none of them is "neural codecs finally beat Opus on a phone for plain calling." That race is mostly over: Opus stays the default for ordinary transport, and a fork called the **AOMedia Open Audio Codec (OAC)** — begun in early 2026 on the Opus source base — is the industry's bet on the _next traditional_ codec, aiming to beat Opus on its own terms rather than replace it with a neural one.

The first real frontier is **making neural codecs cheap enough to deploy at the edge**. The whole field now treats processor cost, not quality, as the hard problem. Research challenges in 2025 explicitly target codecs that run under tight compute, latency, and bitrate limits on everyday devices in noisy, real-world conditions — the constraints that separate a benchmark winner from something you can ship. Lyra showed it is possible for speech on a good phone; the work now is to make it true for more devices, more audio types, and lower power.

The second frontier is the **codec-as-tokenizer line maturing into infrastructure**. As voice-native AI models move from the lab into products — assistants that hear tone, real-time dubbing that preserves a performance, translation that keeps your voice — the neural codec underneath becomes a piece of plumbing as standard as a video codec is today. The open questions there are about token design: how to split semantic from acoustic information cleanly, how many tokens per second a model needs, and whether the next systems even keep discrete tokens at all — some 2025 research generates audio from _continuous_ representations and skips quantization entirely.

The third is the one to watch for your own roadmap: **the line between "codec" and "generator" dissolving**. A neural codec that can rebuild a voice from 1.1 kbps of tokens is, mechanically, very close to a model that can _generate_ that voice from scratch. The same machinery that conceals a dropped packet by predicting the missing audio (covered in [packet loss concealment](https://www.forasoft.com/learn/audio-for-video/articles-audio/packet-loss-concealment-plc)) is the machinery that clones a voice. As we noted in the [AI-in-audio survey](https://www.forasoft.com/learn/audio-for-video/articles-audio/ai-in-audio-cloning-dubbing-restoration-generative), that convergence is where the consent, likeness, and disclosure law lives — and it is arriving through the codec layer, not just the dubbing tools. When your compression and your synthesis are the same network, "is this audio real" stops being a question the codec can answer for you.

## Where Fora Soft fits in

We build conferencing, telemedicine, e-learning, OTT streaming, and surveillance products, and our advice on neural codecs is deliberately unfashionable: for live, real-time audio in 2026, we still reach for Opus first, because its near-zero decode cost, zero licensing, and WebRTC-mandatory status beat a bitrate saving that most clients do not need and most devices cannot afford to run. Where we watch neural codecs closely is the AI layer — when a product needs a model to understand or generate voice, the codec is no longer a compression choice but a foundation choice, and getting the [WebRTC audio pipeline](https://www.forasoft.com/learn/audio-for-video/articles-audio/webrtc-audio-pipeline-end-to-end) right around it (latency budget, per-client compute, fallback to Opus) is the engineering that decides whether the feature is usable. The right framing for a client is rarely "should we switch to a neural codec" and almost always "where in our pipeline does a learned model earn its processor cost" — and the answer is moving, year by year, from nowhere toward the AI features at the edges.

## What to read next

- [AI in audio for video: voice cloning, dubbing, restoration, generative music](https://www.forasoft.com/learn/audio-for-video/articles-audio/ai-in-audio-cloning-dubbing-restoration-generative)
- [Opus: the open codec that ate WebRTC](https://www.forasoft.com/learn/audio-for-video/articles-audio/opus-codec-explained)
- [How audio compression works: the four ideas behind every modern codec](https://www.forasoft.com/learn/audio-for-video/articles-audio/how-audio-compression-works)

## Call to action

- **Talk to a audio engineer** — [book a 30-minute scoping call](https://calendly.com/vadim-fora-soft/30min) to talk through your neural audio codec plan.
- **See our case studies** — [250+ shipped projects across video streaming, WebRTC, OTT, telemedicine, e-learning, surveillance, and AR/VR](https://www.forasoft.com/projects).
- **Download the Neural Audio Codec Cheat Sheet** — [One page: what SoundStream, Lyra v2, EnCodec, and Mimi each do, the 2026 bitrate-vs-quality and processor-cost numbers, residual vector quantization in one diagram, and when Opus still wins](https://www.forasoft.com/learn/downloads/neural-audio-codec-cheatsheet).

## References

01. N. Zeghidour, A. Luebs, A. Omran, J. Skoglund, M. Tagliasacchi, _SoundStream: An End-to-End Neural Audio Codec_, arXiv:2107.03312, July 2021 (published in IEEE/ACM Transactions on Audio, Speech, and Language Processing, 2021). [https://arxiv.org/abs/2107.03312](https://arxiv.org/abs/2107.03312) _(Primary source from the codec's authors, read directly: convolutional encoder-decoder + residual vector quantization + adversarial training; one model spans 3–18 kbps via structured quantizer dropout; at 3 kbps outperforms Opus at 12 kbps and approaches EVS at 9.6 kbps on 24 kHz audio; real-time on a smartphone CPU.)_
02. A. Défossez, J. Copet, G. Synnaeve, Y. Adi, _High Fidelity Neural Audio Compression_ (EnCodec), arXiv:2210.13438, Meta AI, October 2022. [https://arxiv.org/abs/2210.13438](https://arxiv.org/abs/2210.13438) _(Primary source, read directly: streaming encoder-decoder with quantized latent; single multi-scale spectrogram discriminator + loss balancer; 24 kHz mono to 48 kHz stereo, 1.5–24 kbps; higher MUSHRA than SoundStream at equal bitrate; a lightweight Transformer compresses the bitstream a further 25–40%, e.g. 3 kbps → 1.9 kbps.)_
03. Google Open Source Blog, _Lyra V2 — a better, faster, and more versatile speech codec_, 30 September 2022. [https://opensource.googleblog.com/2022/09/lyra-v2-a-better-faster-and-more-versatile-speech-codec.html](https://opensource.googleblog.com/2022/09/lyra-v2-a-better-faster-and-more-versatile-speech-codec.html) _(Primary vendor source, read directly: built on SoundStream; bitrates 3.2 / 6 / 9.2 kbps switchable via RVQ layer count; in MUSHRA, those rates match Opus at 10 / 13 / 14 kbps; 20 ms delay; 0.57 ms to encode+decode a 20 ms frame on a Pixel 6 Pro, ~35× real time; outperforms EVS and AMR-WB at 50–60% of their bandwidth.)_
04. IETF RFC 6716, _Definition of the Opus Audio Codec_, J.-M. Valin, K. Vos, T. Terriberry, September 2012. [https://www.rfc-editor.org/rfc/rfc6716](https://www.rfc-editor.org/rfc/rfc6716) _(Standards primary source: the controlling specification for Opus — the codec every neural codec benchmarks against and the one mandatory in WebRTC; 6–510 kbps operating range, speech + music, microsecond-class decode.)_
05. R. Kumar, P. Seetharaman, A. Luebs, I. Kumar, K. Kumar, _High-Fidelity Audio Compression with Improved RVQGAN_ (Descript Audio Codec), arXiv:2306.06546, June 2023. [https://arxiv.org/abs/2306.06546](https://arxiv.org/abs/2306.06546) _(Primary source: open universal neural codec; 44.1 kHz audio at ~90× compression / 8 kbps; explicitly positioned as a drop-in replacement for EnCodec in audio language models such as AudioLM, MusicLM, MusicGen.)_
06. A. Défossez, L. Mazaré, M. Orsini, A. Royer, P. Pérez, H. Jégou, E. Grave, N. Zeghidour, _Moshi: a speech-text foundation model for real-time dialogue_, arXiv:2410.00037, Kyutai, September 2024. [https://arxiv.org/abs/2410.00037](https://arxiv.org/abs/2410.00037) _(Primary source for the Mimi codec: ~1.1 kbps, 12.5 Hz frame rate; splits tokens into a WavLM-distilled semantic stream and acoustic streams; powers the Moshi full-duplex speech model at ~200 ms latency.)_
07. V. Volhejn (Kyutai), _Neural audio codecs: how to get audio into LLMs_, October 2025. [https://kyutai.org/codec-explainer](https://kyutai.org/codec-explainer) _(First-party explainer from the Mimi/Moshi lab, read directly: walks through VQ-VAE, the straight-through estimator, residual vector quantization, the multi-level token problem, and the semantic-vs-acoustic token split; source for the "codec as LLM tokenizer" framing and the RVQ coin-change intuition.)_
08. Low-Resource Audio Codec Challenge (LRAC) 2025, _Challenge Description_, arXiv:2510.23312, 2025. [https://arxiv.org/abs/2510.23312](https://arxiv.org/abs/2510.23312) _(Current research source: frames the 2026 frontier as neural codecs that meet strict compute, latency, and bitrate limits under everyday noise and reverberation — i.e. edge deployability, not raw quality, is now the hard problem.)_
09. A. Biswas et al., _A Streamable Neural Audio Codec with Residual Scalar-Vector Quantization for Real-Time Communication_, arXiv:2504.06561, 2025. [https://arxiv.org/abs/2504.06561](https://arxiv.org/abs/2504.06561) _(Current research source on streamable low-complexity neural codecs aimed specifically at real-time communication — evidence of the push to make neural codecs deployable in WebRTC-grade pipelines.)_
10. BlogGeek.me (T. Levent-Levi), _OAC: AOMedia Open Audio Codec_, WebRTC Glossary, 2026. [https://bloggeek.me/webrtcglossary/oac/](https://bloggeek.me/webrtcglossary/oac/) _(Industry source: OAC began in early 2026 on the Opus source base, aiming to surpass Opus in efficiency as the next traditional codec — context for why the post-Opus default is not assumed to be neural. Secondary source; flagged for a primary AOMedia citation at review once the working group publishes.)_
11. N. Zeghidour et al., _SoundStream_ IEEE/ACM TASLP version (DOI 10.1109/TASLP.2021.3129994), 2021. [https://dl.acm.org/doi/10.1109/TASLP.2021.3129994](https://dl.acm.org/doi/10.1109/TASLP.2021.3129994) _(Peer-reviewed journal version of reference 1, cited for the formal RVQ definition and the subjective-test methodology.)_

_Living note: the neural-codec field moves fast. SoundStream (2021), Lyra v2 (2022), EnCodec (2022), DAC (2023), and Mimi (2024) are stable primary references; the 2025–2026 frontier (LRAC challenge, streamable RTC codecs, OAC) is current research and industry activity and should be re-verified on the next refresh. Bitrate-equal-quality claims are stated at each codec's own test conditions and sampling rate; they are not a single apples-to-apples benchmark._

Key takeaways

- A neural codec lets a network _learn_ compression from data instead of hand-built rules.
- Residual vector quantization — refining sound in stacked layers — is the idea that made it work.
- SoundStream set the blueprint; Lyra v2 shipped it on phones; EnCodec extended it to music.
- Neural codecs reach Opus quality at 3–4× fewer bits, but cost far more processor and battery.
- For live real-time calling in 2026, Opus is still the right default; neural codecs rarely beat it.
- Their real 2026 home is voice AI: the codec is how sound becomes tokens an LLM can predict.

## Related glossary terms

[**Opus** \\
An open, royalty-free codec that combines speech (SILK) and music (CELT) modes in one bitstream. The default audio codec for every WebRTC implementation.](https://www.forasoft.com/learn/audio-for-video/glossary/terms-audio/opus)

[**SoundStream** \\
Google's pioneering end-to-end neural codec and the basis of Lyra v2. Introduced the residual-vector-quantization bottleneck that most neural codecs now use.](https://www.forasoft.com/learn/audio-for-video/glossary/terms-audio/soundstream)

[**Lyra** \\
Google's neural speech codec that delivers intelligible voice around 3 kbps — far below where classical codecs collapse. Built on SoundStream-style techniques.](https://www.forasoft.com/learn/audio-for-video/glossary/terms-audio/lyra)

[**Bitrate** \\
How many bits per second a compressed stream uses, in kbps. The master quality-vs-size knob: 128 kbps AAC stereo, 24 kbps Opus voice, 768 kbps E-AC-3 5.1.](https://www.forasoft.com/learn/audio-for-video/glossary/terms-audio/bitrate)

[**EnCodec** \\
Meta's open neural audio codec. An encoder-quantizer-decoder trained end-to-end with residual vector quantization, handling speech and music from ~1.5 to 24 kbps.](https://www.forasoft.com/learn/audio-for-video/glossary/terms-audio/encodec)

[**Audio codec** \\
Coder-Decoder — the algorithm that compresses audio to fewer bits and reconstructs it on playback. The unit of compression efficiency in any audio product.](https://www.forasoft.com/learn/audio-for-video/glossary/terms-audio/audio-codec)

[**Quantization** \\
Rounding each continuous sample to the nearest value the bit depth allows. The rounding error is quantization noise, lowered by adding dither.](https://www.forasoft.com/learn/audio-for-video/glossary/terms-audio/quantization)

[**Neural audio codec** \\
A codec that learns compression from data instead of hand-designed transforms — Lyra, EnCodec, SoundStream. Often using residual vector quantization to hit very low bitrates.](https://www.forasoft.com/learn/audio-for-video/glossary/terms-audio/neural-codec)

![Nikolay Sapunov](https://cdn.prod.website-files.com/64e8910adc5a63966a68acea/6a0c72a04c29b22a8ddb991b_6a0c728d356dff40621f0967_nikolay_sapunov.jpeg)

Nikolay Sapunov

CEO at Fora Soft

Nikolay Sapunov is the CEO of Fora Soft, a software studio specialising in video-centric products — streaming platforms, WebRTC apps, video conferencing, and AI-driven video tools. He writes the Video Encoding knowledge base to give product and engineering teams a clear, no-jargon way to reason about codecs, compression, HDR, and the trade-offs behind every video architecture decision.

[View profile →](https://www.forasoft.com/team/nikolay-sapunov)

[← Previous article\\
\\
AI in Audio for Video: Voice Cloning, Dubbing, Restoration, Generative Music](https://www.forasoft.com/learn/audio-for-video/articles-audio/ai-in-audio-cloning-dubbing-restoration-generative)

[Next article →\\
\\
The Future: End-To-End Learned Audio, AI-Driven Loudness, Personalized Mixes](https://www.forasoft.com/learn/audio-for-video/articles-audio/future-end-to-end-learned-audio-personalized-mixes)

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