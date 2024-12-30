# Six Sines

Six Sines is a small synthesizer which explores audio rate inter-modulation of signals.
Some folks would call it "an FM synth" but it's really a bit more PM/AM, and anyway that's
kinda not the point.

If you want to read a manual, [go here](doc/manual.md)

If you want to download a release or recent version, [go here](https://github.com/baconpaul/six-sines/releases)

## Background

The project exists for a few reasons

1. At the end of 2024, I was in a bit of a slump dev-wise for a variety of reasons and wanted a 
   small project to sort of jump-start me for 2025. I had watched [Newfangled Dan ship obliterate](https://www.newfangledaudio.com/obliteratefrom 
from idea to loved plugin in November, and I thought hey can I do the same in December
2. As I wrapped up 2024, I also wanted to take account of how well our project to factor code into libraries
went and make a sort of todo-list of why it was hard to build a synth. This includes some 
insights into the sst- libraries and the clap-wrapper
2. Another india audio dev, who asked to not be named here in case this sucks, which it doesn't I dont think,
said to a group of users in a discord "the most useless thing is a 6 op fill matrix pure FM synth".
3. It seemed fun, and I thought some people would download it.
4. It had been in my head for a while and I wanted to hear how it sounded

## How to build this software

We provide pre-build windows, linux, and macOS binaries at the [release page](https://github.com/baconpaul/six-sines/releases) but especially on 
Linux, you may want to build it yourself, since we use ubuntu 24 machines and linux doesn't really exist.

So to build it, do the standard

```aiignore
git clone https://github.com/baconpaul/six-sines
cd six-sines
cmake -Bignore/build -DCMAKE_BUILD_TYPE=Release
cmake --build ignore/build --config Release --target six-sines_all
```

various plugins and executables will now be scattered around ignore/build.

## I found a bug or want a feature added

So this was really a one month sprint. And its pretty self contained thing. So feature requests
are things I may say no to. But open an issue and chat. Or send a PR!

Bugs especially let me know.

But remember, "Programming an FM/PM/RM synth is hard" is not a bug in six-sines!

## Why is this a `baconpaul` and not a `surge-synthesizer` project

Well you know. It's not quite up to snuff for a surge project. And its pretty idiosyncratic.
I may do a few more side quest projects in 2025. Lets see.