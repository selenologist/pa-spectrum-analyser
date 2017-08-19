#Minimal spectrum analyser for pulseaudio.

Requires pulseaudio, fftw3, GLFW, GLEW and an opengl implementation.

#Options
* `-m` for mono (defaults to stereo)
* `-s[N]` to set FFT size (must be a power of two; note the lack of a space between the s and the number. Defaults to 512)
* `-r[N]` to set sample rate (also no space between the r and the number. Defaults to 44100)

#License: MIT.

*note: does **not** comply with ISO C++ (variadic macros where the extra arguments are not used, and C99 designated initializers), but g++ and clang++ are fine with it*
