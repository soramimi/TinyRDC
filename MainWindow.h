#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDebug>
#include <QImage>
#include <QInputDialog>
#include <QMainWindow>
#include <QMessageBox>
#include <QTimer>
#include <freerdp/channels/channels.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/primary.h>
#include <thread>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
	Q_OBJECT
private:
	Ui::MainWindow *ui;
	struct Private;
	struct Private *m;
	
	// FreeRDPコールバック関数
	static BOOL rdp_pre_connect(freerdp *instance);
	static BOOL rdp_post_connect(freerdp *instance);
	static void rdp_post_disconnect(freerdp *instance);
	static BOOL rdp_authenticate(freerdp *instance, char **username, char **password, char **domain);
	static BOOL rdp_begin_paint(rdpContext *context);
	static BOOL rdp_end_paint(rdpContext *context);

	void doConnect(const QString &hostname, const QString &username, const QString &password, const QString &domain);
	void doDisconnect();
	BOOL onRdpPostConnect(freerdp *instance);
	void start_rdp_thread();
protected:
	void closeEvent(QCloseEvent *event);
public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow();
private slots:
	void on_action_connect_triggered();
	void on_action_disconnect_triggered();
	void updateScreen();
signals:
	void requestUpdateScreen();
};
#endif // MAINWINDOW_H
