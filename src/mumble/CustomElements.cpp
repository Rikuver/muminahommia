// Copyright 2009-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "CustomElements.h"

#include "ClientUser.h"
#include "Log.h"
#include "MainWindow.h"
#include "QtWidgetUtils.h"
#include "Utils.h"
#include "Global.h"

#include <QMimeData>
#include <QtCore/QTimer>
#include <QtGui/QAbstractTextDocumentLayout>
#include <QtGui/QClipboard>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QTextBlock>
#include <QtGui/QTextFragment>
#include <QtWidgets/QScrollBar>

#include <QtCore/QBuffer>
#include <QtGui/QImageReader>
#include <QtGui/QTextImageFormat>

#include <limits>

namespace {
// Marks an image size set by fitImagesToWidth(), so it can grow back when there is room again
constexpr int FITTED_IMAGE_PROPERTY = QTextFormat::UserProperty + 1;

QSize naturalImageSize(QTextDocument *doc, const QString &name) {
	const QVariant res = doc->resource(QTextDocument::ImageResource, QUrl(name));
	if (res.canConvert< QImage >()) {
		return res.value< QImage >().size();
	}
	if (res.canConvert< QPixmap >()) {
		return res.value< QPixmap >().size();
	}
	// data: URLs load as raw bytes; read only the header for the size
	QByteArray bytes = res.toByteArray();
	QBuffer buffer(&bytes);
	return QImageReader(&buffer).size();
}
} // namespace

void fitImagesToWidth(QTextDocument *doc, int maxWidth, int maxHeight) {
	if (maxWidth <= 0) {
		return;
	}
	struct Change {
		int position;
		int length;
		QTextImageFormat format;
	};
	QVector< Change > changes;
	for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
		for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
			const QTextFragment fragment = it.fragment();
			if (!fragment.isValid() || !fragment.charFormat().isImageFormat()) {
				continue;
			}
			QTextImageFormat format = fragment.charFormat().toImageFormat();
			const bool fitted       = format.hasProperty(FITTED_IMAGE_PROPERTY);
			// A size the sender set is the base, unless it is one this function set
			const QSize base = (!fitted && format.width() > 0 && format.height() > 0)
								   ? QSize(qRound(format.width()), qRound(format.height()))
								   : naturalImageSize(doc, format.name());
			if (base.isEmpty()) {
				continue;
			}
			const QSize bound(maxWidth, maxHeight > 0 ? maxHeight : std::numeric_limits< int >::max());
			const QSize target = (base.width() > bound.width() || base.height() > bound.height())
									 ? base.scaled(bound, Qt::KeepAspectRatio)
									 : base;
			if (fitted ? (qRound(format.width()) == target.width() && qRound(format.height()) == target.height())
					   : target == base) {
				continue;
			}
			format.setWidth(target.width());
			format.setHeight(target.height());
			format.setProperty(FITTED_IMAGE_PROPERTY, true);
			changes.append({ fragment.position(), fragment.length(), format });
		}
	}
	// Applied after the walk, since changing formats while iterating can merge or split fragments
	for (const Change &change : changes) {
		QTextCursor tc(doc);
		tc.setPosition(change.position);
		tc.setPosition(change.position + change.length, QTextCursor::KeepAnchor);
		tc.setCharFormat(change.format);
	}
}

LogTextBrowser::LogTextBrowser(QWidget *p) : QTextBrowser(p) {
}

void LogTextBrowser::scheduleFitImages() {
	// shortcut: rescans the whole log after each change; fine for iMaxLogBlocks-sized logs, scan only the changed
	// range if it gets slow
	if (m_fitPending) {
		return;
	}
	m_fitPending = true;
	QTimer::singleShot(0, this, [this]() {
		m_fitPending = false;
		fitImagesToWidth(document(), viewport()->width() - IMAGE_WIDTH_SLACK);
	});
}

void LogTextBrowser::resizeEvent(QResizeEvent *e) {
	QTextBrowser::resizeEvent(e);
	scheduleFitImages();
}

