#ifndef PLATFORMUTILS_H
#define PLATFORMUTILS_H

#include <QObject>

class AppSettings;
#include <QColor>
#include <QVariant>
#if !defined(Q_OS_SYMBIAN) && !defined(Q_OS_WINPHONE)
#include <QSystemTrayIcon>
#include <QMenu>
#endif
#include <QUrl>
#include <QHash>
#include <QWidget>

#ifdef SYMBIAN3_READY
#include "QPiglerAPI.h"
#endif

class PlatformUtils : public QObject
{
    Q_OBJECT
private:
    QWidget* window;
#if !defined(Q_OS_SYMBIAN) && !defined(Q_OS_WINPHONE)
    QSystemTrayIcon trayIcon;
    QMenu trayMenu;
#endif
    QHash<qint64, QVariantMap> unread;
#ifdef SYMBIAN3_READY
    QPiglerAPI pigler;
    qint32 piglerId;
#endif
    AppSettings *_settings;

public:
    explicit PlatformUtils(QObject *parent = 0);

signals:

public slots:
    void showAndRaise();
    void quit();
    // Copying a message needs somewhere to put it.
    void copyToClipboard(QString text);

#if !defined(Q_OS_SYMBIAN) && !defined(Q_OS_WINPHONE)
    void trayActivated(QSystemTrayIcon::ActivationReason reason);
    void messageClicked();
    void menuTriggered(QAction* action);
#endif

#ifdef SYMBIAN3_READY
    void piglerHandleTap(qint32 notificationId);
#endif

    void windowsExtendFrameIntoClientArea(int left, int top, int right, int bottom);
    bool windowsIsCompositionEnabled();
    QColor windowsRealColorizationColor();
    bool isWindows();

    void gotNewMessage(qint64 peerId, QString peerName, QString senderName, QString text, bool silent);

public:
    // Set once from main.cpp. Notifications and vibration are switchable, and
    // this is how the platform side gets to see the switches.
    void setSettings(AppSettings *settings);
};

void openUrl(QUrl url);
// Hands a local file to whatever application the phone associates with it.
void openFile(QString path);

// Есть ли на телефоне приложение для файла с таким расширением. Клиент
// спрашивает заранее, чтобы сказать об этом вслух, а не молча ничего не
// сделать по нажатию.
bool canOpenFile(QString path);

#endif // PLATFORMUTILS_H
