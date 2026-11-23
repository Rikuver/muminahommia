// Personal customisation (muminahommia): settings page for features added in this fork.

#include "CustomisationConfig.h"

#include "ChatHistory.h"
#include "Settings.h"

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
}

void CustomisationConfig::save() const {
	s.bChatHistoryEnabled     = qgbChatHistory->isChecked();
	s.iChatHistoryMaxMessages = qsbChatHistoryMax->value();
	s.iChatHistoryRestoreCount = qsbChatHistoryRestore->value();
	s.bChatHistoryStoreImages = qcbChatHistoryImages->isChecked();
}

void CustomisationConfig::on_qpbClearChatHistory_clicked() {
	// Takes effect immediately, it is not part of Apply/Cancel
	ChatHistory::clear();
}
