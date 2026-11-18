# spikes/ - throwaway experiments

Small test programs that answer a design question before the real code is written. They are built
with the project (`KWIKLOOK_BUILD_SPIKES`, on by default), never installed, and deleted once their
decision is recorded in Todo.md.

## kwiklook-spike (milestone 1, step 1)

Answers two questions:

1. **Window frame:** which frame should the preview window use?
2. **Focus on Wayland:** does the preview get keyboard focus when another app (Dolphin) asks for it,
   and is a focus token needed for that?

It only opens its own windows. It changes no settings; variant B needs one setting that you add yourself
(see below) and can remove afterwards.

### Frame test

```sh
build/dev/bin/kwiklook-spike frame
```

Open the three variants from the window that appears and compare them with your other windows:
corners, outline, shadow, and how they feel to move (drag the toolbar). Space or Esc closes one.

| Variant | What draws the frame | Strips at the top |
|---|---|---|
| **A** Native frame + toolbar | KWin and your window decoration | Two: title bar, then our toolbar |
| **B** Breeze, title bar hidden | KWin and Breeze, with a Breeze override that hides the title bar for this app | One: our toolbar |
| **C** Frameless, own shadow | KwikLook itself: corners, outline and shadow copied from Breeze | One: our toolbar |

**B needs a Breeze override first** (without it, B looks like A): System Settings → Colours &
Themes → Window Decorations → Breeze (edit button) → Window-Specific Overrides → Add.
Regular expression: `KwikLook\.Spike\.NoTitle`, tick "Hide window title bar". Remove it afterwards
the same way.

Things to know when judging:

- **A** always matches, with any decoration and any setting, but has two strips.
- **B** matches exactly, but only with Breeze, and only after the override exists. KwikLook could
  add the override for the user (asking first) or explain how to add it.
- **C** works with any decoration, but only imitates Breeze: corner radius 4 px, outline colour and
  shadow were measured from a Breeze window and fitted. With another decoration, or other Breeze
  shadow/outline settings, it won't match. This is KDEGlance's approach done more carefully.

### Focus test

```sh
build/dev/bin/kwiklook-spike focus
```

A window stands in for Dolphin, and a background process stands in for KwikLook's viewer. Press
Space in the stand-in to ask the viewer to show its preview; press Space or Esc in the preview to
hide it again. The log in the stand-in says whether the preview got keyboard focus and whether focus
came back afterwards.

Try it several times with "Pass a focus token" on, then off. Also try clicking into another app
between attempts, and typing in the stand-in first. The preview window itself also says whether
it has focus.

### Results so far (nested session, 2026-11-18)

- Frame: B and C look the same in screenshots. C's shadow was fitted to Breeze's measured shadow
  (within 0.01 on all sides), its corner radius and outline measured from a Breeze window.
- Focus: in the nested session the preview got focus with and without a token, also with KWin's
  focus stealing prevention at "High". The nested session has had no real input, so this doesn't
  show whether the token matters; the live test does.

### Decision (live test, 2026-11-18)

- Frame: **C**, made to follow Breeze's settings and the colour scheme (see Todo.md, milestone 1).
- Focus: on the user's desktop the preview got focus with and without a token. The hook passes
  one anyway, for users with stricter focus stealing prevention.

The frame code (`applyShadow`) is the starting point for the real window in step 3; this spike
goes away once that is ported.
