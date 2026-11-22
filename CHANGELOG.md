# Changelog

All notable changes to KwikLook. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses [semantic versioning](https://semver.org/). Before 1.0, settings may still change between versions.

## [Unreleased]

### Added
- Videos can be trimmed and rotated (✂ in the video controls, when ffmpeg is installed): choose the part to keep and quarter turns, then “Save Copy” writes a new file next to the original, which is never changed. A start on a keyframe (small marks under the slider; a handle let go close to one moves onto it) is cut at once without losing quality; a start anywhere else re-encodes the video to begin exactly there, with a progress bar.
- Index sheet: with several files, Ctrl+Enter (or the bar button next to →) shows them all as a grid of thumbnails; arrow keys and Enter, or a click, pick one.
- PDFs with several pages have a page overview on the left (button in the bar, next to →): small pages with the current one marked; a click goes there. Whether it is open is remembered.
- Contacts (.vcf) show as cards with photo, phone numbers, mail, addresses and birthday; calendar files (.ics) show their events with time, place and organizer. The bar button shows the file's text instead.
- CSV and TSV files show as a table (the first row as column headers), or as text with the bar button. Commas, semicolons and tabs are recognised.
- Fonts can be installed from the preview (“Install Font”, for your user); fonts you already have say “Installed”.
- More files show formatted, switchable to their source with the same bar button as Markdown: SVG pictures (or their text), JSON (indented), HTML pages (simple layouts; nothing is loaded from the internet) and .desktop launchers (icon, name, command). Each kind of file remembers whether you last looked at it formatted or as source.
- With several files selected in Dolphin, the preview goes through just those (← →), and Dolphin keeps your selection.
- “Open With…” offers Spectacle for pictures when it is installed, to draw on them, crop them or add text in Spectacle's editor.
- Videos and audio: a volume slider (remembered), a repeat button (stays on until the preview closes) and, for videos, full screen (button, double-click or F). In full screen only the video shows; the controls appear when you move the mouse. Esc leaves full screen first.
- Markdown files show formatted; a button in the bar (only for Markdown files) switches to the source text, and the choice is remembered. Links open in your web browser. Only pictures stored on your computer are shown: the preview never loads anything from the internet.
- Fonts show their name, the alphabet, digits and punctuation, and a sample sentence, all in the font itself. The sentence starts at 36 px; − and + (or Ctrl+wheel) change its size. Type your own text to see it instead; text and size stay for the next fonts until the preview closes.
- Find in text files and PDFs (also converted office documents) with Ctrl+F: matches are marked, “3 of 12” shows where you are, Enter and Shift+Enter (or F3 and Shift+F3) go to the next and previous one, Esc closes the search. The search stays while you go through more files.
- Choose the buttons in the preview's bar in the settings: drag them into the bar, along it or out of it, like the window decoration buttons in System Settings. New buttons to choose from: Open (in the usual application), Show in Dolphin and Copy Path, plus a flexible space. The button that switches between formatted text and source can be moved too. “Reset Bar to Defaults” brings back the usual bar.

### Fixed
- Going from one PDF to the next no longer logs a “Cannot open” warning.

## [0.2.0] - 2026-11-19

Office documents, video and audio, and zoom.

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
