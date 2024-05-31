# Fixer Midi Demo

This is an experiment with the low level wave sound API in Win32, for example [waveOutOpen](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveoutopen). 

It's an homage to a NES game Golgo 13: Top Secret Episode.

As for the midi player itself, it supports a single channel square wave. The program spins up a worker thread that sends the sound buffer to the device in a loop, and an event gets signaled when each submission is done. The music data is baked in. The graphics have some simple animations displayed with Direct2D.

Graphical assets required for this demo are not stored in the repository, and need to be supplied locally.

---

Demo video:

[![IMAGE ALT TEXT](http://img.youtube.com/vi/4MCsu98_5PM/0.jpg)](http://www.youtube.com/watch?v=4MCsu98_5PM "Video Title")

Press the 'A' key at any time to exit the demo.
