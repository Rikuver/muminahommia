Mumble 1.5 - modified build
============================

A modified build of the Mumble client (1.5.x, Windows x64) with these changes:

VOLUME SLIDER
- An output volume slider at the right end of the toolbar (next to
  Mute/Deafen/Record). It controls the same setting as
  Settings -> Audio Output -> Output volume (0-200 %).

CHAT TABS (at the bottom of the chat box)
- Server tab (named after the server, e.g. psygoa.org; "System" when not
  connected): joins, leaves, kicks, mutes, connection info, welcome
  message and all other non-chat messages.
- Chat tab: only the text messages people send in the channel.
- Private tabs: every private conversation gets its own tab, named after
  the person. Private messages are not shown in the Chat tab.
  * Shows an unread count, e.g. "Vike (2)", until you open the tab.
  * Type in the chat bar while a private tab is open to reply to that
    person. Start a new private chat: right-click a user -> Send Message.
  * Close a private tab with its X button or a middle-click (scroll wheel
    click). The other person gets a message that you closed the
    conversation.
- The server and Chat tabs can't be closed, but all tabs can be dragged.

CHAT FORMATTING
- Messages are shown as  [time] Nickname: message
  (no "To <channel>:", "(Channel)" or "(Private)" labels).
- The chat bar sends text exactly as typed: line breaks (Shift+Enter),
  spaces and characters like * _ # ` > are kept as they are (no Markdown).
  Links are still clickable.
- Messages from others are shown the same way: bold, italics, headings,
  colours and fonts are removed. Links and pasted images are kept.

HOW TO RUN
1. Extract this whole folder anywhere (e.g. Desktop or Documents).
   Keep all files together - mumble.exe loads the DLLs and the
   "plugins" folder from the same folder.
2. Double-click mumble.exe.
   Windows SmartScreen may warn "Windows protected your PC" because this
   build is not signed. Click "More info" -> "Run anyway".

IF IT SAYS MSVCP140.dll / VCRUNTIME140.dll IS MISSING
Install the Microsoft Visual C++ Redistributable (x64):
https://aka.ms/vs/17/release/vc_redist.x64.exe

GOOD TO KNOW
- If the normal Mumble is installed, this build uses the same settings,
  servers and certificate. Run only one of them at a time.
- Automatic update checks are turned off in this build.
- Private conversations are not saved; closing a tab or Mumble clears them.
- Mumble is open source (BSD license, see LICENSE.txt).
