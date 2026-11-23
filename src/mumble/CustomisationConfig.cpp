// Personal customisation (muminahommia): settings page for features added in this fork.

#include "CustomisationConfig.h"

#include "ChatHistory.h"
#include "Settings.h"

#include <QtWidgets/QFileDialog>

const QString CustomisationConfig::name = QLatin1String("CustomisationConfig");

static ConfigWidget *CustomisationConfigNew(Settings &st) {
	return new CustomisationConfig(st);
}

// Last page in the list
static ConfigRegistrar registrar(6100, CustomisationConfigNew);

CustomisationConfig::CustomisationConfig(Settings &st) : ConfigWidget(st) {
	setupUi(this);
}

QString CustomisationConfig::title() const {
	return tr("Customisation");
}

const QString &CustomisationConfig::getName() const {
	return CustomisationConfig::name;
}

QIcon CustomisationConfig::icon() const {
	return QIcon(QLatin1String("skin:config_basic.png"));
}

void CustomisationConfig::load(const Settings &r) {
	qgbChatHistory->setChecked(r.bChatHistoryEnabled);
	qsbChatHistoryMax->setValue(r.iChatHistoryMaxMessages);
	qsbChatHistoryRestore->setValue(r.iChatHistoryRestoreCount);
	qcbChatHistoryImages->setChecked(r.bChatHistoryStoreImages);
	qleChatHistoryDirectory->setText(r.qsChatHistoryDirectory);
}

void CustomisationConfig::save() const {
	s.bChatHistoryEnabled     = qgbChatHistory->isChecked();
	s.iChatHistoryMaxMessages = qsbChatHistoryMax->value();
	s.iChatHistoryRestoreCount = qsbChatHistoryRestore->value();
	s.bChatHistoryStoreImages = qcbChatHistoryImages->isChecked();
	s.qsChatHistoryDirectory  = qleChatHistoryDirectory->text().trimmed();
}

void CustomisationConfig::on_qpbChatHistoryBrowse_clicked() {
	const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose save folder"),
														  qleChatHistoryDirectory->text());
	if (!dir.isEmpty()) {
		qleChatHistoryDirectory->setText(dir);
	}
}

void CustomisationConfig::on_qpbClearChatHistory_clicked() {
	// Takes effect immediately, it is not part of Apply/Cancel
	ChatHistory::clear();
}
