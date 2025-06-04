#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ConnectionDialog.h"
#include "MySettings.h"
#include <QPainter>
#include <QWindow>
#include <thread>
#include <mutex>
#include "Global.h"

struct MyClientContext {
	rdpClientContext rdpcc;
	MainWindow *self = nullptr;
	DispClientContext *disp = nullptr;
};

class Session {
public:
	enum Version {
		V1,
		V2,
	};
#if 0
	Version version() { return V1; }

	freerdp *rdp = nullptr;

	void context_new(MainWindow *self)
	{
		rdp = freerdp_new();
		freerdp_context_new(rdp);
	}

	void context_free()
	{
		freerdp_free(rdp);
		rdp = nullptr;
	}

	freerdp *rdp_instance()
	{
		return rdp;
	}

	s_disp_client_context *disp_client_context()
	{
		return nullptr;
	}
#else
	Version version() { return V2; }

	union {
		rdpContext *rdp;
		MyClientContext *cc;
	} d = {};

	void context_new(MainWindow *self)
	{
		RDP_CLIENT_ENTRY_POINTS entry = {};
		entry.Version = RDP_CLIENT_INTERFACE_VERSION;
		entry.Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
		entry.ContextSize = sizeof(MyClientContext);
		// entry.GlobalInit = clientGlobalInit;
		// entry.GlobalUninit = clientGlobalUninit;
		// entry.ClientNew = clientContextNew;
		// entry.ClientFree = clientContextFree;
		// entry.ClientStart = clientContextStart;
		// entry.ClientStop = clientContextStop;

		d.rdp = freerdp_client_context_new(&entry);
		d.cc->self = self;

		d.rdp->update->EndPaint = self->rdp_end_paint;
	}

	void context_free()
	{
		freerdp_client_context_free(d.rdp);
		d.rdp = nullptr;
	}

	freerdp *rdp_instance()
	{
		return d.rdp ? d.rdp->instance : nullptr;
	}

	s_disp_client_context *disp_client_context()
	{
		return d.cc->disp;
	}
#endif
	rdpSettings *rdp_settings()
	{
		auto *inst = rdp_instance();
		return (inst && inst->context) ? inst->context->settings : nullptr;
	}

	rdpGdi *rdp_gdi()
	{
		auto *inst = rdp_instance();
		return (inst && inst->context) ? inst->context->gdi : nullptr;
	}
};

struct MainWindow::Private {
	Session session;
	QTimer update_timer;
	bool connected = false;
	int width = 1280;
	int height = 720;
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
	delete m;
	delete ui;
}

freerdp *MainWindow::rdp_instance()
{
	return m->session.rdp_instance();
}

rdpSettings *MainWindow::rdp_settings()
{
	return m->session.rdp_settings();
}

s_disp_client_context *MainWindow::disp_client_context()
{
	return m->session.disp_client_context();
}

rdpGdi *MainWindow::rdp_gdi()
{
	return m->session.rdp_gdi();
}

void MainWindow::doConnect(const QString &hostname, const QString &username, const QString &password, const QString &domain)
{
	if (m->connected) {
		doDisconnect();
	}

	// 動的解像度が有効な場合は、現在のビューサイズに合わせる
	if (isDynamicResizingEnabled()) {
		int scale = ui->widget_view->scale();
		int w = ui->widget_view->width();
		int h = ui->widget_view->height();
		m->width = std::clamp(w / scale, DISPLAY_CONTROL_MIN_MONITOR_WIDTH, DISPLAY_CONTROL_MAX_MONITOR_WIDTH);
		m->height = std::clamp(h / scale, DISPLAY_CONTROL_MIN_MONITOR_HEIGHT, DISPLAY_CONTROL_MAX_MONITOR_HEIGHT);
		qDebug() << "Initial size for dynamic resolution:" << m->width << "x" << m->height;
	}

	m->interrupted = false;

	m->session.context_new(this);

	// コールバック関数の設定
	rdp_instance()->PreConnect = rdp_pre_connect;
	rdp_instance()->PostConnect = rdp_post_connect;
	rdp_instance()->PostDisconnect = rdp_post_disconnect;
	rdp_instance()->Authenticate = rdp_authenticate;

	// 接続設定
	rdpSettings *settings = rdp_instance()->context->settings;
	freerdp_settings_set_string(settings, FreeRDP_ServerHostname, hostname.toUtf8().constData());
	freerdp_settings_set_string(settings, FreeRDP_Username, username.toUtf8().constData());
	freerdp_settings_set_string(settings, FreeRDP_Password, password.toUtf8().constData());
	freerdp_settings_set_string(settings, FreeRDP_Domain, domain.toUtf8().constData());
	freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, m->width);
	freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, m->height);

	if (m->session.version() == Session::V2) {
		// Display拡張を有効化（動的解像度変更のため）
		freerdp_settings_set_bool(settings, FreeRDP_SupportDisplayControl, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_DynamicResolutionUpdate, TRUE);
	}

	// 安全なパフォーマンス最適化設定のみ適用
	freerdp_settings_set_bool(settings, FreeRDP_FastPathOutput, TRUE);
	freerdp_settings_set_bool(settings, FreeRDP_FastPathInput, TRUE);
	freerdp_settings_set_bool(settings, FreeRDP_BitmapCacheEnabled, TRUE);
	freerdp_settings_set_uint32(settings, FreeRDP_CompressionLevel, PACKET_COMPR_TYPE_RDP8);
	freerdp_settings_set_uint32(settings, FreeRDP_OffscreenSupportLevel, 1);
	freerdp_settings_set_uint32(settings, FreeRDP_GlyphSupportLevel, 1);
	freerdp_settings_set_bool(settings, FreeRDP_SurfaceCommandsEnabled, TRUE);
	freerdp_settings_set_bool(settings, FreeRDP_NetworkAutoDetect, TRUE);

