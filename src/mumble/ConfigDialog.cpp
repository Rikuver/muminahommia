// Copyright 2007-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "ConfigDialog.h"

#include "AudioInput.h"
#include "AudioOutput.h"
#include "widgets/EventFilters.h"
#include "Global.h"

#include <QScrollArea>
#include <QShortcut>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QMutexLocker>
#include <QtCore/QTimer>
#include <QtGui/QKeyEvent>
#include <QtGui/QScreen>
#include <QtGui/QTextDocument>
#include <QtGui/QTextDocumentFragment>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QTabWidget>

#include <algorithm>

namespace {
/// Turns a UI string into plain text that can be searched: strips HTML and keyboard mnemonics ("&File")
QString normalizeSearchText(QString text) {
	if (Qt::mightBeRichText(text)) {
		text = QTextDocumentFragment::fromHtml(text).toPlainText();
	}

	// "&&" is a literal ampersand, a single "&" marks a keyboard mnemonic
	const QChar placeholder(0x1);
	text.replace(QLatin1String("&&"), QString(placeholder));
	text.remove(QLatin1Char('&'));
	text.replace(placeholder, QLatin1Char('&'));

	return text.simplified();
}

bool matchesSearch(const QString &text, const QString &query) {
	return !text.isEmpty() && normalizeSearchText(text).contains(query, Qt::CaseInsensitive);
}

/// @returns Whether the given widget would be visible to the user once the given page is shown. Widgets that are
/// 	only hidden because they live on another tab of a QTabWidget count as visible, as we can switch to that tab.
bool isSearchable(const QWidget *widget, const QWidget *page) {
	for (const QWidget *current = widget; current && current != page; current = current->parentWidget()) {
		if (current->isWindow()) {
			// e.g. the popup of a QComboBox
			return false;
		}

		const bool explicitlyHidden =
			current->testAttribute(Qt::WA_WState_Hidden) && current->testAttribute(Qt::WA_WState_ExplicitShowHide);
		if (!explicitlyHidden) {
			continue;
		}

		const QWidget *parent    = current->parentWidget();
		const bool isInactiveTab = qobject_cast< const QStackedWidget * >(parent) && parent->parentWidget()
								   && qobject_cast< const QTabWidget * >(parent->parentWidget());
		if (!isInactiveTab) {
			return false;
		}
	}

	return true;
}

/// Collects the first matching cell of every row of the given model (recursively)
void collectModelMatches(const QAbstractItemModel *model, const QModelIndex &parent, const QString &query,
						 QList< QModelIndex > &matches) {
	for (int row = 0; row < model->rowCount(parent); ++row) {
		for (int column = 0; column < model->columnCount(parent); ++column) {
			const QModelIndex index = model->index(row, column, parent);
			if (matchesSearch(model->data(index, Qt::DisplayRole).toString(), query)) {
				matches.append(index);
				break;
			}
		}

		const QModelIndex firstColumn = model->index(row, 0, parent);
		if (model->hasChildren(firstColumn)) {
			collectModelMatches(model, firstColumn, query, matches);
		}
	}
}

/// @returns The text that is shown for the given widget, if it has any
QString displayedText(const QWidget *widget) {
	if (const QLabel *label = qobject_cast< const QLabel * >(widget)) {
		return label->text();
	}
	if (const QAbstractButton *button = qobject_cast< const QAbstractButton * >(widget)) {
		return button->text();
	}
	if (const QGroupBox *groupBox = qobject_cast< const QGroupBox * >(widget)) {
		return groupBox->title();
	}
	return QString();
}
} // namespace


// init static member fields
QMutex ConfigDialog::s_existingWidgetsMutex;
QHash< QString, ConfigWidget * > ConfigDialog::s_existingWidgets;

