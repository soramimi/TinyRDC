#include "MyView.h"
#include <QApplication>
#include <QPainter>
#include <QWheelEvent>
#include <freerdp/scancode.h>

MyView::MyView(QWidget *parent)
	: QWidget { parent }
	, rdp_instance_(nullptr)
{
	setFocusPolicy(Qt::StrongFocus);
	setMouseTracking(true);
}

QPoint MyView::mapToRdp(const QPoint &pos) const
{
	// RDPの座標系に変換
	return QPoint((pos.x() + offset_x_) / scale_, (pos.y() + offset_y_) / scale_);
}

void MyView::setImage(const QImage &newImage)
{
	image_ = newImage;
	image_scaled_ = {};
	update();
}

void MyView::setRdpInstance(freerdp *instance)
{
	rdp_instance_ = instance;
}

int MyView::scale() const
{
	return scale_;
}

void MyView::setScale(int scale)
{
	scale_ = scale;
	image_scaled_ = {};
	update();
}

void MyView::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	QPainter painter(this);
	if (image_.isNull()) {
		painter.fillRect(rect(), Qt::black);
	} else {
		int w = image_.width();
		int h = image_.height();
		if (image_scaled_.width() != w || image_scaled_.height() != h) {
			if (scale_ == 1) {
				image_scaled_ = image_;
			} else {
				image_scaled_ = image_.scaled(w * scale_, h * scale_, Qt::KeepAspectRatio, Qt::FastTransformation);
			}
		}
		painter.drawImage(-offset_x_, -offset_y_, image_scaled_, Qt::KeepAspectRatio, Qt::FastTransformation);
	}
}

void MyView::mousePressEvent(QMouseEvent *event)
{
	if (rdp_instance_ && rdp_instance_->context) {
		UINT16 flags = PTR_FLAGS_DOWN;
		UINT16 button = qtToRdpMouseButton(event->button());
		if (button != 0) {
			flags |= button;
			QPoint pos = mapToRdp(event);
			freerdp_input_send_mouse_event(rdp_instance_->context->input, flags, pos.x(), pos.y());
		}
	}
	setFocus();
}

void MyView::mouseReleaseEvent(QMouseEvent *event)
{
	if (rdp_instance_ && rdp_instance_->context) {
		UINT16 button = qtToRdpMouseButton(event->button());
		if (button != 0) {
			QPoint pos = mapToRdp(event);
			freerdp_input_send_mouse_event(rdp_instance_->context->input, button, pos.x(), pos.y());
		}
	}
}

void MyView::mouseMoveEvent(QMouseEvent *event)
{
	if (rdp_instance_ && rdp_instance_->context) {
		QPoint pos = mapToRdp(event);
		freerdp_input_send_mouse_event(rdp_instance_->context->input, PTR_FLAGS_MOVE, pos.x(), pos.y());
	}
}

void MyView::wheelEvent(QWheelEvent *event)
{
	if (rdp_instance_ && rdp_instance_->context) {
		auto delta = event->angleDelta();
		QPoint pos = mapToRdp(event);
		if (delta.y() != 0) {
			// 垂直スクロール（一般的なマウスホイール）
			int flags = std::abs(delta.y());
			flags = std::max(0, std::min(flags, 255));
			flags |= PTR_FLAGS_WHEEL;
			if (delta.y() < 0) {
				flags |= PTR_FLAGS_WHEEL_NEGATIVE;
			}
			freerdp_input_send_mouse_event(rdp_instance_->context->input, (UINT16)flags, pos.x(), pos.y());
		} else if (delta.x() != 0) {
			// 水平スクロール（ホイールチルト）
			int flags = std::abs(delta.x());
			flags = std::max(0, std::min(flags, 255));
			flags |= PTR_FLAGS_HWHEEL;
			if (delta.x() < 0) {
				flags |= PTR_FLAGS_WHEEL_NEGATIVE;  // 左スクロール
			}
			freerdp_input_send_mouse_event(rdp_instance_->context->input, (UINT16)flags, pos.x(), pos.y());
		}
	}
	
	event->accept();
}

bool MyView::keyPress(int key)
{
	UINT16 keycode = qtToRdpKeyCode(key);
	if (keycode != 0) {
		if (rdp_instance_ && rdp_instance_->context) {
			freerdp_input_send_keyboard_event(rdp_instance_->context->input, KBD_FLAGS_DOWN, keycode);
		}
		return true;
	}
	return false;
}

bool MyView::keyRelease(int key)
{
	UINT16 keycode = qtToRdpKeyCode(key);
	if (keycode != 0) {
		if (rdp_instance_ && rdp_instance_->context) {
			freerdp_input_send_keyboard_event(rdp_instance_->context->input, KBD_FLAGS_RELEASE, keycode);
		}
		return true;
	}
	return false;
}

void MyView::keyPressEvent(QKeyEvent *event)
{
	keyPress(event->key());
}

void MyView::keyReleaseEvent(QKeyEvent *event)
{
	keyRelease(event->key());
}

UINT16 MyView::qtToRdpMouseButton(Qt::MouseButton button)
{
	switch (button) {
	case Qt::LeftButton:
		return PTR_FLAGS_BUTTON1;
	case Qt::RightButton:
		return PTR_FLAGS_BUTTON2;
	case Qt::MiddleButton:
		return PTR_FLAGS_BUTTON3;
	default:
		return 0;
	}
}

