#ifndef SCREENWATCH_H
#define SCREENWATCH_H

#include <QObject>
#include <QSize>

class QWidget;

// The E90 is a clamshell with two displays: 240x320 on the cover and 800x352
// inside. Opening the lid is supposed to reach Qt as a layout switch, which
// resizes the window to the new screen. When it does not, the application
// keeps drawing at the old size on the old screen device and the inner display
// shows the desktop with nothing on it -- which is exactly what was reported.
//
// So the screen is asked directly, once a second: the window server knows the
// size of the device the application is on even when no Qt event was sent.
// Every change is written to the log next to whatever resize Qt did or did not
// deliver, so the two can be compared afterwards; if Qt missed it, the window
// is put back to full screen by hand.
class ScreenWatch : public QObject
{
    Q_OBJECT

public:
    explicit ScreenWatch(QWidget *view);

    // The size of the display the application is actually on. An invalid size
    // means the platform cannot tell us.
    static QSize screenSize();

private slots:
    void check();

private:
    QWidget *_view;
    QSize _lastScreen;
    int _ticks;
};

#endif // SCREENWATCH_H
