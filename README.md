# C FFT Visualizer

Visualize the Fast Fourier Transform (FFT) of audio signals in real-time using C, FFTW3, Raylib and PortAudio. Includes RTA-style smoothing, per-band peak-hold, and pink-noise compensation.

> [!WARNING]
> Early prototype.
> Only supports .wav audio files for now or Live microphone.
> Sample rates of 48kHz and higher may make the visualization lag relative to the audio.

## Requirements
- C Compiler
- [Raylib](https://www.raylib.com/)
- [FFTW3](http://www.fftw.org/)

Arch Linux:
```bash
sudo pacman -S clang raylib fftw portaudio
```

## Build and run
```bash
chmod +x build.sh
./build.sh

# Audio file
./build/c_fft_visualizer <path_to_audio_file> <--loop (optional)>

# Live microphone
./build/c_fft_visualizer --mic
```

## Features
- Log-frequency bars with fractional-octave smoothing (1/1…1/48)
- dB-domain time averaging (EMA) with Fast/Slow presets
- Per-band peak-hold with timed decay
- Pink compensation (pink-flat display)
- dB grid overlay and peak/RMS meters
- Use a live microphone with the flag: --mic

## Controls
- O: Change octave scaling (1/1…1/48)
- C: Cycle color gradients
- P: Toggle pink compensation
- A: Toggle dB-domain averaging (v.s. linear)
- F: Toggle Fast/Slow averaging preset
- H: Cycle peak-hold (Off, 0.5s, 1.0s, 2.0s)
- F11: Toggle fullscreen

## Configuration
Edit include/config.h to tune defaults

## Screenshot
![screenshot](assets/screenshot.png)

## License
MIT License