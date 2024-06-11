Workbench for my attempts to get uARM running on the 68k Mac. So far, all I've managed to do was get uARM running on some platforms that have C99 support. One key implementation detail has changed: I moved the emulated SoC's working RAM (16MB) from process memory into a file. 

# Current platforms (just 3 sadly)

- Apple Silicon/macOS/clang

<img width="668" alt="Screenshot 2024-05-27 at 4 13 09 AM" src="https://github.com/rplacd/uARM-mac68k/assets/147152/1e2508e5-ffe3-4df2-9cd2-228b0e80a409">

– Apple Silicon/Windows 11/MSVC 2022;

- DOSBox 386/DOSBox/DJGPP

![Image 6-11-24 at 4 01 AM](https://github.com/rplacd/uARM-mac68k/assets/147152/6ee5d399-dcf9-49e0-9f32-41c89036f998)
