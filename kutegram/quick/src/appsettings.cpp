#include "appsettings.h"

#include <QNetworkReply>
#include <QTimer>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>

#include "tgclient.h"

AppSettings::AppSettings(QObject *parent) :
    QObject(parent),
    _checkReply(0)
{
    QSettings settings(TgClient::settingsFilePath("settings"), QSettings::IniFormat);

    _markAsRead = settings.value("MarkAsRead", true).toBool();
    _notifications = settings.value("Notifications", true).toBool();
    _vibrate = settings.value("Vibrate", true).toBool();

    // Off and empty by default: the client has to be useful to someone who has
    // no server at all.
    _mediaServer = settings.value("MediaServer", false).toBool();
    _mediaServerUrl = settings.value("MediaServerUrl", "").toString();
    _mediaServerToken = settings.value("MediaServerToken", "").toString();
}

void AppSettings::save()
{
    QSettings settings(TgClient::settingsFilePath("settings"), QSettings::IniFormat);

    settings.setValue("MarkAsRead", _markAsRead);
    settings.setValue("Notifications", _notifications);
    settings.setValue("Vibrate", _vibrate);
    settings.setValue("MediaServer", _mediaServer);
    settings.setValue("MediaServerUrl", _mediaServerUrl);
    settings.setValue("MediaServerToken", _mediaServerToken);
    settings.sync();
}

bool AppSettings::markAsRead() const
{
    return _markAsRead;
}

void AppSettings::setMarkAsRead(bool value)
{
    if (_markAsRead == value) {
        return;
    }

    _markAsRead = value;
    save();
    emit markAsReadChanged();
}

bool AppSettings::notifications() const
{
    return _notifications;
}

void AppSettings::setNotifications(bool value)
{
    if (_notifications == value) {
        return;
    }

    _notifications = value;
    save();
    emit notificationsChanged();
}

bool AppSettings::vibrate() const
{
    return _vibrate;
}

void AppSettings::setVibrate(bool value)
{
    if (_vibrate == value) {
        return;
    }

    _vibrate = value;
    save();
    emit vibrateChanged();
}

bool AppSettings::mediaServer() const
{
    return _mediaServer;
}

void AppSettings::setMediaServer(bool value)
{
    if (_mediaServer == value) {
        return;
    }

    _mediaServer = value;
    save();
    emit mediaServerChanged();
}

QString AppSettings::mediaServerUrl() const
{
    return _mediaServerUrl;
}

void AppSettings::setMediaServerUrl(QString value)
{
    value = value.trimmed();

    if (_mediaServerUrl == value) {
        return;
    }

    _mediaServerUrl = value;
    save();
    emit mediaServerChanged();
}

QString AppSettings::mediaServerToken() const
{
    return _mediaServerToken;
}

void AppSettings::setMediaServerToken(QString value)
{
    value = value.trimmed();

    if (_mediaServerToken == value) {
        return;
    }

    _mediaServerToken = value;
    save();
    emit mediaServerChanged();
}

QString AppSettings::mediaServerStatus() const
{
    return _mediaServerStatus;
}

void AppSettings::setMediaServerStatus(QString status)
{
    if (_mediaServerStatus == status) {
        return;
    }

    _mediaServerStatus = status;
    emit mediaServerStatusChanged();
}

QString AppSettings::activeMediaServerUrl() const
{
    if (!_mediaServer) {
        return QString();
    }

    return _mediaServerUrl;
}

void AppSettings::checkMediaServer()
{
    if (_mediaServerUrl.isEmpty()) {
        setMediaServerStatus("no address");
        return;
    }

    if (_checkReply) {
        _checkReply->abort();
        _checkReply = 0;
    }

    QNetworkRequest request((QUrl(_mediaServerUrl)));
    if (!_mediaServerToken.isEmpty()) {
        request.setRawHeader("X-Kutegram-Token", _mediaServerToken.toUtf8());
    }

    _checkReply = _network.get(request);
    // Проверка адреса тоже должна заканчиваться: иначе «checking...» висит, и
    // непонятно, плохой адрес или просто долго.
    QTimer::singleShot(20000, _checkReply, SLOT(abort()));
    connect(_checkReply, SIGNAL(finished()), this, SLOT(checkFinished()));

    setMediaServerStatus("checking...");
}

void AppSettings::checkFinished()
{
    QNetworkReply *reply = _checkReply;
    _checkReply = 0;

    if (!reply) {
        return;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        setMediaServerStatus("not reachable: " + reply->errorString());
        return;
    }

    QString answer = QString::fromUtf8(reply->readAll()).trimmed();
    setMediaServerStatus(answer.isEmpty() ? "answered" : answer.left(60));
}

void AppSettings::toggle(QString name)
{
    if (name == "markAsRead") {
        setMarkAsRead(!_markAsRead);
    } else if (name == "notifications") {
        setNotifications(!_notifications);
    } else if (name == "vibrate") {
        setVibrate(!_vibrate);
    } else if (name == "mediaServer") {
        setMediaServer(!_mediaServer);
    }
}

bool AppSettings::value(QString name) const
{
    if (name == "markAsRead") {
        return _markAsRead;
    }

    if (name == "notifications") {
        return _notifications;
    }

    if (name == "vibrate") {
        return _vibrate;
    }

    if (name == "mediaServer") {
        return _mediaServer;
    }

    return false;
}
