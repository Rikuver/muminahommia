# Handoff: port Harri's tab design (origin/harri-build) onto main

## Current state (2026-11-24): merged into `main` (fast-forward), not pushed

Owner said "port it" and asked for a setting for the message changes. Commits on `port-harri-tabs` (after `caf93df69`):

| Commit | What |
|---|---|
| `cae01871f` | This doc + CLAUDE.md pointer |
| `1b58caac0` | Volume slider (`Qt::TabFocus`) |
| `e210ba445` | System / Chat tabs, per-tab dates, unread count, System counts only errors/warnings/denied |
| `ba6f8c8a0` | Movable tabs, middle-click close (local only, no close notice), chat bar PMs only from a private tab |
| `bc8c82d5b` | `Nickname: message` formatting (`ownChatName()`) |
| `ee179414e` | Customisation page "Chat formatting": `bChatSendAsTyped` (`chat_send_as_typed`), `bChatStripReceivedFormatting` (`chat_strip_received_formatting`), both default on. Helpers `Log::plainTextToHtml`, `Log::stripFormatting` |
| `6295afdce` | Ping label next to the chat bar |

Kept from main: session-keyed private tabs (`privateSession` param), `(offline)` label, ChatHistory (own PMs excluded), `updateLogSettings()`.
Not taken: Harri's close notice, `README_PACKAGE.txt`, his `build_windows*.bat` changes, his `dist/` packaging (parked).

Verified: full build + link OK, `change-reviewer` verdict "commit" (nits fixed in `ab8f8e4d4`), owner tested the build and said merge.
Next step: push `main` to `origin` when the owner says so. Not done: unit test for `Log::plainTextToHtml`/`stripFormatting`, `.ts` translations. Parked: harri's `dist/` packaging.

## Update 2026-11-24: 3 new commits, and cherry-pick is possible after all

`origin/harri-build` now has 4 commits on top of upstream `dfbac8d3d`:

| Commit | What | Take? |
|---|---|---|
| `4d3361a53` | Volume slider, tabs, formatting (section below) | Yes, with the must-fixes below |
| `ae08bfaae` | Chat bar sends text as typed: no Markdown, keeps line breaks/spaces, only links clickable (`chatTextToHtml`) | Yes |
| `1cadf0410` | Received messages shown as plain text (`plainChatHtml`: strips bold/colours/fonts/headings, keeps links + images); `build_windows.bat` copies a clean build to `dist/`; `README_PACKAGE.txt` | Code yes; build script + README no |
| `c9dfc952e` | Ping label right of the chat bar, updated on each real ping reply (`ServerHandler::pingUpdated`, atomic `m_lastUDPPing`/`m_lastTCPPing`) | Yes |

**Upstream drift: none.** Main's root commit `0e4780ef2` "Import upstream Mumble 1.5.x" is a snapshot of the same
upstream `dfbac8d3d`. `git diff 0e4780ef2 dfbac8d3d -- src` is empty. So harri's diffs apply to main's code base 1:1;
only main's own customisations conflict. The "no common ancestor" problem is only history, not content.

Dry run (`git merge-tree --merge-base=<c>~1 main <c>`), nothing touched:
- `4d3361a53`: conflicts in `Log.cpp` (5 hunks), `Log.h` (1), `MainWindow.cpp` (5), `build_windows.bat` (add/add).
- The other 3 conflict only because they sit on top of `4d3361a53`; expect them to apply nearly clean after it.

**Revised approach:** work on branch `port-harri-tabs` off `main`.
1. `4d3361a53`: manual port as in the plan below (it needs the most reshaping anyway), using `git show 4d3361a53 -- <file>` as reference.
2. `ae08bfaae`, `1cadf0410`, `c9dfc952e`: `git cherry-pick -n <c>`, resolve, apply the extra fixes below, commit each separately.

**Extra must-fixes for the new commits:**
8. `plainChatHtml` is declared by hand in `Messages.cpp` (`QString plainChatHtml(const QString &);`) and defined as a non-static free function in `MainWindow.cpp`. Put both helpers in one place with a header declaration.
9. `sendChatbarText(QString, bool plainText)` keeps an unused parameter (`Q_UNUSED`). Remove the parameter and update callers (no dead code).
10. Drop the `MUMBLE_PM_CLOSED_MARKER` context these commits touch (close notice is dropped, fix 1).
11. Don't take `README_PACKAGE.txt` (describes the close notice we drop) or harri's `build_windows.bat` changes.

