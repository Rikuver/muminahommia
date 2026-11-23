# Poke + chat tabs: design notes (muminahommia fork)

## Current state (2026-11-22)

- **Chat tabs: code written and builds cleanly (2026-11-22). Not tested in use yet, not committed.**
  Next step: run `build\mumble.exe`, test with the checklist below (needs a second person or a second client).
- **Poke: not started.** Recommended (not yet confirmed by Riku): the "[Poke] text-message marker"
  variant instead of PluginDataTransmission, so stock Mumble users get it as a normal private message
  (see section 1, "Stock-compatible variant").

### What was built for chat tabs (minimum version)

- Log dock holds a `QTabWidget` (`MainWindow::qtwLogTabs`, set up in `MainWindow::setupGui`).
  Tab "Channel" = old `qteLog` (all events + channel chat), cannot be closed.
  Tab bar is always visible, so the Channel tab shows even with no private conversations.
- `Log::log(..., unsigned int privateSession = 0)`: non-zero puts the line into that user's tab
  (`MainWindow::privateLog()` creates it). Used for received PMs (`Messages.cpp` msgTextMessage)
  and sent PMs (chat bar and "Send message" dialog).
- Chat bar sends to the active private tab's user; on the Channel tab it behaves as before.
  Sending a PM from the tree selection or dialog switches to that user's tab.
- Unread tab gets ` *`; notification (flash/balloon) also fires when the message lands in a
  background tab while Mumble is focused.
- User leaves or we disconnect: tab becomes "Name (offline)", chat bar refuses to send and keeps the typed text.
  Tabs are keyed by session, so if the person reconnects their next message opens a NEW tab.
- Right-click menu, link clicks, link tooltips, "Save Image As", theme and max-log-length settings
  work in all tabs.

### Test checklist

1. Before any PM: only the "Channel" tab is shown (no close button), log works as before.
2. Receive a PM: "Name *" tab appears in background, window flashes/notifies, sound plays.
3. Click the tab: `*` goes away, chat bar says "Type message to user 'Name' here", Enter sends to them.
4. Channel tab: chat bar sends to the channel again.
5. Other user disconnects: tab shows "(offline)", Enter does nothing, text stays.
6. Close a private tab with its X; Channel tab has no X.
7. Right-click in a private tab: menu + Clear work. Image in a PM: Save Image As works.

Both features are client-only. No server change, works on stock Mumble servers >= 1.4.
Both are Qt 5 compatible, so they can be built and tested with the local Qt 5 env.

---

## 1. Poke (like TeamSpeak 3)

### How it travels: PluginDataTransmission, no server change

Mumble already has a "send arbitrary bytes to chosen users" message, used by plugins:
`message PluginDataTransmission` in `src/Mumble.proto:611`.

- Server just relays it: `src/murmur/Messages.cpp:2410` (`Server::msgPluginDataTransmission`).
  It sets the real sender session (no spoofing), has its own rate limit bucket
  (`m_pluginMessageBucket`), caps data at 1000 bytes, dataID at 100 chars (`src/MumbleConstants.h:13`).
- Client receives it in `src/mumble/Messages.cpp:1286` (`MainWindow::msgPluginDataTransmission`)
  and hands it to the plugin manager.

Plan: use `dataID = "muminahommia.poke"`, `data = UTF-8 poke text` (empty allowed).
Intercept that dataID in `msgPluginDataTransmission` before the plugin-manager call.

Trade-off: people on stock Mumble get nothing (their client passes it to plugins, nobody handles it).
Fine if everyone in the group uses this fork. If not, a fallback is to also send a normal
private text message ("pokes you: ..."), but then fork users would see it twice unless filtered.
Recommendation: plugin data only. (Superseded, see next section.)

### Stock-compatible variant (recommended)

Send the poke as a normal private text message starting with a marker, e.g. `[Poke] wake up`:
- Send: `Global::get().sh->sendUserTextMessage(session, "[Poke] " + text)`.
- Receive: in `MainWindow::msgTextMessage` (`Messages.cpp:1001`), if the message is private and its plain
  text starts with `[Poke]`, show the popup + alert and log it as `Log::Poke`.
- Stock Mumble users see a normal private message. Anyone can fake one by typing `[Poke]`, which is harmless.

### Sending

- `src/mumble/MainWindow.ui`: add action `qaUserPoke` ("Poke...") next to `qaUserTextMessage` (line ~416).
- `src/mumble/MainWindow.cpp:1744`: add it to the user context menu (`qmUser->addAction(qaUserPoke)`),
  enable/disable alongside `qaUserTextMessage` (lines ~1790/1801).
- `on_qaUserPoke_triggered()`: `QInputDialog::getText` for the message (trim to ~900 bytes),
  build `MumbleProto::PluginDataTransmission` like `src/mumble/API_v_1_x_x.cpp:1553`
  and send with `Global::get().sh->sendMessage(mpdt)`. Log locally: "You poked X: msg".
- Optional: a global shortcut "Poke selected user" later.

### Receiving

