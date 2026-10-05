#include "keyrepeat.h"
#include "kglog.h"

#include <QCoreApplication>
#include <QDeclarativeItem>
#include <QDeclarativeView>
#include <QEvent>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QResizeEvent>

// Only used while no key release has ever been seen. Long enough to swallow a
// repeat stream (Symbian repeats several times a second), short enough that a
// deliberate second press is not lost for long.
static const int KG_REPEAT_GUARD_MS = 800;

KeyRepeatFilter::KeyRepeatFilter(QObject *parent) :
    QObject(parent),
    _lastKey(0),
    _releasesSeen(false)
{
    _sinceLastPress.start();

    connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), this, SLOT(flushLog()));
}

KeyRepeatFilter::~KeyRepeatFilter()
{
    kgLogFlush();
}

QString KeyRepeatFilter::logPath()
{
    return kgLogPath();
}

void KeyRepeatFilter::flushLog()
{
    kgLog("application quitting");
    kgLogFlush();
}

bool KeyRepeatFilter::isNavigationKey(int key)
{
    switch (key) {
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Select:
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Context1:
    case Qt::Key_Context2:
    case Qt::Key_Back:
    case Qt::Key_Yes:
    case Qt::Key_No:
        return true;
    default:
        return false;
    }
}

void KeyRepeatFilter::log(const char *what, int key, bool autoRepeat)
{
    // Key events are batched: they are frequent, and a file write per press
    // would be felt.
    kgLog(QString(what) + " key=0x" + QString::number(key, 16)
          + (autoRepeat ? " autorepeat" : ""), false);
}

// Есть ли в сцене хоть что-нибудь, что получит эту клавишу. Если нет -- на
// кнопочном телефоне это тупик: вернуть фокус нечем, кроме перезапуска.
void KeyRepeatFilter::restoreFocusIfLost()
{
    QDeclarativeView *view = qobject_cast<QDeclarativeView*>(parent());

    if (!view || !view->scene() || view->scene()->focusItem()) {
        return;
    }

    QObject *root = view->rootObject();
    if (!root) {
        return;
    }

    kgLog("nothing had the focus, restoring it");
    QMetaObject::invokeMethod(root, "restoreFocus");
}

bool KeyRepeatFilter::eventFilter(QObject *watched, QEvent *event)
{
    const QEvent::Type type = event->type();

    // The E90 moves the application between its two screens when the lid is
    // opened, and the scene is rebuilt for the new size. Rare enough to write
    // through immediately, and the last line in the file after a freeze says
    // whether that is where it happened.
    if (type == QEvent::Resize) {
        QResizeEvent *resizeEvent = static_cast<QResizeEvent *>(event);
        kgLog("resize " + QString::number(resizeEvent->size().width()) + "x"
              + QString::number(resizeEvent->size().height())
              + " (was " + QString::number(resizeEvent->oldSize().width()) + "x"
              + QString::number(resizeEvent->oldSize().height()) + ")");
        return QObject::eventFilter(watched, event);
    }

    if (type == QEvent::WinIdChange) {
        kgLog("window id changed");
        return QObject::eventFilter(watched, event);
    }

    if (type == QEvent::WindowActivate) {
        kgLog("window activated");
        return QObject::eventFilter(watched, event);
    }

    if (type == QEvent::WindowDeactivate) {
        kgLog("window deactivated");
        return QObject::eventFilter(watched, event);
    }

    if (type != QEvent::KeyPress && type != QEvent::KeyRelease) {
        return QObject::eventFilter(watched, event);
    }

    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    const int key = keyEvent->key();

    if (type == QEvent::KeyRelease) {
        log("release", key, keyEvent->isAutoRepeat());

        // A release that is itself flagged as a repeat (X11 sends those) says
        // nothing about the key being let go.
        if (isNavigationKey(key) && !keyEvent->isAutoRepeat()) {
            _releasesSeen = true;
            _down.remove(key);
        }

        return QObject::eventFilter(watched, event);
    }

    if (!isNavigationKey(key)) {
        log("press", key, keyEvent->isAutoRepeat());
        return QObject::eventFilter(watched, event);
    }

    if (keyEvent->isAutoRepeat()) {
        log("press dropped (autorepeat flag)", key, true);
        return true;
    }

    if (_down.contains(key)) {
        if (_releasesSeen) {
            log("press dropped (still down)", key, false);
            return true;
        }

        if (key == _lastKey && _sinceLastPress.elapsed() < KG_REPEAT_GUARD_MS) {
            log("press dropped (within guard)", key, false);
            return true;
        }
    }

    log("press", key, false);
    restoreFocusIfLost();

    _down.insert(key);
    _lastKey = key;
    _sinceLastPress.restart();
    return QObject::eventFilter(watched, event);
}
