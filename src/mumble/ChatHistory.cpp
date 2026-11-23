// Personal customisation (muminahommia): see ChatHistory.h.

#include "ChatHistory.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>

namespace ChatHistory {

QString filePath() {
	return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
		   + QLatin1String("/muminahommia_channel_chat_history.jsonl");
}

void append(const QDateTime &time, const QString &html, bool storeImages) {
	QString text = html;
	if (!storeImages) {
		static const QRegularExpression img(QLatin1String("<img[^>]*>"), QRegularExpression::CaseInsensitiveOption);
		text.replace(img, QLatin1String("[image]"));
	}

	QDir().mkpath(QFileInfo(filePath()).absolutePath());
	QFile file(filePath());
	if (!file.open(QIODevice::Append)) {
		qWarning("Failed to open chat history %s: %s", qUtf8Printable(filePath()), qUtf8Printable(file.errorString()));
		return;
	}

	QJsonObject obj;
	obj.insert(QLatin1String("t"), static_cast< double >(time.toMSecsSinceEpoch()));
	obj.insert(QLatin1String("html"), text);
	file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n');
}

QVector< Entry > load(int restoreCount, int maxMessages) {
	QVector< Entry > entries;

	QFile file(filePath());
	if (!file.open(QIODevice::ReadOnly)) {
		return entries;
	}
	// shortcut: reads the whole file, fine for thousands of text lines; stream it if "unlimited" grows huge
	QList< QByteArray > lines = file.readAll().split('\n');
	file.close();
	lines.removeAll(QByteArray());

	if (maxMessages > 0 && lines.size() > maxMessages) {
		lines = lines.mid(lines.size() - maxMessages);

		QSaveFile out(filePath());
		if (out.open(QIODevice::WriteOnly)) {
			for (const QByteArray &line : lines) {
				out.write(line + '\n');
			}
			out.commit();
		} else {
			qWarning("Failed to trim chat history %s", qUtf8Printable(filePath()));
		}
	}

	if (restoreCount > 0 && lines.size() > restoreCount) {
		lines = lines.mid(lines.size() - restoreCount);
	}

	for (const QByteArray &line : lines) {
		const QJsonObject obj = QJsonDocument::fromJson(line).object();
		if (!obj.contains(QLatin1String("html"))) {
			continue; // skips corrupt lines
		}
		entries.append({ QDateTime::fromMSecsSinceEpoch(static_cast< qint64 >(obj.value(QLatin1String("t")).toDouble())),
						 obj.value(QLatin1String("html")).toString() });
	}
	return entries;
}

void clear() {
	QFile::remove(filePath());
}

} // namespace ChatHistory
