# GfeCLI

A simple wrapper for GfeSDK to allow you to request and save highlights through a CLI.

## Examples

- Save the past 5 minutes of recording:
  `gfecli --process game.exe --highlight 00:05:00`
  
- Save something that happened 10 minutes ago:

  `gfecli --process game.exe --highlight 00:00:30 --offset 00:10:00 `

### Arguments

- `process` argument can be the full process name or partial match.
- `highlight` and `offset` arguments can be written as timestamps `HH:MM:SS` or as a whole decimal in seconds.
- `offset` is optional, but the other two arguments are mandatory for the CLI to find the correct game and clip duration.

## Requirements

In order to compile the source code yourself you have to request access to the [GeForce Experience SDK](https://developer.nvidia.com/highlights#:~:text=Get%20Highlights%20with%20the%20GeForce%20Experience%20SDK%20on%20GitHub).

1. Follow the link and perform the required steps.
2. Open the [GfeSDK](https://github.com/NVIDIAGameWorks/GfeSDK) repository and copy the header files.
3. Place these files inside the `includes/gfesdk` folder.
4. You should now be able to compile without errors.

Note that the project comes with bundled Windows 32/64-bit binaries in the `lib` folder. However, if the GfeSDK code has changed significantly, compilation might fail, or you might have issues during runtime, if that is the case simply also update the `lib/x64` and `lib/x86`appropriately with the latest files from the repository.

