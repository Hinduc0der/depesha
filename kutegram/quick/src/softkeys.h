#ifndef SOFTKEYS_H
#define SOFTKEYS_H

#include <QObject>

#if QT_VERSION >= 0x050000
#include <QQuickView>
#else
#include <QDeclarativeView>
#endif

// On S60 the two keys under the screen are not ordinary key presses: Symbian
// draws a command button array for them and Qt fills it from QActions carrying
// a soft key role. With no such action the right key keeps its stock "Exit"
// binding and simply closes the application, which is why the back arrow drawn
// by the QML was unreachable on a non-touch phone -- there was nothing but a
// MouseArea behind it.
//
// This routes the two keys into the QML root object: the negative key into
// goBack(), the positive one into menuPressed(). goBack() returns false when
// there is nothing left to unwind, and only then is the window closed, so the
// key still behaves the way an S60 user expects at the top level.
class SoftKeys : public QObject
{
    Q_OBJECT
public:
#if QT_VERSION >= 0x050000
    explicit SoftKeys(QQuickView *view);
#else
    explicit SoftKeys(QDeclarativeView *view);
#endif

public slots:
    void back();
    void menu();

private:
#if QT_VERSION >= 0x050000
    QQuickView *_view;
#else
    QDeclarativeView *_view;
#endif
};

#endif // SOFTKEYS_H
