#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ConnectionDialog.h"
#include "MySettings.h"
#include <QPainter>
#include <QWindow>
#include <thread>
#include <mutex>
#include <freerdp/client/disp.h>
#include <freerdp/client/cliprdr.h>

MainWindow *g_mainwindow = nullptr;

struct MainWindow::Private {
	freerdp *rdp_instance = nullptr;
	QTimer update_timer;
	bool connected = false;
	int width = 1920;
	int height = 1080;
	QImage image;
	std::thread rdp_thread;
	std::mutex rdp_mutex;
	bool interrupted = false;
	int dynamic_resize_counter = 0;
};

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, m(new Private)
	, ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	g_mainwindow = this;

	qApp->installEventFilter(this);

	connect(&m->update_timer, &QTimer::timeout, this, &MainWindow::onIntervalTimer);
	m->update_timer.setInterval(10);
	m->update_timer.start();

	connect(this, &MainWindow::requestUpdateScreen, this, &MainWindow::updateScreen);


	{
		Qt::WindowStates state = windowState();
		MySettings settings;

		settings.beginGroup("MainWindow");
		bool maximized = settings.value("Maximized").toBool();
		restoreGeometry(settings.value("Geometry").toByteArray());
		settings.endGroup();
		if (maximized) {
			state |= Qt::WindowMaximized;
			setWindowState(state);
		}
	}

	// フォーカスポリシーの設定
	ui->widget_view->setFocusPolicy(Qt::StrongFocus);
	setFocusPolicy(Qt::StrongFocus);
}

MainWindow::~MainWindow()
{
	doDisconnect();
	g_mainwindow = nullptr;
	delete m;
	delete ui;
}

void MainWindow::doConnect(const QString &hostname, const QString &username, const QString &password, const QString &domain)
{
	if (m->connected) {
		doDisconnect();
	}

	m->interrupted = false;

	// FreeRDPインスタンスの作成
	m->rdp_instance = freerdp_new();
	if (!m->rdp_instance) {
		QMessageBox::critical(this, "Error", "Failed to create FreeRDP instance");
		return;
	}

	freerdp_context_new(m->rdp_instance);

	// コールバック関数の設定
	m->rdp_instance->PreConnect = rdp_pre_connect;
	m->rdp_instance->PostConnect = rdp_post_connect;
	m->rdp_instance->PostDisconnect = rdp_post_disconnect;
	m->rdp_instance->Authenticate = rdp_authenticate;

	// 接続設定
	rdpSettings *settings = m->rdp_instance->context->settings;
	freerdp_settings_set_string(settings, FreeRDP_ServerHostname, hostname.toUtf8().constData());
	freerdp_settings_set_string(settings, FreeRDP_Username, username.toUtf8().constData());
	freerdp_settings_set_string(settings, FreeRDP_Password, password.toUtf8().constData());
	freerdp_settings_set_string(settings, FreeRDP_Domain, domain.toUtf8().constData());
	freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, m->width);
	freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, m->height);
	freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32);

	// 安全なパフォーマンス最適化設定のみ適用
	freerdp_settings_set_bool(settings, FreeRDP_FastPathOutput, TRUE);
	freerdp_settings_set_bool(settings, FreeRDP_FastPathInput, TRUE);
	freerdp_settings_set_bool(settings, FreeRDP_BitmapCacheEnabled, TRUE);
	freerdp_settings_set_uint32(settings, FreeRDP_CompressionLevel, PACKET_COMPR_TYPE_RDP8);
	freerdp_settings_set_uint32(settings, FreeRDP_OffscreenSupportLevel, 1);
	freerdp_settings_set_uint32(settings, FreeRDP_GlyphSupportLevel, 1);
	freerdp_settings_set_bool(settings, FreeRDP_SurfaceCommandsEnabled, TRUE);
	freerdp_settings_set_bool(settings, FreeRDP_NetworkAutoDetect, TRUE);

	// 接続実行
	if (freerdp_connect(m->rdp_instance)) {
		m->connected = true;
		m->update_timer.start();
		ui->widget_view->setRdpInstance(m->rdp_instance);

		start_rdp_thread();

		statusBar()->showMessage("Connected to " + hostname);
	} else {
		QMessageBox::critical(this, "Error", "Failed to connect to " + hostname);
		freerdp_free(m->rdp_instance);
		m->rdp_instance = nullptr;
	}
}

void MainWindow::doDisconnect()
{
	ui->widget_view->setRdpInstance(nullptr);
	m->update_timer.stop();

	m->interrupted = true;
	if (m->rdp_thread.joinable()) {
		m->rdp_thread.join();
	}

	if (m->rdp_instance) {
		freerdp_disconnect(m->rdp_instance);
		freerdp_free(m->rdp_instance);
		m->rdp_instance = nullptr;
	}
	m->connected = false;
	statusBar()->showMessage("Disconnected");

	QImage image(m->width, m->height, QImage::Format_RGB888);
	image.fill(Qt::black);
	m->image = image;
	ui->widget_view->setImage(m->image);

}

