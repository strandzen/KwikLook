# Changelog

All notable changes to KwikLook. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses [semantic versioning](https://semver.org/). Before 1.0, settings may still change between versions.

## [Unreleased]

### Added
- Enter opens the previewed file in its usual application and closes the preview.
- With several files, the bar shows the position next to the file name, like “2 / 5”.
- Videos and audio play in the preview, with play/pause, a seek bar and a mute button; audio shows its cover, title and artist. Media keys play and pause. Two settings: start playing when a file opens (on) and start without sound (off).
- Office documents (Word, Excel, PowerPoint and OpenDocument) can be previewed when LibreOffice is installed. LibreOffice turns them into a PDF in the background, shut away from your files and the network; the preview shows the file's icon and “Preparing preview…” meanwhile. Previews are kept in a cache (at most 200 MB), so a document opens at once the next time. A setting turns this off. Macros and updates of linked files are always off during the conversion.
- Animated pictures (GIF, WebP and other formats with several frames) play in the preview.
- Zoom pictures, text and PDFs with Ctrl+wheel, Ctrl+plus and Ctrl+minus (Ctrl+0 goes back). Pictures zoom towards the mouse pointer, can be pinched on a touchpad and moved by dragging or scrolling, and a double-click switches between fitted and real size. Text keeps its size for the next files until the preview closes; pictures and PDFs start fitted again.

### Fixed
- Long text files (thousands of lines, like zsh's `.zcompdump`) no longer freeze the preview.
- An office document no longer shows “Preparing preview…” for a whole minute when LibreOffice can't be started.
- Scratch folders left behind when KwikLook was stopped during an office preview are now cleaned up.
- KwikLook could crash when it was stopped while a PDF preview was still drawing its pages.
- PDF pages no longer scroll over the preview's bar, and all pages are centred the same way.
- Scripts and other text files without a file name extension now show syntax colours too.

## [0.1.0] - 2026-11-19

The first release.

### Added
- Preview files from the command line with `kwiklook <files>`. KwikLook keeps running in the background after the first preview, so later ones open right away.
- The preview window looks like your other windows: corners, outline and shadow follow your Breeze window decoration settings and colour scheme, also when you change them.
- “Open with…” and Share buttons in the preview's bar.
- Press Space in Dolphin to preview the selected file; ← and → go through the folder in Dolphin's order and Dolphin's selection follows along.
- Pictures fill the preview window (small ones stay at their real size).
- Text and code files show with syntax colours and line numbers (long files show their beginning); PDFs show all their pages.
- An (i) button opens a panel with the file's details: name, type, size, dates, picture size and camera details, owner and permissions, with “Show in Dolphin” and “Copy Path”.
- Every file gets a preview: a large thumbnail (as in Dolphin) or the file type's icon, with its type, size and when it was changed.
- “KwikLook Settings” in the app launcher: choose the corner radius, the key that opens the preview in Dolphin (Space, Shift+Space or Ctrl+Space), whether the preview closes when you switch to another window (on by default), and whether KwikLook starts in the background at login.

<!-- Sections, in this order, only those that have entries:
### Added       new features
### Changed     changes to existing behaviour
### Deprecated  features that will be removed
### Removed     features that are gone
### Fixed       bug fixes
### Security    vulnerabilities

Write for users: what changed for them, in plain language, one entry per change.
On release: rename [Unreleased] to [x.y.z] - YYYY-MM-DD and start a new empty [Unreleased]. -->
