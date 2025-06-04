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

void MyView::setImage(const QImage &image, QRect const &rect)
{
	if (rect.isNull()) {
		image_ = image;
	} else if (image_.size() != image.size()) {
		image_ = image.copy();
	} else {
		QPainter pr(&image_);
		pr.drawImage(rect, image, rect);
	}
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

bool MyView::onKeyEvent(QKeyEvent *event)
{
	if (rdp_instance_ && rdp_instance_->context && rdp_instance_->context->input) {
		auto vc = GetVirtualKeyCodeFromKeycode(event->nativeScanCode(), WINPR_KEYCODE_TYPE_XKB);
		auto code = GetVirtualScanCodeFromVirtualKeyCode(vc, WINPR_KBD_TYPE_IBM_ENHANCED);
		freerdp_input_send_keyboard_event_ex(rdp_instance_->context->input, event->type() == QEvent::KeyPress, event->isAutoRepeat(), code);
		return true;
	}
	return false;
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

