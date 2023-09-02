# GfeCLI

A simple wrapper for GfeSDK to allow you to control the Nvidia Geforce Highlights API through a CLI.

For example you can save the past 5 minutes of recording:

`gfecli --process "game.exe" --highlight 00:05:00`

# Requirements

The project uses bundled Windows 32/64-bit binaries in `lib` and contains a copy of the GfeSDK in the `includes` folder.

These folders are not part of the project source code and is just a reference and requirement for build and runtime.