int LogTextBrowser::getLogScroll() {
	return verticalScrollBar()->value();
}

void LogTextBrowser::setLogScroll(int scroll_pos) {
	verticalScrollBar()->setValue(scroll_pos);
}

bool LogTextBrowser::isScrolledToBottom() {
	const QScrollBar *scrollBar = verticalScrollBar();
	return scrollBar->value() == scrollBar->maximum();
}


void ChatbarTextEdit::focusInEvent(QFocusEvent *qfe) {
	inFocus(true);
	QTextEdit::focusInEvent(qfe);
}

void ChatbarTextEdit::focusOutEvent(QFocusEvent *qfe) {
	inFocus(false);
	QTextEdit::focusOutEvent(qfe);
}

void ChatbarTextEdit::inFocus(bool focus) {
	if (focus) {
		if (bDefaultVisible) {
			QFont f = font();
			f.setItalic(false);
			setFont(f);
			setPlainText(QString());
			setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
			bDefaultVisible = false;
		}
	} else {
		if (toPlainText().trimmed().isEmpty() || bDefaultVisible) {
			applyPlaceholder();
		} else {
			setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
			bDefaultVisible = false;
		}
	}
}

void ChatbarTextEdit::contextMenuEvent(QContextMenuEvent *qcme) {
	QMenu *menu = createStandardContextMenu();

	QAction *action = new QAction(tr("Paste and &Send"), menu);
	action->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
	action->setEnabled(!QApplication::clipboard()->text().isEmpty() || !QApplication::clipboard()->image().isNull());
	connect(action, SIGNAL(triggered()), this, SLOT(pasteAndSend_triggered()));
	if (menu->actions().count() > 6) {
		menu->insertAction(menu->actions()[6], action);
	} else {
		menu->addAction(action);
	}

	menu->exec(qcme->globalPos());
	delete menu;
}

void ChatbarTextEdit::dragEnterEvent(QDragEnterEvent *evt) {
	inFocus(true);
	evt->acceptProposedAction();
}

void ChatbarTextEdit::dragMoveEvent(QDragMoveEvent *evt) {
	inFocus(true);
	evt->acceptProposedAction();
}

void ChatbarTextEdit::dropEvent(QDropEvent *evt) {
	inFocus(true);
	if (insertImagesFromMimeData(evt->mimeData())) {
		evt->acceptProposedAction();
	} else {
		QTextEdit::dropEvent(evt);
	}
}

ChatbarTextEdit::ChatbarTextEdit(QWidget *p) : QTextEdit(p), iHistoryIndex(-1) {
	setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setMinimumHeight(0);
	connect(this, &ChatbarTextEdit::textChanged, this, &ChatbarTextEdit::doResize);

	bDefaultVisible = true;
	setDefaultText(tr("<center>Type chat message here</center>"));
	setAcceptDrops(true);

	m_justPasted = false;
}

QSize ChatbarTextEdit::minimumSizeHint() const {
	return QSize(0, fontMetrics().height());
}

QSize ChatbarTextEdit::sizeHint() const {
	QSize sh                    = QTextEdit::sizeHint();
	const int minHeight         = minimumSizeHint().height();
	const int documentHeight    = static_cast< int >(document()->documentLayout()->documentSize().height());
	const int chatBarLineHeight = QFontMetrics(ChatbarTextEdit::font()).height();

	sh.setHeight(std::max(minHeight, std::min(chatBarLineHeight * 10, documentHeight)));
	const_cast< ChatbarTextEdit * >(this)->setMaximumHeight(sh.height());
	return sh;
}

void ChatbarTextEdit::resizeEvent(QResizeEvent *e) {
	QTextEdit::resizeEvent(e);
	fitImagesToBar();
	QTimer::singleShot(0, this, SLOT(doScrollbar()));

	if (bDefaultVisible) {
		QTimer::singleShot(0, [this]() { applyPlaceholder(); });
	}
}

void ChatbarTextEdit::doResize() {
	updateGeometry();
	QTimer::singleShot(0, this, SLOT(doScrollbar()));
}

