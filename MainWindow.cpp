#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ConnectionDialog.h"
#include <QPainter>

MainWindow *g_mainwindow = nullptr;

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, rdp_instance(nullptr)
	, rdp_context(nullptr)
	, update_timer(new QTimer(this))
	, connected(false)
{
	ui->setupUi(this);
	g_mainwindow = this;

	update_timer->setInterval(16);
	connect(update_timer, &QTimer::timeout, this, &MainWindow::onUpdateTimer);

	// フォーカスポリシーの設定
	ui->widget_view->setFocusPolicy(Qt::StrongFocus);
	setFocusPolicy(Qt::StrongFocus);
}

MainWindow::~MainWindow()
{
	doDisconnect();
	g_mainwindow = nullptr;
	delete ui;
}

void MainWindow::doConnect(const QString &hostname, const QString &username, const QString &password, const QString &domain)
{
	if (connected) {
		doDisconnect();
	}

	// FreeRDPインスタンスの作成
	rdp_instance = freerdp_new();
	if (!rdp_instance) {
		QMessageBox::critical(this, "Error", "Failed to create FreeRDP instance");
		return;
	}

	freerdp_context_new(rdp_instance);

	rdp_context = rdp_instance->context;

	// コールバック関数の設定
	rdp_instance->PreConnect = rdp_pre_connect;
	rdp_instance->PostConnect = rdp_post_connect;
	rdp_instance->PostDisconnect = rdp_post_disconnect;
	rdp_instance->Authenticate = rdp_authenticate;

	// 接続設定
	rdpSettings *settings = rdp_instance->context->settings;
	freerdp_settings_set_string(settings, FreeRDP_ServerHostname, hostname.toUtf8().constData());
	freerdp_settings_set_string(settings, FreeRDP_Username, username.toUtf8().constData());
	freerdp_settings_set_string(settings, FreeRDP_Password, password.toUtf8().constData());
	freerdp_settings_set_string(settings, FreeRDP_Domain, domain.toUtf8().constData());
	freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, width_);
	freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, height_);
	freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32);

	// 安全なパフォーマンス最適化設定のみ適用
	freerdp_settings_set_bool(settings, FreeRDP_FastPathOutput, TRUE);
	freerdp_settings_set_bool(settings, FreeRDP_FastPathInput, TRUE);
	freerdp_settings_set_bool(settings, FreeRDP_BitmapCacheEnabled, TRUE);
	freerdp_settings_set_uint32(settings, FreeRDP_CompressionLevel, 1);
	freerdp_settings_set_uint32(settings, FreeRDP_OffscreenSupportLevel, 1);
	freerdp_settings_set_uint32(settings, FreeRDP_GlyphSupportLevel, 1);
	freerdp_settings_set_bool(settings, FreeRDP_SurfaceCommandsEnabled, TRUE);
	freerdp_settings_set_bool(settings, FreeRDP_NetworkAutoDetect, TRUE);

	// 接続実行
	if (freerdp_connect(rdp_instance)) {
		connected = true;
		first_update = true;  // 新しい接続時は初回更新フラグをリセット
		update_timer->start();
		ui->widget_view->setRdpInstance(rdp_instance);
		statusBar()->showMessage("Connected to " + hostname);
	} else {
		QMessageBox::critical(this, "Error", "Failed to connect to " + hostname);
		freerdp_free(rdp_instance);
		rdp_instance = nullptr;
		rdp_context = nullptr;
	}
}

void MainWindow::doDisconnect()
{
	if (connected && rdp_instance) {
		update_timer->stop();
		freerdp_disconnect(rdp_instance);
		freerdp_free(rdp_instance);
		rdp_instance = nullptr;
		rdp_context = nullptr;
		connected = false;
		ui->widget_view->setRdpInstance(nullptr);
		statusBar()->showMessage("Disconnected");
	}
}

void MainWindow::updateScreen()
{
	if (connected && rdp_context && rdp_context->gdi) {
		rdpGdi *gdi = rdp_context->gdi;
		if (gdi->primary_buffer) {
			BYTE *data = gdi->primary_buffer;
			int width = gdi->width;
			int height = gdi->height;
			int stride = gdi->stride;

			ui->widget_view->setImage(image_);
		}
	}
}

void MainWindow::on_action_connect_triggered()
{
	ConnectionDialog dlg;
	if (dlg.exec() == QDialog::Accepted) {
		QString hostname = dlg.hostname();
		QString username = dlg.username();
		QString password = dlg.password();
		QString domain = dlg.domain();
		doConnect(hostname, username, password, domain);
		return;
	}
}

void MainWindow::on_action_disconnect_triggered()
{
	doDisconnect();
}

void MainWindow::onUpdateTimer()
{
	if (connected && rdp_instance) {
		// イベント処理
		HANDLE handles[64];
		DWORD count = freerdp_get_event_handles(rdp_context, handles, 64);
		if (count > 0) {
			if (WaitForMultipleObjects(count, handles, FALSE, 0) != WAIT_TIMEOUT) {
				if (!freerdp_check_event_handles(rdp_context)) {
					doDisconnect();
					return;
				}
			}
		}
		updateScreen();
	}
}

// FreeRDPコールバック関数の実装
BOOL MainWindow::rdp_pre_connect(freerdp *instance)
{
	Q_UNUSED(instance);
	return TRUE;
}

BOOL MainWindow::onRdpPostConnect(freerdp *instance)
{
	image_ = QImage(width_, height_, QImage::Format_RGBX8888);

	// GDIの初期化
	if (!gdi_init_ex(instance, PIXEL_FORMAT_RGBX32, image_.bytesPerLine(), image_.bits(), nullptr)) {
		return FALSE;
	}

	return TRUE;
}

BOOL MainWindow::rdp_post_connect(freerdp *instance)
{
	if (g_mainwindow) {
		return g_mainwindow->onRdpPostConnect(instance);
	}
	return FALSE;
}

void MainWindow::rdp_post_disconnect(freerdp *instance)
{
	if (instance->context->gdi) {
		gdi_free(instance);
		instance->context->gdi = nullptr;
	}
}

BOOL MainWindow::rdp_authenticate(freerdp *instance, char **username, char **password, char **domain)
{
	Q_UNUSED(instance);
	Q_UNUSED(username);
	Q_UNUSED(password);
	Q_UNUSED(domain);
	return TRUE;
}

BOOL MainWindow::rdp_begin_paint(rdpContext *context)
{
	Q_UNUSED(context);
	return TRUE;
}

BOOL MainWindow::rdp_end_paint(rdpContext *context)
{
	Q_UNUSED(context);
	if (g_mainwindow) {
		g_mainwindow->updateScreen();
	}
	return TRUE;
}
