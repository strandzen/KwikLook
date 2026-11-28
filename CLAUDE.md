# CLAUDE.md

This file is the authoritative reference for working on KwikLook. Keep it current: when
something here turns out wrong or a new pitfall is found, update it in the same change.

## Project Overview

**KwikLook** shows a quick preview of the file selected in Dolphin when you press Space, for KDE Plasma 6 users (like Quick Look on macOS). Wayland only: X11 sessions aren't a target (decided 2026-11-26); don't spend effort on X11 paths.

**Goal:** look at a file without opening an application: instant, keyboard-driven, looking like a native Breeze window. Done means the common types (pictures, text, PDF, media, office files) preview well and the Dolphin hook survives Dolphin updates.
**Philosophy:** Simplicity and native Plasma behaviour. Every feature must justify its existence.
**Stack:** C++20, Qt 6, KDE Frameworks 6, Kirigami (+ Kirigami Addons if used), QML, CMake with
ECM. Project version `0.8.2` (single source: `project(... VERSION ...)` in `CMakeLists.txt`).
**App ID:** `io.github.strandzen.KwikLook` (desktop file, metainfo, D-Bus names). Never `org.kde.*`.
**Licence:** GPL-3.0-or-later.
**Credit:** the Dolphin hook is based on KDEGlance (`comp/KDEGlance`, GPL-3.0-or-later,
https://github.com/AlexGraveler/KDEGlance). Ported files keep KDEGlance's
`SPDX-FileCopyrightText` line next to ours; the README's Credits section names it.

Planned work lives in [Todo.md](Todo.md); user-visible changes in [CHANGELOG.md](CHANGELOG.md).
Reference projects to compare against live in [comp/](comp/README.md).

---

## Working rules (for Claude)

- **Ask, don't guess.** The user prefers too many questions over too few. When a choice is the
  user's (UX, scope, wording, what to remove, defaults), ask before acting. When asking, give a
  recommendation and say why.
- **Verify APIs, don't guess them.** Check Qt/KF6/Kirigami names against the installed headers and
  QML modules (`/usr/include/qt6`, `/usr/include/KF6`, `/usr/lib/qt6/qml`) before using them.
  Several pitfalls below came from plausible-looking APIs that don't exist or behave differently.
- **Measure before optimising, test before claiming.** Time the slow path, reproduce the bug,
  then fix. Say what was verified and how, and what wasn't.
- **Never touch the user's real setup while testing.** See Safety below and Verification.
- **Commits:** Conventional Commits (`feat:`, `fix:`, `docs:`, `chore:`, `refactor:`, `test:`).
  Commit and push only when the user asks. Git identity: `strandzen <strandzen@gmail.com>`.
- **Todo.md:** open tasks are grouped by priority (High / Medium / Low), not feature area. When
  adding a task, ask which priority (or section) it belongs in. When a task is done, move it to
  **Completed** (grouped by feature area there) instead of marking it done in place.
- **CHANGELOG.md:** add a line under `## [Unreleased]` for every user-visible change, written for
  users (what changed for them), not for developers.
- **comp/:** reference projects, read-only and not committed. See [comp/README.md](comp/README.md).
- **Writing style:** in `README.md`, no em dashes (—); use regular dashes (-). User-facing text
  (UI strings, README, notifications) in plain language: say what happens, not how it's built.
  UI strings use typographic quotes (“Applications”).

### Safety

This is the user's daily machine (KDE Plasma on Wayland). Nothing here may cost them their
session, their files or their accounts.

- **Session-critical processes:** never restart, kill or replace KWin
  (`plasma-kwin_wayland.service`, `kwin_wayland --replace`): it can hard-lock the session.
  Code that runs inside KWin (effects, scripts) only reloads on log out / log in; say so and
  let the user do it. Restarting plasmashell (`systemctl --user restart plasma-plasmashell`)
  is safer but still closes open popups and widgets: ask first.
- **Root and system files:** no `sudo` (installs, `/usr`, `/etc`, system services) unless the
  user asks for that step. Never write to `/etc` or system config; ship a snippet or file and let
  the user enable it. Installing goes through the package manager or `cmake --install`, when asked.
- **The user's own files and settings:** don't edit dotfiles or other apps' config. Tests and
  experiments use QStandardPaths test mode, a throwaway `HOME`, a private D-Bus session and a
  nested compositor. Before deleting or overwriting anything outside the build directory, look at
  it; user files go to the Trash. Never use a bare file name with APIs that resolve it into
  standard folders (`KDesktopFile`, `KSharedConfig`): use absolute scratch paths.
- **Screen and privacy:** screenshots only of the nested session (private bus); if a capture turns
  out to show the real desktop, delete it unseen and say so.
- **Running untrusted code:** don't execute downloaded binaries, AppImages or install scripts
  unless the user asked; test with stand-ins (a fake script, a local file) instead.
- **Outward-facing actions:** pushing, tagging, releases, package uploads (AUR), issue or PR
  comments and anything else others can see happen only when the user asks, each time.
- **Git:** no force-push, history rewrite, `reset --hard`, branch or tag deletion unless asked.
  Rewriting published history breaks tarball checksums and other people's clones.
- **Secrets:** never put tokens, passwords or keys in the repository, logs or commit messages. If
  a step needs one (an SSH key passphrase, a login), the user runs that step.
- **Network:** don't hammer external services in loops (APIs have rate limits); cache what's
  fetched while testing.
- **Background processes:** stop what you started (test daemons, nested sessions, watchers);
  never kill processes you didn't start.

### Behavioral principles (from Karpathy's observations on LLM coding)

1. **Think before coding.** State assumptions. If there are several interpretations, surface them;
   if something is unclear, stop and ask. Push back when a simpler approach exists.
2. **Simplicity first.** Minimum code that solves the problem. No speculative features,
   abstractions for single use, or configurability nobody asked for.
3. **Surgical changes.** Touch only what the request needs. Match the existing style. Mention
   unrelated dead code instead of deleting it; remove only what your change made unused.
4. **Goal-driven execution.** Turn tasks into verifiable goals; for multi-step work, state a short
   plan with how each step is checked. Loop until verified.

---

## Build Commands

```bash
# Configure + build (dev: Debug, exports compile_commands.json)
cmake --preset dev
cmake --build --preset dev

# Tests
ctest --test-dir build/dev --output-on-failure

# Install
sudo cmake --install build/dev

# Release build
cmake --preset release && cmake --build --preset release

# CI: .github/workflows/ci.yml builds (release + dev) and runs ctest in an Arch container
# on every push and weekly (Mondays: tst_dolphinsymbols against the newest Dolphin, Dolphin
# installed there and KWIKLOOK_REQUIRE_DOLPHIN=1); crash reports: KCrash in main.cpp, bug form in .github/ISSUE_TEMPLATE/

# Stress tests (stress/README.md): sanitizer build + fast switching in a nested session,
# and libFuzzer targets for the parsers (clang)
cmake --preset asan && cmake --build --preset asan && stress/run-nested.sh switching.sh 30
cmake --preset fuzz && cmake --build --preset fuzz && stress/fuzz/run.sh 600

# Clean rebuild
rm -rf build/ && cmake --preset dev && cmake --build --preset dev

# Lint / syntax-check QML (qmllint knows the module when given the build dir)
qmllint -I build/dev/src qml/<File>.qml
QT_QPA_PLATFORM=offscreen QML_IMPORT_PATH=build/dev/src timeout 5 qml6 qml/<File>.qml; echo $?
#   124 = ran until the timeout (fine), 2 = real error. A Page/Item root reports "Did not load
#   any objects" (2) without being wrong. Syntax only: still test live.
```

**When releasing:** bump `VERSION` in `CMakeLists.txt` (generated `version.h` and the About page
read it), the version in this file and the README, move CHANGELOG's `[Unreleased]` entries under
the new version with the date, add a `<release>` entry to the metainfo
(`appstreamcli validate --pedantic data/io.github.strandzen.KwikLook.metainfo.xml`), then tag and publish.
Packaging: `packaging/arch/PKGBUILD` and its `.SRCINFO` (builds the GitHub tag tarball; for the AUR package `kwiklook`). On release: bump `pkgver`, reset `pkgrel=1`; after pushing the tag `updpkgsums`; test with `makepkg -f` in a scratch copy, never in `packaging/arch/` (its output once ended up in git; `.gitignore` keeps only the two files now); `makepkg --printsrcinfo > .SRCINFO`; commit both. The AUR: a clone of `ssh://aur@aur.archlinux.org/kwiklook.git` outside this repository gets the same two files, a commit and a push: the user's AUR account and SSH key, so the user pushes (or says so each time). A PKGBUILD-only fix (a dependency) bumps `pkgrel` instead.

---

## Repository Layout

| Path | Contents |
|------|----------|
| `src/core/` | Pure C++ logic, no Qt Quick |
| `src/gui/` | Qt Quick backend classes (models, windows) |
| `src/dolphin/` | The Dolphin hook plugin (runs inside Dolphin; Qt Widgets, no QML) |
| `src/sandbox/` | `kwiklook-sandbox`: runs converters locked down (Landlock + seccomp; no Qt) |
| `src/cards/` | `kwiklook-cards.so`: contacts and events as cards; the only code linked against KContacts / KCalendarCore (both optional) |
| `src/archive/` | `kwiklook-archive`: lists an archive, or hands over a book's files (`--files`), with libarchive, run in `kwiklook-sandbox` (no Qt) |
| `qml/` | All QML files (top level, wired in with `qt_add_qml_module`) |
| `stress/` | Stress tests: `switching.sh` (run by `run-nested.sh` in a nested session), `fuzz/` (libFuzzer targets, `run.sh`); reports in `stress/reports/` (not in git) |
| `tests/` | Unit tests (`tst_*.cpp`), QML smoke tests; `tests/dolphin/` the hook against KDEGlance's mock Dolphin; `run-with-bus.sh` runs a test on a private bus with scratch XDG folders (state too; extra arguments = test functions: the hook test's `-garbage` / `-crashed` runs) |
| `data/` | Desktop file, metainfo, icons, notifyrc, D-Bus service files; `kwin/` the KWin script |
| `po/` | `Messages.sh` and the `.pot`; reviewed translations in `po/<lang>/` |
| `packaging/arch/` | PKGBUILD |
| `comp/` | Reference projects (read-only, gitignored) |

## Architecture

**Targets:** `kwiklook` (the app), `kwiklook_qml` (static QML module `kwiklook`: C++ types + `qml/`),
`kwiklook_dolphinhook` (plugin Dolphin loads into its own process, `kf6/overlayicon`), `kwiklook-cards` (optional library opened at runtime, `src/cards/`), helpers `kwiklook-sandbox` and `kwiklook-archive`, tests in `tests/`.

**One background process.** `kwiklook` owns `io.github.strandzen.KwikLook` on the session bus and
keeps running when its window closes, so the next preview opens instantly. After 15 minutes with no window shown (`Viewer::restartRequested()`; not while the settings window is open; setting FreeMemoryWhenIdle, on by default) main.cpp ends the event loop as on quit, gives up the bus name and `execv`s itself with `--service`: same process, fresh memory (96 → 34 MB measured), the first preview after it ~125 ms instead of ~25 ms.

