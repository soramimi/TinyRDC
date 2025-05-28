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
	freerdp *rdp_instance_;
	rdpContext *rdp_context_;
	QTimer *update_timer_;
	bool connected_;
	int width_ = 1920;
	int height_ = 1080;
	QImage image_;
	std::thread rdp_thread_;
	bool interrupted_ = false;
	
	// FreeRDPコールバック関数
	static BOOL rdp_pre_connect(freerdp *instance);
	static BOOL rdp_post_connect(freerdp *instance);
	static void rdp_post_disconnect(freerdp *instance);
	static BOOL rdp_authenticate(freerdp *instance, char **username, char **password, char **domain);
	static BOOL rdp_begin_paint(rdpContext *context);
	static BOOL rdp_end_paint(rdpContext *context);

	void doConnect(const QString &hostname, const QString &username, const QString &password, const QString &domain);
	void doDisconnect();
	void updateScreen();
	BOOL onRdpPostConnect(freerdp *instance);
	void start_rdp_thread();
public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow();

private slots:
	void on_action_connect_triggered();
	void on_action_disconnect_triggered();
	void onUpdateTimer();

	// QWidget interface
protected:
	void closeEvent(QCloseEvent *event);
};
#endif // MAINWINDOW_H
