#ifndef MYVIEW_H
#define MYVIEW_H

#include <QKeyEvent>
#include <QMouseEvent>
#include <QWidget>
#include <freerdp/freerdp.h>
#include <freerdp/input.h>
#include <type_traits>

class MyView : public QWidget {
	Q_OBJECT
private:
	QImage image_;
	QImage image_scaled_;
	int scale_ = 1;
	int offset_x_ = 0;
	int offset_y_ = 0;
	freerdp *rdp_instance_;

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;  // マウスホイールイベント追加

public:
	explicit MyView(QWidget *parent = nullptr);
	void setImage(const QImage &image, const QRect &rect);
	void setRdpInstance(freerdp *instance);

	int scale() const;
	void setScale(int scale);

	void layoutView();
	
	bool onKeyEvent(QKeyEvent *event);
private:
	QPoint mapToRdp(const QPoint &pos) const;
	template <typename T> QPoint mapToRdp(T const *e) const
	{
		if constexpr (std::is_same_v<T, QWheelEvent>) {
			return mapToRdp(e->position().toPoint());
		} else {
			return mapToRdp(e->pos());
		}
	}
private:
	UINT16 qtToRdpMouseButton(Qt::MouseButton button);
};

#endif // MYVIEW_H
