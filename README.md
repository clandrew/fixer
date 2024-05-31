# Fixer Midi Demo

This is an experiment with the low level wave API in Win32, for example [waveOutOpen](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveoutopen). 

It's an homage to a NES game Golgo 13: Top Secret Episode.
As for the midi player itself, it supports a single channel square wave. The program spins up a worker thread that sends the sound buffer to the device in a loop, and an event gets signaled when each submission is done. The music data is baked in.
Graphical assets required for this demo are not stored in the repository, and need to be supplied locally.

Press the 'A' key at any time to exit the demo.
