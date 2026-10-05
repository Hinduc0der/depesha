#ifndef TGSRP_H
#define TGSRP_H

#include <QThread>
#include <QByteArray>
#include <QString>
#include "tgstream.h"

// SRP-6a in the exact shape Telegram specifies for two-factor authentication,
// see https://core.telegram.org/api/srp. Input is the account.password object
// returned by account.getPassword plus the password the user typed; output is
// the A / M1 pair that goes into inputCheckPasswordSRP.
struct TgSrpResult
{
    bool ok;
    QByteArray A;
    QByteArray M1;
    QString error;

    TgSrpResult() : ok(false) {}
};

// Reports how far the key derivation has got. Without it a slow computation
// and a wedged one look exactly the same on screen: a spinner.
class TgSrpProgress
{
public:
    virtual ~TgSrpProgress();
    virtual void srpProgress(qint32 percent) = 0;
};

TgSrpResult computeSrp(TgObject password, QString userPassword, TgSrpProgress *reporter = 0);

// The key derivation alone is 100000 rounds of HMAC-SHA512. On the 332 MHz
// ARM11 of an E90 that is tens of seconds, so it cannot run on the GUI thread:
// Symbian would show the application as not responding and the joystick would
// go dead for the whole time.
class TgSrpWorker : public QThread, public TgSrpProgress
{
    Q_OBJECT
public:
    TgSrpWorker(TgObject password, QString userPassword, QObject *parent = 0);

    void srpProgress(qint32 percent);

signals:
    void computed(QByteArray A, QByteArray M1, QString error);
    void progress(qint32 percent);

protected:
    void run();

private:
    TgObject _password;
    QString _userPassword;
};

#endif // TGSRP_H
