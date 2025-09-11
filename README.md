# C FFT Visualizer

Visualize the Fast Fourier Transform (FFT) of audio signals in real-time using C, Raylib, and FFTW3.

## Requirements
- C Compiler
- [Raylib](https://www.raylib.com/)
- [FFTW3](http://www.fftw.org/)

Arch Linux:
```bash
sudo pacman -S clang raylib fftw
```


## Build and run
```bash
./build.sh

./build/c_fft_visualizer <path_to_audio_file> <--loop (optional)>
```

## Controls
- `O`: Change octave scaling
- `C`: Cycle through color gradients
- `F11`: Toggle fullscreen


## Screenshot
![screenshot](assets/screenshot.png)

## License
MIT License