[All](https://thewolfsound.com/posts) [Audio FX](https://thewolfsound.com/audio-fx) [C/C++](https://thewolfsound.com/c-cpp) [Digital Signal Processing](https://thewolfsound.com/dsp) [Podcast](https://thewolfsound.com/podcast) [Programming in General](https://thewolfsound.com/programming-in-general) [Python](https://thewolfsound.com/python) [Sound in General](https://thewolfsound.com/sound-in-general) [Sound Synthesis](https://thewolfsound.com/sound-synthesis)

* * *

How to generate sound in code using the wavetable synthesis technique?

Play Video: Wavetable Synthesis Algorithm Explained

Please
accept marketing cookies
to access the video player.

In this article, you will learn:

- how to generate sound using wave tables,
- step-by-step wavetable synthesis algorithm (also known as _fixed-waveform synthesis_ \[7\]),
- what are pros and cons of wavetable synthesis, and
- how is wavetable synthesis related to other synthesis methods.

In the follow-up articles, [an implementation of this technique in the Python programming language](https://thewolfsound.com/sound-synthesis/wavetable-synth-in-python/), [the JUCE framework](https://thewolfsound.com/sound-synthesis/wavetable-synth-plugin-in-juce/), and [the Rust programming language](https://thewolfsound.com/sound-synthesis/wavetable-synth-in-rust/) are presented.

## A Need for a Fast and Efficient Synthesis Method

_Computer-based sound synthesis is the art of generating sound through software._

In the early days of digital sound synthesis, sound was synthesised using specialized digital signal processing hardware. Later on, the community started using software for the same purposes but the underlying principles and algorithms remained the same. To obtain real-time performance capabilities with that technology, there was a great need to generate sound efficiently in terms of memory and processing speed. Thus, the wavetable technique was convceived: it is both fast and memory-inexpensive.

## From Gesture to Sound

The process of generating sound begins with a _musician_’s _gesture_. Let’s put aside who a musician might be or what kind of gestures they perform. For the purpose of this article, a gesture could be as simple as pressing a key on a MIDI keyboard, clicking on a virtual keybord’s key, or pressing a button on any controller device.

![](https://thewolfsound.com/assets/img/posts/synthesis/2021-08-13-wavetable-synthesis-theory/gesture_to_sound.png)_Figure 1. In sound synthesis, a gesture of a musician controls the sound generation process._

A gesture provides _control information_. In the case of pressing a key on a MIDI keyboard, control information would incorporate information on which key was pressed and how fast was it pressed ( _velocity_ of a keystroke). We can change the note number information into frequency fff and the velocity information into amplitude AAA. This information is sufficient to generate sound using most of the popular synthesis algorithms.

## Sine Generator

Let’s imagine that given frequency and amplitude information we want to generate a sine wave. The general formula of a sine waveform is

s(t)=Asin⁡(2πft+ϕ),(1)s(t) = A \\sin (2 \\pi f t + \\phi), \\quad (1)s(t)=Asin(2πft+ϕ),(1)

where fff is the frequency in Hz, AAA is the amplitude in range \[0,1\]\[0, 1\]\[0,1\], ttt is time in seconds, and ϕ\\phiϕ is the initial phase, which we will ignore for now (i.e., assume that ϕ=0\\phi=0ϕ=0).

As we discussed in the [digital audio basics article](https://thewolfsound.com/what-is-sound-the-notion-of-an-audio-signal/), digital audio operates using samples rather than physical time. The nnn-th sample occurs at time ttt when

n=fst,(2)n = f\_s t, \\quad (2)n=fs​t,(2)

where fsf\_sfs​ is the sampling rate, i.e., the number of samples per second that the system (software or hardware) produces.

After inserting Equation 2 into Equation 1, we obtain the formula for a digital sine wave

s\[n\]=Asin⁡(2πfn/fs),(3)s\[n\] = A \\sin (2 \\pi f n / f\_s), \\quad (3)s\[n\]=Asin(2πfn/fs​),(3)

How to compute the sin⁡\\sinsin in the above formula? In programming languages (and any calculators for that matter), we often have a `sin()` function, but how does it compute its return value?

`sin()` calls use the _Taylor expansion_ of the sine function \[1\]

sin⁡(x)=x−x33!+x55!−x77!…(4)\\sin(x) = x - \\frac{x^3}{3!} + \\frac{x^5}{5!} - \\frac{x^7}{7!} \\dots \\quad (4)sin(x)=x−3!x3​+5!x5​−7!x7​…(4)

Above expansion is infinite, so on real-world hardware, it needs to be truncated at some point (after obtaining sufficient accuracy). Its advantage is, that it uses operations realizable in hardware (multiplication, division, addition, subtraction). Its disadvantage is that it involves **a lot** of these operations. If we need to produce 44100 samples per second and want to play a few hundred sines simultaneously (what is typical of additive synthesis), we need to be able to compute the sin⁡\\sinsin function more efficiently than with Taylor expansion.

## A Wave Table

A _wave table_ is an array in memory in which we store a fragment of a waveform. A _waveform_ is a plot of a signal over time. Thus, one period of a sine wave stored in memory looks as follows:

![](https://thewolfsound.com/assets/img/posts/synthesis/2021-08-13-wavetable-synthesis-theory/sine_wave_table.png)_Figure 2. A wave table with 64 samples of the sine waveform._

The above wave table uses 64 samples to store one period of the sine wave. These values **can** be calculated using the Taylor expansion because we compute them only once and store them in memory.

sin⁡\\sinsin period is exactly 2π2 \\pi2π. The period of a wave table is its length, let’s denote it by LLL. For each sample index k∈{0,…,L−1}k \\in \\{0, \\dots, L-1\\}k∈{0,…,L−1} in the wave table, there exists a corresponding argument θ∈\[0,2π)\\theta \\in \[0, 2\\pi)θ∈\[0,2π) of the sine function.\
\
kL=θ2π.(5)\\frac{k}{L} = \\frac{\\theta}{2 \\pi}. \\quad (5)Lk​=2πθ​.(5)\
\
The above equation tells us that there is a mapping between the values in the wave table and the values of the original waveform.\
\
## Computing a Waveform Value from the Wave Table\
\
Equation 5 holds for θ∈\[0,2π)\\theta \\in \[0, 2\\pi)θ∈\[0,2π). If we want to calculate the values of arbitrary x∈Rx \\in \\mathbb{R}x∈R, we need to remove the multiplicity of 2π2 \\pi2π contained in xxx to bring it to the \[0,2π)\[0, 2\\pi)\[0,2π) range. In other words, if\
\
x=2πl+ϕx,ϕx∈\[0,2π),(6)x = 2\\pi l + \\phi\_x, \\quad \\phi\_x \\in \[0, 2\\pi), \\quad (6)x=2πl+ϕx​,ϕx​∈\[0,2π),(6)\
\
then we want to find ϕx\\phi\_xϕx​. In software, it can be done by subtracting or adding 2π2 \\pi2π to xxx until we obtain a value in the desired range. Alternatively, we can use a function called `fmod()`, which allows us to obtain the remainder of a floating-point division.\
\
We can subsequently compute the corresponding index in the wave table from the proportion in Equation 5.\
\
k=ϕxL2π.(7)k = \\frac{\\phi\_x L}{2\\pi}. \\quad (7)k=2πϕx​L​.(7)\
\
Now `waveTable[k]` should return the value of sin⁡(x)\\sin(x)sin(x), right? There is one more step that we need…\
\
## What If kkk Is Non-Integer?\
\
In most cases, kkk computed in Equation 7 won’t be an integer. It will rather be a floating-point number between some two integers denoting the wave table indices, i.e., i<=k<i+1,i∈{0,…,L−1},k∈\[0,L)i <= k < i+1, \\quad i \\in \\{0, \\dots, L-1\\}, k \\in \[0, L)i<=k<i+1,i∈{0,…,L−1},k∈\[0,L).\
\
To make kkk an integer, we have 3 options:\
\
- _truncation (0th-order interpolation)_: removing the non-integer part of kkk, a.k.a. `floor(k)`,\
- _rounding_: rounding kkk to iii or i+1i+1i+1, whichever is nearest, a.k.a. `round(k)`,\
- _linear interpolation (1st-order interpolation)_: computing a weighted sum of the wave table values at iii and i+1i+1i+1. The weights correspond to kkk’s distance to i+1i+1i+1 and iii respectively, i.e., we return `(k-i)*waveTable[i+1] + (i+1 - k)*waveTable[i]`,\
- ~~_higher-order interpolation_~~: too expensive and unnecessary for wavetable synthesis.\
\
Each recall of a wave table value is called a _wave table lookup_.\
\
## Wave Table Looping\
\
We know how to efficiently compute a waveform’s value for an arbitrary argument. In theory, given amplitude AAA, frequency fff, and sampling rate fsf\_sfs​, we are able to evaluate Equation 3 for any integer nnn. Using different wave tables, we can obtain different waveforms. It means we can generate an arbitrary waveform at an arbitrary frequency! Now, how to implement it algorithmically?\
\
Thanks to the information on fff and fsf\_sfs​, we don’t have to calculate the 2πfn/fs2 \\pi f n / f\_s2πfn/fs​ argument of sin⁡\\sinsin in Equation 3 for each nnn separately. nnn gets incremented by 1 on a sample-by-sample basis, so as long as fff does not change (i.e., we play at a constant pitch), the argument of sin⁡\\sinsin gets incremented in a predictable manner. Actually, the argument 2πfn/fs+ϕ2 \\pi f n / f\_s + \\phi2πfn/fs​+ϕ is called the _phase_ of the sine (again, in our considerations ϕ=0\\phi=0ϕ=0). The difference between the phase of the waveform for neighboring samples is called a _phase increment_ and can be calculated as\
\
θinc(f)=2πf(n+1)/fs−2πfn/fs=2πf/fs.(8)\\theta\_\\text{inc}(f) = 2 \\pi f (n+1) / f\_s - 2 \\pi f n / f\_s = 2 \\pi f / f\_s. \\quad (8)θinc​(f)=2πf(n+1)/fs​−2πfn/fs​=2πf/fs​.(8)\
\
θinc(f)\\theta\_\\text{inc}(f)θinc​(f) depends explicitly on fff (tone frequency) and implicitly on fsf\_sfs​ (which typically remains unchanged during processing so we can treat it as a constant). With θinc(f)\\theta\_\\text{inc}(f)θinc​(f) we can initialize a `phase` variable to 0 and increment it by θinc(f)\\theta\_\\text{inc}(f)θinc​(f) after generating each sample. When a key is pressed we reset `phase` to 0, calculate θinc(f)\\theta\_\\text{inc}(f)θinc​(f) according to the pitch of the pressed key, and start producing the samples.\
\
### Index Increment\
\
Having the information on phase increment, we can calculate the _index increment_, i.e., how the index to the wave table changes with each sample.\
\
kinc=(k+1)−k=(ϕx+θinc)L2π−ϕxL2π=θincL2π=fLfs.(9)k\_\\text{inc} = (k+1) - k = \\frac{(\\phi\_x + \\theta\_\\text{inc})L}{2\\pi} - \\frac{\\phi\_x L}{2\\pi} \\newline = \\frac{\\theta\_\\text{inc} L}{2\\pi} = \\frac{fL}{f\_s}. \\quad (9)kinc​=(k+1)−k=2π(ϕx​+θinc​)L​−2πϕx​L​=2πθinc​L​=fs​fL​.(9)\
\
When a key is pressed, we set an `index` variable to 0. For each sample, we increase the `index` variable by kinck\_\\text{inc}kinc​ and do a lookup. As long as the key is pressed, kinck\_\\text{inc}kinc​ is nonzero and we perform the wave table lookup.\
\
When `index` exceeds the wave table size, we need to bring it back to the \[0,L)\[0, L)\[0,L) range. In implementation, we can keep subtracting LLL as long as `index` is greater or equal to LLL or we can use the `fmod` operation. This “index wrap” results from the _phase wrap_ which we discussed below Equation 5; since the signal is periodic, we can shift its phase by the period without changing the resulting signal.\
\
### Phase Increment vs Index Increment\
\
Phase increment and index increment are two sides of the same coin. The former has a physical meaning, the latter has an implementational meaning. You can increment the phase and use it to calculate the index or you can increment the index itself. Index increment is more efficient because we don’t need to perform the multiplication by LLL and the division by 2π2\\pi2π for each sample (Equation 7); we calculate only the increment when the instantaneous frequency changes. We’ll therefore restrict ourselves to the implementations using the index increment.\
\
## Wavetable Synthesis Algorithm\
\
Below is a schematic of how wavetable synthesis using index increment works.\
\
![](https://thewolfsound.com/assets/img/posts/synthesis/2021-08-13-wavetable-synthesis-theory/wavetable-synthesis-algorithm-diagram.png)_Figure 3. A diagram of the wavetable synthesis algorithm using index increment. After \[2\]._\
\
kinc\[n\]k\_\\text{inc}\[n\]kinc​\[n\] is the increment of the index into the wave table. It is denoted as a digital signal because in practice it can be changed on a sample-by-sample basis. It is directly dependent on the frequency of the played sound. If no sound is played kinc\[n\]k\_\\text{inc}\[n\]kinc​\[n\] is 0 and the `index` should be reset to 0. Alternatively, one could specify that if no sound is played this diagram is inactive (no signals are supplied to or taken from it).\
\
For each new output sample, index increment is added to the `index` variable stored in a single-sample buffer (denoted by z−1z^{-1}z−1 as explained in the [article on delays](https://thewolfsound.com/identity-element-of-the-convolution/)). This index is then “brought back” into the range of wavetable indices \[0,L)\[0, L)\[0,L) using the `fmod` operation. We still keep the fractional part of the index.\
\
Then, we perform the lookup into the wavetable. The lookup can be done using an interpolation strategy of choice.\
\
Finally, we multiply the signal by a sample-dependent amplitude A\[n\]A\[n\]A\[n\]. A\[n\]A\[n\]A\[n\] signal is called the _amplitude envelope_. It may be, for example, a constant, i.e., A\[n\]=1,∀n∈ZA\[n\] = 1, \\forall n \\in \\mathbb{Z}A\[n\]=1,∀n∈Z.\
\
The output signal y\[n\]y\[n\]y\[n\] is determined by the wave table used for the lookup and currently generated frequency.\
\
**We thus created a wavetable synthesizer!**\
\
## Oscillator\
\
The diagram in Figure 3 presents an _oscillator_. An oscillator is any unit capable of generating sound. It is typically depicted as a rectangle combined with a half-circle \[3, 4\] as in Figure 4. That symbol typically has an amplitude input A (A\[n\]A\[n\]A\[n\] in Figure 3) and a frequency input fff (used to calculate kinc\[n\]k\_\\text{inc}\[n\]kinc​\[n\] in Figure 3).\
\
![](https://thewolfsound.com/assets/img/posts/synthesis/2021-08-13-wavetable-synthesis-theory/oscillator.png)_Figure 4. The oscillator symbol._\
\
Additionally, what is not shown in Figure 4, an oscillator pictogram usually has some indication of what type of waveform it generates. For example, it may have the sine symbol  inside to show that it outputs a sine wave.\
\
Oscillators are sometimes denoted using the VCO abbreviation, which stands for _voltage-controlled oscillator_. This term originates from the analog days of sound synthesis, when electric voltage determined oscillators’ amplitude and frequency.\
\
Oscillators are the workhorse of sound synthesis. What is presented in Figure 3 is one realization of an oscillator but the oscillator itself is a more general concept. Wavetable synthesis is just one way of implementing an oscillator.\
\
## Sound Example: Sine\
\
Let’s use a precomputed wave table with 64 samples of one sine period from Figure 2 to generate 5 seconds of a sine waveform at 440 Hz using 44100 Hz sampling rate.\
\
We thus have L=64L = 64L=64, f=440f=440f=440 Hz, fs=44100f\_s=44100fs​=44100 Hz, kinc=0.6395…k\_\\text{inc} = 0.6395\\dotskinc​=0.6395…. The resulting sound is:\
\
Your browser does not support the audio tag.\
\
The magnitude spectrum of this tone is shown below.\
\
![](https://thewolfsound.com/assets/img/posts/synthesis/2021-08-13-wavetable-synthesis-theory/sine_spectrum.png)_Figure 5. Magnitude frequency spectrum of a sine generated with wavetable synthesis._\
\
Great! It sounds like a sine and we obtain just one frequency component. Everything as expected! Now, let’s generate sound using a different wavetable, shall we?\
\
## Sound Example: Sawtooth\
\
To generate a sawtooth, we use the same parameters as before just a different wave table:\
\
![](https://thewolfsound.com/assets/img/posts/synthesis/2021-08-13-wavetable-synthesis-theory/sawtooth_wave_table.png)_Figure 6. A wave table with 64 samples of the sawtooth waveform._\
\
Let’s listen to the output:\
\
Your browser does not support the audio tag.\
\
That sounds ok, but we hear some ringing. How does it look in the spectrum?\
\
![](https://thewolfsound.com/assets/img/posts/synthesis/2021-08-13-wavetable-synthesis-theory/sawtooth_spectrum.png)_Figure 7. Magnitude frequency spectrum of a sawtooth generated with wavetable synthesis._\
\
We can notice that there are some inharmonic frequency components that do not correspond to the typical decay of the sawtooth spectrum. These are aliased partials which occur because the spectrum of the sawtooth crossed the Nyquist frequency. To learn more about why this happens, you can [check out my article on aliasing](https://thewolfsound.com/what-is-aliasing-what-causes-it-how-to-avoid-it/).\
\
Aliasing increases if we go 1 octave higher:\
\
Your browser does not support the audio tag.\
\
Ouch, that doesn’t sound nice. The frequency spectrum reveals aliased partials that appear as inharmonicities:\
\
![](https://thewolfsound.com/assets/img/posts/synthesis/2021-08-13-wavetable-synthesis-theory/sawtooth880_spectrum.png)_Figure 8. Magnitude frequency spectrum of a 880Hz sawtooth generated with wavetable synthesis._\
\
We’ve just discovered the main drawback of wavetable synthesis: aliasing at high frequencies. If we went even higher with the pitch, we would obtain a completely distorted signal.\
\
How to fix aliasing for harmonic-rich waveforms? We can only increase the sampling rate of the system. Since it is not something we would like to do, pure wavetable synthesis is rarely used nowadays.\
\
The type of digital distortion seen in Figure 8 was typical of the early digital synthesizers of the 1980s. A lot of effort was put into the development of alternative algorithms to synthesize sound. The main focus was to obtain an algorithm that would produce partial-rich waveforms at low frequencies and partial-poor waveforms at high frequencies. These algorithms are sometimes called _antialiasing oscillators_. An example of such an oscillator can be found in [“Oscillator and Filter Algorithms for Virtual Analog Synthesis” paper by Vesa Välimäki and Antti Huovilainen](https://www.researchgate.net/publication/220386519_Oscillator_and_Filter_Algorithms_for_Virtual_Analog_Synthesis) \[5\].\
\
## Abstract Waveforms\
\
With wavetable synthesis we can use arbitrary wavetables. For example, in Figure 9, I summed 5 Gaussians, subtracted the mean and introduced a fade-in and fade-out.\
\
![](https://thewolfsound.com/assets/img/posts/synthesis/2021-08-13-wavetable-synthesis-theory/gaussians_wave_table.png)_Figure 9. An abstract wave table constructed with 5 Gaussians._\
\
Here is a sound generated using this wave table at 110 Hz.\
\
Your browser does not support the audio tag.\
\
Sounds like a horn, doesn’t it?\
\
Here’s its spectrum:\
\
![](https://thewolfsound.com/assets/img/posts/synthesis/2021-08-13-wavetable-synthesis-theory/gaussians_spectrum.png)_Figure 10. Magnitude frequency spectrum of a 110 Hz sound generated from an abstract wavetable._\
\
As we can see, it decays quite nicely, so no audible aliasing is present.\
\
## Sampling: Extended Wavetable Synthesis?\
\
Sampling is a technique of recording real-world instruments and playing back these sounds according to user input. We could, for example, record single guitar notes with pitches corresponding to all keys on the piano keyboard. In practice, however, notes for only some of the keys are recorded and the notes in between are interpolated versions of its neighbors. In this way, we store separate samples for high-pitched notes and thus avoid the problem of aliasing because it’s not present in the data in the first place.\
\
Wavetable synthesis could be viewed as sampling with the samples truncated to one waveform period \[4\].\
\
With sampling, a lot more implementation issues come up. Since sampling is not the topic of this article, we won’t discuss it here.\
\
## Single-Cycle, Multi-Cycle, and Multiple Wavetable\
\
What we discussed so far is a _single-cycle_ variant of the wavetable synthesis, where we use just 1 period of a waveform stored in memory to generate the sound (Figure 11). There are more options available.\
\
![](https://thewolfsound.com/assets/img/posts/synthesis/2021-08-13-wavetable-synthesis-theory/single_cycle_wavetable_synthesis.png)_Figure 11. Single-cycle wavetable synthesis loops over 1 wave table._\
\
In _multi-cycle_ wavetable synthesis, we effectively concatenate different wavetables, whose order can be fixed or random (Figure 12).\
\
![](https://thewolfsound.com/assets/img/posts/synthesis/2021-08-13-wavetable-synthesis-theory/multi_cycle_wavetable_synthesis.png)_Figure 12. Multi-cycle wavetable synthesis loops over multiple wave tables, possibly in a cycle._\
\
For example, we could concatenate sine, square, and sawtooth wave tables to obtain a more interesting timbre.\
\
The resulting wave table would look like this:\
\
![](https://thewolfsound.com/assets/img/posts/synthesis/2021-08-13-wavetable-synthesis-theory/multi_cycle_wave_table.png)_Figure 13. A wave table from a concatenation of sine, square, and sawtooth wave tables._\
\
Here is a sound generated using this wave table at 330 Hz.\
\
Your browser does not support the audio tag.\
\
One can hear the characteristics of all 3 waveforms.\
\
Here’s its spectrum:\
\
![](https://thewolfsound.com/assets/img/posts/synthesis/2021-08-13-wavetable-synthesis-theory/multi_cycle_spectrum.png)_Figure 14. Magnitude frequency spectrum of a 330 Hz sound generated from a concatenation of wave tables._\
\
The above spectrum is heavily aliased. Additionally, we got a frequency component at 110 Hz. That is because by concatenating 3 wave tables, we essentially lengthened the base period of the waveform, effectively lowering its fundamental frequency 3 times. Original waveform was at 330 Hz; the fundamental is now at 110 Hz.\
\
In _multiple wavetable_ variant, one mixes a few wave tables at the same time (Figure 15).\
\
![](https://thewolfsound.com/assets/img/posts/synthesis/2021-08-13-wavetable-synthesis-theory/multiple_wavetable_synthesis.png)_Figure 15. Multiple wavetable synthesis mixes between multiple wave tables while looping over them._\
\
The impact of each of the used wave tables may depend on control parameters. For example, if we press a key mildly, we can get a sine-like timbre, but if we press it fast, we may hear more high-frequency partials. That could be realized by mixing the sine and sawtooth wave tables. The ratio of these waveforms would directly depend on the velocity of the key stroke. There could also be some gradual change in the ratio while a key is pressed.\
\
## Summary\
\
Wavetable synthesis is an efficient method that allows us to generate arbitrary waveforms at arbitrary frequencies. Its low complexity comes at a cost of high amounts of digital distortion caused by the harmonics crossing the Nyquist frequency at high pitches.\
\
Pros of wavetable synthesis:\
\
- computational efficiency,\
- direct frequency-to-parameters mapping,\
- arbitrary waveform generation.\
\
Cons of wavetable synthesis:\
\
- aliasing already at moderately high frequencies,\
- requires further processing and/or extensions to be musically interesting.\
\
Software synthesizers typically use more sophisticated algorithms than the one presented in this article. Nevertheless, wavetable synthesis underlies many other synthesis methods. The produced waveform could be further transformed. Therefore, the discussion of wavetable synthesis allows us to understand the basic principles of digital sound synthesis.\
\
## Bibliography\
\
These are the references I used for this article. If you are interested in the topic of sound synthesis, each of them is a valuable source of information. Alternatively, [subscribe to WolfSound’s newsletter](https://thewolfsound.com/newsletter/) to stay up to date with the newly published articles on sound synthesis!\
\
\[1\] [Taylor series expansion of the sine function on MIT Open CourseWare](https://ocw.mit.edu/courses/mathematics/18-01sc-single-variable-calculus-fall-2010/unit-5-exploring-the-infinite/part-b-taylor-series/session-99-taylors-series-continued/MIT18_01SCF10_Ses99c.pdf)\
\
\[2\] [F. Richard Moore, _Elements of Computer Music_, Prentice Hall 1990](https://www.amazon.com/gp/product/0132525526/ref=as_li_tl?ie=UTF8&camp=1789&creative=9325&creativeASIN=0132525526&linkCode=as2&tag=wolfsound05-20&linkId=71285ec31668f2e8d8cf81094ff51f5f)\
\
\[3\] [Curtis Roads, _Computer Music Tutorial_, MIT Press 1996](https://www.amazon.com/gp/product/0262680823/ref=as_li_tl?ie=UTF8&camp=1789&creative=9325&creativeASIN=0262680823&linkCode=as2&tag=wolfsound05-20&linkId=86e6c4d45c7126d32c13ab2deff2afa2)\
\
\[4\] Marek Pluta, _Sound Synthesis for Music Reproduction and Performance_, monograph, AGH University of Science and Technology Press 2019.\
\
\[5\] [Vesa Välimäki and Antti Huovilainen, _Oscillator and Filter Algorithms for Virtual Analog Synthesis_, Computer Music Journal 30(2):19-31, June 2006](https://www.researchgate.net/publication/220386519_Oscillator_and_Filter_Algorithms_for_Virtual_Analog_Synthesis)\
\
\[6\] [Martin Russ, _Sound Synthesis and Sampling_, 3rd Edition, Focal Press, 2009.](https://www.amazon.com/gp/product/0240521056/ref=as_li_tl?ie=UTF8&camp=1789&creative=9325&creativeASIN=0240521056&linkCode=as2&tag=wolfsound05-20&linkId=6b14259801a2e3d4db314d1df0b2b2f1)\
\
\[7\] [Giovanni De Poli, _A Tutorial on Digital Sound Synthesis Techniques_, Computer Music Journal, January 1992.](https://www.researchgate.net/publication/245122776_A_Tutorial_on_Digital_Sound_Synthesis_Techniques)\
\
Links above may be affiliate links. That means that I may earn a commission if you decide to make a purchase. This does not incur any cost for you. Thank you.\
\
Share this page on:\
\
* * *\
\
[← Previous\\
Post](https://thewolfsound.com/convolution-in-probability-sum-of-independent-random-variables-with-proof/) [Next\\
Post →](https://thewolfsound.com/sound-synthesis/wavetable-synth-in-python/)\
\
Comments powered by\
[Talkyard](https://www.talkyard.io/).\
\
Please\
accept marketing cookies\
to access the display and add comments.