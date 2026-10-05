#include "mediaconverter.h"
#include "appsettings.h"
#include "cachemanager.h"
#include "kglog.h"

#include <QFile>
#include <QStringList>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

// A converted animation is small, but not free: 128 pixels of GIF at eight
// frames a second for a few seconds. Anything the server refuses to convert
// comes back as an error rather than as a huge file.
// Файл читается в память целиком, а куча на телефоне -- 32 МБ на всё. Двенадцать
// мегабайт тут не помещаются вместе с копией, которую сделает сам запрос.
static const qint64 KG_MAX_UPLOAD = 3 * 1024 * 1024;

// Запрос к серверу должен когда-нибудь закончиться.
//
// Qt 4.7 своего таймаута на ответ не имеет, а соединение с адресом, который
// просто молчит (неверно набран, сервер выключен, сеть отвалилась), висит
// неопределённо долго. Для пользователя это выглядит как «нажал и ничего»:
// строка так и стоит «converting...». Поэтому ответ ждём ограниченное время и
// обрываем сами -- abort() приводит к обычному finished() с ошибкой, и дальше
// всё идёт по уже написанному пути.
static const int KG_NETWORK_TIMEOUT_MS = 45000;


MediaConverter::MediaConverter(AppSettings *settings, QObject *parent) :
    QObject(parent),
    _status("idle"),
    _settings(settings)
{
}

QString MediaConverter::status() const
{
    return _status;
}

void MediaConverter::setStatus(QString status)
{
    if (_status == status) {
        return;
    }

    _status = status;
    kgLog("media convert: " + status);
    emit statusChanged();
}

void MediaConverter::convert(int index, QString path, QString kind)
{
    if (path.startsWith("file:///")) {
        path = path.mid(8);
    }

    if (path.isEmpty() || !QFile::exists(path)) {
        setStatus("no file");
        return;
    }

    const QString serverUrl = _settings ? _settings->activeMediaServerUrl() : QString();
    if (serverUrl.isEmpty()) {
        setStatus("media server is off");
        return;
    }

    // Уже converted -- второй раз гонять мегабайты на сервер и обратно незачем.
    QStringList ready;
    if (kind == "video") {
        ready << ".3gp" << ".png";
    } else if (kind == "audio") {
        ready << ".mp3" << ".amr";
    } else {
        ready << ".gif" << ".png";
    }

    for (int i = 0; i < ready.size(); ++i) {
        const QString existing = CacheManager::convertedPath(path, ready[i]);

        if (QFile::exists(existing)) {
            setStatus("idle");
            emit converted(index, existing);
            return;
        }
    }

    QFile file(path);
    if (file.size() > KG_MAX_UPLOAD) {
        setStatus("too large to convert");
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        setStatus("cannot read");
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QNetworkRequest request(QUrl(serverUrl + "?want=" + kind));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");

    const QString token = _settings->mediaServerToken();
    if (!token.isEmpty()) {
        request.setRawHeader("X-Kutegram-Token", token.toUtf8());
    }

    QNetworkReply *reply = _network.post(request, data);
    QTimer::singleShot(KG_NETWORK_TIMEOUT_MS, reply, SLOT(abort()));
    _rows.insert(reply, index);
    _paths.insert(reply, path);
    connect(reply, SIGNAL(finished()), this, SLOT(requestFinished()));

    setStatus("converting");
}

void MediaConverter::requestFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    const int index = _rows.take(reply);
    const QString path = _paths.take(reply);
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        setStatus("media server unreachable");
        return;
    }

    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        setStatus("nothing came back");
        return;
    }

    const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();

    QString extension = ".gif";
    if (contentType.contains("png")) {
        extension = ".png";
    } else if (contentType.contains("3gpp") || contentType.contains("mp4")) {
        extension = ".3gp";
    }

    // Into the cache, not next to the download; see voiceplayer.cpp.
    const QString convertedPath = CacheManager::convertedPath(path, extension);

    QFile file(convertedPath);
    if (!file.open(QIODevice::WriteOnly)) {
        setStatus("cannot save");
        return;
    }

    file.write(data);
    file.close();

    setStatus("idle");
    emit converted(index, convertedPath);
}
