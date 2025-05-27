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

// SIMD最適化のためのヘッダー
#ifdef __x86_64__
#include <immintrin.h>  // SSE/AVX
#elif __aarch64__
#include <arm_neon.h>   // ARM NEON
#endif

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
	Q_OBJECT
private:
	Ui::MainWindow *ui;
	freerdp *rdp_instance;
	rdpContext *rdp_context;
	QTimer *update_timer;
	bool connected;
	
	// 部分更新機能のための変数
	QImage current_image;       // 現在の画像バッファ
	QImage previous_image;      // 前回の画像（比較用）
	bool first_update;          // 初回更新フラグ

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
	void updateScreenPartial();  // 部分更新メソッド
	QVector<QRect> findDirtyRegions(const QImage &current, const QImage &previous);  // 変更領域検出
	
	// SIMD最適化関数
	bool compareBlocksSIMD(const uchar *current_data, const uchar *previous_data, int width, int height, int stride);
	bool compareBlocksSSE(const uchar *current_data, const uchar *previous_data, int width, int height, int stride);
	bool compareBlocksNEON(const uchar *current_data, const uchar *previous_data, int width, int height, int stride);

public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow();

private slots:
	void on_action_connect_triggered();
	void on_action_disconnect_triggered();
	void onUpdateTimer();
};
#endif // MAINWINDOW_H
