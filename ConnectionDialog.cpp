#include "ConnectionDialog.h"
#include "ui_ConnectionDialog.h"

ConnectionDialog::ConnectionDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::ConnectionDialog)
{
	ui->setupUi(this);
}

ConnectionDialog::~ConnectionDialog()
{
	delete ui;
}

void ConnectionDialog::setCredential(const Credential &cred)
{
	ui->lineEdit_host->setText(cred.hostname);
	ui->lineEdit_domain->setText(cred.domain);
	ui->lineEdit_username->setText(cred.username);
	ui->lineEdit_password->setText(cred.password);

	if (ui->lineEdit_host->text().isEmpty()) {
		ui->lineEdit_host->setFocus();
	} else if (ui->lineEdit_username->text().isEmpty()) {
		ui->lineEdit_username->setFocus();
	} else if (ui->lineEdit_password->text().isEmpty()) {
		ui->lineEdit_password->setFocus();
	}
}

QString ConnectionDialog::hostname() const
{
	return ui->lineEdit_host->text();
}

QString ConnectionDialog::domain() const
{
	return ui->lineEdit_domain->text();
}

QString ConnectionDialog::username() const
{
	return ui->lineEdit_username->text();
}

QString ConnectionDialog::password() const
{
	return ui->lineEdit_password->text();
}