#if 0
	freerdp_settings_set_bool(settings, FreeRDP_SupportGraphicsPipeline, true);
#endif
	freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, true);
	freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444v2, true);
	freerdp_settings_set_bool(settings, FreeRDP_GfxH264, true);
	freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, true);
	freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32);

	// 接続実行
	if (freerdp_connect(rdp_instance())) {
		m->connected = true;
		m->update_timer.start();
		ui->widget_view->setRdpInstance(rdp_instance());

		start_rdp_thread();

		statusBar()->showMessage("Connected to " + hostname);
	} else {
		QMessageBox::critical(this, "Error", "Failed to connect to " + hostname);
		m->session.context_free();
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

	if (rdp_instance()) {
		freerdp_disconnect(rdp_instance());
		m->session.context_free();
	}
	m->connected = false;
	statusBar()->showMessage("Disconnected");

	QImage image(m->width, m->height, QImage::Format_RGBX8888);
	image.fill(Qt::black);
	m->image = image;
	if (m->session.version() == Session::V2) {
		image = image.copy();
	}
	ui->widget_view->setImage(image, QRect{});

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
		if (m->session.version() == Session::V1) {
			qDebug() << Q_FUNC_INFO;
			ui->widget_view->setImage(image, QRect{});
		}
	}
}