ConfigDialog::ConfigDialog(QWidget *p) : QDialog(p) {
	setupUi(this);

	{
		QMutexLocker lock(&s_existingWidgetsMutex);
		s_existingWidgets.clear();
	}


	s = Global::get().s;

	unsigned int idx = 0;
	ConfigWidgetNew cwn;
	foreach (cwn, *ConfigRegistrar::c_qmNew) {
		ConfigWidget *cw = cwn(s);
		{
			QMutexLocker lock(&s_existingWidgetsMutex);
			s_existingWidgets.insert(cw->getName(), cw);
		}

		addPage(cw, ++idx);
	}

	updateListView();

	QPushButton *okButton = dialogButtonBox->button(QDialogButtonBox::Ok);
	okButton->setToolTip(tr("Accept changes"));
	okButton->setWhatsThis(tr("This button will accept current settings and return to the application.<br />"
							  "The settings will be stored to disk when you leave the application."));

	QPushButton *cancelButton = dialogButtonBox->button(QDialogButtonBox::Cancel);
	cancelButton->setToolTip(tr("Reject changes"));
	cancelButton->setWhatsThis(tr("This button will reject all changes and return to the application.<br />"
								  "The settings will be reset to the previous positions."));

	QPushButton *applyButton = dialogButtonBox->button(QDialogButtonBox::Apply);
	applyButton->setToolTip(tr("Apply changes"));
	applyButton->setWhatsThis(tr("This button will immediately apply all changes."));

	QPushButton *resetButton = pageButtonBox->addButton(QDialogButtonBox::Reset);
	resetButton->setToolTip(tr("Undo changes for current page"));
	resetButton->setWhatsThis(
		tr("This button will revert any changes done on the current page to the most recent applied settings."));

	QPushButton *restoreButton = pageButtonBox->addButton(QDialogButtonBox::RestoreDefaults);
	restoreButton->setToolTip(tr("Restore defaults for current page"));
	restoreButton->setWhatsThis(
		tr("This button will restore the defaults for the settings on the current page. Other pages will not be "
		   "changed.<br />"
		   "To restore all settings to their defaults, you can press the \"Defaults (All)\" button."));

	QPushButton *restoreAllButton = pageButtonBox->addButton(tr("Defaults (All)"), QDialogButtonBox::ResetRole);
	restoreAllButton->setToolTip(tr("Restore all defaults"));
	restoreAllButton->setWhatsThis(tr("This button will restore the defaults for all settings."));
	restoreAllButton->installEventFilter(new OverrideTabOrderFilter(restoreAllButton, applyButton));

	qleSearch->installEventFilter(this);
	QShortcut *searchShortcut = new QShortcut(QKeySequence::Find, this);
	connect(searchShortcut, &QShortcut::activated, this, [this]() {
		qleSearch->setFocus(Qt::ShortcutFocusReason);
		qleSearch->selectAll();
	});

	updateTabOrder();
	qlwIcons->setFocus();
}

void ConfigDialog::addPage(ConfigWidget *cw, unsigned int idx) {
	int w = INT_MAX, h = INT_MAX;

	const QList< QScreen * > screens = qApp->screens();
	for (int i = 0; i < screens.size(); ++i) {
		const QRect ds = screens[i]->availableGeometry();
		if (ds.isValid()) {
			w = qMin(w, ds.width());
			h = qMin(h, ds.height());
		}
	}

	QSize ms = cw->minimumSizeHint();
	cw->resize(ms);
	cw->setMinimumSize(ms);

	ms.rwidth() += 128;
	ms.rheight() += 192;
	if ((ms.width() > w) || (ms.height() > h)) {
		QScrollArea *qsa = new QScrollArea();
		qsa->setFrameShape(QFrame::NoFrame);
		qsa->setWidgetResizable(true);
		qsa->setWidget(cw);
		qsa->setFocusPolicy(Qt::NoFocus);
		qhPages.insert(cw, qsa);
		qswPages->addWidget(qsa);
	} else {
		qhPages.insert(cw, cw);
		qswPages->addWidget(cw);
	}
	qmWidgets.insert(idx, cw);
	cw->load(Global::get().s);
}

ConfigDialog::~ConfigDialog() {
	{
		QMutexLocker lock(&s_existingWidgetsMutex);
		s_existingWidgets.clear();
	}

	foreach (QWidget *qw, qhPages)
		delete qw;
}

