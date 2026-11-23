# KwikLook

KwikLook shows a quick preview of a file when you press Space in Dolphin, on KDE Plasma 6. Press Space or Esc again and it's gone; ← and → go through the folder.

It keeps running in the background, so previews open at once. It shows pictures, text and code, Markdown and web pages, PDFs, office documents, videos and audio, fonts, tables, contacts and calendar events, and the details of any file, without starting a full application. Small edits work right in the preview: crop and turn a picture, trim and turn a video. The preview window fits in with Plasma: its corners, outline and shadow follow your Breeze window decoration settings, and its buttons, colours and fonts follow your application style and colour scheme.

This is version 0.3.0 (see [CHANGELOG.md](CHANGELOG.md)). I mostly test it on my own machine (Arch Linux, Plasma 6 on Wayland, Dolphin 26.08), so other distributions and setups may need some work.

## Screenshots

![A photo previewed from Dolphin, with the information panel open](assets/screenshot-1.png)

## Features

- Press Space in Dolphin to preview the selected file; ← and → go through the folder in Dolphin's order and Dolphin's selection follows along, and the bar shows where you are (“2 / 5”). With several files selected, the preview goes through just those. Enter opens the file in its usual application.
- An index sheet shows all the files as thumbnails to pick one from (Ctrl+Enter).
- Pictures fill the preview window; small ones stay at their real size. Animated GIFs and WebPs play.
- Pictures can be cropped and turned, saved as a copy next to the original or in its place (the original goes to the Trash). Date taken, camera and other photo details are kept.
- Text and code files show with syntax colours and line numbers; long files show their beginning. Find with Ctrl+F, also in PDFs.
- Markdown, HTML, JSON, SVG and .desktop files show formatted, and one button in the bar switches to their source. Nothing is ever loaded from the internet.
- CSV and TSV files show as a table; contacts (.vcf) and calendar files (.ics) as cards.
- PDFs show all their pages, fitted to the window, with a page overview on the side.
- Office documents (Word, Excel, PowerPoint and OpenDocument files) show like PDFs, when LibreOffice is installed.
- Videos and audio play, with the usual controls, volume, repeat and full screen; audio shows its cover, title and artist. With ffmpeg installed, videos can be trimmed and turned; the result is saved as a new file next to the original.
- Fonts show their alphabet and a sample in the font itself, at a size you choose, or your own text; “Install Font” installs one for you.
- Zoom pictures, text and PDFs; pictures zoom towards the mouse pointer and can be dragged around.
- Any other file shows a large thumbnail (as in Dolphin) or its type's icon, with its type, size and when it was changed.
- An (i) panel with the file's details: name, type, size, dates, picture size and camera details, owner and permissions, plus “Show in Dolphin” and “Copy Path”.
- “Open with…” and Share buttons in the preview's bar. For pictures, “Open with…” also offers Spectacle's editor, to draw on them or add text.
- Choose the bar's buttons and their order in the settings, by dragging them.
- Preview files from the command line too: `kwiklook <files>`.

## Requirements

- KDE Plasma 6 with Dolphin.
- Qt 6.8 or newer, with Qt PDF (part of Qt WebEngine on Arch) and Qt Multimedia with its FFmpeg backend.
- KDE Frameworks 6.19 or newer, including Kirigami, KIO, Purpose, KFileMetaData, KSyntaxHighlighting, KContacts and KCalendarCore.
- exiv2, to keep photo details when a picture is cropped or turned.
- Kirigami Addons 1.0 or newer.
- libseccomp, and a recent Linux kernel for the sandbox around office previews and video editing (Landlock with its newer rules for local sockets). If the kernel is too old, office documents just show their details and a note says why, and videos can't be edited.
- Optional: LibreOffice, for previews of office documents.
- Optional: ffmpeg, for trimming and turning videos.

## Installation

### Arch Linux

There's no AUR package yet. The repository has a PKGBUILD that builds the release:

```sh
git clone https://github.com/strandzen/KwikLook.git
cd KwikLook/packaging/arch
makepkg -si
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
sudo pacman -S --needed cmake extra-cmake-modules qt6-base qt6-declarative qt6-webengine qt6-multimedia qt6-multimedia-ffmpeg \
    kconfig kcoreaddons ki18n kwindowsystem kio kservice purpose kfilemetadata \
    kirigami kirigami-addons syntax-highlighting kcontacts kcalendarcore exiv2 libseccomp
```

Other distributions: the development packages of CMake, Extra CMake Modules, Qt 6 (Base, Declarative, PDF, Multimedia) and the KDE Frameworks 6 listed above (Config, CoreAddons, I18n, WindowSystem, KIO, Service, Purpose, FileMetaData, Kirigami, SyntaxHighlighting, Contacts, CalendarCore), plus Kirigami Addons, exiv2 and libseccomp. Package names differ between distributions.

</details>

After installing, close every Dolphin window (or log out and back in) so Dolphin picks up KwikLook.

## Usage

- In Dolphin, select a file and press Space. The preview opens; Dolphin's selection follows it as you go through the folder.
- Drag the preview's bar to move the window, drag an edge or corner to resize it.
- (i) shows the file's details; “Open with…” opens the file in another application and closes the preview.
- The crop button (pictures) and ✂ (videos, in the controls below) start editing; “Save Copy” writes a new file next to the original.

### Keys

