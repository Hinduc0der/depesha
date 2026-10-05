#include "screenwatch.h"
#include "kglog.h"
#include "messageutil.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QTimer>
#include <QWidget>

#ifdef Q_OS_SYMBIAN
#include <coecntrl.h>
#include <coemain.h>
#include <w32std.h>
#endif

// Once a second. The check is one call into the window server and nothing at
// all when the size has not changed, which is all of the time except the
// moment the lid moves.
static const int KG_SCREEN_POLL_MS = 1000;

// Раз в минуту в журнал уходит строка о памяти. Журнал за ночь показал, что
// между запуском и смертью телефона в нём нет ни одной записи: тратить на это
// одну строку в минуту -- честная цена за возможность увидеть, как именно
// заканчивается память.
static const int KG_HEARTBEAT_TICKS = 60;

ScreenWatch::ScreenWatch(QWidget *view) :
    QObject(view),
    _view(view),
    _ticks(0)
{
    _lastScreen = screenSize();
    kgLog("screen at start " + QString::number(_lastScreen.width()) + "x"
          + QString::number(_lastScreen.height()) + " " + kgMemoryInfo());

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(check()));
    timer->start(KG_SCREEN_POLL_MS);
}

QSize ScreenWatch::screenSize()
{
#ifdef Q_OS_SYMBIAN
    CCoeEnv *env = CCoeEnv::Static();
    if (!env) {
        return QSize();
    }

    CWsScreenDevice *device = env->ScreenDevice();
    if (!device) {
        return QSize();
    }

    TSize size = device->SizeInPixels();
    return QSize(size.iWidth, size.iHeight);
#else
    return QApplication::desktop()->screenGeometry().size();
#endif
}

void ScreenWatch::check()
{
    if (++_ticks >= KG_HEARTBEAT_TICKS) {
        _ticks = 0;
        kgLog("alive " + kgMemoryInfo() + " peers="
              + QString::number(globalUsers().size() + globalChats().size()));
    }

    const QSize screen = screenSize();
    if (!screen.isValid() || screen == _lastScreen) {
        return;
    }

    _lastScreen = screen;

    kgLog("screen changed to " + QString::number(screen.width()) + "x"
          + QString::number(screen.height())
          + ", window is " + QString::number(_view->width()) + "x"
          + QString::number(_view->height()) + " " + kgMemoryInfo());

    if (_view->size() == screen) {
        // Qt already did it: the resize line just above this one in the log is
        // the proof.
        return;
    }

    kgLog("resizing the window by hand");
    _view->setGeometry(0, 0, screen.width(), screen.height());
    _view->showFullScreen();
    _view->update();
    kgLog("resized by hand, window is " + QString::number(_view->width()) + "x"
          + QString::number(_view->height()) + " " + kgMemoryInfo());
}