**Behaviour changes to know about (owner decision, default = take as is):**
- Markdown in the chat bar is gone for everyone (also Ctrl+Enter path).
- Formatting from others (bots' tables, colours, headings) is stripped. Saved chat history stores the stripped version.
- Known small limits, accept: ping colours are hard-coded green/orange/red (text still shows ms); `isUsingUdp()` reads `bUdp` from the GUI thread, same unsynchronised pattern upstream already uses in `ServerHandler.cpp:306`.

## Earlier state (2026-11-23)

- Source: branch `origin/harri-build`, commit `4d3361a538038f977d29c6291001a731dcb7358a`
  ("FEAT(client): Volume slider, chat formatting, system/chat/private tabs").
- Goal (owner's decision): adopt this as **the** chat tab design on `main`, replacing main's current tab implementation.
- Reviewed by `change-reviewer`. Verdict: adopt after fixes, as a **manual port**. Nothing changed on `main` yet; nothing built.
- Status: **waiting for owner's OK on the defaults below** before starting.

## Why a manual port, not merge/cherry-pick

- `harri-build` has **no common git ancestor** with `main` (built from upstream `1.5.x`, likely from a ZIP).
  `git diff main origin/harri-build -- src` = 24 files, +539 / -1723.
- Main's own tab commits touch the same code: `22dd401c7` (private chat tabs), `b2ac22885` (always show tab bar),
  `ff1966a2a` (save chat to disk + Customisation page), `1ed392bb2` (choose save folder).
- Before porting, diff harri's upstream base (`4d3361a5^` = `dfbac8d3d`) against main's upstream base so upstream drift isn't ported by accident.
- To see harri's change alone: `git show 4d3361a5 -- src/`

## What the commit does

1. Toolbar output volume slider 0–200 % + "NNN %" label (red above 100 %), writes `Settings::fVolume`, synced with VolumeUp/Down shortcuts and config dialog.
2. Log as `QTabWidget` (tabs at bottom): **System** (everything except chat, labelled with server host), **Chat** (old `qteLog`), **one private tab per user** (key: cert hash, else `name:`), `(n)` unread counter, middle-click closes private tabs, movable tabs.
3. Chat bar: in a private tab it sends a PM to that user; a user selected in the user list no longer makes the chat bar a PM.
4. Formatting: `[time] Nickname: message` for own and others' channel/private messages (no `To <channel>:`, no `(Channel)`/`(Private)` prefix).
5. Closing a private tab sends the other person a PM `<!--mumble-pm-closed--><i>X closed the private conversation.</i>`.
6. `build_windows.bat`, `build_windows_low_load.bat`, `#include <QDebug>` in `GlobalShortcut_win.cpp`.

## Defaults proposed (pending owner OK)

**Take from harri:** System / Chat / private tab split (route non-chat types in `Log::log`), `(n)` unread counter, middle-click close,
per-view date tracking, volume slider, `Nickname: message` formatting, chat bar PMs only from a private tab.

**Keep from main:**
- Persistent chat history: `ChatHistory::append` (`src/mumble/Log.cpp:875`), `Log::restoreChatHistory()` (`Log.cpp:716`, called from `main.cpp`, writes into `mw->qteLog` = Chat tab), Customisation page (`CustomisationConfig.cpp`).
- `Log::log(..., unsigned int privateSession)` parameter (`Log.h:159`, `Log.cpp:773`) instead of harri's `m_targetLog` member hack / `logToBrowser`.
- `MainWindow::updateLogSettings()` (`MainWindow.cpp:3903`, called from `Log.cpp:318`, `Themes.cpp:103`); make sure it covers the new System tab.
- Offline handling: `endPrivateChat` and the `(offline)` tab label.
- The `mw` null guard in `Log::log`.
- Main's existing `build_windows*.bat` (do not take harri's; add/add conflict).

**Must-fix while porting:**
1. Drop the close notice entirely (closing is local-only). Reason: marker check `contains()` in `Messages.cpp` is spoofable and silently swallows any PM containing it; stock clients get a real PM + sound on every tab close.
2. Volume slider: `Qt::TabFocus`/`StrongFocus` instead of `Qt::NoFocus` (accessibility).
3. Own PMs must never reach `ChatHistory::append` (harri logs them as `Log::TextMessage`; use `privateSession`/`PrivateTextMessage`).
4. Escape user names in tab tooltips (`toHtmlEscaped()`).
5. System tab: unread counter only for errors/warnings (`CriticalError`, `Warning`, `PermissionDenied`), not for every join/leave.
6. No dead code: harri's `qwOutputVolumeSpacer` / `qaOutputVolumeSpacer` are unused; no duplicate `#define`.
7. Untranslated fallback `QStringLiteral("Me")` in `ownChatName()` → `tr()`.

Known limits to accept or fix: name-keyed tabs for users without a certificate can collide; same cert connected twice picks the first session.

## Plan (one small commit each, build + check after each)

1. Volume slider (+ focus fix).
2. System / Chat split in `Log::log` + System tab + unread rules.
3. Replace main's private tab UI with harri's behaviour (counter, middle-click, chat bar targeting), keeping `privateSession`, offline label, history exclusion.
4. `Nickname: message` formatting.
4b. Cherry-pick `ae08bfaae` (plain chat bar), `1cadf0410` (plain received messages, code only), `c9dfc952e` (ping label), one commit each, with fixes 8–11.
5. Verify: build with skill `build-mumble-windows`, check saved chat history and Customisation page still work, then run `change-reviewer` on the range.
6. Push to `origin` only when owner says so. Never PR upstream.

## Next step

Owner: reply "port it" (or change any default above, including the 2026-11-24 section). Then create branch
`port-harri-tabs` from `main` and start at plan step 1.
