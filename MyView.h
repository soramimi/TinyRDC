#ifndef MYVIEW_H
#define MYVIEW_H

#include <QKeyEvent>
#include <QMouseEvent>
#include <QWidget>
#include <freerdp/freerdp.h>
#include <freerdp/input.h>

class MyView : public QWidget {
	Q_OBJECT
private:
	QImage image_;
	freerdp *rdp_instance_;

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;

public:
	explicit MyView(QWidget *parent = nullptr);
	void setImage(const QImage &newImage);
	void setRdpInstance(freerdp *instance);

private:
	UINT16 qtToRdpKeyCode(int qtKey);
	UINT16 qtToRdpMouseButton(Qt::MouseButton button);
};

#endif // MYVIEW_H
