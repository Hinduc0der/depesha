#ifndef KEYREPEAT_H
#define KEYREPEAT_H

#include <QObject>
#include <QSet>
#include <QString>
#include <QTime>

// Turns a held navigation key back into a single press.
//
// Symbian delivers a key that is held down as a stream of ordinary key press
// events, and Qt on this platform does not reliably set isAutoRepeat() on
// them, so QML cannot tell a repeat from a deliberate second press by the flag
// alone. On a phone this slow an ordinary press easily outlasts the repeat
// delay, which is why one push of the joystick used to move two rows.
//
// Press and release are paired here instead: while a key is known to be down,
// every further press of it is its own repeat and is dropped. Only the keys
// that drive navigation are filtered -- text keys keep their repeat, so
// holding backspace in the compose box still erases.
//
// Every key event is also written to a log file, because what the phone
// actually sends still cannot be seen from here; see logPath().
class KeyRepeatFilter : public QObject
{
    Q_OBJECT

public:
    explicit KeyRepeatFilter(QObject *parent = 0);
    ~KeyRepeatFilter();

    // Memory card first, so the file can be picked up over USB; phone memory
    // when there is no card.
    static QString logPath();

public slots:
    void flushLog();

protected:
    bool eventFilter(QObject *watched, QEvent *event);

private:
    static bool isNavigationKey(int key);

    void log(const char *what, int key, bool autoRepeat);
    void restoreFocusIfLost();

    QSet<int> _down;
    QTime _sinceLastPress;
    int _lastKey;
    // Set once a real release arrives. Until then the guard cannot rely on
    // releases and falls back to a time window, so that a platform which never
    // sends them leaves the keypad slow rather than dead.
    bool _releasesSeen;
};

#endif // KEYREPEAT_H
