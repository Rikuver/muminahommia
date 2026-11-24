// Copyright 2009-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_MUMBLE_CUSTOMELEMENTS_H_
#define MUMBLE_MUMBLE_CUSTOMELEMENTS_H_

#include <QtCore/QObject>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QTextEdit>

class QTextDocument;

/// Room left beside images fitted to a text widget's viewport (document margin, frame padding)
constexpr int IMAGE_WIDTH_SLACK = 20;
/// Scales images in doc down to fit maxWidth x maxHeight (0 = no height limit), never up, keeping the aspect ratio
void fitImagesToWidth(QTextDocument *doc, int maxWidth, int maxHeight = 0);

class LogTextBrowser : public QTextBrowser {
private:
	Q_OBJECT
	Q_DISABLE_COPY(LogTextBrowser)

public:
	LogTextBrowser(QWidget *p = nullptr);

	int getLogScroll();
	void setLogScroll(int scroll_pos);
	bool isScrolledToBottom();
	/// Fits the log's images to its width, once the current changes are done
	void scheduleFitImages();

protected:
	void resizeEvent(QResizeEvent *e) Q_DECL_OVERRIDE;

private:
	bool m_fitPending = false;
};

class ChatbarTextEdit : public QTextEdit {
private:
	Q_OBJECT
	Q_DISABLE_COPY(ChatbarTextEdit)
	void inFocus(bool);
	void applyPlaceholder();
	QStringList qslHistory;
	QString qsHistoryTemp;
	int iHistoryIndex;
	static const int MAX_HISTORY = 50;
	bool m_justPasted;

protected:
	QString qsDefaultText;
	bool bDefaultVisible;
	void focusInEvent(QFocusEvent *) Q_DECL_OVERRIDE;
	void focusOutEvent(QFocusEvent *) Q_DECL_OVERRIDE;
	void contextMenuEvent(QContextMenuEvent *) Q_DECL_OVERRIDE;
	void dragEnterEvent(QDragEnterEvent *) Q_DECL_OVERRIDE;
	void dragMoveEvent(QDragMoveEvent *) Q_DECL_OVERRIDE;
	void dropEvent(QDropEvent *) Q_DECL_OVERRIDE;
	bool event(QEvent *) Q_DECL_OVERRIDE;
	QSize minimumSizeHint() const Q_DECL_OVERRIDE;
	QSize sizeHint() const Q_DECL_OVERRIDE;
	void resizeEvent(QResizeEvent *e) Q_DECL_OVERRIDE;
	bool canInsertFromMimeData(const QMimeData *source) const Q_DECL_OVERRIDE;
	void insertFromMimeData(const QMimeData *source) Q_DECL_OVERRIDE;
	bool insertImagesFromMimeData(const QMimeData *source);
	bool insertPastedImage(QImage image);
	void fitImagesToBar();

public:
	void setDefaultText(const QString &, bool = false);
	unsigned int completeAtCursor();
	/// Plain text with one U+FFFC per pasted image, the images' <img> html in the same order
	QString textWithImages(QStringList &images) const;
signals:
	void tabPressed(void);
	void backtabPressed(void);
	void ctrlSpacePressed(void);
	void entered(QString);
	void ctrlEnterPressed(QString);
public slots:
	void pasteAndSend_triggered();
	void doResize();
	void doScrollbar();
	void addToHistory(const QString &str);
	void historyUp();
	void historyDown();

public:
	ChatbarTextEdit(QWidget *p = nullptr);
};

class DockTitleBar : public QLabel {
private:
	Q_OBJECT
	Q_DISABLE_COPY(DockTitleBar)
protected:
	QTimer *qtTick;
	int size;
	int newsize;

public:
	DockTitleBar();
	QSize sizeHint() const Q_DECL_OVERRIDE;
	QSize minimumSizeHint() const Q_DECL_OVERRIDE;
public slots:
	void tick();

protected:
	bool eventFilter(QObject *, QEvent *) Q_DECL_OVERRIDE;
};

#endif // CUSTOMELEMENTS_H_