void ChatbarTextEdit::doScrollbar() {
	const int documentHeight = static_cast< int >(document()->documentLayout()->documentSize().height());
	setVerticalScrollBarPolicy(documentHeight > height() ? Qt::ScrollBarAlwaysOn : Qt::ScrollBarAlwaysOff);
	ensureCursorVisible();
}

void ChatbarTextEdit::setDefaultText(const QString &new_default, bool force) {
	qsDefaultText = new_default;

	if (bDefaultVisible || force) {
		applyPlaceholder();
	}
}

void ChatbarTextEdit::applyPlaceholder() {
	QFont f = font();
	f.setItalic(true);
	setFont(f);
	setWordWrapMode(QTextOption::NoWrap);
	setHtml(qsDefaultText);

	Mumble::QtUtils::elideText(*document(), static_cast< uint32_t >(size().width()));

	bDefaultVisible = true;
}

bool ChatbarTextEdit::canInsertFromMimeData(const QMimeData *source) const {
	return (QTextEdit::canInsertFromMimeData(source) || source->hasImage() || source->hasUrls());
}

void ChatbarTextEdit::insertFromMimeData(const QMimeData *source) {
	if (!insertImagesFromMimeData(source)) {
		QTextEdit::insertFromMimeData(source);
	}
}

bool ChatbarTextEdit::insertImagesFromMimeData(const QMimeData *source) {
	if ((source->hasImage() || source->hasUrls())) {
		if (Global::get().bAllowHTML) {
			if (source->hasImage()) {
				// Process the image pasted onto the chatbar.
				QImage image = qvariant_cast< QImage >(source->imageData());
				if (insertPastedImage(image)) {
					return true;
				} else {
					Global::get().l->log(Log::Information, tr("Unable to send image: too large."));
					return false;
				}

			} else if (source->hasUrls()) {
				// Process the files dropped onto the chatbar. URLs here should be understood as the URIs of files.
				QList< QUrl > urlList = source->urls();

				int count = 0;
				for (int i = 0; i < urlList.size(); ++i) {
					QString path = urlList[i].toLocalFile();
					QImage image(path);

					if (image.isNull())
						continue;
					if (insertPastedImage(image)) {
						++count;
					} else {
						Global::get().l->log(Log::Information, tr("Unable to send image %1: too large.").arg(path));
					}
				}

				return (count > 0);
			}
		} else {
			Global::get().l->log(Log::Information, tr("This server does not allow sending images."));
		}
	}
	return false;
}

bool ChatbarTextEdit::insertPastedImage(QImage image) {
	const QString processedImage = Log::imageToImg(image, static_cast< int >(Global::get().uiImageLength));
	if (processedImage.length() > 0) {
		// Customisation: the image waits in the chat bar as a thumbnail and goes out with Enter
		textCursor().insertHtml(processedImage);
		fitImagesToBar();
		return true;
	}
	return false;
}

void ChatbarTextEdit::fitImagesToBar() {
	fitImagesToWidth(document(), viewport()->width() - IMAGE_WIDTH_SLACK, fontMetrics().height() * 4);
}

QString ChatbarTextEdit::textWithImages(QStringList &images) const {
	QString text;
	for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
		if (block != document()->begin()) {
			text += QLatin1Char('\n');
		}
		for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
			const QTextFragment fragment = it.fragment();
			if (!fragment.isValid()) {
				continue;
			}
			const QTextCharFormat format = fragment.charFormat();
			if (format.isImageFormat()) {
				// Only images pasted as data, never remote images that came in with pasted rich text
				const QString name = format.toImageFormat().name();
				if (name.startsWith(QLatin1String("data:"))) {
					// The same image pasted twice in a row is one fragment of two characters
					for (int i = 0; i < fragment.length(); ++i) {
						// Each image on its own line, so text is not bottom-aligned next to it in the log
						if (!text.isEmpty() && !text.endsWith(QLatin1Char('\n'))) {
							text += QLatin1Char('\n');
						}
						text += QChar::ObjectReplacementCharacter;
						images << QString::fromLatin1("<img src=\"%1\" />").arg(name.toHtmlEscaped());
					}
				}
			} else {
				const QString part = fragment.text().remove(QChar::ObjectReplacementCharacter);
				if (text.endsWith(QChar::ObjectReplacementCharacter) && !part.isEmpty()
					&& !part.startsWith(QChar::LineSeparator)) {
					text += QLatin1Char('\n');
				}
				text += part;
			}
		}
	}
	return text.replace(QChar::LineSeparator, QLatin1Char('\n')).replace(QChar::Nbsp, QLatin1Char(' '));
}

