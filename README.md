# KwikLook

<One or two sentences: what it is and what it does for the user on KDE Plasma.>

<A short paragraph on why it exists or how it differs from similar tools.>

This is version 0.1.0 (see [CHANGELOG.md](CHANGELOG.md)). <How widely it's tested, e.g. "I mostly test it on my own machine on Arch, so other distributions may need some work.">

## Screenshots

![<What the first screenshot shows>](assets/<screenshot-1>.png)

## Features

- <Feature, written as what the user can do.>
- <...>

## Requirements

- KDE Plasma 6 (Qt <6.x> or newer and KDE Frameworks 6).
- <Runtime dependencies.>
- Optional: <optional tools and what they enable>.

## Installation

### Arch Linux

```sh
<install command>
```

### Other distributions

These aren't tested yet. Install the build dependencies (see below), then:

```sh
cmake --preset release
cmake --build --preset release
sudo cmake --install build/release
```

<details>
<summary><strong>Build dependencies</strong></summary>

Arch:

```sh
sudo pacman -S <packages>
```

Fedora:

```sh
sudo dnf install <packages>
```

Debian and Ubuntu:

```sh
sudo apt install <packages>
```

</details>

## Usage

<How to start it and the main things to do, one bullet each.>

### Keys

| Key | What it does |
|---|---|
| <Key> | <Action> |

### From the command line

| Command | What it does |
|---|---|
| `kwiklook <files>` | Shows a preview of the files; ← and → go between them, Space or Esc closes it |
| `kwiklook --quit` | Stops KwikLook running in the background |

## Settings

<Where settings are and what each page holds.>

## Troubleshooting

**<Problem as the user would describe it.>** <What to do.>

Bugs and ideas are welcome in the [issue tracker](<https://github.com/strandzen/KwikLook/issues>).

## Uninstall

<How to remove it, and what is left behind (settings, caches) if anything.>

## Translations

<Which languages it ships in, and how to help translate.>

## Credits

The Dolphin integration (pressing Space in Dolphin, following its file order and selection) is based on [KDEGlance](https://github.com/AlexGraveler/KDEGlance) by AlexGraveler and the KDEGlance contributors, used under the GPL-3.0-or-later. Thank you!

## License

GPL-3.0-or-later, see [LICENSE](LICENSE).
