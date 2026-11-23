// Personal customisation (muminahommia): settings page for features added in this fork.

#ifndef MUMBLE_MUMBLE_CUSTOMISATIONCONFIG_H_
#define MUMBLE_MUMBLE_CUSTOMISATIONCONFIG_H_

#include "ConfigDialog.h"

#include "ui_CustomisationConfig.h"

class CustomisationConfig : public ConfigWidget, Ui::CustomisationConfig {
private:
	Q_OBJECT
	Q_DISABLE_COPY(CustomisationConfig)
public:
	/// The unique name of this ConfigWidget
	static const QString name;
	CustomisationConfig(Settings &st);
	QString title() const Q_DECL_OVERRIDE;
	const QString &getName() const Q_DECL_OVERRIDE;
	QIcon icon() const Q_DECL_OVERRIDE;

public slots:
	void save() const Q_DECL_OVERRIDE;
	void load(const Settings &r) Q_DECL_OVERRIDE;
	void on_qpbClearChatHistory_clicked();
	void on_qpbChatHistoryBrowse_clicked();
};

#endif