ConfigWidget *ConfigDialog::getConfigWidget(const QString &name) {
	QMutexLocker lock(&s_existingWidgetsMutex);

	return s_existingWidgets.value(name, nullptr);
}

void ConfigDialog::on_pageButtonBox_clicked(QAbstractButton *b) {
	ConfigWidget *conf = qobject_cast< ConfigWidget * >(qswPages->currentWidget());
	if (!conf) {
		QScrollArea *qsa = qobject_cast< QScrollArea * >(qswPages->currentWidget());
		if (qsa)
			conf = qobject_cast< ConfigWidget * >(qsa->widget());
	}
	if (!conf)
		return;
	switch (pageButtonBox->standardButton(b)) {
		case QDialogButtonBox::RestoreDefaults: {
			Settings def;
			conf->load(def);
			break;
		}
		case QDialogButtonBox::Reset: {
			conf->load(Global::get().s);
			break;
		}
		// standardButton returns NoButton for any custom buttons. The only custom button
		// in the pageButtonBox is the one for resetting all settings.
		case QDialogButtonBox::NoButton: {
			// Ask for confirmation before resetting **all** settings
			QMessageBox msgBox;
			msgBox.setIcon(QMessageBox::Question);
			msgBox.setText(QObject::tr("Reset all settings?"));
			msgBox.setInformativeText(QObject::tr("Do you really want to reset all settings (not only the ones "
												  "currently visible) to their default value?"));
			msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
			msgBox.setDefaultButton(QMessageBox::No);

			if (msgBox.exec() == QMessageBox::Yes) {
				Settings defaultSetting;
				foreach (ConfigWidget *cw, qmWidgets) { cw->load(defaultSetting); }
			}
			break;
		}
		default:
			break;
	}
}

void ConfigDialog::on_dialogButtonBox_clicked(QAbstractButton *b) {
	switch (dialogButtonBox->standardButton(b)) {
		case QDialogButtonBox::Apply: {
			apply();
			break;
		}
		default:
			break;
	}
}

void ConfigDialog::on_qlwIcons_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous) {
	if (!current)
		current = previous;

	clearSearchHighlight();

	if (current) {
		QWidget *w = qhPages.value(qmIconWidgets.value(current));
		if (w)
			qswPages->setCurrentWidget(w);
		if (previous) {
			updateTabOrder();
		}
	}
}

void ConfigDialog::updateTabOrder() {
	QPushButton *okButton         = dialogButtonBox->button(QDialogButtonBox::Ok);
	QPushButton *cancelButton     = dialogButtonBox->button(QDialogButtonBox::Cancel);
	QPushButton *applyButton      = dialogButtonBox->button(QDialogButtonBox::Apply);
	QPushButton *resetButton      = pageButtonBox->button(QDialogButtonBox::Reset);
	QPushButton *restoreButton    = pageButtonBox->button(QDialogButtonBox::RestoreDefaults);
	QPushButton *restoreAllButton = static_cast< QPushButton * >(pageButtonBox->buttons().last());

	QWidget *contentFocusWidget = qswPages;

	ConfigWidget *page;
	QScrollArea *qsa = qobject_cast< QScrollArea * >(qswPages->currentWidget());
	if (qsa) {
		page = qobject_cast< ConfigWidget * >(qsa->widget());
	} else {
		page = qobject_cast< ConfigWidget * >(qswPages->currentWidget());
	}

	if (page) {
		contentFocusWidget = page;
	}

	setTabOrder(cancelButton, okButton);
	setTabOrder(okButton, qleSearch);
	setTabOrder(qleSearch, qlwIcons);
	setTabOrder(qlwIcons, contentFocusWidget);
	if (resetButton && restoreButton && restoreAllButton) {
		setTabOrder(contentFocusWidget, resetButton);
		setTabOrder(resetButton, restoreButton);
		setTabOrder(restoreButton, restoreAllButton);
		setTabOrder(restoreAllButton, applyButton);
	} else {
		setTabOrder(contentFocusWidget, applyButton);
	}
	setTabOrder(applyButton, cancelButton);
}