bool ChatbarTextEdit::event(QEvent *evt) {
	if (evt->type() == QEvent::ShortcutOverride) {
		return false;
	}

	if (evt->type() == QEvent::KeyPress) {
		QKeyEvent *kev = static_cast< QKeyEvent * >(evt);
		if ((kev->key() == Qt::Key_Enter || kev->key() == Qt::Key_Return) && !(kev->modifiers() & Qt::ShiftModifier)) {
			const QString msg = toPlainText();
			if (!msg.isEmpty()) {
				addToHistory(msg);
				if ((kev->modifiers() & Qt::ControlModifier) && !m_justPasted) {
					emit ctrlEnterPressed(msg);
				} else {
					emit entered(msg);
				}
				m_justPasted = false;
			}
			return true;
		}
		if (kev->key() == Qt::Key_Tab) {
			emit tabPressed();
			return true;
		} else if (kev->key() == Qt::Key_Backtab) {
			emit backtabPressed();
			return true;
		} else if (kev->key() == Qt::Key_Space && kev->modifiers() == Qt::ControlModifier) {
			emit ctrlSpacePressed();
			return true;
		} else if (kev->key() == Qt::Key_Up && kev->modifiers() == Qt::ControlModifier) {
			historyUp();
			return true;
		} else if (kev->key() == Qt::Key_Down && kev->modifiers() == Qt::ControlModifier) {
			historyDown();
			return true;
		} else if (kev->key() == Qt::Key_V && (kev->modifiers() & Qt::ControlModifier)) {
			if (kev->modifiers() & Qt::ShiftModifier) {
				pasteAndSend_triggered();
				return true;
			} else {
				// Remember that we just pasted into the chat field
				// and allow CTRL+Enter only when we are sure it was
				// released for at least one GUI cycle.
				// See #6568
				m_justPasted = true;
			}
		}
	}

	if (evt->type() == QEvent::KeyRelease) {
		QKeyEvent *kev = static_cast< QKeyEvent * >(evt);
		if (kev->key() == Qt::Key_Control) {
			m_justPasted = false;
		}
	}

	return QTextEdit::event(evt);
}

/**
 * The bar will try to complete the username, if the nickname
 * is already complete it will try to find a longer match. If
 * there is none it will cycle the nicknames alphabetically.
 * Nothing is done on mismatch.
 */
unsigned int ChatbarTextEdit::completeAtCursor() {
	// Get an alphabetically sorted list of usernames
	unsigned int id = 0;

	QList< QString > qlsUsernames;

	if (ClientUser::c_qmUsers.empty())
		return id;
	foreach (ClientUser *usr, ClientUser::c_qmUsers) { qlsUsernames.append(usr->qsName); }
	std::sort(qlsUsernames.begin(), qlsUsernames.end());

	QString target = QString();
	QTextCursor tc = textCursor();

	if (toPlainText().isEmpty() || tc.position() == 0) {
		target = qlsUsernames.first();
		tc.insertText(target);
	} else {
		bool bBaseIsName = false;
		int iend         = tc.position();
		int istart       = toPlainText().lastIndexOf(QLatin1Char(' '), iend - 1) + 1;
		QString base     = toPlainText().mid(istart, iend - istart);
		tc.setPosition(istart);
		tc.setPosition(iend, QTextCursor::KeepAnchor);

		if (qlsUsernames.last() == base) {
			bBaseIsName = true;
			target      = qlsUsernames.first();
		} else {
			if (qlsUsernames.contains(base)) {
				// Prevent to complete to what's already there
				while (qlsUsernames.takeFirst() != base) {
				}
				bBaseIsName = true;
			}

			foreach (QString name, qlsUsernames) {
				if (name.startsWith(base, Qt::CaseInsensitive)) {
					target = name;
					break;
				}
			}
		}

		if (bBaseIsName && target.isEmpty() && !qlsUsernames.empty()) {
			// If autocomplete failed and base was a name get the next one
			target = qlsUsernames.first();
		}

		if (!target.isEmpty()) {
			tc.insertText(target);
		}
	}

	if (!target.isEmpty()) {
		setTextCursor(tc);

		foreach (ClientUser *usr, ClientUser::c_qmUsers) {
			if (usr->qsName == target) {
				id = usr->uiSession;
				break;
			}
		}
	}
	return id;
}

