#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ConnectionDialog.h"

#include <QPainter>

MainWindow *g_mainWindow = nullptr;

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, rdp_instance(nullptr)
	, rdp_context(nullptr)
	, update_timer(new QTimer(this))
	, connected(false)
	, first_update(true)  // 初回更新フラグを初期化
{
	ui->setupUi(this);
	g_mainWindow = this;

	update_timer->setInterval(4);
	connect(update_timer, &QTimer::timeout, this, &MainWindow::onUpdateTimer);

	// フォーカスポリシーの設定
	ui->widget_view->setFocusPolicy(Qt::StrongFocus);
	setFocusPolicy(Qt::StrongFocus);
}

MainWindow::~MainWindow()
{
	doDisconnect();
	g_mainWindow = nullptr;
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
	freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, 1024);
	freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, 768);
	freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32);

	// 安全なパフォーマンス最適化設定のみ適用
	freerdp_settings_set_bool(settings, FreeRDP_FastPathOutput, TRUE);
	freerdp_settings_set_bool(settings, FreeRDP_FastPathInput, TRUE);
	freerdp_settings_set_bool(settings, FreeRDP_BitmapCacheEnabled, TRUE);
	freerdp_settings_set_uint32(settings, FreeRDP_CompressionLevel, 2);  // 高圧縮に変更
	freerdp_settings_set_uint32(settings, FreeRDP_OffscreenSupportLevel, 1);
	freerdp_settings_set_uint32(settings, FreeRDP_GlyphSupportLevel, 1);
	freerdp_settings_set_bool(settings, FreeRDP_SurfaceCommandsEnabled, TRUE);
	freerdp_settings_set_bool(settings, FreeRDP_NetworkAutoDetect, TRUE);

	// 接続実行
	if (freerdp_connect(rdp_instance)) {
		connected = true;
		first_update = true;  // 新しい接続時は初回更新フラグをリセット
		current_image = QImage();  // 画像バッファをクリア
		previous_image = QImage();
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
	// 部分更新機能を使用
	updateScreenPartial();
}

void MainWindow::updateScreenPartial()
{
	if (connected && rdp_context && rdp_context->gdi) {
		rdpGdi *gdi = rdp_context->gdi;
		if (gdi->primary_buffer) {
			BYTE *data = gdi->primary_buffer;
			int width = gdi->width;
			int height = gdi->height;
			int stride = gdi->stride;

			// 新しい画像データを作成
			QImage new_image(data, width, height, stride, QImage::Format_ARGB32);

			if (first_update || current_image.isNull()) {
				// 初回更新：全画面を更新
				current_image = new_image.copy();
				ui->widget_view->setImage(current_image);
				ui->widget_view->update();
				previous_image = current_image.copy();
				first_update = false;
			} else {
				// 変更された領域を検出
				QVector<QRect> dirty_regions = findDirtyRegions(new_image, previous_image);
				
				if (!dirty_regions.isEmpty()) {
					// 変更があった場合：現在の画像バッファの該当部分のみを更新
					QPainter painter(&current_image);
					for (const QRect &rect : dirty_regions) {
						// 変更された領域のみをコピー
						QImage region = new_image.copy(rect);
						painter.drawImage(rect.topLeft(), region);
					}
					painter.end();
					
					// ビューに新しい画像を設定し、変更領域のみを再描画
					ui->widget_view->setImage(current_image);
					for (const QRect &rect : dirty_regions) {
						ui->widget_view->update(rect);
					}
					
					previous_image = new_image.copy();
				}
			}
		}
	}
}

QVector<QRect> MainWindow::findDirtyRegions(const QImage &current, const QImage &previous)
{
	QVector<QRect> dirty_regions;
	
	if (current.size() != previous.size()) {
		// サイズが変わった場合は全画面更新
		dirty_regions.append(current.rect());
		return dirty_regions;
	}
	
	const int width = current.width();
	const int height = current.height();
	const int block_size = 32;  // 32x32ピクセルのブロックで比較（SIMD最適化）
	
	for (int y = 0; y < height; y += block_size) {
		for (int x = 0; x < width; x += block_size) {
			int block_width = qMin(block_size, width - x);
			int block_height = qMin(block_size, height - y);
			
			// ブロックの開始位置を取得
			const uchar *current_data = current.constScanLine(y) + (x * 4);
			const uchar *previous_data = previous.constScanLine(y) + (x * 4);
			
			// SIMD最適化による高速比較
			bool block_changed = compareBlocksSIMD(current_data, previous_data, 
												   block_width, block_height, current.bytesPerLine());
			
			if (block_changed) {
				dirty_regions.append(QRect(x, y, block_width, block_height));
			}
		}
	}
	
	return dirty_regions;
}

