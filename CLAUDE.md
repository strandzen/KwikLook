# CLAUDE.md

This file is the authoritative reference for working on KwikLook. Keep it current: when
something here turns out wrong or a new pitfall is found, update it in the same change.

## Project Overview

**KwikLook** is <one sentence: what it does, for whom, on KDE Plasma 6>.

**Goal:** <the problem it solves; what "done" looks like>.
**Philosophy:** Simplicity and native Plasma behaviour. Every feature must justify its existence.
**Stack:** C++20, Qt 6, KDE Frameworks 6, Kirigami (+ Kirigami Addons if used), QML, CMake with
ECM. Project version `0.1.0` (single source: `project(... VERSION ...)` in `CMakeLists.txt`).
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
Packaging: <where packages live and what to update>.

---

## Repository Layout

| Path | Contents |
|------|----------|
| `src/core/` | Pure C++ logic, no Qt Quick |
| `src/gui/` | Qt Quick backend classes (models, windows) |
| `src/dolphin/` | The Dolphin hook plugin (runs inside Dolphin; Qt Widgets, no QML) |
| `qml/` | All QML files (top level, wired in with `qt_add_qml_module`) |
| `tests/` | Unit tests (`tst_*.cpp`), QML smoke tests; `tests/dolphin/` the hook against KDEGlance's mock Dolphin; `run-with-bus.sh` runs a test on a private bus |
| `data/` | Desktop file, metainfo, icons, notifyrc, D-Bus service files |
| `po/` | `Messages.sh` and the `.pot`; reviewed translations in `po/<lang>/` |
| `comp/` | Reference projects (read-only, gitignored) |

## Architecture

**Targets:** `kwiklook` (the app), `kwiklook_qml` (static QML module `kwiklook`: C++ types + `qml/`),
`kwiklook_dolphinhook` (plugin Dolphin loads into its own process, `kf6/overlayicon`), tests in `tests/`.

**One background process.** `kwiklook` owns `io.github.strandzen.KwikLook` on the session bus and
keeps running when its window closes, so the next preview opens instantly.

| Command | What happens |
|---|---|
| `kwiklook <files>` | Viewer running: hands the files over (`Open`) and exits. Otherwise becomes the viewer |
| `kwiklook --service` | Becomes the viewer without showing anything; what D-Bus activation runs |
| `kwiklook --quit` | Asks a running viewer to quit |
| `kwiklook --settings` | Viewer running: asks it to show the settings window (`ShowSettings`). Otherwise becomes the viewer and shows it |

**D-Bus interface** `io.github.strandzen.KwikLook.Viewer` at `/Viewer`, described in
`data/io.github.strandzen.KwikLook.Viewer.xml` (hand-written; keep it in step with the
`Q_SCRIPTABLE` members of `Viewer`). `Open(as urls, i index, s sessionId, s activationToken)`,
`Close()`, `Next()`, `Previous()`, `Quit()`, `ShowSettings(s activationToken)`; signals `CurrentChanged(s sessionId, s url, i index)`,
`Closed(s sessionId)`. Session IDs are the caller's (one per Dolphin window); a new session's
`Open` sends `Closed` to the previous one.

