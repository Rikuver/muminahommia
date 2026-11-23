# Investigation: persistent channel chat log + "Customisation" settings page

Status: investigation only, no code written. Date: 2026-11-23.

## 1. Where channel chat flows today

- Single choke point: `Log::log()` in `src/mumble/Log.cpp` (~line 715). Channel/tree chat arrives as `Log::TextMessage`
  with `privateSession == 0`, so `tlog == mw->qteLog` (the "Channel" tab). Received messages come from
  `MainWindow::msgTextMessage` (`Messages.cpp:1003`); sent ones come from 5 call sites in `MainWindow.cpp`
  (lines ~2253, 2260, 2595, 2598, 3280), all of which go through `Log::log` too.
- Only `Log::TextMessage` is "channel chat". `qteLog` also holds joins/leaves/errors etc., which should NOT be saved
  by default (maybe an option).
- The text is stored as HTML in the `QTextDocument` (`LogDocument`). `iMaxLogBlocks` (0 = unlimited) trims the
  live document via `setMaximumBlockCount` (`MainWindow.cpp:3899`).

## 2. Persistent log: recommended design

- **Hook:** at the end of the `(flags & Settings::LogConsole)` block in `Log::log`, if `mt == TextMessage`,
  `tlog == qteLog` and the setting is on, append one record. Nothing else needs touching.
- **Format:** JSON Lines file (`channel_chat_log.jsonl`) in `QStandardPaths::AppDataLocation` (same place
  `Database.cpp` / `Global.cpp` use). One line per message: `{"t": <epoch ms>, "server": "host:port", "html": "..."}`.
  Append-only = cheap, crash-safe, human-readable. Alternative: the existing SQLite DB (`Database.cpp`),
  but it needs schema migration code for no real gain.
- **Restore:** in `MainWindow` after `qteLog` is set up (`setupGui`, near line 463), read the last N lines and insert
  them into `qteLog` using the same frame/format code as `Log::log` (best: factor that insert block into a helper, so
  restore and live use one path). Insert a dim "[Earlier messages]" separator line after the restored history.
  Restored lines must pass through `Log::validHtml` (they are untrusted-ish data on disk; images are data-URLs).
- **Trim:** on startup, if the file has more than `limit` lines, rewrite it keeping the last `limit` (temp file + rename,
  like `Settings::save`). No trimming while running.
- **Images:** embedded images make HTML huge. Option "Don't store images" (replace `<img>` with `[image]`), default on.

### Settings (new fields in `Settings.h`, keys in `SettingsKeys.h`, mapped in `SettingsMacros.h` `UI_SETTINGS`)

| Field | Default | Meaning |
|---|---|---|
| `bPersistChatLog` | false | master switch |
| `iPersistChatLogMaxMessages` | 2000 | saved messages kept (0 = unlimited) |
| `iPersistChatLogRestoreCount` | 200 | how many to show on startup |
| `bPersistChatLogPerServer` | true | restore only messages of the current server |
| `bPersistChatLogStoreImages` | false | keep inline images |
| `bPersistChatLogIncludeEvents` | false | also save joins/leaves/etc. |
| (button) Clear saved chat log | | deletes the file |

`Settings` is JSON-serialised through the macros, so each field is one `PROCESS(...)` line + one key constant + one
member. No legacy `LOAD` entry needed (legacy INI is only for old configs).

Risks: privacy (chat on disk in plain text, so default off and say so in the UI); multiple Mumble instances writing the
same file (append of one line is fine in practice); log restore on connect vs on startup (startup is simpler, but the
history is not tied to the server unless `server` is filtered on restore).

## 3. "Customisation" settings submenu

- Settings dialog (`ConfigDialog.cpp`) is a **flat** list: each page is a `ConfigWidget` subclass registered with
  `static ConfigRegistrar r(priority, factory)`; pages sort by priority (Audio 1000, Look 1100, Shortcuts 1200,
  Network 1300, Log 4000, Plugins 5000, LCD 5900, Overlay 6000). `updateListView()` builds the left list from them.
- There is no real submenu/tree. Two options:
  1. **One page "Customisation"** (recommended): a new `ConfigWidget` (`CustomisationConfig.cpp/.h/.ui`),
     priority ~6100 (last), holding group boxes per feature: "Chat tabs" and "Persistent chat log" now, "Poke"
     later. Search in the dialog (`qleSearch`) already indexes labels in the page, so new options stay findable.
  2. A real submenu (tree in the left list): requires reworking `ConfigDialog.ui` + `updateListView`
     (`QListWidget` -> `QTreeWidget`) and the search/tab-order code. Not worth it for one page.
- Files to touch for option 1: new `.cpp/.h/.ui` in `src/mumble/` (check `src/mumble/CMakeLists.txt` source list /
  glob), plus `Settings.h`, `SettingsKeys.h`, `SettingsMacros.h`. Use `Log.ui`/`LogConfig` as the template
  (`load(const Settings&)` and `save()` copy values both ways; `accept()` applies live).
- Settings that already belong here: none of the existing customisations has a setting yet (the tab bar is always
  visible, tabs are unconditional). Candidate toggles: show tab bar always, "(offline)" tab marking, unread marker.

## 4. Suggested order

1. Customisation page + persistent log (one change): settings fields, page, write hook, restore, trim.
2. Manual test: send/receive, restart, limit trim, per-server filter, images off, clear button.

Parked: the date-marker logic (`qdDate` in `Log::log`) must be kept consistent with restored history (otherwise a
"[Date changed]" line appears right after restore).

## 5. Implemented (2026-11-23)

Current state: code written and compiles; NOT yet run (Mumble was open, so the link step was skipped).
Next step: close Mumble, run `build_windows.bat`, go through the checklist below.

- `src/mumble/ChatHistory.{h,cpp}`: JSONL storage, file `muminahommia_channel_chat_history.jsonl` in the app data folder.
- `src/mumble/CustomisationConfig.{h,cpp,ui}`: new last page "Customisation" in Settings.
- `Log::restoreChatHistory()` (called in `main.cpp` before deferred logs) and the write hook in `Log::log`.
- Settings keys (`ui` group): `chat_history_enabled`, `chat_history_max_messages`, `chat_history_restore_count`,
  `chat_history_store_images`.

Differences from the plan above: no "per server" option (history of all servers is mixed, one file) and no "include
events" option. Only `Log::TextMessage` on the Channel tab is saved (not private chats, not join/leave events).

### Test checklist

1. Settings > Customisation: tick "Save channel chat to disk", Apply. Send and receive channel messages.
2. Restart Mumble: the messages are back, followed by "[Saved chat history ends here]". Date marker appears if the day differs.
3. Set max saved to 5, restart: file keeps only the last 5 lines.
4. Set "Messages shown at startup" to 2: only 2 shown.
5. Send an image: stored as "[image]" unless "Also save inline images" is ticked.
6. "Delete saved chat now" removes the file immediately.
7. Feature off (default): no file is written, nothing restored.