void ConfigDialog::updateListView() {
	QWidget *ccw         = qmIconWidgets.value(qlwIcons->currentItem());
	QListWidgetItem *sel = nullptr;

	qmIconWidgets.clear();
	qlwIcons->clear();

	QFontMetrics qfm(qlwIcons->font());
	int configNavbarWidth = 0;

	foreach (ConfigWidget *cw, qmWidgets) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
		configNavbarWidth = qMax(configNavbarWidth, qfm.horizontalAdvance(cw->title()));
#else
		configNavbarWidth = qMax(configNavbarWidth, qfm.width(cw->title()));
#endif

		QListWidgetItem *i = new QListWidgetItem(qlwIcons);
		i->setIcon(cw->icon());
		i->setText(cw->title());
		i->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

		qmIconWidgets.insert(i, cw);
		if (cw == ccw)
			sel = i;
	}

	// Add space for icon and some padding.
	configNavbarWidth += qlwIcons->iconSize().width() + 25;

	qlwIcons->setMinimumWidth(configNavbarWidth);
	qlwIcons->setMaximumWidth(configNavbarWidth);

	if (sel)
		qlwIcons->setCurrentItem(sel);
	else
		qlwIcons->setCurrentRow(0);
}

void ConfigDialog::apply() {
	Audio::stop();

	foreach (ConfigWidget *cw, qmWidgets)
		cw->save();

	Global::get().s = s;

	foreach (ConfigWidget *cw, qmWidgets)
		cw->accept();

	if (!Global::get().s.bAttenuateOthersOnTalk)
		Global::get().bAttenuateOthers = false;

	// They might have changed their keys.
	Global::get().iPushToTalk = 0;

	Audio::start();

	emit settingsAccepted();
}

void ConfigDialog::accept() {
	apply();

	// Save settings to disk
	Global::get().s.save();

	QDialog::accept();
}

void ConfigDialog::collectSearchMatches(ConfigWidget *page, const QString &query, QList< SearchMatch > &matches) const {
	if (matchesSearch(page->title(), query)) {
		SearchMatch match;
		match.page = page;
		match.text = page->title();
		matches.append(match);
	}

	QList< SearchMatch > widgetMatches;
	auto addMatch = [&](QWidget *widget, const QString &text, const QModelIndex &index = QModelIndex(),
						int tabIndex = -1) {
		SearchMatch match;
		match.page     = page;
		match.widget   = widget;
		match.index    = index;
		match.tabIndex = tabIndex;
		match.text     = normalizeSearchText(text);
		widgetMatches.append(match);
	};

	for (QWidget *widget : page->findChildren< QWidget * >()) {
		if (!isSearchable(widget, page)) {
			continue;
		}

		if (QTabBar *tabBar = qobject_cast< QTabBar * >(widget)) {
			for (int i = 0; i < tabBar->count(); ++i) {
				if (matchesSearch(tabBar->tabText(i), query)) {
					addMatch(tabBar, tabBar->tabText(i), QModelIndex(), i);
				}
			}
			continue;
		}

		if (QAbstractItemView *view = qobject_cast< QAbstractItemView * >(widget)) {
			if (view->model()) {
				QList< QModelIndex > indices;
				collectModelMatches(view->model(), view->rootIndex(), query, indices);
				for (const QModelIndex &index : indices) {
					addMatch(view, index.data(Qt::DisplayRole).toString(), index);
				}
			}
		}

		if (QComboBox *comboBox = qobject_cast< QComboBox * >(widget)) {
			bool found = false;
			for (int i = 0; i < comboBox->count() && !found; ++i) {
				if (matchesSearch(comboBox->itemText(i), query)) {
					addMatch(comboBox, comboBox->itemText(i));
					found = true;
				}
			}
			if (found) {
				continue;
			}
		}

		const QString text = displayedText(widget);
		if (matchesSearch(text, query)) {
			addMatch(widget, text);
		} else if (matchesSearch(widget->accessibleName(), query)) {
			addMatch(widget, widget->accessibleName());
		} else if (matchesSearch(widget->toolTip(), query)) {
			addMatch(widget, text.isEmpty() ? widget->accessibleName() : text);
		}
	}

	// Present the matches in the order in which they appear on the page (top to bottom, left to right)
	std::stable_sort(widgetMatches.begin(), widgetMatches.end(),
					 [page](const SearchMatch &lhs, const SearchMatch &rhs) {
						 const QPoint lhsPos = lhs.widget->mapTo(page, QPoint(0, 0));
						 const QPoint rhsPos = rhs.widget->mapTo(page, QPoint(0, 0));
						 return lhsPos.y() != rhsPos.y() ? lhsPos.y() < rhsPos.y() : lhsPos.x() < rhsPos.x();
					 });

	matches.append(widgetMatches);
}

