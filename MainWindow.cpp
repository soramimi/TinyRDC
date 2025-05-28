#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ConnectionDialog.h"
#include <QPainter>

MainWindow *g_mainwindow = nullptr;

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, rdp_instance_(nullptr)
	, rdp_context_(nullptr)
	, update_timer_(new QTimer(this))
	, connected_(false)
{
	ui->setupUi(this);
	g_mainwindow = this;

	update_timer_->setInterval(16);
	connect(update_timer_, &QTimer::timeout, this, &MainWindow::onUpdateTimer);

	// フォーカスポリシーの設定
	ui->widget_view->setFocusPolicy(Qt::StrongFocus);
	setFocusPolicy(Qt::StrongFocus);

	start_rdp_thread();
}

MainWindow::~MainWindow()
{
	doDisconnect();
	g_mainwindow = nullptr;
	delete ui;
}

void MainWindow::doConnect(const QString &hostname, const QString &username, const QString &password, const QString &domain)
{
	if (connected_) {
		doDisconnect();
	}

	// FreeRDPインスタンスの作成
	rdp_instance_ = freerdp_new();
	if (!rdp_instance_) {
		QMessageBox::critical(this, "Error", "Failed to create FreeRDP instance");
		return;
	}

	freerdp_context_new(rdp_instance_);

	rdp_context_ = rdp_instance_->context;

	// コールバック関数の設定
	rdp_instance_->PreConnect = rdp_pre_connect;
	rdp_instance_->PostConnect = rdp_post_connect;
	rdp_instance_->PostDisconnect = rdp_post_disconnect;
	rdp_instance_->Authenticate = rdp_authenticate;

	// 接続設定
	rdpSettings *settings = rdp_instance_->context->settings;
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
	if (freerdp_connect(rdp_instance_)) {
		connected_ = true;
		update_timer_->start();
		ui->widget_view->setRdpInstance(rdp_instance_);
		statusBar()->showMessage("Connected to " + hostname);
	} else {
		QMessageBox::critical(this, "Error", "Failed to connect to " + hostname);
		freerdp_free(rdp_instance_);
		rdp_instance_ = nullptr;
		rdp_context_ = nullptr;
	}
}

void MainWindow::doDisconnect()
{
	if (connected_ && rdp_instance_) {
		update_timer_->stop();
		freerdp_disconnect(rdp_instance_);
		freerdp_free(rdp_instance_);
		rdp_instance_ = nullptr;
		rdp_context_ = nullptr;
		connected_ = false;
		ui->widget_view->setRdpInstance(nullptr);
		statusBar()->showMessage("Disconnected");
	}
}

void MainWindow::updateScreen()
{
	if (connected_ && rdp_context_ && rdp_context_->gdi) {
		rdpGdi *gdi = rdp_context_->gdi;
		if (gdi->primary_buffer) {
			// BYTE *data = gdi->primary_buffer;
			// int width = gdi->width;
			// int height = gdi->height;
			// int stride = gdi->stride;

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

void MainWindow::start_rdp_thread()
{
	rdp_thread_ = std::thread([this]() {
		while (true) {
			if (interrupted_) break;
			if (rdp_instance_ && connected_) {
				// イベント処理
				HANDLE handles[64];
				DWORD count = freerdp_get_event_handles(rdp_context_, handles, 64);
				if (WaitForMultipleObjects(count, handles, FALSE, 5) == WAIT_FAILED) {
					break;
				}
				if (!freerdp_check_event_handles(rdp_context_)) {
					break;
				}
			} else {
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
		}
		doDisconnect();
	});
}

void MainWindow::onUpdateTimer()
{
	updateScreen();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	interrupted_ = true;
	if (rdp_thread_.joinable()) {
		rdp_thread_.join();
	}
	QMainWindow::closeEvent(event);
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