UINT16 MyView::qtToRdpKeyCode(int qtKey)
{
	// 基本的なキーマッピング
	switch (qtKey) {
	case Qt::Key_A:
		return RDP_SCANCODE_KEY_A;
	case Qt::Key_B:
		return RDP_SCANCODE_KEY_B;
	case Qt::Key_C:
		return RDP_SCANCODE_KEY_C;
	case Qt::Key_D:
		return RDP_SCANCODE_KEY_D;
	case Qt::Key_E:
		return RDP_SCANCODE_KEY_E;
	case Qt::Key_F:
		return RDP_SCANCODE_KEY_F;
	case Qt::Key_G:
		return RDP_SCANCODE_KEY_G;
	case Qt::Key_H:
		return RDP_SCANCODE_KEY_H;
	case Qt::Key_I:
		return RDP_SCANCODE_KEY_I;
	case Qt::Key_J:
		return RDP_SCANCODE_KEY_J;
	case Qt::Key_K:
		return RDP_SCANCODE_KEY_K;
	case Qt::Key_L:
		return RDP_SCANCODE_KEY_L;
	case Qt::Key_M:
		return RDP_SCANCODE_KEY_M;
	case Qt::Key_N:
		return RDP_SCANCODE_KEY_N;
	case Qt::Key_O:
		return RDP_SCANCODE_KEY_O;
	case Qt::Key_P:
		return RDP_SCANCODE_KEY_P;
	case Qt::Key_Q:
		return RDP_SCANCODE_KEY_Q;
	case Qt::Key_R:
		return RDP_SCANCODE_KEY_R;
	case Qt::Key_S:
		return RDP_SCANCODE_KEY_S;
	case Qt::Key_T:
		return RDP_SCANCODE_KEY_T;
	case Qt::Key_U:
		return RDP_SCANCODE_KEY_U;
	case Qt::Key_V:
		return RDP_SCANCODE_KEY_V;
	case Qt::Key_W:
		return RDP_SCANCODE_KEY_W;
	case Qt::Key_X:
		return RDP_SCANCODE_KEY_X;
	case Qt::Key_Y:
		return RDP_SCANCODE_KEY_Y;
	case Qt::Key_Z:
		return RDP_SCANCODE_KEY_Z;

	case Qt::Key_0:
		return RDP_SCANCODE_KEY_0;
	case Qt::Key_1:
		return RDP_SCANCODE_KEY_1;
	case Qt::Key_2:
		return RDP_SCANCODE_KEY_2;
	case Qt::Key_3:
		return RDP_SCANCODE_KEY_3;
	case Qt::Key_4:
		return RDP_SCANCODE_KEY_4;
	case Qt::Key_5:
		return RDP_SCANCODE_KEY_5;
	case Qt::Key_6:
		return RDP_SCANCODE_KEY_6;
	case Qt::Key_7:
		return RDP_SCANCODE_KEY_7;
	case Qt::Key_8:
		return RDP_SCANCODE_KEY_8;
	case Qt::Key_9:
		return RDP_SCANCODE_KEY_9;

	case Qt::Key_Space:
		return RDP_SCANCODE_SPACE;
	case Qt::Key_Return:
		return RDP_SCANCODE_RETURN;
	case Qt::Key_Enter:
		return RDP_SCANCODE_RETURN;
	case Qt::Key_Backspace:
		return RDP_SCANCODE_BACKSPACE;
	case Qt::Key_Tab:
		return RDP_SCANCODE_TAB;
	case Qt::Key_Escape:
		return RDP_SCANCODE_ESCAPE;
	case Qt::Key_Delete:
		return RDP_SCANCODE_DELETE;
	case Qt::Key_Insert:
		return RDP_SCANCODE_INSERT;
	case Qt::Key_Home:
		return RDP_SCANCODE_HOME;
	case Qt::Key_End:
		return RDP_SCANCODE_END;
	case Qt::Key_PageUp:
		return RDP_SCANCODE_PRIOR;
	case Qt::Key_PageDown:
		return RDP_SCANCODE_NEXT;

	case Qt::Key_Left:
		return RDP_SCANCODE_LEFT;
	case Qt::Key_Right:
		return RDP_SCANCODE_RIGHT;
	case Qt::Key_Up:
		return RDP_SCANCODE_UP;
	case Qt::Key_Down:
		return RDP_SCANCODE_DOWN;

	case Qt::Key_Shift:
		return RDP_SCANCODE_LSHIFT;
	case Qt::Key_Control:
		return RDP_SCANCODE_LCONTROL;
	case Qt::Key_Alt:
		return RDP_SCANCODE_LMENU;

	case Qt::Key_F1:
		return RDP_SCANCODE_F1;
	case Qt::Key_F2:
		return RDP_SCANCODE_F2;
	case Qt::Key_F3:
		return RDP_SCANCODE_F3;
	case Qt::Key_F4:
		return RDP_SCANCODE_F4;
	case Qt::Key_F5:
		return RDP_SCANCODE_F5;
	case Qt::Key_F6:
		return RDP_SCANCODE_F6;
	case Qt::Key_F7:
		return RDP_SCANCODE_F7;
	case Qt::Key_F8:
		return RDP_SCANCODE_F8;
	case Qt::Key_F9:
		return RDP_SCANCODE_F9;
	case Qt::Key_F10:
		return RDP_SCANCODE_F10;
	case Qt::Key_F11:
		return RDP_SCANCODE_F11;
	case Qt::Key_F12:
		return RDP_SCANCODE_F12;

	case Qt::Key_NumLock:
		return RDP_SCANCODE_NUMLOCK;

	default:
		return 0;
	}
}