| Key | What it does |
|---|---|
| Space (in Dolphin) | Opens the preview of the selected file (the key can be changed in the settings) |
| Space or Esc | Closes the preview (Esc first closes the search, the index sheet or full screen) |
| Enter | Opens the file in its usual application and closes the preview |
| ← and → | Previous and next file |
| Ctrl+Enter | Index sheet: all files as thumbnails |
| Ctrl+F, F3 and Shift+F3 | Find in text and PDFs, next and previous match |
| Ctrl+wheel, Ctrl++ and Ctrl+- | Zoom in and out (pictures also with a touchpad pinch) |
| Ctrl+0 | Back to the normal size |
| Double-click a picture | Real size, and back to fitted |
| Media keys, or a click on a video | Play and pause |
| F, or double-click a video | Full screen, and back |

Space keeps working as usual in Dolphin's location bar, while renaming, in type-ahead search and in selection mode, and on folders.

### From the command line

| Command | What it does |
|---|---|
| `kwiklook <files>` | Shows a preview of the files; ← and → go between them, Space or Esc closes it |
| `kwiklook --settings` | Opens the settings window |
| `kwiklook --quit` | Stops KwikLook running in the background |

## Settings

Open “KwikLook Settings” from the app launcher (or run `kwiklook --settings`):

- **Corners:** like your Breeze window decoration, or a radius of your choice.
- **Bar:** which buttons the preview's bar shows and in what order; drag them in, along or out.
- **Open key:** the key that opens the preview in Dolphin: Space, Shift+Space or Ctrl+Space.
- **Close when you switch to another window:** on by default.
- **Start in the background at login:** off by default. KwikLook starts with Dolphin anyway; this makes the very first preview after login open faster.
- **Start playing when a video or song opens:** on by default.
- **Start without sound:** off by default.
- **Preview office documents:** on by default, when LibreOffice is installed.

## Security

A preview reads the file, so a file made to attack a bug in a picture, PDF or other file reader could do harm when you preview it. What KwikLook does about that:

- **Not sandboxed:** pictures, PDFs, text and file details are read inside KwikLook itself, with the same rights as you; thumbnails come from KDE's thumbnail service, also unsandboxed. Dolphin already reads the same files the same way when it shows thumbnails or indexes files for search, whether you preview them or not. The protection here is keeping your system up to date.
- **Sandboxed:** external programs KwikLook runs on a file are locked in a sandbox: no network, no connection to other programs, read access only to that one file (plus the system's programs and fonts), and they can only write to a scratch folder in KwikLook's cache. In `/tmp` they may only create and remove entries, because LibreOffice puts its own connection there. This applies to office documents, which LibreOffice turns into PDFs, to videos you trim or turn, which ffmpeg reads and writes, and anything similar added later. The PDF it makes is then shown like any PDF, unsandboxed.
- **No sandbox, no conversion:** on systems whose kernel can't lock these programs in this way, office documents aren't converted (the preview says so) and videos can't be edited.

## Troubleshooting

**Space does nothing in Dolphin.** Close all Dolphin windows and open Dolphin again: it only picks up KwikLook when it starts. Also check the open key in the settings.

**Space previews only one file and ← → don't go through the folder.** KwikLook reads Dolphin's file list through Dolphin's internals, which a Dolphin update can change. Please report it with your Dolphin version.

**Ctrl+Space no longer toggles the selection in Dolphin.** With Ctrl+Space as the open key, KwikLook takes it over. Choose another open key to get Dolphin's back.

**The preview's corners and shadow don't match my other windows.** The preview draws its own frame, made to look like the Breeze window decoration (it follows Breeze's settings). With another window decoration (Oxygen, Klassy, an Aurorae theme) it still looks like Breeze. Buttons, colours, fonts and icons always follow your Application Style and colour scheme.

**Office documents show only their icon and a note.** “LibreOffice couldn't convert this document”: the file may be damaged or password-protected. “This system can't run LibreOffice shut away…”: your kernel is too old for the sandbox, so KwikLook doesn't run LibreOffice at all. No note: check that LibreOffice is installed and “Preview office documents” is on in the settings.

**I want to turn off the Dolphin part for a moment.** Start Dolphin with `KWIKLOOK_DISABLE_DOLPHIN_HOOK=1 dolphin`.

Bugs and ideas are welcome in the [issue tracker](https://github.com/strandzen/KwikLook/issues).

## Uninstall

Turn off “Start in the background at login” first if you turned it on, then run `kwiklook --quit` and remove the package (`sudo pacman -R kwiklook`), or, if you installed with CMake, run `sudo xargs rm < build/release/install_manifest.txt` in the source folder.

Left behind in your home folder: the settings in `~/.config/kwiklookrc`, the window state in `~/.local/state/kwiklookstaterc` and office previews in `~/.cache/kwiklook` (at most 200 MB). Fonts you installed from the preview stay in `~/.local/share/fonts`. Delete them if you like.

## Translations

KwikLook is in English only for now. Translations are welcome: [po/Messages.sh](po/Messages.sh) extracts the strings to translate.

## Credits

The Dolphin integration (pressing Space in Dolphin, following its file order and selection) is based on [KDEGlance](https://github.com/AlexGraveler/KDEGlance) by AlexGraveler and the KDEGlance contributors, used under the GPL-3.0-or-later. Thank you!

## License

GPL-3.0-or-later, see [LICENSE](LICENSE).
