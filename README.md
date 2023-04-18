# Convolution Reverb on Teensy

This system implements a convolution reverb on Teensy. See convolution.h for an AudioStrem object that actually implements the reverb, and see wav2ir.c for how a mono WAV file can be converted to a header class that the convolution.h needs for the impulse response of the reverb.