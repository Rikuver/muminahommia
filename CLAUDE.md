# CLAUDE.md

## Guidelines

Before writing code, stop at the first rung that holds:

1. **Does this need to exist?** No: skip it.
2. **Already in this codebase?** Reuse it.
3. **Does the standard library do it?** Use it (C++ std, Qt).
4. **A native platform feature?** Use it.
5. **An installed dependency?** Use it (check `3rdparty/` and existing submodules first).
6. **Can it be one line?** One line.
7. **Only then: the minimum that works**, plus one small test if it has logic.

Be lazy about the solution, never about reading the code first or about finishing the change.

- **Before writing:** read the code the change touches and trace the real flow. List every place the change must reach (callers, tests, config, `.ui` files, translations) and what it could break.
- **Bug fixes:** grep every caller first, then fix the root cause once in shared code.
- **No extras:** no unrequested features or abstractions. Config options only where they help the user (see Customisation settings below). Keep the diff as short as possible.
- **Moving code:** keep its existing error handling and validation.
- **Comments:** only the "why", in one line.
- **Deliberate shortcuts:** add a `// shortcut: <the limit>, <when to upgrade>` comment.
- **Never cut:** trust-boundary validation (network packets, server messages), data-loss handling, security, accessibility, and anything the user asked for.
- **End every reply** with one or two lines naming what was skipped or not checked, and any risk.

## Project notes

- This is a personal fork of Mumble. Push customisations to `origin` (Rikuver/muminahommia) only; never open PRs upstream.
- **Customisation settings:** every setting a customisation adds goes on the Customisation page in the settings dialog, not on the stock Mumble pages. When a customisation would benefit from user control (on/off, a limit, a path), add a setting for it there; keep defaults sensible so it works untouched.
- The local build environment has Qt 5 only. Upstream master needs Qt 6, so test features on the 1.5.x line.
- Subagents: `mumble-scout` (read-only lookups, Haiku) instead of general-purpose/Explore; `change-reviewer` (read-only review, Sonnet) before committing.
- Building and running: skill `build-mumble-windows` (`.claude/skills/build-mumble-windows/SKILL.md`).
- Design notes: `docs/muminahommia_poke_and_chat_tabs_design.md`.
- Persistent chat log and Customisation settings page: `docs/muminahommia_persistent_chat_log_and_customisation_settings_investigation.md`.
- Pasting an image into the chat bar without sending it (investigation): `docs/muminahommia_paste_image_into_chat_bar_without_autosend_investigation.md`.
- Input lag while gaming (Windows audit, ranked fixes): `docs/muminahommia_gaming_input_lag_audit_shortcuts_overlay_positional_audio.md`.
- Porting Harri's tab design (`origin/harri-build`) onto main, current state and plan: `docs/muminahommia_harri_build_tab_design_port_handoff.md`.
