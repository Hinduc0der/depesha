#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QString>

class QNetworkReply;

// The handful of things the user can turn on and off, kept in the same
// directory as the session so an upgrade does not lose them.
//
// Marking chats as read is deliberately a setting rather than something the
// client always does: telling Telegram a chat has been read is not reversible,
// and reading on the phone should not have to mean reading everywhere.
class AppSettings : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool markAsRead READ markAsRead WRITE setMarkAsRead NOTIFY markAsReadChanged)
    Q_PROPERTY(bool notifications READ notifications WRITE setNotifications NOTIFY notificationsChanged)
    Q_PROPERTY(bool vibrate READ vibrate WRITE setVibrate NOTIFY vibrateChanged)

    // The phone cannot decode Opus, VP9 or Lottie, and never will. What it can
    // do is play what something else has already converted -- see
    // vps-voice-transcode.py. Without a server the client still shows text and
    // pictures; it simply cannot play what needs decoding.
    Q_PROPERTY(bool mediaServer READ mediaServer WRITE setMediaServer NOTIFY mediaServerChanged)
    Q_PROPERTY(QString mediaServerUrl READ mediaServerUrl WRITE setMediaServerUrl NOTIFY mediaServerChanged)
    Q_PROPERTY(QString mediaServerToken READ mediaServerToken WRITE setMediaServerToken NOTIFY mediaServerChanged)
    Q_PROPERTY(QString mediaServerStatus READ mediaServerStatus NOTIFY mediaServerStatusChanged)

public:
    explicit AppSettings(QObject *parent = 0);

    bool markAsRead() const;
    void setMarkAsRead(bool value);

    bool notifications() const;
    void setNotifications(bool value);

    bool vibrate() const;
    void setVibrate(bool value);

    bool mediaServer() const;
    void setMediaServer(bool value);

    QString mediaServerUrl() const;
    void setMediaServerUrl(QString value);

    QString mediaServerToken() const;
    void setMediaServerToken(QString value);

    QString mediaServerStatus() const;

    // Empty when media conversion is off or unconfigured; callers treat that as
    // "this cannot be played".
    QString activeMediaServerUrl() const;

public slots:
    // Named rather than typed, so a menu row can carry the name of what it
    // switches and nothing has to be kept in step with an index.
    void toggle(QString name);
    bool value(QString name) const;

    // Asks the server whether it is there, so the address can be checked
    // without waiting for the next voice message.
    void checkMediaServer();

signals:
    void markAsReadChanged();
    void notificationsChanged();
    void vibrateChanged();
    void mediaServerChanged();
    void mediaServerStatusChanged();

private:
    void save();

    bool _markAsRead;
    bool _notifications;
    bool _vibrate;
    bool _mediaServer;
    QString _mediaServerUrl;
    QString _mediaServerToken;
    QString _mediaServerStatus;

    QNetworkAccessManager _network;
    QNetworkReply *_checkReply;

private slots:
    void checkFinished();

private:
    void setMediaServerStatus(QString status);
};

#endif // APPSETTINGS_H
