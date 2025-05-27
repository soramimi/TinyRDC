#include "ConnectionDialog.h"
#include "ui_ConnectionDialog.h"

ConnectionDialog::ConnectionDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::ConnectionDialog)
{
	ui->setupUi(this);

	ui->lineEdit_host->setText("192.168.0.20");
	ui->lineEdit_domain->setText("WORKGROUP");
	ui->lineEdit_username->setFocus();
}

ConnectionDialog::~ConnectionDialog()
{
	delete ui;
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
