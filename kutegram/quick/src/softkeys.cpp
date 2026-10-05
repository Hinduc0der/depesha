#include "softkeys.h"

#include <QAction>
#include <QVariant>

// rootObject() returns a QGraphicsObject (Qt 4) / QQuickItem (Qt 5); both are
// QObjects, but the upcast needs the complete type.
#if QT_VERSION >= 0x050000
#include <QQuickItem>
#else
#include <QGraphicsObject>
#endif

#if QT_VERSION >= 0x050000
SoftKeys::SoftKeys(QQuickView *view)
#else
SoftKeys::SoftKeys(QDeclarativeView *view)
#endif
    : QObject(view)
    , _view(view)
{
    QAction *backAction = new QAction(tr("Back"), view);
    QAction *menuAction = new QAction(tr("Options"), view);

#ifdef Q_OS_SYMBIAN
    backAction->setSoftKeyRole(QAction::NegativeSoftKey);
    menuAction->setSoftKeyRole(QAction::PositiveSoftKey);
#endif

    connect(backAction, SIGNAL(triggered()), this, SLOT(back()));
    connect(menuAction, SIGNAL(triggered()), this, SLOT(menu()));

    view->addAction(menuAction);
    view->addAction(backAction);
}

void SoftKeys::back()
{
    QObject *root = _view->rootObject();
    if (!root) {
        return;
    }

    QVariant handled = false;
    QMetaObject::invokeMethod(root, "goBack", Q_RETURN_ARG(QVariant, handled));

    if (!handled.toBool()) {
        _view->close();
    }
}

void SoftKeys::menu()
{
    QObject *root = _view->rootObject();
    if (!root) {
        return;
    }

    QMetaObject::invokeMethod(root, "menuPressed");
}
