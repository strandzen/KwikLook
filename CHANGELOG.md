# Changelog

All notable changes to KwikLook. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses [semantic versioning](https://semver.org/). Before 1.0, settings may still change between versions.

## [Unreleased]

### Added

- “Preview” in the right-click menu of files, in Dolphin (just below “Open With”) and on the desktop.
- When the file you're previewing is moved, renamed or deleted, the preview says so (“This file was moved or deleted.”) instead of quietly showing a file that's no longer there.

### Changed

- KwikLook is now licensed under the GPL-2.0-or-later, like Dolphin. The Dolphin integration stays under the GPL-3.0-or-later. Earlier versions keep their GPL-3.0-or-later license.

### Fixed

- Space in Dolphin no longer stops working when a preview couldn't be opened (for example when the viewer failed to start); the next Space tries again.
- If a Dolphin update changes what KwikLook relies on, Space now falls back to previewing the selected file on its own instead of risking a Dolphin crash. If Dolphin does crash in KwikLook's code, KwikLook stops using those parts of Dolphin until Dolphin or KwikLook is updated.

## [0.8.2] - 2026-11-28

Less memory while KwikLook waits in the background.

### Changed

- After 15 minutes without a preview the background viewer starts afresh, which takes it back to about 35 MB. The first preview after such a break opens a moment later (about a tenth of a second). Settings → Behaviour → “Free memory when KwikLook isn't used for a while” turns this off.

### Fixed

- The background viewer gives back more memory a few seconds after a preview closes: about 95 MB stays in use instead of 130 MB after looking at pictures, documents and a video.

## [0.8.1] - 2026-11-27

Less memory after looking at pictures, and KwikLook in the AUR.

### Added

- KwikLook is in the AUR (`kwiklook`): Arch users can install and update it with an AUR helper such as `yay -S kwiklook`.

### Fixed

- The background viewer gives back much more memory after you look at pictures: about 70 MB instead of 110 MB after a mixed session.

## [0.8.0] - 2026-11-27

Song covers and AppImage icons, a lighter background viewer, and crash reports.

### Added

- If KwikLook crashes, KDE's crash dialog (when installed) offers a report with the details needed to fix it, pointing to the project's issue page. The README says how to report a crash, and GitHub's issue form asks for what's needed.

### Fixed

- The background viewer gives back the memory of the last preview a few seconds after it closes (a large photo kept about 60 MB in use until the next preview).
- Song covers and AppImage icons now show: the preview uses the same thumbnail makers as Dolphin (its “Previews” settings), not only the ones for pictures.
- The background viewer could quit by itself shortly after a preview closed (when a thumbnail was still being made), so the next preview opened slowly.

## [0.7.0] - 2026-11-26

Books, and the next file ready before you get there.

### Added

- EPUB books: their cover, title and author, then the chapters as text you can scroll, search and zoom, with the book's table of contents in the sidebar. Books are opened shut away from your files and the network, and nothing in them runs or is fetched.
- Going to the next or previous file is instant for most files: the files next to the one you're looking at are read, and pictures decoded, while you look at it. A camera RAW photo went from about two thirds of a second to at once.
- Camera RAW photos (DNG, CR2, NEF and others) show as pictures, with KDE's image plugins installed.
- Share offers LocalSend, when it's installed.

### Fixed

- The PDF sidebar keeps one width on both tabs, and the “Pages” and “Contents” tabs fill it evenly; “Contents” was cut off on the narrower Pages tab.
- Some EPUB books made the preview crash while it read their details.
- Going from a long text or book to another file could take seconds: the old text was coloured for code just before it went away.
- Web pages and books with a table: after scrolling down past it and back up, the beginning (text and pictures) stayed blank.
- Markdown files with a picture, line break or rule written the HTML way (`<img src="…">`, `<br>`, `<hr>`), as many READMEs start with a centred logo, showed nothing after it, and their contents listed “(Untitled)”.

## [0.6.0] - 2026-11-26

The preview opens where you want it, and pictures give it their shape.

### Added

- AppImages show an “Install with AppImage Manager” button when [AppImage Manager](https://github.com/strandzen/AppImage-Manager) is installed. It opens AppImage Manager's install window for the file and closes the preview.
- Pictures give the preview window their shape, centred: as large as the picture, or up to most of the screen. A size you set by dragging the window's edge stays until the preview closes.
- Pictures fill the preview edge to edge, without a margin. Vector pictures (SVG) are enlarged to fill the window and stay sharp; other small pictures still show at their real size.
- A new setting chooses where the preview opens: centred on the screen, centred on the Dolphin window, at the mouse pointer, or where the window manager puts it. If another window-position script moves the preview back, the README's Troubleshooting section says what to do.

### Changed

- Where you are among the files (“7 / 30”) now shows in the preview's bottom-right corner for a moment when it opens and on each switch, instead of in the bar; it shares that corner box with the zoom level. The bar keeps the file name only (hover it to see the position).
- KContacts and KCalendarCore are no longer required. Without them, contacts (.vcf) and calendar files (.ics) show as text instead of cards; install them to get the cards back.
- ↑ and ↓ go to the previous and next file, like ← and →. In text and PDFs, scroll with Page Up / Page Down or the wheel.

### Fixed

- PDF pages are drawn once their size has settled, instead of at every step while you resize the window or zoom, and the small pages in the sidebar no longer redraw over and over at some window sizes. Both could make the preview crash now and then.
- The preview no longer crashes now and then when you go from a code or text file to another file.
- With the screen scaled (for example to 125 %), pictures shown at their real size were drawn too small, with an empty border around them. They now fill the preview.

## [0.5.0] - 2026-11-24

A visual review, and editing handed to other apps.

### Removed

- Cropping and turning pictures, and trimming and turning videos, inside the preview. Use “Open with…” instead: it offers Spectacle's editor for pictures and your video editor for videos. KwikLook no longer needs exiv2, or ffmpeg for editing.

### Changed

- The contents sidebar looks like a panel: a heading, lines that show which heading belongs under which, and arrows to fold sections in and out. Long documents start with only the top level open.
- Going to the next or previous file no longer flashes an empty or icon page: the current file stays until the next one is ready, then fades over.
- Busy indicators and “Preparing preview…” appear only when something takes a moment, so quick files never flicker.
- Code blocks in Markdown and HTML have a shaded box, and quotes a bar on the left.
- Songs with a cover show it large, with a blurred copy of it filling the background.
- Videos that don't start by themselves show their first picture with a large play button, instead of black.
- PDFs show the page number (“3 / 12”) while you scroll.
- The index sheet marks the chosen file and the one under the pointer the way Dolphin does.
- Archive lists have alternating row colours, with sizes and dates dimmed.
- The bar shows the file's type icon before its name; hovering the name shows its full name, type, size and date, and the picture size or page count.

## [0.4.0] - 2026-11-24

Contents in the sidebar, archives listed, and a quieter background viewer.

### Added

- Contents in the sidebar: a PDF's bookmarks (also the headings of Word and Writer documents), and the headings of Markdown and HTML files, in the formatted and the source view. The section you're reading is marked; a click goes there. PDFs with both show “Pages” and “Contents” tabs, and the sidebar remembers the last one.
- Archives (zip, 7z, tar, RAR, ISO, comic books and more) show what's inside: folders that open and close, sizes, dates and a summary line, with a lock for password-protected ones. Nothing is unpacked; the archive is read by a helper shut away from your other files. Very large archives show their first 10,000 entries.

### Fixed

- The background viewer no longer uses the processor after the preview closes: an animated picture stops playing, and a PDF stops waking it ten times a second.

## [0.3.0] - 2026-11-23

Find, formatted views, fonts, tables and cards, an index sheet, a customisable bar, and cropping pictures and trimming videos.

### Added
- Pictures can be cropped and rotated (the crop button in the bar): drag the frame or its corners, pick a ratio (free, original, square, 4:3, 16:9 and their upright versions), turn left or right, then “Save Copy” next to the original or “Replace” it (the original goes to the Trash). Date taken, camera and other photo details are kept.
- Videos can be trimmed and rotated (✂ in the video controls, when ffmpeg is installed): choose the part to keep and quarter turns, then “Save Copy” writes a new file next to the original, which is never changed. While editing, the video stays still on the frame where you put a handle and plays only when you press Play. A start on a keyframe (small marks under the slider; a handle let go close to one moves onto it) is cut at once without losing quality; a start anywhere else re-encodes the video to begin exactly there, with a progress bar.
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
- Quitting right after zooming into a PDF could crash the viewer.
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