In `msgPluginDataTransmission`, when dataID matches:
1. Drop it if `sender->bLocalIgnore` (same rule as text messages, `Messages.cpp:1006`).
2. Log it with a new message type `Log::Poke`. That gives sound / notification / TTS / highlight
   settings for free in Settings > Messages.
   - `src/mumble/Log.h:78`: add `Poke` at the END of `enum MsgType` (after `PluginMessage`;
     settings are stored by number, so never insert in the middle).
   - Add it to `Log::msgOrder[]` (`Log.cpp:431`), update `lastMsgType` (`Log.h:115`),
     add a name in `Log::msgName()` and a default sound/flags in `Settings.cpp` (search `qmMessages`).
3. Pop up a non-modal dialog like TS3: "**X** pokes you: msg", with a "Reply" button that opens
   the private chat (or tab, once tabs exist). Raise + `QApplication::alert(mw)` even if Mumble
   is in focus.
4. Client-side flood guard: ignore pokes from the same user within ~2 s.

Estimated size: ~150 lines across Messages.cpp, MainWindow.cpp/.h/.ui, Log.cpp/.h, Settings.cpp.

---

## 2. Chat tabs (channel tab + one tab per private conversation)

### Today

- One log view: `qteLog` (`LogTextBrowser`) inside dock `qdwLog` (`MainWindow.ui:98-114`).
- Every message goes through `Log::log()` (`src/mumble/Log.cpp:715`), which writes
  straight into `Global::get().mw->qteLog` (lines 741-814).
- Chat bar target is decided in `MainWindow::sendChatbarMessage` (`MainWindow.cpp:2195`):
  the user/channel selected in the tree (if "use selection" setting is on), else your channel.
- Private message received: `MainWindow::msgTextMessage` (`Messages.cpp:1001`); private when
  `msg.session_size() > 0`, the other person is the actor (`pSrc`).
- Private message sent: `sendChatbarMessage` and `openTextMessageDialog` (`MainWindow.cpp:2056`).
  The server does not echo your own messages back; the client logs them itself (`ownMessage = true`).

### Design

Replace `qteLog` in the dock with a `QTabWidget` (`qtwChatTabs`):

- **Tab 0 "Channel"**: the existing `qteLog`, unchanged. Keeps all server events (joins, mutes...)
  plus channel/tree messages. Cannot be closed.
- **Private tabs**: one `LogTextBrowser` per person, created on first message in or out,
  closable. Title = user name, bold + `*` when unread.

Code changes:

1. `Log::log()` gets an optional target: `unsigned int privateSession = 0`.
   Move the "insert into text browser" block (Log.cpp:741-814) into
   `appendToBrowser(LogTextBrowser *, ...)`. `qdDate` (date-changed marker) becomes per-browser.
   `logOrDefer` + the queued `invokeMethod` in `log()` need the extra argument too.
2. `MainWindow` gets `LogTextBrowser *privateChatBrowser(ClientUser *, bool create)` and a
   `QHash<key, LogTextBrowser*>`.
   Key: user hash (`ClientUser::qsHash`, the certificate hash) so a tab survives reconnects;
   fall back to session for users without a certificate.
3. Callers that pass the target:
   - `msgTextMessage` private branch -> `pSrc->uiSession`.
   - `sendChatbarMessage` user branch and `openTextMessageDialog` -> `p->uiSession`.
   - Poke log lines -> sender's tab too (nice with the "Reply" button).
4. Chat bar follows the active tab:
   private tab -> `sendUserTextMessage(thatUser)`; Channel tab -> today's behaviour.
   Hook `QTabWidget::currentChanged` -> `updateChatBar()` (`MainWindow.cpp:3816`) so the
   placeholder says "Type message to user 'X'".
5. User leaves: append "X left the server" in their tab, disable sending (grey chat bar).
   User comes back with same hash: re-enable.
6. Things that assume a single `qteLog` and must loop over all browsers:
   - setup in `MainWindow.cpp:443-450` (LogDocument, frame style, stylesheet, max blocks) -> helper
     `setupLogBrowser(LogTextBrowser *)`.
   - `Log.cpp:317` max block count, `Themes.cpp:103` stylesheet.
   - Context menu / link click / hover slots are auto-connected by name
     (`on_qteLog_customContextMenuRequested`, `on_qteLog_anchorClicked`, `on_qteLog_highlighted`,
     `MainWindow.cpp:1031, 3879, 3895`): connect new browsers manually and use `sender()`
     instead of `qteLog` inside them.
   - Notification check in `Log::log` (`qdwLog->isVisible()`): also require the target tab to be
     the current tab, otherwise a message in a background tab would not notify.
7. Nice to have: double-click a user in the tree / "Send message" menu item opens their tab and
   focuses the chat bar instead of the modal `TextMessage` dialog.

Not doing: saving tabs or history to disk (Mumble never stores chat logs).

Estimated size: ~400 lines, mostly MainWindow.cpp and Log.cpp.

---

## Order of work

1. Poke (half a day): enum + settings, send action, receive + popup.
2. Chat tabs step A: refactor `Log::log` to write into any browser (no visible change, test that nothing broke).
3. Chat tabs step B: tab widget + private tabs + chat bar follows tab.
4. Chat tabs step C: offline handling, unread markers, notification fix, Poke "Reply" -> tab.
