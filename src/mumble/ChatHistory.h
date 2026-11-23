// Personal customisation (muminahommia): saves channel chat to a local file so it survives restarts.

#ifndef MUMBLE_MUMBLE_CHATHISTORY_H_
#define MUMBLE_MUMBLE_CHATHISTORY_H_

#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtCore/QVector>

/// Append-only JSON Lines file with one channel chat message per line: {"t": <epoch ms>, "html": "..."}
namespace ChatHistory {
struct Entry {
	QDateTime time;
	QString html;
};

QString filePath();

/// Appends one message. Images are replaced with "[image]" unless storeImages is set.
void append(const QDateTime &time, const QString &html, bool storeImages);

/// Returns the last restoreCount saved messages (all if 0). If maxMessages > 0 the file is first cut down to
/// its last maxMessages lines.
QVector< Entry > load(int restoreCount, int maxMessages);

/// Deletes the saved history.
void clear();
} // namespace ChatHistory

#endif