void MainWindow::onIntervalTimer()
{
	if (m->interrupted) return;
	if (!m->connected) return;

	if (m->dynamic_resize_counter > 0) {
		m->dynamic_resize_counter--;
		if (m->dynamic_resize_counter == 0) {
			resizeDynamic();
			return;
		}
	}
}

void MainWindow::updateScreen()
{
	if (m->interrupted) return;
	if (!m->connected) return;

	QImage image;
	std::swap(image, m->image);
	if (!image.isNull()) {
		ui->widget_view->setImage(image);
	}
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == windowHandle()) {
		if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
			bool press = (event->type() == QEvent::KeyPress);
			QKeyEvent *e = static_cast<QKeyEvent *>(event);
			int key = e->key();
			Qt::KeyboardModifiers mod = e->modifiers();
			qDebug() << Q_FUNC_INFO << QString::asprintf("%08x", key) << mod;
			if (key == Qt::Key_F) {
				if (press && (e->modifiers() & Qt::KeyboardModifierMask) == (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier)) {
					// Ctrl+Fでフルスクリーン切り替え
					if (isFullScreen()) {
						menuBar()->setVisible(true);
						statusBar()->setVisible(true);
						showNormal();
					} else {
						menuBar()->setVisible(false);
						statusBar()->setVisible(false);
						showFullScreen();
					}
					return true; // イベントを処理済みとしてマーク
				}
			} else if (key == Qt::Key_D) {
				if (press && (e->modifiers() & Qt::KeyboardModifierMask) == (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier)) {
					if (ui->widget_view->scale() == 1) {
						ui->widget_view->setScale(2);
					} else {
						ui->widget_view->setScale(1);
					}
					return true;
				}
			}
			if (ui->widget_view->onKeyEvent(e)) return true;
		}
	}
#if 0
	if (event->type() == QEvent::ShortcutOverride) {
		qDebug() << event->type() << "ShortcutOverride";
		event->accept();
		return true;
	}
#endif
	return false;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
	QMainWindow::resizeEvent(event);
	resizeDynamicLater();
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
	m->rdp_thread = std::thread([this]() {
		while (true) {
			if (m->interrupted) break;
			int count = 0;
			if (m->rdp_instance && m->connected) {
				// イベント処理
				HANDLE handles[64];
				count = freerdp_get_event_handles(m->rdp_instance->context, handles, 64);
				auto r = WaitForMultipleObjects(count, handles, FALSE, 100);
				if (r == WAIT_FAILED) {
					break;
				}
				if (!freerdp_check_event_handles(m->rdp_instance->context)) {
					break;
				}
				QImage new_image;
				if (m->image.isNull()) {
					std::lock_guard lock(m->rdp_mutex);
					rdpGdi *gdi = m->rdp_instance->context->gdi;
					if (gdi->primary_buffer) {
						BYTE *data = gdi->primary_buffer;
						int width = gdi->width;
						int height = gdi->height;
						int stride = gdi->stride;
						qDebug() << "thread" << width << height;
						new_image = QImage(data, width, height, stride, QImage::Format_RGB888);
					}
				}
				if (!new_image.isNull()) {
					m->image = new_image;
					emit requestUpdateScreen();
				}
			}
			if (count == 0) {
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
		}
	});
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	if (isFullScreen()) {
		event->ignore();
		return;
	}

	if (m->connected) {
		if (QMessageBox::question(this, "Confirm Disconnect", "Are you sure you want to close Remote Desktop Client?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
			event->ignore();
			return;
		}
	}

	doDisconnect();

	{
		MySettings settings;
		setWindowOpacity(0);
		Qt::WindowStates state = windowState();
		bool maximized = (state & Qt::WindowMaximized) != 0;
		if (maximized) {
			state &= ~Qt::WindowMaximized;
			setWindowState(state);
		}
		{
			settings.beginGroup("MainWindow");
			settings.setValue("Maximized", maximized);
			settings.setValue("Geometry", saveGeometry());
			settings.endGroup();
		}
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
	// GDIの初期化
	if (!gdi_init(instance, PIXEL_FORMAT_RGB24)) {
		return FALSE;
	}
	if (isDynamicResizingEnabled()) {
		resizeDynamicLater();
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

bool MainWindow::isDynamicResizingEnabled() const
{
	return ui->action_view_dynamic_resolusion->isChecked();
}

void MainWindow::on_action_view_dynamic_resolusion_toggled(bool arg1)
{
	(void)arg1;
	if (isDynamicResizingEnabled()) {
		resizeDynamic();
	}
}

void MainWindow::resizeDynamicLater()
{
	if (isDynamicResizingEnabled()) {
		m->dynamic_resize_counter = 50;
	}
}

void MainWindow::resizeDynamic()
{
	if (m->interrupted) return;
	if (!m->connected) return;

	bool enabled = isDynamicResizingEnabled();
	if (!enabled) return;

	int scale = ui->widget_view->scale();
	int w = ui->widget_view->width();
	int h = ui->widget_view->height();
	{
		std::lock_guard<std::mutex> lock(m->rdp_mutex);
		rdpGdi *gdi = m->rdp_instance->context->gdi;
		int w = std::max(w / scale, 640);
		int h = std::max(h / scale, 480);
		qDebug() << "resize" << w << h;
		gdi_resize(gdi, w, h);
	}
}

