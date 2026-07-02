Six Sines is possible because of a group of collaborators who helped with design, testing, and ideas over the two big sprints to date (the 0.9->1.1 sprint in early 2025 and the 1.2 sprint in spring 2026)

Members of the surge community were incredibly helpful. Especially EvilDragon, Andreya/A.Liv, Jacky Ligon, Trace98, dj.tuBIG/MaliceX and Kinsey Dulcet were massively helpful in both sprints. Zulu Matrix (Tommy Gerencser) inspired the PD features in 1.2 and was crucial in their design. Discord user MrkRnbrd designed the logo. Discord user Barbouze provided the Selenized themes. Chris Johnson / Airwindows left the bad
resamplers intact for your listening pleasure. And many other folks tried and gave great feedback early versions.

The February 2025 one synth challenge produced many great bug reports, amazing instruction videos from Taron, and a large number of super tracks with the 1.1 build out.

The Factory Patch bank was a result of contributions from early users who gave us large numbers of patches. Kinsey and Jacky did all their patches 2 or 3 times as the synth changed, and gave us huge swaths of the factory library.  The continual rework of their patches as the synth evolved was a labor of love. We also had substantial patch contributions from Metamyther, Trinitou, dj.tuBIG/MaliceX, videco and SiL3NC3

Six Sines is a 'clap-first' synth, using the clap and clap wrapper projects to project into various formats. Thanks to my collaborators on the clap team - especially defiantnerd - for the work on making this technology complete.

The Six Sines UI is based on the JUCE framework. One of our resampling strategies is to use libsamplerate. We support microtuning using MTS-ESP. The excellent simde (simd-everywhere) library gives us portability to arm platforms. fmt gives us string fomatting in C++17. And finally vast swaths of six sines are actually the surge synth team open source libraries configured in ways which pushed them around in new and exciting ways. See the source repo for the full set of deps and their licenses.