void MainWindow::updateScreen2(QImage const &image, QRect const &rect)
{
	if (m->interrupted) return;
	if (!m->connected) return;

	if (!image.isNull()) {
		if (m->session.version() == Session::V2) {
			qDebug() << Q_FUNC_INFO;
			ui->widget_view->setImage(image, rect);
		}
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
			// qDebug() << Q_FUNC_INFO << QString::asprintf("%08x", key) << mod;
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
					if (isDynamicResizingEnabled()) {
						resizeDynamicLater();
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
			if (rdp_instance() && m->connected) {
				// イベント処理
				HANDLE handles[64];
				count = freerdp_get_event_handles(rdp_instance()->context, handles, 64);
				auto r = WaitForMultipleObjects(count, handles, FALSE, 100);
				if (r == WAIT_FAILED) {
					break;
				}
				if (!freerdp_check_event_handles(rdp_instance()->context)) {
					break;
				}
				if (m->session.version() == Session::V1) {
					QImage new_image;
					if (m->image.isNull()) {
						std::lock_guard lock(m->rdp_mutex);
						auto *gdi = rdp_gdi();
						if (gdi->primary_buffer) {
							BYTE *data = gdi->primary_buffer;
							int width = gdi->width;
							int height = gdi->height;
							int stride = gdi->stride;
							new_image = QImage(data, width, height, stride, QImage::Format_RGBX8888);
						}
					}
					if (!new_image.isNull()) {
						m->image = new_image;
						emit requestUpdateScreen();
					}
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
BOOL MainWindow::rdp_pre_connect(freerdp *rdp)
{
	auto ctx = rdp->context;
	int r = 0;
	r = PubSub_SubscribeChannelConnected(ctx->pubSub, channelConnected);
	r = PubSub_SubscribeChannelDisconnected(ctx->pubSub, channelDisconnected);
	return TRUE;
}

BOOL MainWindow::onRdpPostConnect(freerdp *rdp)
{
	if (m->session.version() == Session::V1) {
		if (!gdi_init(rdp, PIXEL_FORMAT_RGBX32)) {
			return FALSE;
		}
	} else if (m->session.version() == Session::V2) {
		m->image = QImage(m->width, m->height, QImage::Format_RGBX8888);
		if (!gdi_init_ex(rdp, PIXEL_FORMAT_RGBX32, m->image.bytesPerLine(), m->image.bits(), nullptr)) {
			return FALSE;
		}
		if (isDynamicResizingEnabled()) {
			resizeDynamicLater();
		}
	}
	return TRUE;
}

BOOL MainWindow::rdp_post_connect(freerdp *instance)
{
	if (global->mainwindow) {
		return global->mainwindow->onRdpPostConnect(instance);
	}
	return FALSE;
}

void MainWindow::rdp_post_disconnect(freerdp *instance)
{
	gdi_free(instance);
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
	MyClientContext *ctx = reinterpret_cast<MyClientContext *>(context);
	MainWindow *self = ctx->self;
	auto *gdi = self->rdp_gdi();
	if (!gdi || !gdi->primary) return FALSE;

	auto invalid = gdi->primary->hdc->hwnd->invalid;
	QRect rect(invalid->x, invalid->y, invalid->w, invalid->h);

	self->updateScreen2(self->m->image, rect);

	return TRUE;
}

bool MainWindow::isDynamicResizingEnabled() const
{
	return ui->action_view_dynamic_resolution->isChecked();
}

void MainWindow::on_action_view_dynamic_resolution_toggled(bool arg1)
{
	(void)arg1;
	if (isDynamicResizingEnabled()) {
		resizeDynamicLater();
	}
}

void MainWindow::resizeDynamicLater()
{
	if (isDynamicResizingEnabled()) {
		m->dynamic_resize_counter = 50;
	}
}

void MainWindow::resizeDynamic(int new_width, int new_height)
{
	if (!rdp_instance() || !rdp_instance()->context) return;

	auto *settings = rdp_settings();
	auto *disp = disp_client_context();
	if (settings && disp && disp->DisplayControlCaps) {
		qDebug() << Q_FUNC_INFO;
		std::lock_guard lock(m->rdp_mutex);

		DISPLAY_CONTROL_MONITOR_LAYOUT layout = { 0 };
		layout.Flags = DISPLAY_CONTROL_MONITOR_PRIMARY;
		layout.Left = 0;
		layout.Top = 0;
		layout.Width = new_width;
		layout.Height = new_height;
		layout.PhysicalWidth = new_width;
		layout.PhysicalHeight = new_height;
		layout.Orientation = freerdp_settings_get_uint16(settings, FreeRDP_DesktopOrientation);
		layout.DesktopScaleFactor = freerdp_settings_get_uint32(settings, FreeRDP_DesktopScaleFactor);
		layout.DeviceScaleFactor = freerdp_settings_get_uint32(settings, FreeRDP_DeviceScaleFactor);

		disp->SendMonitorLayout(disp, 1, &layout);

		freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, new_width);
		freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, new_height);

		auto gdi = rdp_gdi();
		if (gdi) {
			if (m->session.version() == Session::V1) {
				gdi_resize(gdi, new_width, new_height);
			} else if (m->session.version() == Session::V2) {
				m->image = QImage(new_width, new_height, QImage::Format_RGBX8888);
				gdi_resize_ex(gdi, new_width, new_height, m->image.bytesPerLine(), PIXEL_FORMAT_RGBX32, m->image.bits(), nullptr);
			}
		}

		m->width = new_width;
		m->height = new_height;
	}
}

void MainWindow::resizeDynamic()
{
	if (m->interrupted) return;
	if (!m->connected) return;
	if (!rdp_instance()) return;
	if (!isDynamicResizingEnabled()) return;

	int w = ui->widget_view->width();
	int h = ui->widget_view->height();
	
	int scale = ui->widget_view->scale();
	w = std::clamp(w / scale, DISPLAY_CONTROL_MIN_MONITOR_WIDTH, DISPLAY_CONTROL_MAX_MONITOR_WIDTH);
	h = std::clamp(h / scale, DISPLAY_CONTROL_MIN_MONITOR_HEIGHT, DISPLAY_CONTROL_MAX_MONITOR_HEIGHT);
	if (w == m->width && h == m->height) return;
	
	m->width = w;
	m->height = h;
	
	resizeDynamic(w, h);
}

void MainWindow::channelConnected(void *context, const ChannelConnectedEventArgs *e)
{
	if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0) {
	} else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0) {
		MyClientContext *ctx = reinterpret_cast<MyClientContext *>(context);
		ctx->disp = reinterpret_cast<DispClientContext *>(e->pInterface);
		ctx->disp->DisplayControlCaps = onDisplayControlCaps;
		ctx->disp->custom = reinterpret_cast<void *>(ctx->self);
	} else {
		freerdp_client_OnChannelConnectedEventHandler(context, e);
	}
}

void MainWindow::channelDisconnected(void *context, const ChannelDisconnectedEventArgs *e)
{
	if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0) {
	} else if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0) {
		MyClientContext *ctx = reinterpret_cast<MyClientContext *>(context);
		ctx->disp->custom = nullptr;
		ctx->disp = nullptr;
	} else {
		freerdp_client_OnChannelDisconnectedEventHandler(context, e);
	}
}

UINT MainWindow::onDisplayControlCaps(DispClientContext *disp, UINT32 maxNumMonitors, UINT32 maxMonitorAreaFactorA, UINT32 maxMonitorAreaFactorB)
{
	return CHANNEL_RC_OK;
}

