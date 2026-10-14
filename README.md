# AppleSkin

AppleSkin is an open-source native mod for Minecraft Bedrock on Android, made for [LeviLauncher](https://github.com/LiteLDev/LeviLaunchroid) and inspired by the Java [AppleSkin mod](https://github.com/squeek502/AppleSkin).

## Features

- Hunger, saturation, and healing previews while holding food
- Saturation outlines and an optional exhaustion bar
- Food values in item tooltips
- Classic and Pocket UI support
- Mod-menu settings with persistent configuration

## Requirements

- Android 9 or newer and a 64-bit ARM device (`arm64-v8a`)
- [LeviLauncher](https://github.com/LiteLDev/LeviLaunchroid) to load the mod
- Minecraft Bedrock **26.52**

## Installation

1. Install LeviLauncher and import Minecraft 26.52.
2. Import `AppleSkin.levipack` through the launcher's mod manager and enable it.
3. Launch Minecraft through LeviLauncher.

## Build

Requires Android NDK r28c, xmake, and Python 3.

```sh
xmake f -y -p android -a arm64-v8a -m release --ndk=/path/to/android-ndk-r28c --ndk_sdkver=28
xmake -y
```

The build produces `libappleskin.so` and `AppleSkin.levipack`.

## Credits

Made by **TheRealBatu & MrPokeG**.

## License

AppleSkin is licensed under the [GNU General Public License v3.0](LICENSE).