| Class | Files | Role |
|---|---|---|
| `Viewer` | `src/gui/viewer.*` | File list, current file and its details (`view`, `mimeType`, `details`, `imageSize`, `infoSections` for the panel, read off the main thread with a generation counter), `infoOpen` (kept in `kwiklookstaterc`), the preview window; the D-Bus object; QML singleton `Viewer` |
| `WindowFrame` | `src/gui/windowframe.*` | QML element: compositor shadow (`KWindowShadow`) for the frameless window, corner radius (Breeze's or the setting's) and outline flag; watches breezerc (`KDirWatch`), recreates the shadow on each new surface |
| `KwikLookSettings` | `src/gui/kwiklooksettings.kcfg(c)` | KConfig XT, generated; `kwiklookrc` [General]: MatchBreezeCorners, CornerRadius, OpenKey, CloseOnFocusLoss. QML singleton. main.cpp sets the Notify write flag on every item (no kcfg option for it), so the Dolphin hook's `KConfigWatcher` hears about saves |
| `OpenKey` | `src/core/openkey.h` | The open key choices (Space, Shift+Space, Ctrl+Space) and parsing; shared by the viewer and the Dolphin hook |
| `Autostart` | `src/gui/autostart.*` | QML element: “Start at login” = `~/.config/autostart/io.github.strandzen.KwikLook.desktop` (`kwiklook --service`); the file is the setting |
| `BreezeStyle` | `src/core/breezestyle.*` | Reads breezerc `[Common]` (absolute path only); the fitted shadow layers per size |
| `FileMenus` | `src/gui/filemenus.*` | QML element: “Open with…” (`KFileItemActions::associatedApplications` + KIO's dialog) and Share (`Purpose::Menu`) as QMenus below a button; “Show in Dolphin” (`KIO::highlightInFileManager` with a token), “Copy Path” |
| `renderShadow()` | `src/core/shadowimage.*` | Shadow nine-patch image + tile rects from layers, radius, colour |
| `FileInfo`, `viewFor()` | `src/core/fileinfo.*` | A file's type (by content), icon, size, dates, owner, permissions, picture size, KFileMetaData details, the start of text files (`readTextStart()`, 256 KB) (`read()` may block: Viewer runs it with QtConcurrent); `viewFor()` picks the QML view: PDF → `PdfView`, what QImageReader reads → `ImageView`, types inheriting text/plain → `TextView`, else `FallbackView` |
| `LineNumbers` | `src/gui/linenumbers.*` | QML element: line numbers for a TextArea (block positions from its QTextDocument layout, wrapped lines included, visible part only) |
| `ThumbnailProvider` | `src/gui/thumbnailprovider.*` | `image://thumbnail/<mime>/<url>` from `KIO::PreviewJob` (job on the main thread); 1×1 transparent image = no thumbnail |

| QML file | Role |
|---|---|
| `qml/Main.qml` | The preview window: frameless, content clipped to the corner radius (`MultiEffect` mask), Breeze outline, keys; a `Loader` showing `Viewer.view` |
| `qml/FallbackView.qml` | Any file: thumbnail or type icon, name, one dimmed line (type · size · date) |
| `qml/ImageView.qml` | Pictures: fit the area, never enlarged past real size; decoded at most at screen size; fallback when undecodable |
| `qml/TextView.qml` | Text and code: read-only TextArea, KSyntaxHighlighting (dark/light theme by background), monospace, wrap, line numbers; note when only the start is shown |
| `qml/PdfView.qml` | PDFs: `PdfPageImage` pages in a ListView inside KDE's ScrollView, fitted to the width |
| `qml/InfoPanel.qml` | The (i) panel: sections from `Viewer.infoSections` (File, Details, Permissions), Show in Dolphin, Copy Path |
| `qml/TitleBar.qml` | The only bar: ← →, file name (centred), (i), Share, Open with…, close; a standard toolbar (Kirigami header size); Header colours; drag moves the window |
| `qml/ResizeHandles.qml` | Edge and corner strips calling `startSystemResize()` (frameless windows get no KWin borders) |
| `qml/SettingsWindow.qml` | “KwikLook Settings”: Kirigami Addons form cards, OK / Apply / Cancel (values held in the window until applied); made by main.cpp on the first `ShowSettings` |

```
Dolphin hook / kwiklook <files> / dbus-send
  └─ D-Bus Open(urls, index, session, token) ─→ Viewer ─→ window shown, token → focus
                                                  ├─ CurrentChanged(session, url, index)
                                                  └─ Closed(session)
```

**Dolphin hook** (`src/dolphin/`, based on KDEGlance's; ported files keep its SPDX line). Dolphin
loads every `kf6/overlayicon` plugin; ours adds no icons and only creates a `Bridge` once per
Dolphin process. `DolphinAccess` reads the view's files in display order and the selection, and
selects a file, through libdolphinprivate functions looked up by mangled name (`dlsym`), checked
on load. Without them, Space previews the selected file alone. Space is taken only when the focus
is in a file view with a file selected and Dolphin is not in type-ahead or selection mode. The
bridge starts the viewer when Dolphin starts (`StartServiceByName`), asks for a Wayland activation
token, then calls `Open` with a session ID `<bus name>/<n>`; `CurrentChanged` moves Dolphin's
selection, `Closed` gives the focus back. The open key comes from `kwiklookrc` and is re-read when the settings window saves (`KConfigWatcher`). Diagnostics on Dolphin's bus name at `/KwikLookBridge`:
`State()` (JSON, includes `openKey`), `TriggerFocusedView()` (what Space does). `KWIKLOOK_DISABLE_DOLPHIN_HOOK=1 dolphin`
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
4. ← → (keys and buttons) switch files; the buttons grey out at the ends.
5. Space or Esc closes the preview; `kwiklook <file>` again opens it at once (viewer still running).
6. System Settings → Window Decorations → Breeze: change shadow size, outline or rounded corners; an open preview follows within a moment.
7. `kwiklook --quit` stops the background viewer.
8. Dolphin (after installing, or `QT_PLUGIN_PATH=build/dev/plugins dolphin` with no other Dolphin running): Space on a file opens the preview, ← → move Dolphin's selection along, Space/Esc closes and Dolphin has the focus again. Space still works in the location bar, while renaming, in type-ahead search and in selection mode; on a folder it does what Dolphin does.
9. The bar's buttons are normal toolbar size, easy to hit.
10. Open with…: lists the file's apps; one opens the file and the preview closes; “Other Application…” shows KDE's chooser.
11. A picture, a text file, a folder and an unknown file each show a thumbnail or icon with type, size and date.
12. A large picture fills the preview (no name below it); a small one stays at its real size. (i) opens the panel at the right and the picture shrinks beside it; it stays open for the next file and after a restart. Show in Dolphin opens the folder with the file selected; Copy Path puts the path on the clipboard.
13. A code file and a Markdown file show as coloured text with line numbers; a log over 256 KB shows its start and says so; a PDF shows all pages fitted to the width. ← → and Space still work after clicking into the text.
14. Share: KDE's share menu opens below the button; sharing to a device or by mail works.
15. “KwikLook Settings” from the launcher opens the settings window (also while a preview is open, which stays open). Apply is grey until something changes; Cancel drops changes.
16. Custom corner radius: Apply, and an open preview's corners and shadow follow at once; back to “like Breeze” restores them.
17. Open key Shift+Space: in Dolphin (already open) Space selects as usual and Shift+Space opens the preview; Shift+Space or Esc closes it.
18. With “close when you switch” on, clicking Dolphin or another window closes the preview; “Open with… → Other Application…” and Share dialogs don't.
19. “Start in the background at login” on: the entry shows in System Settings → Autostart; after a new login the viewer is running before Dolphin opens.

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
| **`PdfDocument` with an empty source** | Logs “Cannot open:”. Keep the last source (`Binding { when: …; restoreMode: Binding.RestoreNone }`) instead of clearing it |
| **An id named like the property it feeds** | `PdfPageImage { document: document }` binds the property to itself (blank pages). Name the id differently (`pdf`) |
| **Large text in a QML TextArea** | Measured 2026-11-18: 1 MB plain text took 0.2 s to lay out, 0.46 s with KSyntaxHighlighting, on the main thread; 256 KB 0.05 / 0.11 s. Hence the 256 KB limit |
| **Runtime dependency: Qt PDF** | `QtQuick.Pdf` ships with Qt WebEngine on Arch (`qt6-webengine`); list it in packaging |
| **KConfig XT and `KConfigWatcher`** | Other processes only hear about a save written with `KConfig::Notify`; the kcfgc has no option for it: call `setWriteFlags(KConfigBase::Notify)` on each item. `kconfig_add_kcfg_files` on a target without `GENERATE_MOC` fails with "moc_….cpp: No such file" |
| **Nested session reads the real `kwinrc`** | `kwin_wayland` uses the caller's `XDG_CONFIG_HOME`: run it with a scratch config holding a `kwinrc` copy with KWin scripts off (`rememberwindowpositions` resized the settings window to the preview's remembered size, and applies the scale) |
| **No log output from a nested-session app** | Without a terminal Qt logs to the journal; set `QT_FORCE_STDERR_LOGGING=1` to get it in a file |
| **Focus leaving the app (Wayland)** | `QGuiApplication::focusWindow()` becomes null when another app takes the focus; own menus, dialogs and windows keep it non-null. It can pass through null briefly between own windows: check again after a short delay (150 ms) |
| **`pkill -f` in test scripts** | Matches any command line containing the text, including the shell running the test. Stop processes by the PIDs you started |
