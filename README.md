# C FFT Visualizer

Real-time FFT spectrum analyzer for audio files and live microphone input. Built with C, FFTW3, Raylib, and PortAudio.

> **Note**: Early prototype. Only WAV files and microphone input are supported. Sample rates above 48 kHz may cause the visualization to lag behind the audio.

## Requirements

- Clang
- Make
- [Raylib](https://www.raylib.com/)
- [FFTW3](http://www.fftw.org/)
- [PortAudio](http://www.portaudio.com/)

**Arch Linux**

```
sudo pacman -S clang raylib fftw portaudio
```

## Build

```
make build
make run
make clean
make format
make help
```

## Usage

```
# Audio file (loop optional)
./build/c_fft_visualizer <path_to_audio_file> --loop

# Live microphone
./build/c_fft_visualizer --mic
```

## Controls

| Key | Action |
|-----|--------|
| `O` | Cycle octave scaling (1/1 ... 1/48) |
| `C` | Cycle color gradients |
| `P` | Toggle pink compensation |
| `A` | Toggle dB-domain averaging vs linear |
| `F` | Toggle Fast/Slow averaging preset |
| `H` | Cycle peak-hold (Off, 0.5s, 1.0s, 2.0s) |
| `W` | Cycle frequency weighting (Z/A/C) |
| `T` | Cycle meter time weighting (Fast/Slow/Impulse) |
| `K` | Calibrate SPL to 94 dB reference (mic mode) |
| `G` | Peak-find from max-hold trace and lock cursor |
| `Left/Right` | Step locked band by one bar |
| `Mouse Left` | Toggle nearest-band lock |
| `R` | Reset peak and max-hold traces |
| `Space` | Freeze/Unfreeze live trace |
| `F11` | Toggle fullscreen |

## Features

- Log-frequency bars with fractional-octave smoothing (1/1 ... 1/48)
- dB-domain time averaging (EMA) with Fast/Slow presets
- Frequency weighting modes (Z/A/C)
- SPL calibration workflow (94 dB calibrator via key command)
- Per-band peak-hold with timed decay
- Persistent max-hold trace (manual clear)
- Peak-find and nearest-band lock navigation
- Pink compensation (pink-flat display)
- dB grid overlay and peak/RMS meters
- Cursor readout (hover for exact Hz and level)

## Configuration

Edit `include/config.h` to tune defaults.

Default FFT: `FFT_WINDOW_SIZE=8192`, `FFT_HOP_SIZE=FFT_WINDOW_SIZE/16`. Increase window size for finer bass resolution; decrease for faster transient tracking. Adjust `UI_SCALE` if text and panels look too small or too large.

## Screenshot

![screenshot](assets/screenshot.png)

## License

GPL-3.0