| Command | What happens |
|---|---|
| `kwiklook <files>` | Viewer running: hands the files over (`Open`) and exits. Otherwise becomes the viewer |
| `kwiklook --service` | Becomes the viewer without showing anything; what D-Bus activation runs |
| `kwiklook --quit` | Asks a running viewer to quit |
| `kwiklook --settings` | Viewer running: asks it to show the settings window (`ShowSettings`). Otherwise becomes the viewer and shows it |

**D-Bus interface** `io.github.strandzen.KwikLook.Viewer` at `/Viewer`, described in
`data/io.github.strandzen.KwikLook.Viewer.xml` (hand-written; keep it in step with the
`Q_SCRIPTABLE` members of `Viewer`). `Open(as urls, i index, s sessionId, s activationToken)`,
`Close()`, `Next()`, `Previous()`, `Quit()`, `ShowSettings(s activationToken)`, `Placement(s caption) → s` (for the KWin script, JSON); signals `CurrentChanged(s sessionId, s url, i index)`,
`Closed(s sessionId)`. Session IDs are the caller's (one per Dolphin window); a new session's
`Open` sends `Closed` to the previous one.

| Class | Files | Role |
|---|---|---|
| `Viewer` | `src/gui/viewer.*` | File list, current file and its details (`view`, `mimeType`, `details`, `imageSize`, `infoSections` for the panel, read off the main thread with a generation counter), `infoOpen`, `pagesOpen` (the sidebar) and `sidebarTab` (kept in `kwiklookstaterc`), the preview window; the D-Bus object; QML singleton `Viewer`. A switch while the window is shown happens in one step (`showCurrent()`): `m_index` moves at once (Next / Previous, D-Bus `CurrentChanged`), the shown file (`currentUrl`, `currentIndex`, details) stays until the next one's details are read (`switching` meanwhile), then `aboutToSwitch()`, one rendered frame (`frameSwapped`, or 100 ms), and all change together: `currentChanged` before `fileInfoChanged`. Hidden window: at once, as before. Read ahead (`readAhead()`, after each file is shown): the details of the next and previous files (local only) are read off the main thread into `m_ahead` (pruned to the neighbours, cleared on close); 6 s after the window hides (Main.qml drops the view at 5 s) `releaseMemory()` gives memory back: QML garbage collection, `trimComponentCache()`, `malloc_trim(0)`; 15 min after it hides (other windows hidden too) `restartRequested()` (`setIdleRestartDelay()` for tests); `readFileInfo()` takes them instead of reading again if size and date are unchanged (`unchangedOnDisk()`), through a ready future (same path as a read); `picturesAhead` [next, previous] ({url, width, height} for still non-SVG pictures, else {}) feeds Main.qml's preload slots |
| `WindowFrame` | `src/gui/windowframe.*` | QML element: compositor shadow (`KWindowShadow`) for the frameless window, corner radius (Breeze's or the setting's) and outline flag; watches breezerc (`KDirWatch`), recreates the shadow on each new surface |
| `KwikLookSettings` | `src/gui/kwiklooksettings.kcfg(c)` | KConfig XT, generated; `kwiklookrc` [General]: MatchBreezeCorners, CornerRadius, OpenKey, CloseOnFocusLoss, FreeMemoryWhenIdle, AutoplayMedia, StartMuted, OfficePreviews, WindowPosition (Screen, Dolphin, Pointer, KWin = no script), BarLeft, BarRight (item names from `qml/BarItems.qml`). QML singleton. main.cpp sets the Notify write flag on every item (no kcfg option for it), so the Dolphin hook's `KConfigWatcher` hears about saves |
| `OpenKey` | `src/core/openkey.h` | The open key choices (Space, Shift+Space, Ctrl+Space) and parsing; shared by the viewer and the Dolphin hook |
| `Autostart` | `src/gui/autostart.*` | QML element: “Start at login” = `~/.config/autostart/io.github.strandzen.KwikLook.desktop` (`kwiklook --service`); the file is the setting |
| `BreezeStyle` | `src/core/breezestyle.*` | Reads breezerc `[Common]` (absolute path only); the fitted shadow layers per size |
| `FileMenus` | `src/gui/filemenus.*` | QML element: Enter's default application (`KIO::OpenUrlJob`, executables not run), “Open with…” (`KFileItemActions::associatedApplications`, plus Spectacle for pictures: it declares no types, so `spectacle --edit-existing` via `CommandLauncherJob`; + KIO's dialog; `openWithMenu()` builds it for tests) and Share (`Purpose::Menu`, `shareMenu()` for tests; after its `reload()` LocalSend is added when its desktop entry exists (`localsend`, Flathub's `org.localsend.localsend_app`): it declares no types, so Purpose doesn't list it; started with the file on its command line, the preview stays) as QMenus below a button; “Show in Dolphin” (`KIO::highlightInFileManager` with a token), “Copy Path”; `openWith(desktopEntry)` / `applicationIcon(desktopEntry)` (empty = not installed) for one named application |
| `renderShadow()` | `src/core/shadowimage.*` | Shadow nine-patch image + tile rects from layers, radius, colour |
| `FileInfo`, `viewFor()` | `src/core/fileinfo.*` | A file's type (by content), icon, size, dates, owner, permissions, picture size, KFileMetaData details, the start of text files (`readTextStart()`, 256 KB) (`read()` may block: Viewer runs it with QtConcurrent); `viewFor()` picks the QML view: PDF → `PdfView`, what QImageReader reads → `ImageView`, types inheriting text/plain → `TextView`, fonts (`isFont()`, read whole into `fontData`, ≤ 64 MB) → `FontView`, else `FallbackView` (EPUB too, until BookReader has read it: `Viewer::startBookPreview()`; KFileMetaData is skipped for EPUB). `formattedKind` (markdown, html, svg, json, desktop, csv, vcard, calendar) marks files with a formatted view; `viewFor(info, formatted)` gives the source as `TextView`, .desktop formatted as `DesktopView` (`desktopEntry` via KDesktopFile, absolute path), CSV / TSV as `CsvView` (`parseDelimited()`: RFC 4180 quotes, separator guessed from the first line), vCard / iCalendar as `CardsView` (`cards` from `kwiklook-cards.so` through `Cards`, at most 200; embedded photos as PNG data URLs, linked ones never fetched; without the library the kind is cleared: plain text, no toggle) |
| `Markdown` | `src/core/markdown.*` | `forQt()`: Markdown as Qt's reader can take it (empty HTML elements closed outside fenced, indented and inline code; see Known Pitfalls) |
| `Headings` | `src/core/headings.*` | Headings of a rendered `QTextDocument` (`headingLevel()`, set by Qt's Markdown and HTML readers) and of the source (Markdown `#` and underlined lines outside code fences, HTML `<h1>`-`<h6>` outside comments, by line); `match()` pairs them by order, else by text |
| `TextBlocks` | `src/core/textblocks.*` | Code blocks and quotes of a rendered `QTextDocument` (`find()`: code runs split where the language changes, a quote run per level), `padCode()` (margins inside the box; idempotent) |
| `Contents`, `PdfContents` | `src/gui/contents.*` | The contents sidebar's entries as plain lists. `Contents` (QML singleton): `fromText(document, kind, rendered, source)` → [{title, level, position}] (source view: rendered titles at their lines' block positions); `blocks(document, padding)` → [{kind, start, end, level}] for TextView's code boxes and quote bars; `fromAnchors(document, bookContents)` → [{title, level, position}] from where the `<a name>`s landed (unfound ones left out). `PdfContents` (QML element): `source` → `entries` [{title, level, page, y}] read with its own `QPdfDocument` + `QPdfBookmarkModel` off the main thread (generation counter); `y` in points from the page's top |
| `LineNumbers` | `src/gui/linenumbers.*` | QML element: line numbers for a TextArea (block positions from its QTextDocument layout, wrapped lines included, visible part only) |
| `OfficeConverter` | `src/gui/officeconverter.*` | Office documents → PDF: `kwiklook-sandbox … soffice --convert-to pdf` (QProcess, fresh profile and scratch folder each time, /tmp as socket dir, 60 s timeout, cancel kills the process group; macros and link updates off in the profile, finalized; stale `work-*` folders removed); cache `~/.cache/kwiklook/office/<sha1 of path, size, date>.pdf`, 200 MB, least recently used pruned. Viewer shows the fallback with “Preparing preview…” (`previewNote`, `previewBusy`), then `PdfView` with `pdfUrl` |
| `KWinScript`, `kwiklook-placement.js` | `src/gui/kwinscript.*`, `data/kwin/kwiklook-placement.js` | KwikLook's KWin script, loaded into the running KWin over D-Bus `/Scripting` (`loadScript(path, "kwiklook-placement")`, then `Scripting.start()`) by main.cpp unless WindowPosition is KWin, following saves (`configChanged`), unloaded on quit; a copy left by a crashed viewer is replaced. Found next to the binary (copied there by CMake), else `<datadir>/kwiklook/`. The script asks `Viewer::Placement(captionNormal)` when a preview window appears (`windowAdded`: places it on the screen / on the last focused Dolphin window / at the pointer, kept in the placement area; placed once more if KWin undoes it within 1 s) and when its title changes (a new file: reshaped around the centre). Pictures: `fitSize()` (real size + `chrome`, scaled down to 80 % of the area, aspect kept, not below the minimum); a non-picture after one gets the size from before back; a hand resize (`interactiveMoveResizeFinished`) or full screen stops it for that window. Viewer: `Placement()` JSON {preview (the caption is the preview's title, not the settings window's), mode (dolphin only with a session ID), picture (logical px, ImageView only), chrome (`windowChrome` from Main.qml), minimum}; while the script is loaded a hidden window waits for the first file's details (≤ 150 ms) before showing; a `Placement` call over D-Bus while the details are still being read gets its reply once they are in (delayed reply, ≤ 1 s), so a slow file still opens in its shape. The undo guard only answers KWin's first placement, never a later one of ours. Pure functions above the `workspace` part: `tst_placement` runs them in a QJSEngine |
| `Sandbox` | `src/gui/sandbox.*` | Finding `kwiklook-sandbox` (build tree, then libexec) and whether this kernel can lock a program in (asked once); exit code 77 = no |
| `kwiklook-sandbox` | `src/sandbox/main.cpp` | Runs a program locked in: Landlock (system folders read-only, `--read`/`--write`/`--socket-dir`, UNIX sockets only to its own, scoped abstract sockets and signals) + seccomp (AF_UNIX only; no io_uring, ptrace, bpf, keyrings); empty environment, no inherited fds, own process group. Exit 77: kernel's Landlock too old (ABI < 6 or no RESOLVE_UNIX): caller refuses |
| `Cards`, `kwiklook-cards.so` | `src/core/cards.*`, `src/cards/cards.cpp` | Contact and event cards in a library of their own, opened once with `QLibrary` (lock: called from worker threads), build tree next to the binary, else `KWIKLOOK_CARDS_LIBRARY` (`<libdir>/kwiklook/`); one C entry point `kwiklook_read_cards_v1` (a new version gets a new name). Built only when KContacts and KCalendarCore are found; `setLibraryPath()` for tests |
| `Epub`, `BookReader` | `src/core/epub.*`, `src/gui/bookreader.*` | EPUB books: `BookReader` runs `kwiklook-archive --files <book> 16 MB 96 MB <suffixes>` in the sandbox (`--read` book and helper; 15 s timeout, process group killed on cancel), then `Epub::build()` off the main thread: `parseHelperOutput()`, container.xml → OPF (title, creator, manifest, spine, cover by `cover-image` or `meta name=cover`, nav or NCX), each spine chapter sanitized with `QXmlStreamReader` (HTML entities resolved without a DTD; kept: headings, paragraphs, emphasis, lists, tables with colspan/rowspan, quotes, pre/code, pictures; dropped whole: script, style, iframe, object, form, media, head; links only http(s)/mailto; `id`s and chapter starts become `<a name="path#id">` written just before the next text, inside its block), pictures as data: URLs (≤ 600 px wide, ≤ 24 MB in all), a cover-only page dropped, text ≤ 1 MB (`truncated`). Viewer sets `text` = the HTML, `formattedKind` "book" (no source view, `showFormatted` always true), `bookContents` [{title, level, anchor}], title and author into `details`; fallback with “Preparing preview…” meanwhile, a note on failure (no sandbox / unreadable) |
| `kwiklook-archive`, `ArchiveListing` | `src/archive/main.cpp`, `src/core/archivelisting.*` | `--files` (books): zip only, files whose names end in the given suffixes, each `f <size> <path>` + bytes + newline, `s` for one over the per-file limit, stops at the total limit or 5 s (`end … ok/limit/time/error`). The listing: the helper prints one tab-separated line per entry (kind, size, date, encrypted, escaped path; at most 10,000), counts on until 5 s, then `end` (entries, unpacked bytes, ok/time/locked/error); sets a UTF-8 locale itself (the sandbox's empty environment drops non-ASCII names otherwise); a single compressed file is one entry (`format_raw`). `ArchiveListing`: `isArchive()` by exact type (office files and EPUB are zips with views of their own), `parse()`, `parts()` (names that leave the archive stay whole) |
| `ArchiveModel` | `src/gui/archivemodel.*` | QML element, tree model for `TreeView`: runs the helper in the sandbox (`--read` the archive and the helper; process group killed on a new source, 15 s timeout), builds folders from paths (implied ones too; hash lookup), folders first, numeric names; `status` (reading, ready, partial, locked, failed, unsupported), `summary`, `encrypted`, `more`, `singleTopFolder()` |
| `OfflineNetworkFactory` | `src/gui/offlinenetwork.*` | The QML engine's network access: only file/qrc/data load, everything else goes to the unknown protocol `kwiklook-offline` and fails at once (nothing sent). Set on the engine in main.cpp and the QML tests |
| `ThumbnailProvider` | `src/gui/thumbnailprovider.*` | `image://thumbnail/<mime>/<url>` from `KIO::PreviewJob` (job on the main thread) with Dolphin's thumbnailers (`dolphinrc` [PreviewSettings] Plugins, read-only; else `PreviewJob::defaultPlugins()`); 1×1 transparent image = no thumbnail |

| QML file | Role |
|---|---|
| `qml/Main.qml` | The preview window (sets `Viewer.windowChrome`: its size minus the picture area at real size; minimum width from `TitleBar.minimumWidth`, so a picture-shaped window keeps the buttons clear of the name): frameless, content clipped to the corner radius (`MultiEffect` mask), Breeze outline, keys (open key, ← → and ↑ ↓, Enter off while a text field has the focus: `typing`); a `Loader` showing `Viewer.view`, active while shown and for 5 s after the window hides (`keepView`, `dropView`: then the view and what it decoded are freed; not at once, so pages still rendering aren't cancelled); `lastView` (a `ShaderEffectSource`, `live: false`) takes a still picture of the view on `Viewer.aboutToSwitch`, stays over the new view until its `ready` (views without one: at once; at most 300 ms) and fades, then drops its source; a `DelayedBusyIndicator` while `Viewer.switching`; two hidden preload `Image`s (`pictureAhead`, next / previous) loading `Viewer.picturesAhead` with ImageView's cache key (address, sourceSize = screen size × the window's scale when larger, autoTransform, PreserveAspectFit), taking a new picture only when not loading (`take()`, `taken` key), nothing while hidden; the `CornerLabel` (position on opening and each switch, zoom level while zooming; lifted over a view's `bottomBarHeight`: find bar, player controls) |
| `qml/PanelHeading.qml`, `qml/DelayedBusyIndicator.qml`, `qml/FloatingLabel.qml`, `qml/CornerLabel.qml` | Shared pieces of the visual rules: a side panel's heading row with its line; a busy indicator shown after 300 ms (`busy`, `shown`); a note floating over a view (`flash()`, fades a second later: page number); the bottom-right corner box (`flashPosition()`, `flashZoom()`: “7 / 30 \| 90%” while both show, each part a second, the box fades after the last; 4 px from the edges, radius nested in the window's: its radius − 4, at least 2) |
| `qml/FileThumbnail.qml` | The current file's KDE thumbnail (or audio cover), else its type icon; used by FallbackView and MediaView |
| `qml/FallbackView.qml` | Any file: thumbnail or type icon, name, one dimmed line (type · size · date). AppImages (`application/vnd.appimage`, `x-iso9660-appimage`): “Install with AppImage Manager” when `io.github.strandzen.AppImageManager` is installed (its own `FileMenus`: `openWith()` runs `appimagemanager <file>`, its install window; the preview closes) |
| `qml/ImageView.qml` | Pictures (`stillImage` with `cache: true`, so the preloaded ones are found): fit the area edge to edge (no margin), never enlarged past real size (real size in the window's scale, `Window.window.devicePixelRatio`) except SVG (`vector`: fills, `sourceSize` follows the size shown so it's drawn sharp; zooming out goes down to real size); decoded at most at screen size; animations paused while the window is hidden; fallback when undecodable |
| `qml/TextView.qml` | Text and code: read-only TextArea, KSyntaxHighlighting (dark/light theme by background), monospace, wrap, line numbers; note when only the start is shown. Markdown: rendered or source by the bar's view toggle (`Viewer.showFormatted`, per `FileInfo::formattedKind`, saved in `kwiklookstaterc` [FormattedView]); HTML the same as rich text, EPUB books (`kind` "book": always rendered as rich text, contents from `Viewer.bookContents` via `Contents.fromAnchors`, else its headings), JSON indented (`formattedText`) with colours; rendered = `TextEdit.MarkdownText` in a centred 40-grid-unit column, highlighter unloaded, links only for http(s)/mailto. Markdown and HTML with headings: the same sidebar (`Viewer.pagesOpen`) with Contents only (`Contents.fromText`), rendered and source; a jump moves the cursor too (the TextArea scrolls back to its cursor otherwise). Rendered: code blocks in a shaded box and quotes with a bar on the left (`Contents.blocks`, rectangles under the text like the find marks); the contents sidebar has a `PanelHeading` |
| `qml/PdfView.qml` | PDFs (and converted office documents, `Viewer.pdfUrl`, set by `updateSource()`, never empty): `PdfPageImage` pages in a ListView inside KDE's ScrollView, fitted to the width (rendered at `renderWidth`, which follows the width once it has settled for 150 ms); the sidebar on the left (12 grid units on both tabs, the two tabs half each; `Viewer.pagesOpen`, bar item “Pages”, shown as “Sidebar”, when the PDF has several pages or bookmarks): “Pages” (thumbnails in a Loader, active only while shown) and “Contents” (`PdfContents`, `ContentsList`; tabs when both, `Viewer.sidebarTab` remembered); the current bookmark from the page at the top and the offset in it (`currentContentsEntry`), `showDestination(page, y)`. `ready` once opened (Main fades then); “3 / 12” in a `FloatingLabel` while scrolling (not within 500 ms of opening); one tab only: a `PanelHeading` in the tabs' place; the page list is `pdfPages` |
| `qml/CardsView.qml` | Contacts and events as cards (`Viewer.cards`): avatar or calendar icon, name / title, organization / when (local time, all-day as dates), then label-value rows |
| `qml/CsvView.qml` | CSV / TSV formatted: `TableView` on `Viewer.table` (`TableModel`: first row = headers, rows numbered), KDE's header views outside the scrolling area, column widths from the first 200 rows (`columnLength()`) |
| `qml/DesktopView.qml` | .desktop files formatted: icon, name, generic name and comment, then command / address / categories / terminal / hidden in a `Kirigami.FormLayout` |
| `qml/FontView.qml` | Fonts: name in the font, Install Font or “Installed” (`Viewer.fontInstalled`: same family and style in `QFontDatabase` before the preview adds its copy; `installFont()`: `KIO::file_copy` to `~/.local/share/fonts`, never overwriting, then `fc-cache` except in test mode), a field for own text (`Viewer.fontSampleText`, until close), alphabet / digits / punctuation, a pangram (translatable) at one size (`Viewer.fontSampleSize`, 36 px, − / + through common sizes, Ctrl+wheel and Ctrl+plus/minus/0 via the usual `zoomBy()`, 8-288 px, until close). Viewer adds the file with `QFontDatabase::addApplicationFontFromData` (main thread, removed on the next file and on close) and takes family and style from `QRawFont`, so a Bold file shows bold; unloadable → fallback |
| `qml/MediaView.qml` | Video and audio: `MediaPlayer` + `VideoOutput`, or cover (thumbnail) / icon with title and artist; footer `ToolBar` with play/pause, times, seek `Slider`, repeat (`Viewer.mediaRepeat`, until close), mute, volume (`Viewer.mediaVolume`, saved in `kwiklookstaterc`, cubed for the output), fullscreen (videos; button, double-click, F: `Main.qml` drops bar, frame and shadow, Esc leaves it first, a file change ends it; the footer floats and hides after 2 s still). Autoplay and start muted from the settings; stops when the window hides or the file changes (`mediaUrl` cleared, so reopening restarts); media keys. Audio with a cover: a blurred copy (`ShaderEffectSource` of the thumbnail + `MultiEffect`, under 60 % of the background colour) fills the area. Videos without autoplay: `pause()` on load shows the first frame, a large play button over it while not playing |
| `qml/ContentsList.qml` | The sidebar's contents list as a tree: parents from the levels (skipped levels allowed), tree lines (├ └ │, `Kirigami.Separator`s drawn by each row), fold arrows (`folded` by index, reset per list: over 30 entries only the top level open), rows across the full width (KDE's ItemDelegate insets its background by the left padding, so the indentation is in `contentItem`), top level bold; the current entry, or the folded parent it's in, highlighted and kept in sight; `activated(index)` on click |
| `qml/ArchiveView.qml` | Archives: summary line (lock when encrypted), an InlineMessage for partial / locked / unreadable / no sandbox, KDE header view + `TreeView` with `TreeViewDelegate` (it draws `model.display` itself; `text` stays empty), name / size / modified; the one top folder opens by itself; source set on `fileInfoChanged` when the view is ArchiveView (the next file's address during a switch may be anything); alternating rows, size and date dimmed (the delegate's `Kirigami.Theme.textColor` from the tree's theme) |
| `qml/FindBar.qml` | The find bar below TextView and PdfView: field (`Viewer.searchText`), “3 of 12”, previous / next, close (`Viewer.searchOpen`); both kept until the preview closes. Main.qml: Ctrl+F, F3 / Shift+F3, Esc closes the bar first. TextView finds in `getText()` (ignoring case) and marks only the matches on screen (rectangles under the text: KSyntaxHighlighting owns the layout's extra formats); PdfView uses `PdfSearchModel` (made only while the bar is open) and draws `boundingPolygonsOnPage()` per page |
| `qml/IndexSheet.qml` | All files as a GridView over the view area (`Viewer.indexOpen`, until close; Ctrl+Enter or the bar's “IndexSheet”): `Viewer.fileAt(i)` (type by name only), tiles are `ItemDelegate`s (the style's highlight and hover), thumbnails through the thumbnail provider, arrows and Enter / click → `Viewer.showFile()`; Main's ← → and Enter are off while it's open, Esc closes it first |
| `qml/InfoPanel.qml` | The (i) panel: sections from `Viewer.infoSections` (File, Details, Permissions), Show in Dolphin, Copy Path |
| `qml/TitleBar.qml` | The only bar: type icon and file name (centred; tooltip: full name, “File 2 of 5”, details, picture size or `pageCount` from the view) and the items from `KwikLookSettings.barLeft` / `barRight` (default ← →, index sheet (several files), sidebar (PDFs with several pages or bookmarks, Markdown / HTML with headings: the view's `sidebarAvailable`); view toggle (“ViewMode”, only when `Viewer.formattedKind` isn't empty or "book"), crop and rotate (still pictures), (i), Share, Open with…, close; “Spacer” stretches); a standard toolbar (Kirigami header size); Header colours; drag moves the window |
| `qml/BarItems.qml` | Singleton: the bar's items by name (Previous, Next, IndexSheet, Pages, ViewMode, Information, Share, OpenWith, Open, ShowInDolphin, CopyPath, Close, Spacer), icon and text; `known()`: the bar, its editor and the settings window drop names they don't know (saved bars from older versions, e.g. “CropRotate”) |
| `qml/BarEditor.qml` | The settings' bar editor: a copy of the bar (left / right drop zones around a fixed “File name”) and the unused items; drag with a ghost (items in a layout can't be dragged themselves), `move()` holds the rules (out = back to “Available”, the spacer is copied); reset in SettingsWindow |
| `qml/ResizeHandles.qml` | Edge and corner strips calling `startSystemResize()` (frameless windows get no KWin borders) |
| `qml/SettingsWindow.qml` | “KwikLook Settings”: Kirigami Addons form cards (Appearance, Bar, Behaviour, Videos and Audio, Office Documents), OK / Apply / Cancel (values held in the window until applied); made by main.cpp on the first `ShowSettings` |

```
Dolphin hook / kwiklook <files> / dbus-send
  └─ D-Bus Open(urls, index, session, token) ─→ Viewer ─→ window shown, token → focus
                                                  ├─ CurrentChanged(session, url, index)
                                                  └─ Closed(session)
```

**Dolphin hook** (`src/dolphin/`, based on KDEGlance's; ported files keep its SPDX line). Dolphin
loads every `kf6/overlayicon` plugin; ours adds no icons and only creates a `Bridge` once per
Dolphin process. `DolphinAccess` reads the view's files in display order and the selection, and
selects a file (with two or more files selected, only those go to the viewer and the
bridge leaves Dolphin's selection alone: `Snapshot::selectionOnly`), through libdolphinprivate functions looked up by mangled name (`dlsym`), checked
on load. Their names live in one table, `src/dolphin/dolphinsymbols.h` (required / optional / older form); `tst_dolphinsymbols` checks it against the installed Dolphin and the mock. A mangled name doesn't include the return type: a re-typed function still resolves, so two more guards: a self-check on the first Space in a folder with files, per Dolphin process (`DolphinAccess::selfCheck()`: count, current item in range, the file found again by its address; calls every read function once; a failure = fallback for that process only, the Bridge then tracks the public selection), and `HookGuard` (`src/dolphin/hookguard.*`): the first “read” and “select” step under a Dolphin + KwikLook version pair runs with a marker in `kwiklookstaterc` [DolphinHook] (absolute path under `XDG_STATE_HOME`; inside Dolphin `openStateConfig()` would be Dolphin's file); a marker left at the next start = Dolphin died in our call: fallback for those versions (`Broken`); passed steps write nothing. Without them, Space previews the selected file alone. Space is taken only when the focus
is in a file view with a file selected and Dolphin is not in type-ahead or selection mode. The
bridge starts the viewer when Dolphin starts (`StartServiceByName`), asks for a Wayland activation
token, then calls `Open` with a session ID `<bus name>/<n>`; `CurrentChanged` moves Dolphin's
selection, `Closed` gives the focus back. The open key comes from `kwiklookrc` and is re-read when the settings window saves (`KConfigWatcher`). Diagnostics on Dolphin's bus name at `/KwikLookBridge`:
`State()` (JSON, includes `openKey`, `check` pending / ok / failed and `problem`), `TriggerFocusedView()` (what Space does). `KWIKLOOK_DISABLE_DOLPHIN_HOOK=1 dolphin`
turns it off.

---

## Key Conventions

### Threading
- Blocking work (file parsing, hashing, scanning, process runs) goes through `QtConcurrent::run`
  and returns via `QFutureWatcher::finished` on the main thread.
- Never touch model data from worker threads. A generation counter drops results from work
  started before a refresh.
- Large lists (thousands of rows): filter/sort/merge off the main thread; no per-row lookups
  (theme icons, regexes, scans of another list) in a loop. Use hashes.

### Files and settings
- User files are moved to the Trash (`KIO::trash()` / `QFile::moveToTrash()`), never removed.
  Connect `KJob::result()` before `start()`.
- Settings through KConfig XT (`.kcfg` + `.kcfgc`); a QML singleton wraps it. Several processes
  (app, daemon, tray widget) mean several caches: re-read with `reparseConfiguration()` /
  `load()` and tell the others over D-Bus.
- Per-item data keyed by something stable (file name, id), and kept in its own config group.

### QML and UI
- Stock Kirigami controls and theme colours only; no custom palettes or borders. Every window:
  `Kirigami.Theme.colorSet: Kirigami.Theme.Window` + `Kirigami.Theme.inherit: false`.
- Keep it quiet: dim secondary text (`Kirigami.Theme.disabledTextColor`), one metadata line
  instead of chips, a primary action plus a `⋯` menu, passive notifications or
  `Kirigami.InlineMessage` instead of dialogs. Modals only for destructive or blocking choices.
- Master-detail pages: a View-coloured list pane, a centred details column capped in width,
  "About" and "Details" sections (`Kirigami.ListSectionHeader` + key/value `GridLayout`).
- Animated show/hide: `visible: opacity > 0` + `Behavior on opacity`.
- Use one bulk `itemData(row)` call for a detail pane instead of many per-role reads.
- Visual rules (decided 2026-11-24, the visual review in Todo.md):
  - One surface per job: content (text, pages, pictures, lists of the file's contents) on the
    View colour; panels around it (sidebars, the info panel, edit bars) on the Window colour,
    with a `Kirigami.Separator` between.
  - Every panel has a heading row with a separator under it.
  - List rows span the panel's whole width; indentation goes inside the row, and the
    highlight fills the row.
  - Floating labels (zoom, position, page number) share one look and fade the same way. Position and zoom share one box in the bottom-right corner, its corners nested in the window's (decided 2026-11-25).
  - Nothing flashes: the old view stays until the new one is ready; busy indicators only
    after about 300 ms.
  - Secondary information (sizes, dates, counts) dimmed, everywhere.

### i18n
- QML `i18n()` / `i18np()` / `i18nc()`; C++ `i18n()` from KI18n. Pass arguments to `i18n()`
  itself (`i18n("Got %1", n)`), never `i18n("Got %1").arg(n)`.
- New source files with strings go into `po/Messages.sh`.
- Unreviewed machine drafts stay out of `po/`: fuzzy entries are never shown at runtime.

### D-Bus
- Names derive from the app ID (`io.github.strandzen.KwikLook.<Part>`). Service files use the
  section name `[D-BUS Service]` exactly.

---

## Verification

1. Build with zero errors, `ctest` all green.
2. QML: `qmllint` on every changed file.
3. Visual checks without the user's desktop:
   - Small pieces: render a scratch QML file offscreen and `grabToImage()` it
     (`QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME=kde QT_QUICK_CONTROLS_STYLE=org.kde.desktop`).
   - Whole windows: a nested compositor on a private bus, and only then a screenshot tool:
     `dbus-run-session -- kwin_wayland --virtual --width 1500 --height 950 --exit-with-session <script>`.
     Without the private bus, `spectacle` reaches the user's real session and captures their
     screen. Check the image size matches the virtual screen before looking at it.
4. End-to-end runs of code that writes files (installs, menu entries): a throwaway `HOME`
   (`env -i HOME=<scratch> ... dbus-run-session -- <program>`), so nothing lands in the user's
   folders.
5. Manual checklist below for what tests can't cover.

### Manual testing checklist

1. `kwiklook <two files>`: the preview opens with keyboard focus, file name centred in the bar.
2. Corners, outline and shadow look like other Breeze windows next to it.
3. Drag the bar: the window moves. Drag an edge or corner: it resizes (cursor changes over the edge).
4. ← → and ↑ ↓ (keys) and the buttons switch files, also after clicking into a text or PDF; the buttons grey out at the ends.
5. Space or Esc closes the preview; `kwiklook <file>` again opens it at once (viewer still running).
6. System Settings → Window Decorations → Breeze: change shadow size, outline or rounded corners; an open preview follows within a moment.
7. `kwiklook --quit` stops the background viewer.
8. Dolphin (after installing, or `QT_PLUGIN_PATH=build/dev/plugins dolphin` with no other Dolphin running): Space on a file opens the preview, ← → move Dolphin's selection along, Space/Esc closes and Dolphin has the focus again. Space still works in the location bar, while renaming, in type-ahead search and in selection mode; on a folder it does what Dolphin does. With three files selected (Ctrl+click), Space shows just those, ← → stay within them, and Dolphin's selection is unchanged afterwards.
9. The bar's buttons are normal toolbar size, easy to hit.
10. Open with…: lists the file's apps; one opens the file and the preview closes; “Other Application…” shows KDE's chooser. For a picture, “Spectacle” is listed and opens it in Spectacle's editor.
11. A picture, a text file, a folder and an unknown file each show a thumbnail or icon with type, size and date.
12. A large picture fills the preview edge to edge (no name below it), the window shaped like it; a small one stays at its real size; a small SVG fills the window and is sharp. In the smallest window the bar's buttons don't overlap the name (it's shortened in the middle). (i) opens the panel at the right and the picture shrinks beside it; it stays open for the next file and after a restart. Show in Dolphin opens the folder with the file selected; Copy Path puts the path on the clipboard.
13. A code file shows as coloured text with line numbers; a Markdown file shows formatted (a readable column, a link opens the browser, a picture next to it shows, a web picture doesn't), the bar's view toggle (left of (i), only for Markdown) shows its coloured text and stays chosen for the next Markdown file and after a restart; a log over 256 KB shows its start and says so; a PDF shows all pages fitted to the width. ← → and Space still work after clicking into the text.
14. Share: KDE's share menu opens below the button; sharing to a device or by mail works. With LocalSend installed it's listed last; it opens LocalSend ready to send the file.
15. “KwikLook Settings” from the launcher opens the settings window (also while a preview is open, which stays open). Apply is grey until something changes; Cancel drops changes.
16. Custom corner radius: Apply, and an open preview's corners and shadow follow at once; back to “like Breeze” restores them.
17. Open key Shift+Space: in Dolphin (already open) Space selects as usual and Shift+Space opens the preview; Shift+Space or Esc closes it.
18. With “close when you switch” on, clicking Dolphin or another window closes the preview; “Open with… → Other Application…” and Share dialogs don't.
19. “Start in the background at login” on: the entry shows in System Settings → Autostart; after a new login the viewer is running before Dolphin opens.
20. An animated GIF and WebP play; a still GIF stays still.
21. Zoom a large picture with Ctrl+wheel: it zooms towards the pointer and stops at 100 % on the way; dragging and scrolling (wheel or touchpad) move it; pinch on the touchpad zooms; double-click switches real size / fitted; the zoom label fades out; the next picture starts fitted.
22. Ctrl+wheel in a text file: the font grows and stays for the next text file, normal again after closing. In a PDF: pages grow with a sideways scrollbar; Ctrl+0 fits them again.
23. A DOCX, XLSX and ODT (LibreOffice installed): icon and “Preparing preview…” for a moment, then the pages; the second time at once. With “Preview office documents” off: the icon only. `ls /tmp | grep OSL_PIPE` shows nothing left after previews.
24. A video and an MP3: the video plays at once (setting), the MP3 shows its cover or icon, title and artist; play/pause, seek and mute work; media keys play and pause; closing the preview stops the sound; reopening starts again. With “Start without sound”: silent until the mute button. Volume slider changes the loudness and is the same after a restart; repeat plays the video again until the preview closes. Full screen (button, double-click, F): only the video, controls appear on mouse move and hide after 2 s, the pointer hides with them; Esc returns to the window, a second Esc closes; ← → leave full screen.
25. Enter on a file opens it in its usual application and the preview closes (also after clicking into a text file); a script opens in its editor, it doesn't run. With several files “2 / 5” shows in the bottom-right corner for a second on opening and on each switch (above the find bar or player controls when they're there), also in the name's tooltip; with one file nothing. Zooming while it shows: “2 / 5 | 150%” in one box.
26. A font (TTF, OTF): its name, the alphabet and the sample (36 px; − / + and Ctrl+wheel change it, kept for the next font, 36 again after closing) in that font (a bold italic file shows bold italic); typed text replaces the sample, Enter and ← → don't act while typing, the text stays for the next font and is gone after closing. A font that isn't installed (e.g. from an application's own folder) shows Install Font: it lands in `~/.local/share/fonts`, the button turns into “Installed”, and after a restart other applications list it.
27. Ctrl+F in a text file, a Markdown file (rendered and source) and a PDF: matches marked, “3 of 12”, Enter / Shift+Enter and F3 / Shift+F3 move and scroll there; Space types a space, ← → move in the field; Esc closes the bar, a second Esc the preview; the search stays for the next file.
28. Settings → Bar: drag Copy Path into the bar, Close out of it, Previous after Next, two flexible spaces in; Apply and an open preview shows the new bar at once; a dragged button works; “Reset Bar to Defaults” and Apply bring the usual bar back. Dragging up or down moves the button, it doesn't scroll the page.
29. Formatted views, each switched by the bar's toggle and remembered per kind: an SVG as picture / XML, a JSON file indented / as written, an HTML page rendered / source, a .desktop file as a card (icon, name, command) / text. Switching SVG to text leaves Markdown formatted. A CSV (also one with semicolons) as a table with its first row as headers, scrolling keeps headers and row numbers in place. A .vcf with two contacts: two cards, the photo or initials, phones, mail, address, birthday. An .ics: its events in order, local times, all-day ones as dates, place, organizer, “Repeats”.
30. A PDF with several pages: the bar's sidebar button opens small pages on the left (with bookmarks too: the “Pages” and “Contents” tabs fill the sidebar evenly, and it keeps its width when switching tabs); the current page is marked and follows scrolling, a click goes there; it stays open for the next PDF and after a restart; a one-page PDF without bookmarks has no button.
31. Several files (or a folder): Ctrl+Enter shows the index sheet with thumbnails, the current file marked; arrows move, Enter or a click shows that file and the sheet closes; Esc closes the sheet, a second Esc the preview; one file: no sheet, no button.
32. The background viewer stays idle: after showing an animated GIF, a PDF (also with a search) and a video, close the preview; `top -p $(pidof kwiklook)` shows 0 % CPU (or `powertop`: no wakeups from kwiklook).
33. Contents (`testfiles/pdf/example-contents.pdf`, `office/example-contents.docx`, `text/example-full.md`, `text/example-table.html`): the sidebar opens on “Contents” with three levels for the PDF (tabs “Pages” / “Contents”, the last one stays chosen); a click on “1.1.1 PNG” or “1.1.2 JPEG” puts that heading near the top, not just its page; scrolling moves the mark along, at the end the last heading on screen is marked. Markdown: its headings, also in the source view (a click goes to the `#` line). `pdf/example.pdf` (no bookmarks, several pages): Pages only, no tabs.
34. Archives (`testfiles/archives/`): example.zip / .7z / .tar.* / .iso / .cpio show the same folders (docs, pictures, deep 12 levels, empty folder, the Unicode name, “link to readme” without a size), a summary with sizes; a click on a folder's arrow opens it. example-password.zip: a lock and “Password protected”. example-5000-files.zip: at once. example-1GB-unpacked.*: “1 GiB unpacked”, nothing written to disk. example-broken.zip: what could be read and a note. example-unsafe-names.zip: “../outside.txt” and “/etc/absolute.txt” as plain names. example.txt.gz: one entry. A .docx or .odt is still a document.
35. Visual review: Markdown with headings and a PDF with bookmarks show the sidebar with a “Contents” heading, tree lines and fold arrows (the Ghostscript manual starts with only its top level open; folding a heading marks it while you read inside it); code blocks in a shaded box, two in a row as two boxes, quotes with a bar. ← → through pictures, PDFs and text: no empty or icon page in between, a short fade. A song with a cover: the cover blurred behind; a video with autoplay off: its first picture and a play button. Scrolling a PDF shows “3 / 12”. The index sheet marks the chosen file and hovers like Dolphin. An archive list has alternating rows. The bar has the type icon; hovering the name shows the details.
36. An AppImage with AppImage Manager installed: “Install with AppImage Manager” under the details line; a click opens its install window for that file and the preview closes. Without AppImage Manager (or for any other file): no button.
37. Window placement (Settings → “Open the preview”), each after Apply: “Centred on the screen”: the preview opens centred, without a jump. “Centred on Dolphin” with a small Dolphin window off to one side: centred on it; `kwiklook <file>` from a terminal: centred on the screen. “At the mouse pointer”: around the pointer, kept on the screen (try a corner and a second screen). Pictures: a large photo makes a window of its shape (at most 80 % of the screen), ← → reshape it around its centre, a text file after them gets the normal size back; drag an edge, then ← →: the size stays until the preview closes. Full screen video: no reshaping. “Where the window manager puts it”: as before, and `qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.isScriptLoaded kwiklook-placement` says false; `kwiklook --quit` also leaves it false. The settings window is never moved.
38. Books (`testfiles/books/`): example.epub shows its cover, title and author, then the chapters (picture, list, quote bar, table, code box); the sidebar lists 1. Pictures / 1.1 Formats / 1.2 Zoom / 2. Documents and a click goes there; the web link opens the browser, the link inside the book does nothing; no red text; no formatted / source toggle; Ctrl+F finds words; (i) shows title and author. example-long.epub: a note that only the beginning shows. example-no-namespace.epub: like example.epub, no crash. example-broken.epub: icon and “This book couldn't be read”.
39. Loading ahead: in `testfiles/images/`, ← → between example-raw.dng, example-huge.jpg and example.jpg after a moment on each: the next picture appears at once (no busy indicator, no fade from an empty view); the RAW photo shows as a picture. Closing the preview: `top` shows the memory going back down a little (the preloaded pictures are released).
40. Free memory when idle (Settings → Behaviour, on): preview a picture, a PDF and a video, close, wait 15 minutes: `kwiklook` has the same PID and about 35 MB (`grep Pss /proc/$(pidof kwiklook)/smaps_rollup`), and the next preview from Dolphin opens normally; with the settings window open the whole time it doesn't restart. Turned off: still the higher number after 15 minutes.

---

## Known Pitfalls

General Qt / KF6 / Kirigami ones first; add project-specific rows below them.

| Issue | Detail |
|-------|--------|
| **`Kirigami.Units.fonts` doesn't exist** | Use `Kirigami.Theme.smallFont` (a `font`) |
| **Kirigami Dialog `preferredHeight` binding loop** | Binding it to `window.height` loops through the dialog's own `y` → "Binding loop detected for property y" and an abort. Use a fixed `Kirigami.Units.gridUnit * N` |
| **Wrapped labels widen dialogs** | A wrapping `Label` reports its unwrapped width as `implicitWidth`, which layouts pass up. Set `Layout.preferredWidth: 0` (plus `fillWidth`) on long labels |
| **QML file scope** | A component file can't see ids of the file that instantiates it; pass values in as properties |
| **`Kirigami.Page` outside a PageRow** | In a `StackLayout`, a page's `actions`, `title` and toolbar are never shown. Put controls in the page or the window's toolbar |
| **Lists go inside `Controls.ScrollView`** | A bare `ListView` + `ScrollBar` draws the bar over rows and scrolls slowly; the desktop `ScrollView` pads for it and adds wheel handling. Size the ScrollView, not the ListView |
| **ItemDelegate last-row inset** | The Plasma style gives the last row a bigger bottom inset; set all insets and `verticalPadding` explicitly |
| **Delegate property named like a context property** | `property int index: index` binds to itself (always 0). Use another name |
| **`anchors.fill` + `drag.target`** | Anchors override the drag position; drag targets need explicit width/height |
| **`AnchorChanges` and `anchors.fill`** | Not supported in States; set individual sides |
| **`clip` on a rounded Rectangle** | Clips to the bounding box, ignores `radius` |
| **Remote images in long lists** | `Kirigami.Icon` decodes http(s) images on the UI thread; use `Image { asynchronous: true; sourceSize: … }` |
| **Theme icon lookups in bulk** | `QIcon::hasThemeIcon()` for thousands of names took 34 s (and isn't thread-safe through KIconTheme). Let `Kirigami.Icon { fallback: … }` handle missing names for visible rows |
| **Disabled controls get no hover** | Tooltips on a disabled button need a `HoverHandler` (enabled only then) |
| **Checkable menu items** | In the Plasma style a menu reserves a checkbox column on every row once one item is checkable; icons and checkboxes both show |
| **QML singleton ownership** | A `QML_SINGLETON` `create()` returning an existing object must set `QJSEngine::CppOwnership`, or the engine deletes it on exit (abort) |
| **`QLatin1String` with non-ASCII** | Multi-byte UTF-8 turns into garbage; use `QStringLiteral` |
| **QStringBuilder into KConfig** | `writeEntry(key, a + b)` fails to compile with QStringBuilder; wrap in `QString(...)` |
| **`KDesktopFile` with a bare file name** | Resolves into `~/.local/share/applications` (or similar); always pass absolute paths, also in scratch tests |
| **Translated desktop keys via KConfig** | `writeEntry("Name[de]", …)` escapes the brackets, and `setLocale()` + `Localized` can file a text under the wrong language. Write your keys with KDesktopFile, then insert translated lines verbatim |
| **`i18n(...).arg(...)`** | Logs "0 instead of 1 arguments" warnings; pass arguments to `i18n()` |
| **Fuzzy translations** | gettext ignores fuzzy entries at runtime; machine drafts marked fuzzy show as English |
| **D-Bus service files** | dbus-broker accepts only `[D-BUS Service]` (not `[D-Bus Service]`); otherwise activation silently never happens |
| **Plasma `SignalWatcher`** | Calls a function named `dbus` + the signal name (`StateChanged` → `dbusStateChanged()`) |
| **Long timers** | `QTimer` overflows past 24.8 days; schedule from a stored timestamp checked on a short tick |
| **`KDirWatch`** | Doesn't replay existing files on `addDir`; `dirty(path)` also fires for the watched directory itself when a child changes |
| **Directory-mtime caches** | Can miss changes within one timestamp tick; verify entries still exist before using them |
| **`KDBusService`** | `Unique` raises the running instance (forward arguments via `activateRequested`); use `Multiple` for modes that must run side by side |
| **Several processes, one config** | `KSharedConfig` caches; re-read before use when another process writes |
| **GitHub API rate limit** | 60 requests/hour unauthenticated, and conditional `304` replies still count without a token. The public `releases.atom` feed isn't limited |
| **Catalog "download" links** | Often web pages (release pages), not files; resolve them and refuse `text/html` before saving anything |
| **QML Menu screenshots** | Recent Qt opens menus as separate native windows; an offscreen grab of the main window doesn't include them. Render the `MenuItem`s in a plain column instead |
| **App-private QML module** | Build it as a static library (`qt_add_library(... STATIC)` before `qt_add_qml_module`), link `<target>` and `<target>plugin`, and `Q_IMPORT_QML_PLUGIN(<uri>Plugin)` in `main.cpp` and tests. A shared backing library makes the plugin a MODULE that can't be linked |
| **QML resource prefix** | Use `RESOURCE_PREFIX "/qt/qml"`: the engine searches there by default. With `"/"` `loadFromModule()` fails with "Module … contains no type named …" |
| **`Kirigami.ShadowedTexture` source** | The source `Image` is still drawn by itself at its own size; give it `visible: false` and the texture's size |
| **Other window decorations** | Decided 2026-11-19: the frame stays Breeze-only (README Troubleshooting says so). Controls follow the Application Style anyway (`org.kde.desktop` paints through the QStyle). Considered and declined: KWin's real decoration for non-Breeze users (two bars), imitating other decorations, reading a user-chosen rc file (klassyrc, oxygenrc, Aurorae themerc: keys and looks differ; only Breeze forks would map, per fork), manual shadow/outline settings |
| **Breeze look in a frameless window** | No API exposes Breeze's look. From breezerc `[Common]`: ShadowSize (by name, e.g. `ShadowLarge`), ShadowStrength (scales the shadow linearly), ShadowColor, OutlineEnabled, RoundedCorners. Measured 2026-11-18 (Plasma 6.8, 100%): radius 5 px; outline = Header background mixed 20% towards Header text, per channel; shadow layers per size in `breezestyle.cpp` |
| **Breeze title bar height** | Not a fixed number: Breeze computes it from the title font (`QFontDatabase::TitleFont` = kdeglobals `[WM] activeFont`) and breezerc `[Windeco] ButtonSize`: `max(fm.height(), button) + 4·smallSpacing`, grid unit = `boundingRect('M').height()` rounded up to even, button = 2·grid unit (Default), smallSpacing = max(2, grid/4). Checked against 5 font sizes in a nested session. Not used now (the preview has a toolbar-height bar), kept in case it's needed |
| **Measuring Breeze in a nested session** | Breeze reads breezerc once when KWin starts; `org.kde.KWin.reconfigure` doesn't apply file edits. Use a fresh nested session per setting. Compare against a wallpaper-only screenshot (darkening = 1 − shot/wallpaper) and find window edges by the outline colour |
| **`QColor` entries from KConfig** | Need KF6::ConfigGui linked; with ConfigCore only, `readEntry("…", QColor)` silently returns the default |
| **KDE style for Qt Quick** | Call `QQuickStyle::setStyle("org.kde.desktop")` when `QT_QUICK_CONTROLS_STYLE` is unset (KDE apps do). Otherwise Kirigami uses basic colours: the Header colour set equals Window |
| **`Kirigami.ColorUtils.linearInterpolation`** | Doesn't give a plain per-channel mix (off by several steps per channel); compute `Qt.rgba(a.r + (b.r - a.r) * t, …)` when matching a measured colour |
| **`ToolBar` with a custom background and several children** | Gets no height from its content; set `contentHeight` from the layout |
| **Nested session with copied `kwinrc`** | It also loads the user's KWin scripts (tiling, window-position memory), which move test windows. Turn them off in the scratch copy. Without real input, KWin's focus stealing prevention never triggers there: focus behaviour needs a live test |
| **Default-constructible `QML_SINGLETON`** | If the class can be built without arguments, the engine builds its own instance and never calls `create()`: QML and C++ then see two objects. Give the constructor no default argument |
| **Generated QML registration includes headers by bare name** | `#include <viewer.h>`, not `gui/viewer.h`: add each source subfolder to the module's include directories |
| **`qdbuscpp2xml -S -M`** | Also lists non-scriptable signals (a property's NOTIFY). Write the interface XML by hand and compare with the live introspection |
| **D-Bus activation in a nested session** | `dbus-run-session` starts before KWin, so activated services get no `WAYLAND_DISPLAY` and abort. Run `dbus-update-activation-environment WAYLAND_DISPLAY …` first; give the bus a `--config-file` with an extra `<servicedir>` holding a service file that points at the build binary |
| **QMenu from a QML window (Wayland)** | Call `ensurePolished()`, then `winId()` and `windowHandle()->setTransientParent(item->window())`, then `popup()`. Creating the native window before polishing loses Breeze's translucency (black, square menu corners); without a transient parent the popup fails. Popups need real input: they can't be opened in the nested session |
| **`dbus-send` without `--print-reply`** | Sends a *signal* (its default `--type`), not a method call: the service ignores it. Use `--print-reply` (or `--type=method_call`), `busctl call` or `gdbus call` |
| **Async D-Bus calls to your own connection** | `asyncCall()` to a service registered on the same `QDBusConnection` never arrives. In tests, give a stand-in service its own connection (`QDBusConnection::connectToBus(..., name)`) |
| **Mock of a shared library under KDE compiler settings** | ECM builds with `-fvisibility=hidden`: a mock meant to be found by `dlsym` (or linked) needs `CXX_VISIBILITY_PRESET default` |
| **Testing the Dolphin hook in a nested session** | `QT_PLUGIN_PATH=<build>/plugins dolphin --new-window --select <file>`; no keyboard there, so call `TriggerFocusedView` on `org.kde.dolphin-<pid>` `/KwikLookBridge` |
| **“Could not register app ID: App info not found” from the build tree** | Qt registers the app with the desktop portal by its desktop file name; uninstalled, there's no `io.github.strandzen.KwikLook.desktop`, so the portal refuses. Harmless (no portal features used); gone once installed |
| **`QQuickImageResponse` and the main thread** | Qt creates, cancels and deletes the response on its image loader thread. Don't `moveToThread()` it (crashed on fast switching: SIGSEGV in `deleteLater` on the loader thread). Run main-thread work (KIO jobs) in a separate relay object that sends the result back by queued signal and deletes itself; the response never touches it again |
| **Reproducing races in the viewer** | The offscreen QML test did not reproduce the thumbnail crash; fast D-Bus Next/Previous in the nested session did (kept as the check: Next/Previous with 0–80 ms pauses over folders, text, binary and pictures) |
| **Qt's image reader claims PDFs** | With Qt PDF installed, `QImageReader::supportedMimeTypes()` includes `application/pdf`: test for PDF before pictures |
| **`PdfMultiPageView` rows** | Its column width is the view's width plus the scrollbar plus 2 px: always a horizontal scrollbar and off-centre pages. Use `PdfPageImage` in a ListView instead |
| **`PdfDocument` with an empty source** | Logs “Cannot open:”. Keep the last source instead of clearing it, and set it from a function, not a `Binding { when: … }`: its value can update before its condition and set an empty source for a moment (seen going from one PDF to the next) |
| **`PdfPageImage` kept across documents** | A delegate the list keeps when the document changes warns “document and source properties in conflict”. Drop such views before setting the new source (the overview's Loader) |
| **An id named like the property it feeds** | `PdfPageImage { document: document }` binds the property to itself (blank pages). Name the id differently (`pdf`) |
| **Large text in a QML TextArea** | Measured 2026-11-18: 1 MB plain text took 0.2 s to lay out, 0.46 s with KSyntaxHighlighting, on the main thread; 256 KB 0.05 / 0.11 s. Hence the 256 KB limit |
| **Runtime dependency: Qt PDF** | `QtQuick.Pdf` ships with Qt WebEngine on Arch (`qt6-webengine`); list it in packaging |
| **Runtime dependency: Qt Multimedia backend** | `QtMultimedia` needs a backend plugin to play anything: `qt6-multimedia-ffmpeg` on Arch; list both in packaging |
| **KConfig XT and `KConfigWatcher`** | Other processes only hear about a save written with `KConfig::Notify`; the kcfgc has no option for it: call `setWriteFlags(KConfigBase::Notify)` on each item. `kconfig_add_kcfg_files` on a target without `GENERATE_MOC` fails with "moc_….cpp: No such file" |
| **Nested session reads the real `kwinrc`** | `kwin_wayland` uses the caller's `XDG_CONFIG_HOME`: run it with a scratch config holding a `kwinrc` copy with KWin scripts off (`rememberwindowpositions` resized the settings window to the preview's remembered size, and applies the scale) |
| **No log output from a nested-session app** | Without a terminal Qt logs to the journal; set `QT_FORCE_STDERR_LOGGING=1` to get it in a file |
| **Focus leaving the app (Wayland)** | `QGuiApplication::focusWindow()` becomes null when another app takes the focus; own menus, dialogs and windows keep it non-null. It can pass through null briefly between own windows: check again after a short delay (150 ms) |
| **Sizing a Repeater from a view's height** | A ScrollView can be as tall as its whole content while a long text lays out; `model: scroll.height / lineHeight` then made thousands of line-number labels, rebuilt on every size change (froze the viewer on a 2,300-line file). Size it from something fixed (`Screen.height`) |
| **ListView sets its delegates' x** | A vertical ListView moves delegates back to x = 0 when it lays them out again, dropping a binding on `x` (some pages centred, others not). Make the delegate as wide as the content and centre a child in it |
| **Views don't clip by themselves** | A ListView, Flickable or a zoomed Image paints outside its bounds, over the bar above it. The area around the views in `Main.qml` has `clip: true` |
| **Qt PDF and engine teardown** | Deleting a QML engine while a `PdfPageImage` renders crashes the image thread (SIGSEGV in the qpdf image plugin). `Viewer::finishRendering()` waits until no page has rendered for 100 ms (a single check let a page started by a queued relayout after a zoom slip through: 1 crash in ~15 test runs); main() and the PDF tests call it before the engine goes |
| **Mouse or touchpad wheel on Wayland** | Qt's Wayland client reports one pointer device for both, typed `TouchPad` (checked 2026-11-19, Qt 6.12, with a probe listing `QInputDevice::devices()`). A `WheelHandler` takes only mice by default, so on Wayland it gets nothing: always set `acceptedDevices: PointerDevice.Mouse \| PointerDevice.TouchPad`. Don't tell wheel and touchpad apart by `pixelDelta` either: a plain-wheel zoom built on that (touchpad scrolling has a `pixelDelta`) didn't react to the user's mouse wheel. Tests send wheel events from a TouchPad-typed `QPointingDevice` |
| **Qt Quick text fetches web pictures** | A TextEdit with Markdown or rich text loads `![](http://…)` and `<img>` through the QML engine's network access (`QQuickTextEdit::loadResource`, before any `QTextDocument::setResourceProvider`). Only an engine-wide `QQmlNetworkAccessManagerFactory` stops it (`OfflineNetworkFactory`; the smoke test's stand-in server got a connection without it). Qt's image loader drops a custom reply's error text ("Unknown error", late): fail through an unknown scheme instead |
| **Qt's Markdown drops links on pictures** | `QTextDocument::setMarkdown` / `TextEdit.MarkdownText` (Qt 6.12): a picture inside a link (`[![a](p.png)](url)`, also with text beside it) gets no anchor, so `linkAt()` and clicks find nothing there; inline `<a href><img></a>` in Markdown is dropped completely. The picture's title survives (as its tooltip). HTML (`RichText`) keeps the link and clicks work. Accepted, see Todo.md “To watch” |
| **LibreOffice in a sandbox** | Needs AF_UNIX sockets and creates its single-instance pipe in `/tmp` (hard-coded in libuno_sal, `TMPDIR` doesn't move it; fallback `/var/tmp`): give `/tmp` create-socket + delete rights, else it exits 1 without a message. A separate `-env:UserInstallation` keeps it apart from the user's running LibreOffice. Debug without strace: bisect the sandbox (rules off one by one, `--read /`) |
| **Running a test binary directly** | ctest sets `QT_QPA_PLATFORM=offscreen` (and the hook test's private bus); `build/dev/bin/tst_*` run by hand opens real windows on the user's desktop, and focus-dependent tests fail there. Run tests through `ctest -R <name>`, or set the same environment |
| **libarchive in an empty environment** | With no `LANG`/`LC_*` (the sandbox's default) names that aren't ASCII can't be converted: `archive_entry_pathname_utf8()` gives nothing and a warning. Call `setlocale(LC_CTYPE, "C.UTF-8")` in the program |
| **bsdtar stores zero-filled files as sparse** | A “1 GB” test file packed with bsdtar came out 251 bytes and listed instantly; pack with `--no-read-sparse` for a real one |
| **QML's `PdfBookmarkModel` corrupts memory** | Checked 2026-11-24 (Qt 6.12, qt6-webengine 6.140): a `PdfBookmarkModel { document: pdf }` in PdfView made later tests crash in unrelated places (heap corruption; gone without it), probably when the engine deletes the document before the model. Read bookmarks in C++ with a `QPdfDocument` + `QPdfBookmarkModel` of their own, deleted in order (`PdfContents`). Its `location` is in points from the page's top, pages 0-based |
| **A TextArea scrolls back to its cursor** | Setting the ScrollView's `contentY` to jump to a heading was undone at once (back to the cursor at the start). Move `cursorPosition` to the target first |
| **`positionToRectangle()` beyond the text** | Logs “QTextCursor::setPosition: Position … out of range” (happens when a list of positions meets the other format's text for a moment): clamp to `length` |
| **`PdfSearchModel` in QML** | Its `count` (6.8) and `status` (6.12) properties are C++ only: QML says "not available in QtQuick.Pdf". Count with `rowCount()` on `rowsInserted` / `rowsRemoved` / `modelReset`. Each page's highlights need a manual refresh (`boundingPolygonsOnPage()` on `currentPageBoundingPolygonsChanged`), as Qt's own views do |
| **An idle `PdfSearchModel` wakes the app** | Measured 2026-11-23 (Qt 6.12): with a `PdfSearchModel` on a document, the main thread woke ten times a second, also with an empty search and while the window was hidden; gone without the model. The model sits in a Loader active only while the find bar is open. Check idle cost by counting `voluntary_ctxt_switches` in `/proc/<pid>/task/*/status` over 10 s |
| **Hidden views keep running** | The preview window hides but its view stays loaded for 5 s (Main.qml `keepView`): animations, timers and players would go on unseen meanwhile (an animated GIF cost 1.6 % CPU after closing). Pause or stop them on `Window.window.visible`. Before 2026-11-26 the view stayed until the next file and kept its decoded picture (64 MB for a 4000×3000 photo on a 4K screen) |
| **`StandardKey` with several keys** | `sequence: StandardKey.FindPrevious` warns "Only binding to one of multiple key bindings" (a QML error in the tests); use `sequences: [StandardKey.FindPrevious]` |
| **Offscreen window grabs** | `grabWindow()` of the preview comes out blank while the rounded-corner mask layer is on; set a custom radius of 0 for the grab |
| **`left` / `right` as QML property names** | `Item` already has them (anchor lines): "Cannot override FINAL property" when the file loads; qmllint doesn't say so |
| **Overriding a control's `implicitWidth`** | `implicitWidth: implicitContentWidth + leftPadding + rightPadding` on a ToolButton gave width 0 (nothing to click). Set `width` for the special case and leave `implicitWidth` to the style |
| **Dragging inside a scrolling page** | A `DragHandler` in a FormCardPage needs `grabPermissions: CanTakeOverFromAnything \| TakeOverForbidden`, else a vertical drag scrolls the page |
| **A delegate's handler changing its own model** | The Repeater deletes the delegate while its handler still runs ("ReferenceError: … is not defined"). Keep what's needed in locals and change the model with `Qt.callLater` |
| **qmllint and `list<…>` properties** | qmllint (Qt 6.12) exits 255 without a message on a file with a top-level `property list<string>` (or `list<int>`); the file isn't checked at all. Use `var` arrays (the runtime behaves the same) |
| **Finding Repeater rows in tests** | `findChild` from the window doesn't reach items a Repeater made; walk `contentItem()->childItems()` instead |
| **Kirigami `Avatar` warning** | Kirigami 6.32's Avatar declares `sourceSize: { width: width; height: height }` (a JS block, not an object): "Unable to assign double to QSizeF" for every avatar. Set `sourceSize: Qt.size(width, height)` on each |
| **moc and string literals** | A raw string with `//` in it (`R"(<svg xmlns="http://…")"`) made moc emit an empty `.moc`: "undefined reference to vtable" at link time. Use ordinary escaped strings in test files |
| **Video in offscreen grabs** | `grabWindow()` offscreen shows no video frames (the player's surface isn't painted): check video looks in the nested session |
| **KIO's Trash in tests** | `KIO::trash()` of a file on another disk than home (here `/mnt/Storage`) goes to that disk's own trash, `<mount>/.Trash-1000`, the user's, whatever `XDG_DATA_HOME` or test mode say (it happened twice before this was found; the entries were removed). Tests never call it; code that trashes files takes a stand-in in tests |
| **Qt's Markdown reader and code blocks** | Checked 2026-11-24 (Qt 6.12): each line of a code block is its own paragraph; fenced ones carry `BlockCodeFence` + `BlockCodeLanguage`, indented ones only `BlockCodeLanguage` (empty); two fences in a row have nothing between them (a new language is the only sign). Quotes: `BlockQuoteLevel`, 40 px margin per level. Neither is drawn by Qt: shade and bar from QML |
| **Block margins collapse** | Where two blocks meet, `QTextDocument` lays out the larger of the bottom and top margins, not their sum: room for two padded boxes and a gap needs it in one margin |
| **KDE's `ItemDelegate` insets its background by the left padding** | `leftInset: leftPadding - smallSpacing` in org.kde.desktop: indenting a row with `leftPadding` moves its highlight along. Indent inside the `contentItem` and set the insets |
| **A still picture of a view before it changes** | `ShaderEffectSource { live: false }` + `scheduleUpdate()` captures in the next frame, after anything changed in between: the change has to wait for that frame (`QQuickWindow::frameSwapped`, queued, with a timer in case a covered window gets no frames) |
| **Audio covers in the nested session** | KIO only runs thumbnailers listed in `[PreviewSettings] Plugins`; the scratch config has none, so covers don't show. Put `Plugins=audiothumbnail,…` in the scratch `kdeglobals` |
| **Finding a view's ListView in tests** | A search by type (`inherits("QQuickListView")`) also finds the list inside a popup Menu, which goes away (crash). Give the list an `objectName` |
| **`pkill -f` in test scripts** | Matches any command line containing the text, including the shell running the test. Stop processes by the PIDs you started |
| **KWin undoes a move made in `windowAdded`** | Checked 2026-11-25 (KWin 6.8.3): setting `frameGeometry` (position only) when a new Wayland window appears is undone 0-50 ms later, back to KWin's own placement (its first configure, acked later, carries the placed position). Placing again on the first `frameGeometryChanged` that undoes it holds. A size change sends a new configure and sticks |
| **What KWin scripts can't read** | `readyForPainting` isn't exported (undefined; the window is already ready when `windowAdded` arrives); `noBorder` is false for Qt's frameless windows; every window of an app shares `desktopFileName` and `resourceClass`: tell windows apart by caption (`captionNormal` has Qt's “— KwikLook” suffix) |
| **Testing KWin scripts** | Only in the nested session: load with `gdbus call --session -d org.kde.KWin -o /Scripting -m org.kde.kwin.Scripting.loadScript <file> <name>`, then `…Scripting.start`; a script reports through `callDBus` to a stand-in service (Python dbus) since `print()` goes to KWin's log. A copied `kwinrc` brings the user's scripts along: Remember Window Positions moves windows back about 1 s after opening and keeps positions in `kde.org/kwin.conf` (QtCore `Settings`, under the scratch `XDG_CONFIG_HOME`) |
| **KWin script ids and D-Bus paths** | Seen 2026-11-26 on the live session (KWin 6.8.3): `loadScript` gave our reloaded script an id whose `/Scripting/Script<id>` was still held by an older object, so `run()` there answered fine but the new script never started (loaded, silent, did nothing; not reproduced in a nested session). Start with `/Scripting` `start()` (runs every loaded script not yet running) |
| **Remember Window Positions' blacklist** | Counts only in its “All except blacklisted apps” mode (`restoreType=2`); with “All apps” (`0`) it logs “on blacklist” and restores the window anyway, about 1 s after it opens |
| **KSyntaxHighlighting and replaced text** | Checked 2026-11-26 (KF 6.32, Qt 6.12): when a line's folding changes, the highlighter queues `rehighlightBlock(next line)`; if the document's text is replaced before it runs, the stale `QTextBlock` crashes in `QTextBlock::position()`. Changing the rules (`definition`) colours the old text again and queues such calls: change them only after the new text is in (TextView: `updateRules()` via `Qt.callLater` on `fileInfoChanged`) |
| **`Screen.devicePixelRatio` with fractional scaling** | Checked 2026-11-26 (Qt 6.12, Plasma at 125 %): QML's `Screen.devicePixelRatio` (the `QScreen`'s) said 2 while the window drew at 1.25 (`QWindow::devicePixelRatio()`, QML `Window.window.devicePixelRatio` since 6.11). Sizes in real pixels go through the window's value (ImageView's `dpr`, `Viewer::Placement`); the screen's is fine only for `sourceSize` oversampling. A nested session doesn't show the difference |
| **Hidden Qt windows spin in the nested session** | Checked 2026-11-26 (Qt 6.12.1, `kwin_wayland --virtual`): after its 4th-5th hide any Qt Quick window, a bare `qml6` one too, burns ~70 % CPU in its WaylandEventThread and Mesa `gl0` threads while hidden. Not seen on the real desktop (0 wake-ups after many previews). Idle numbers from the nested session are meaningless after a few cycles; measure idle on the real desktop. Checked again 2026-11-27: KwikLook's viewer (unchanged code, too) then freezes at its ~5th show: render thread in `eglSwapBuffers` (Mesa `dri_flush`), Mesa's `gl0` in a Wayland roundtrip, main thread in a synchronous expose waiting for the render thread; whatever file, also with nothing done on hide. Nested tests: at most 3 hides per session |
| **KJob ends the background viewer** | Found 2026-11-26 by `stress/switching.sh`: every KJob (KIO's thumbnail `PreviewJob`) holds a `QEventLoopLocker`; when the last one is released while no window is shown, Qt posts `QEvent::Quit` and `exec()` returns 0, with no message. `setQuitOnLastWindowClosed(false)` doesn't cover it: main.cpp also calls `QCoreApplication::setQuitLockEnabled(false)` |
| **Sanitizers and the sandbox** | Programs inside kwiklook-sandbox get an empty environment (ASAN_OPTIONS doesn't reach them) and no ptrace, which LeakSanitizer needs: “LeakSanitizer has encountered a fatal error”, exit 1. In the asan build `src/sanitizeroptions.cpp` sets `detect_leaks=0` in them |
| **Kirigami's CMake config with clang** | `find_package(KF6Kirigami)` wants OpenMP, which clang here lacks (no libomp): the fuzz preset points `OpenMP_*` at gcc's libgomp |
| **Debugging a quit under gdb** | In `gdb -batch`, breakpoints in libraries not loaded yet are refused unless `set breakpoint pending on`; `break QCoreApplication::exit` also matches `QThread::exit` (the player stops threads all the time): use exact signatures or a command file (`-x`), and `$rsi`-style conditions only there (the shell eats them) |
| **Qt Quick TextEdit loses rich text before a table** | Checked 2026-11-26 (Qt 6.12.1, also in a bare TextArea, nested session; offscreen didn't show it): rich text with an HTML `<table>` in the middle of a long document, scrolled past the table and back, shows the part before it blank (text and pictures). TextEdit only builds the part on screen (`ItemObservesViewport`); with that flag off it builds everything and the bug is gone (`Contents.drawWhole()`, rendered views only; the 1.4 MB test book: first frame 196 ms, scroll steps ≤ 52 ms). A Markdown table, or a table near the start or end, didn't trigger it |
| **Qt's Markdown reader and empty HTML elements** | Checked 2026-11-26 (Qt 6.12.1): `<img src="a.png">`, `<br>` or `<hr>` without the slash, in an HTML block or inline, makes `setMarkdown` drop all the text after it (headings left empty). `<img … />` and `![](…)` work. `Markdown::forQt()` adds the slash outside code; FileInfo keeps the result in `formattedText` for the rendered view and the contents (lines unchanged); the source view shows the file as written. `tst_markdown` says when Qt no longer needs it |
| **TextView switches: old text, new kind** | During a switch the view's `kind` can be empty (so `rendered` false) for a moment while `text` is still the last file's: anything bound to `!rendered` acts on the old text. The highlighter's Loader did, and coloured a 1.4 MB book for 18 s (2026-11-26). It now turns on only from `updateRules()` (`Qt.callLater` after the switch), off at once |
| **QTest::qWait and process output** | qWait sleeps between event rounds: a QProcess's large output (an 11 MB book from the helper) then arrives in pipe-sized pieces, 1.5 s instead of 20 ms. Time I/O-heavy paths in the real app (nested session), not inside QTRY loops |
| **KFileMetaData's EPUB extractor crashes** | Seen 2026-11-26 (KF 6.32): `kfilemetadata_epubextractor` (libepub, run in our process by `FileInfo::readDetails`) crashed in `FindNode()` on a valid book whose container.xml has no namespace. EPUBs skip KFileMetaData; BookReader gives title and author |
| **Qt drops empty anchors between blocks** | Checked 2026-11-26 (Qt 6.12): `<p>x</p><a name="y"></a><h1>U</h1>` loses the anchor (only one at the very start of the document survives); inside a block (`<h1><a name="y"></a>U</h1>`) it lands on the next character's `anchorNames()`. Write anchors just before the next text |
| **Qt's image cache and preloading** | `Image { cache: false }` never looks in the cache (QQuickPixmap::load skips it), so a picture decoded ahead elsewhere isn't found. The key is address + sourceSize + region + provider options (autoTransform, fillMode's aspect handling): preloader and view must match exactly. Unreferenced entries are dropped soon (small cost limit): the preloader keeps holding its picture until the view has taken it |
| **RAW photos and decode size** | Checked 2026-11-26 (kimageformats 6.32, LibRaw 0.22): `kimg_raw` can't decode to a requested size (`ScaledSize` unsupported): a 12-megapixel DNG takes ~620 ms in full. `QImageReader::setQuality()`: 0 = the embedded preview (672×504 here, 10 ms), 1-9 = half size (~360 ms), else full. Qt Quick's Image has no quality setting; the read-ahead hides the full decode instead |
| **Raw strings in test files** | Besides moc's trouble with `//` (above), a raw string with `)"` in it (`onclick="x()"`) ends early: use ordinary escaped strings |
| **Qt 6.12 crashes when an image load is cancelled at the wrong moment** | `QQuickPixmapReader::processJob` (6.12.0 and 6.12.1) checks `runningJob->data` once, then reads it again without the lock; a cancel from the main thread nulls it in between: SIGSEGV in thread “QQuickPixmapRea”, reading address 0x58. Qt PDF's pages (`PdfPageImage`, a “special device”) go that way. Every `source` / `sourceSize` change of a loading image is a cancel: keep them from changing in quick succession (PdfView renders pages at a width that settles for 150 ms). Still seen 2026-11-27: `tst_qmlsmoke` crashes this way in `pdfPagesStayCentredAndInside` about once in 25-50 runs; rerun before suspecting a change |
| **`KIO::PreviewJob` without a plugin list** | Checked 2026-11-27 (KF 6.32): with no list it reads kdeglobals `[PreviewSettings] Plugins`, usually unset (Dolphin keeps its list in `dolphinrc`), and then has no thumbnailer for audio covers or AppImage icons (“Could not get plugin” in `kf.kio.gui`); pictures still work, so it goes unnoticed. Pass a list. The audio thumbnailer only takes a picture marked as front cover (ffmpeg: `-metadata:s:v comment="Cover (front)"`) |
| **Sizing contents from a ScrollView's width** | With the scrollbar `AsNeeded`, contents whose height follows their width can loop: wide → taller than the view → scrollbar → narrower → fits → no scrollbar → wide… (the PDF thumbnails reloaded endlessly, which set off the crash above in every test run). Size from the width minus the scrollbar's width, shown or not |
| **glibc keeps freed picture buffers** | Checked 2026-11-27 (glibc 2.44): after a large block allocated with its own mapping is freed, glibc raises its mapping threshold (up to 32 MB), so later decoded pictures came from its arenas and stayed there: 100 MB kept after a pictures-only run, 55 MB with a fixed threshold. `malloc_trim(0)` and `arena_max` barely helped. main.cpp sets `mallopt(M_MMAP_THRESHOLD, 256 KB)` (fixed = no adjusting); CPU time unchanged. Freed memory in 64 MB-aligned anonymous blocks of smaps = glibc arenas. Measured again 2026-11-27 on a mixed session (pictures, PDF, text, office, video, audio, archive): ~20 MB sat in the arenas of threads (render thread, decoders, FFmpeg); `malloc_trim(0)` once the view is gone, with the QML engine's garbage collection and `trimComponentCache()`, took the idle viewer from 133 to 96 MB (`Viewer::releaseMemory()`) |
| **Environment variables don't reach apps in the nested session** | `kwin_wayland` has a file capability (secure mode), so glibc drops `GLIBC_TUNABLES`, `MALLOC_*`, `LD_*` for it and everything it starts. Set them inside the session's script, and check `/proc/<pid>/environ`. The nested session renders on the real GPU (radeonsi here), so its memory numbers are like the desktop's |
| **The nested session locks with the real one** | Seen 2026-11-27: started with `env -i`, the nested `kwin_wayland --virtual` attaches to the user's logind session; when the real screen locked (user away), the nested one showed its own lock screen, over every window: screenshots show it (wallpaper and clock), previews still answer over D-Bus. `loginctl show-session <id> -p LockedHint` says whether the real session is locked; don't trust screenshots then |
| **`dlclose()` of Dolphin's library** | Checked 2026-11-28 (Dolphin 26.08.3): a test that `dlopen`s `libdolphinprivate.so.6`, looks up symbols and `dlclose`s it again crashed in the next QObject event (its static objects unloaded under a running Qt app). Keep it loaded |
| **Trying the hook in the real Dolphin without a session** | Works offscreen (checked 2026-11-28, Dolphin 26.08.3): `env -i HOME=… XDG_*=… QT_QPA_PLATFORM=offscreen QT_PLUGIN_PATH=build/dev/plugins dbus-run-session --config-file=<bus.conf> -- <script>`, the script starting `dolphin --new-window --select <file>` and calling `/KwikLookBridge` `State` / `TriggerFocusedView` on `org.kde.dolphin-<pid>`; a Python stand-in viewer can answer `Open` and send `CurrentChanged`. The bus config: a `<listen>unix:dir=…</listen>` short enough (the scratchpad path is too long for a socket: “Socket name too long”) and no `<servicedir>`, so nothing installed is activated. The build plugin wins over an installed one of the same name |
| **D-Bus calls start the installed viewer** | In a test session whose bus can see `/usr/share/dbus-1/services`, a call to `io.github.strandzen.KwikLook` before the build's viewer has the name activates the installed one (the build's then exits: “Another KwikLook viewer is starting”). Wait with `NameHasOwner`, which activates nothing. heaptrack also opens its GUI when done (and waits): use `--record-only` |
