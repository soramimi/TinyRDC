#ifndef CONNECTIONDIALOG_H
#define CONNECTIONDIALOG_H

#include <QDialog>

namespace Ui {
class ConnectionDialog;
}

class ConnectionDialog : public QDialog {
	Q_OBJECT
public:
	struct Credential {
		QString hostname;
		QString domain;
		QString username;
		QString password;
	};
	explicit ConnectionDialog(QWidget *parent = nullptr);
	~ConnectionDialog();

	void setCredential(Credential const &cred);

	QString hostname() const;
	QString domain() const;
	QString username() const;
	QString password() const;
private:
	Ui::ConnectionDialog *ui;
};

#endif // CONNECTIONDIALOG_H