void ConfigDialog::on_qleSearch_textChanged(const QString &text) {
	clearSearchHighlight();
	m_searchMatches.clear();
	m_currentSearchMatch = -1;

	const QString query = normalizeSearchText(text);

	for (int row = 0; row < qlwIcons->count(); ++row) {
		QListWidgetItem *item = qlwIcons->item(row);
		ConfigWidget *page    = qmIconWidgets.value(item);

		if (query.isEmpty() || !page) {
			item->setHidden(false);
			continue;
		}

		QList< SearchMatch > pageMatches;
		collectSearchMatches(page, query, pageMatches);
		item->setHidden(pageMatches.isEmpty());
		m_searchMatches.append(pageMatches);
	}

	// Never leave a hidden category selected
	QListWidgetItem *current = qlwIcons->currentItem();
	if (!current || current->isHidden()) {
		for (int row = 0; row < qlwIcons->count(); ++row) {
			if (!qlwIcons->item(row)->isHidden()) {
				qlwIcons->setCurrentRow(row);
				break;
			}
		}
	}

	updateSearchStatus();
}

void ConfigDialog::updateSearchStatus() {
	if (qleSearch->text().trimmed().isEmpty()) {
		qlSearchStatus->clear();
		qlSearchStatus->setVisible(false);
		qleSearch->setAccessibleDescription(QString());
		return;
	}

	QString status;
	if (m_searchMatches.isEmpty()) {
		status = tr("No matching settings");
	} else if (m_currentSearchMatch < 0) {
		status = tr("%n match(es). Press Enter to show the first one.", "", static_cast< int >(m_searchMatches.size()));
	} else {
		const SearchMatch &match = m_searchMatches[m_currentSearchMatch];
		status                   = tr("Match %1 of %2").arg(m_currentSearchMatch + 1).arg(m_searchMatches.size());
		status += QLatin1Char('\n');
		if (match.widget && !match.text.isEmpty()) {
			//: Shown below the settings search field. %1 is the settings page, %2 the matching setting.
			status += tr("%1: %2").arg(match.page->title(), match.text);
		} else {
			status += match.page->title();
		}
	}

	qlSearchStatus->setText(status);
	qlSearchStatus->setVisible(true);
	qleSearch->setAccessibleDescription(status);
}

void ConfigDialog::showSearchMatch(int matchIndex) {
	if (m_searchMatches.isEmpty()) {
		return;
	}

	const int count         = static_cast< int >(m_searchMatches.size());
	m_currentSearchMatch    = ((matchIndex % count) + count) % count;
	const SearchMatch match = m_searchMatches[m_currentSearchMatch];

	// Switch to the page containing the match
	for (auto it = qmIconWidgets.constBegin(); it != qmIconWidgets.constEnd(); ++it) {
		if (it.value() == match.page) {
			qlwIcons->setCurrentItem(it.key());
			break;
		}
	}

	if (match.widget) {
		// Switch to the tab containing the match (if any)
		for (QWidget *current = match.widget; current && current != match.page; current = current->parentWidget()) {
			QWidget *parent = current->parentWidget();
			if (parent && qobject_cast< QStackedWidget * >(parent)) {
				if (QTabWidget *tabWidget = qobject_cast< QTabWidget * >(parent->parentWidget())) {
					tabWidget->setCurrentWidget(current);
				}
			}
		}

		if (match.tabIndex >= 0) {
			if (QTabBar *tabBar = qobject_cast< QTabBar * >(match.widget)) {
				tabBar->setCurrentIndex(match.tabIndex);
			}
		}
	}

	updateSearchStatus();

	// Make sure that the geometry of the widgets on the newly shown page is up to date before highlighting
	QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
	highlightSearchMatch(match);
}

