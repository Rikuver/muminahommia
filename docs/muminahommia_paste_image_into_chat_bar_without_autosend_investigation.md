# Paste an image into the chat bar without sending it

Status (2026-11-24): done, tested by hand, committed on main ("FEAT(client): Pasted images wait in the chat bar until Enter").

## Behaviour

Pasting or dropping an image into the chat bar puts it there as a thumbnail (max 4 text lines high) at the cursor. Enter sends the text and the images together, each image where it was placed. Ctrl+Shift+V (Paste and Send) still sends at once.

## How it works

- `ChatbarTextEdit::insertPastedImage` (`src/mumble/CustomElements.cpp`): `Log::imageToImg` as before, then `insertHtml` of the `data:` `<img>`, sized by `fitImagesToWidth`. The `data:` URI is the image name, so no resource map and nothing to clear.
- `ChatbarTextEdit::textWithImages`: walks the document fragments. Each `data:` image gives one U+FFFC in the text and its `<img>` in a list. Stray U+FFFC in text is removed, remote images from pasted rich text are dropped (as `toPlainText` did before).
- `MainWindow::sendChatbarText`: takes the text from `textWithImages` (after the departed-user return), converts it as before (Markdown / plain / send-as-typed), then swaps each U+FFFC for its `<img>`. If the result is longer than `uiImageLength`, it logs "Unable to send message: the images are too large together." and keeps the bar content.
- `on_qteChat_tabPressed`: deletes only the leading `@` so images stay.
- `addToHistory`: strips U+FFFC; history keeps text only.
- Removed: `pastedImage` signal and its connect.
- `fitImagesToWidth` (`CustomElements.cpp`): shrinks images to the widget width (never enlarges), marks the sizes it set so they grow back when the window widens. Chat bar: width and 4 lines high, on paste and resize. Log (all tabs, hooked in `MainWindow::setupLogBrowser`): width only, after each change and on resize. Applies to images from other users too.
- In the sent message each image is on its own line (`textWithImages` adds the line breaks), so text is not bottom-aligned beside a large image.

## Decisions

- No Customisation setting: Paste and Send already gives the old "send at once" path.
- Data `<img>` name instead of `addResource` map (from Opus review): survives undo, `clear()`, internal copy and paste.

## Known limits

- Browsing history (Ctrl+Up) and back loses images in the draft (draft is saved as plain text).
- An image inside a Markdown link `[x](image)` breaks the link html.
- Dropped images go in at the text cursor, not at the drop point.

## To test by hand

1. Paste a screenshot, type text before and after, Enter: one message, image in place.
2. Paste the same image twice in a row: two images arrive.
3. Paste two large screenshots: "too large together" message, bar content kept.
4. Ctrl+Shift+V with an image on the clipboard: sends at once.
5. Drop an image file onto the chat bar: waits for Enter.