void ChatbarTextEdit::addToHistory(const QString &str) {
	iHistoryIndex = -1;
	// Pasted images show up as U+FFFC in the plain text; history keeps text only
	qslHistory.push_front(QString(str).remove(QChar::ObjectReplacementCharacter));
	if (qslHistory.length() > MAX_HISTORY) {
		qslHistory.pop_back();
	}
}

void ChatbarTextEdit::historyUp() {
	if (qslHistory.length() == 0)
		return;

	if (iHistoryIndex == -1) {
		qsHistoryTemp = toPlainText();
	}

	if (iHistoryIndex < qslHistory.length() - 1) {
		setPlainText(qslHistory[++iHistoryIndex]);
		moveCursor(QTextCursor::End);
	}
}

void ChatbarTextEdit::historyDown() {
	if (iHistoryIndex < 0) {
		return;
	} else if (iHistoryIndex == 0) {
		setPlainText(qsHistoryTemp);
		iHistoryIndex--;
	} else {
		setPlainText(qslHistory[--iHistoryIndex]);
	}
	moveCursor(QTextCursor::End);
}

void ChatbarTextEdit::pasteAndSend_triggered() {
	if (bDefaultVisible) {
		// Clear placeholder
		setPlainText(QString());
	}

	paste();

	if (!toPlainText().isEmpty()) {
		addToHistory(toPlainText());
		emit entered(toPlainText());
	}
}

DockTitleBar::DockTitleBar() : QLabel(tr("Drag here")) {
	setAlignment(Qt::AlignCenter);
	setEnabled(false);
	qtTick = new QTimer(this);
	qtTick->setSingleShot(true);
	connect(qtTick, SIGNAL(timeout()), this, SLOT(tick()));
	size = newsize = 0;
}

QSize DockTitleBar::sizeHint() const {
	return minimumSizeHint();
}

QSize DockTitleBar::minimumSizeHint() const {
	return QSize(size, size);
}

bool DockTitleBar::eventFilter(QObject *, QEvent *evt) {
	QDockWidget *qdw = qobject_cast< QDockWidget * >(parentWidget());

	if (!this->isEnabled())
		return false;

	switch (evt->type()) {
		case QEvent::Leave:
		case QEvent::Enter:
		case QEvent::MouseMove:
		case QEvent::MouseButtonRelease: {
			newsize  = 0;
			QPoint p = qdw->mapFromGlobal(QCursor::pos());
			if ((p.x() >= static_cast< int >(static_cast< float >(qdw->width()) * 0.1f + 0.5f))
				&& (p.x() < static_cast< int >(static_cast< float >(qdw->width()) * 0.9f + 0.5f)) && (p.y() >= 0)
				&& (p.y() < 15))
				newsize = 15;
			if (newsize > 0 && !qtTick->isActive())
				qtTick->start(500);
			else if ((newsize == size) && qtTick->isActive())
				qtTick->stop();
			else if (newsize == 0)
				tick();
		}
		default:
			break;
	}

	return false;
}

void DockTitleBar::tick() {
	QDockWidget *qdw = qobject_cast< QDockWidget * >(parentWidget());

	if (newsize == size)
		return;

	size = newsize;
	qdw->setTitleBarWidget(this);
}