void ConfigDialog::highlightSearchMatch(const SearchMatch &match) {
	clearSearchHighlight();

	QWidget *widget = match.widget;
	if (!widget) {
		// The page title matched, there is nothing specific to highlight
		return;
	}

	if (QScrollArea *scrollArea = qobject_cast< QScrollArea * >(qhPages.value(match.page))) {
		scrollArea->ensureWidgetVisible(widget);
	}

	QWidget *base = widget;
	QRect rect    = widget->rect();

	if (match.index.isValid()) {
		if (QAbstractItemView *view = qobject_cast< QAbstractItemView * >(widget)) {
			view->scrollTo(match.index);
			base = view->viewport();
			rect = view->visualRect(match.index);
		}
	} else if (match.tabIndex >= 0) {
		if (QTabBar *tabBar = qobject_cast< QTabBar * >(widget)) {
			rect = tabBar->tabRect(match.tabIndex);
		}
	}

	if (rect.isEmpty()) {
		return;
	}

	rect = QRect(base->mapTo(this, rect.topLeft()), rect.size()).adjusted(-3, -3, 3, 3);
	// Don't draw outside of the area that displays the settings pages
	rect = rect.intersected(QRect(qswPages->mapTo(this, QPoint(0, 0)), qswPages->size()));
	if (rect.isEmpty()) {
		return;
	}

	QFrame *highlight = new QFrame(this);
	highlight->setObjectName(QLatin1String("qfSearchHighlight"));
	highlight->setAttribute(Qt::WA_TransparentForMouseEvents);
	highlight->setFocusPolicy(Qt::NoFocus);
	highlight->setStyleSheet(QString::fromLatin1("QFrame#qfSearchHighlight { border: 2px solid %1; border-radius: 3px; "
												 "background: transparent; }")
								 .arg(palette().color(QPalette::Highlight).name()));
	highlight->setGeometry(rect);
	highlight->show();
	highlight->raise();

	m_searchHighlight = highlight;
	QTimer::singleShot(2000, highlight, &QObject::deleteLater);
}

void ConfigDialog::clearSearchHighlight() {
	if (m_searchHighlight) {
		m_searchHighlight->deleteLater();
		m_searchHighlight = nullptr;
	}
}

bool ConfigDialog::eventFilter(QObject *watched, QEvent *event) {
	if (watched == qleSearch && event->type() == QEvent::KeyPress) {
		QKeyEvent *keyEvent = static_cast< QKeyEvent * >(event);

		switch (keyEvent->key()) {
			case Qt::Key_Escape:
				if (!qleSearch->text().isEmpty()) {
					// Clear the search instead of closing the dialog
					qleSearch->clear();
					return true;
				}
				break;
			case Qt::Key_Return:
			case Qt::Key_Enter: {
				// Don't let Enter trigger the dialog's default (OK) button while searching
				const bool backwards = keyEvent->modifiers() & Qt::ShiftModifier;
				if (m_currentSearchMatch < 0) {
					showSearchMatch(backwards ? -1 : 0);
				} else {
					showSearchMatch(m_currentSearchMatch + (backwards ? -1 : 1));
				}
				return true;
			}
			case Qt::Key_Down:
				qlwIcons->setFocus(Qt::TabFocusReason);
				return true;
			default:
				break;
		}
	}

	return QDialog::eventFilter(watched, event);
}