bool MainWindow::compareBlocksSIMD(const uchar *current_data, const uchar *previous_data, 
								   int width, int height, int stride)
{
#ifdef __x86_64__
	return compareBlocksSSE(current_data, previous_data, width, height, stride);
#elif __aarch64__
	return compareBlocksNEON(current_data, previous_data, width, height, stride);
#else
	// フォールバック: 従来の比較方法
	for (int y = 0; y < height; y += 2) {  // 2行おきにサンプリング
		const uchar *current_line = current_data + (y * stride);
		const uchar *previous_line = previous_data + (y * stride);
		if (memcmp(current_line, previous_line, width * 4) != 0) {
			return true;
		}
	}
	return false;
#endif
}

#ifdef __x86_64__
bool MainWindow::compareBlocksSSE(const uchar *current_data, const uchar *previous_data, 
								  int width, int height, int stride)
{
	// SSE2を使用した128bit並列比較
	const int pixels_per_vector = 4;  // 128bit / 32bit(ARGB) = 4ピクセル
	const int vector_width = width / pixels_per_vector;
	const int remaining_pixels = width % pixels_per_vector;
	
	for (int y = 0; y < height; y += 2) {  // 2行おきにサンプリング
		const __m128i *current_line = reinterpret_cast<const __m128i*>(current_data + (y * stride));
		const __m128i *previous_line = reinterpret_cast<const __m128i*>(previous_data + (y * stride));
		
		// ベクトル化された比較
		for (int i = 0; i < vector_width; i++) {
			__m128i current_vec = _mm_load_si128(&current_line[i]);
			__m128i previous_vec = _mm_load_si128(&previous_line[i]);
			__m128i diff = _mm_xor_si128(current_vec, previous_vec);
			
			// 差分があるかチェック
			if (!_mm_testz_si128(diff, diff)) {
				return true;  // 変更あり
			}
		}
		
		// 残りのピクセル（ベクトルサイズで割り切れない部分）
		if (remaining_pixels > 0) {
			const uchar *current_remainder = reinterpret_cast<const uchar*>(&current_line[vector_width]);
			const uchar *previous_remainder = reinterpret_cast<const uchar*>(&previous_line[vector_width]);
			if (memcmp(current_remainder, previous_remainder, remaining_pixels * 4) != 0) {
				return true;
			}
		}
	}
	
	return false;  // 変更なし
}
#endif

#ifdef __aarch64__
bool MainWindow::compareBlocksNEON(const uchar *current_data, const uchar *previous_data, 
								   int width, int height, int stride)
{
	// ARM NEONを使用した128bit並列比較
	const int pixels_per_vector = 4;  // 128bit / 32bit(ARGB) = 4ピクセル
	const int vector_width = width / pixels_per_vector;
	const int remaining_pixels = width % pixels_per_vector;
	
	for (int y = 0; y < height; y += 2) {  // 2行おきにサンプリング
		const uint32x4_t *current_line = reinterpret_cast<const uint32x4_t*>(current_data + (y * stride));
		const uint32x4_t *previous_line = reinterpret_cast<const uint32x4_t*>(previous_data + (y * stride));
		
		// ベクトル化された比較
		for (int i = 0; i < vector_width; i++) {
			uint32x4_t current_vec = vld1q_u32(reinterpret_cast<const uint32_t*>(&current_line[i]));
			uint32x4_t previous_vec = vld1q_u32(reinterpret_cast<const uint32_t*>(&previous_line[i]));
			uint32x4_t diff = veorq_u32(current_vec, previous_vec);
			
			// 差分があるかチェック
			uint64x2_t diff64 = vreinterpretq_u64_u32(diff);
			if (vgetq_lane_u64(diff64, 0) != 0 || vgetq_lane_u64(diff64, 1) != 0) {
				return true;  // 変更あり
			}
		}
		
		// 残りのピクセル
		if (remaining_pixels > 0) {
			const uchar *current_remainder = reinterpret_cast<const uchar*>(&current_line[vector_width]);
			const uchar *previous_remainder = reinterpret_cast<const uchar*>(&previous_line[vector_width]);
			if (memcmp(current_remainder, previous_remainder, remaining_pixels * 4) != 0) {
				return true;
			}
		}
	}
	
	return false;  // 変更なし
}
#endif

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

BOOL MainWindow::rdp_post_connect(freerdp *instance)
{
	// GDIの初期化
	if (!gdi_init(instance, PIXEL_FORMAT_BGRA32)) {
		return FALSE;
	}

	return TRUE;
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
	if (g_mainWindow) {
		g_mainWindow->updateScreen();
	}
	return TRUE;
}
