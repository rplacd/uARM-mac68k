Workbench for my attempts to get uARM running on the 68k Mac.

So far, I have:

– Gotten uARM to compile on macOS on Apple Silicon.

<img width="668" alt="Screenshot 2024-05-27 at 4 13 09 AM" src="https://github.com/rplacd/uARM-mac68k/assets/147152/1e2508e5-ffe3-4df2-9cd2-228b0e80a409">

Next, I will:

– Investigate how to move emulated RAM in the PC/macOS target into a file
– Create a rump mac68k target, that launches to some form of console IO, and make it work with some mac68k cross-compiler.