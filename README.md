# Six Sines

Six Sines is a small synthesizer which explores audio rate inter-modulation of signals.
Some folks would call it "an FM synth" but it's really a bit more PM/AM, and anyway that's
kinda not the point.

If you want to read a manual, [go here](doc/manual.md)

If you want to download a release or recent version, [go here](https://github.com/baconpaul/six-sines/releases). We have versioned
releases on that download page and additionally provide a [nightly](https://github.com/baconpaul/six-sines/releases/tag/Nightly) with the latest code at all times.

In March 2025, we released version 1.1. You can read the changelog [here](doc/changelog.md)

And please read the [acknowldgements](doc/ack.md) for a list of thanks.

## Hey are there any demos of this?

Kinsey Dulcet, who designed many of the factory patches, has a demo
track showing the more-80s-inspired sounds you can make. She's also
working on a more 2020s inspired version, and that's pretty exciting!

[Listen to Kinsey's track, 'Retrocade Nights'](https://soundcloud.com/kinseydulcet/retrocade-nights-six-sines-demo)

The [One Synth Challenge](https://www.kvraudio.com/forum/viewtopic.php?t=618178) community used Six Sines as we were developing
1.1 to host OSC 192, and the results are amazing. You can hear all the entries
here, made entirely with Six Sines and built in DAW effects.

[Listen to the One Synth Challenge Playlist](https://soundcloud.com/kvrosc/sets/one-synth-challenge-192-six)

## Background

The project exists for a few reasons

1. At the end of 2024, I was in a bit of a slump dev-wise for a variety of reasons and wanted a 
   small project to sort of jump-start me for 2025. I had watched [Newfangled Dan ship obliterate](https://www.newfangledaudio.com/obliterate) from idea to loved plugin in November, and I thought hey can I do the same in December
2. As I wrapped up 2024, I also wanted to take account of how well our project to factor code into libraries
went and make a sort of todo-list of why it was hard to build a synth. This includes some 
insights into the sst- libraries and the clap-wrapper
2. Another indie audio dev, who asked to not be named here in case this sucks, which it doesn't I dont think,
said to a group of users in a discord in an offhand way something like "the least commercially viable thing is
a 6 op full matrix pure FM synth". So my response was 1. implement one, 2. give it away, 3 ???, 4. you know the meme.

4. It seemed fun, and I thought some people would download it.
5. It had been in my head for a while and I wanted to hear how it sounded

## How to build this software

We provide pre-build windows, linux, and macOS binaries at the [release page](https://github.com/baconpaul/six-sines/releases) but especially on 
Linux, you may want to build it yourself, since we use ubuntu 24 machines and linux doesn't really exist.

So to build it, do the standard

```aiignore
git clone https://github.com/baconpaul/six-sines
cd six-sines
git submodule update --init --recursive
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

## Some Credits

Members of the surge synth team helped with pre-release
testing. Large parts of the factory patch library
at first release were authored by Jacky Ligon and Kinsey
Dulcet, and large parts of the workflow design
improvements came from Andreya and EvilDragon. 
Thanks folks!

We use loads of open source software of course
including all the sst libraries, clap and
the clap wrapper (which itself includes 
vst3, ausdk, rtaudio, rtmidi, and more).
The definitive list right now comes from 
the source directory.
