#include "avatardownloader.h"

#include <QFile>
#include <QImage>
#include <QPainter>
#include <QBrush>
#include <QCoreApplication>

#include "kglog.h"

AvatarDownloader::AvatarDownloader(QObject *parent)
     : QObject(parent)
     , _mutex(QMutex::Recursive)
     , _client(0)
     , _userId(0)
     , _lowMemoryLogged(false)
     , _mediaEnabled(true)
     , _avatarsEnabled(true)
     , _roundAvatars(true)
     , _requestsAvatars()
     , _requestsPhotos()
     , _requestsStickers()
{
}

// The cache file used to hold a list of every avatar, photo and sticker ever
// downloaded, rewritten in full after each one. It grew forever, it made every
// download cost more than the one before it, and it went out of step the moment
// a file was deleted. The files on disk answer the same question better, so the
// old lists are simply dropped -- this clears them out of any cache file left
// over from an older version.
void AvatarDownloader::saveDatabase()
{
}

void AvatarDownloader::readDatabase()
{
    QSettings settings(TgClient::settingsFilePath("cache"), QSettings::IniFormat);

    if (settings.contains("DownloadedAvatars")) {
        settings.remove("DownloadedAvatars");
        settings.remove("DownloadedPhotos");
        settings.remove("DownloadedStickers");
    }
}

void AvatarDownloader::setClient(QObject *client)
{
    QMutexLocker lock(&_mutex);

    if (_client) {
        _client->disconnect(this);
        saveDatabase();
    }

    _client = dynamic_cast<TgClient*>(client);
    _userId = _client->getUserId();

    _requestsAvatars.clear();
    _requestsPhotos.clear();
    _requestsStickers.clear();
    readDatabase();

    if (!_client) return;

    _client->sessionDirectory().mkdir("Kutegram_avatars");
    _client->sessionDirectory().mkdir("Kutegram_photos");
    _client->sessionDirectory().mkdir("Kutegram_stickers");

    connect(_client, SIGNAL(authorized(TgLongVariant)), this, SLOT(authorized(TgLongVariant)));
    connect(_client, SIGNAL(fileDownloaded(TgLongVariant,QString)), this, SLOT(fileDownloaded(TgLongVariant,QString)));
    connect(_client, SIGNAL(fileDownloadCanceled(TgLongVariant,QString)), this, SLOT(fileDownloadCanceled(TgLongVariant,QString)));
}

void AvatarDownloader::authorized(TgLongVariant userId)
{
    QMutexLocker lock(&_mutex);

    if (_userId != userId) {
        _requestsAvatars.clear();
        _requestsPhotos.clear();
        _requestsStickers.clear();
        _userId = userId;
    }
}

QObject* AvatarDownloader::client() const
{
    return _client;
}

// Ниже этого порога телефон уже начинает закрывать приложения сам, и добавлять
// к этому фоновую загрузку картинок -- худшее, что можно сделать.
#define KG_LOW_RAM_BYTES (6 * 1024 * 1024)

bool AvatarDownloader::lowOnMemory()
{
    const int freeRam = kgFreeRam();

    // Ноль означает, что HAL не ответил, а не что памяти не осталось: иначе
    // одна неудачная проверка тихо выключила бы все загрузки разом.
    if (freeRam <= 0 || freeRam >= KG_LOW_RAM_BYTES) {
        _lowMemoryLogged = false;
        return false;
    }

    // Один раз на каждый заход в это состояние: строка в минуту от
    // сердцебиения уже есть, дублировать её на каждую картинку незачем.
    if (!_lowMemoryLogged) {
        _lowMemoryLogged = true;
        kgLog("low on memory (" + QString::number(freeRam / 1024)
              + "k free), pausing downloads");
    }

    return true;
}

void AvatarDownloader::setMediaEnabled(bool enabled)
{
    QMutexLocker lock(&_mutex);

    _mediaEnabled = enabled;
}

void AvatarDownloader::setAvatarsEnabled(bool enabled)
{
    QMutexLocker lock(&_mutex);

    _avatarsEnabled = enabled;
}

void AvatarDownloader::setRoundAvatars(bool enabled)
{
    QMutexLocker lock(&_mutex);

    _roundAvatars = enabled;
}

qint64 AvatarDownloader::downloadPhoto(TgObject photo)
{
    QMutexLocker lock(&_mutex);

    if (!_mediaEnabled || !_client || !_client->isAuthorized() || GETID(photo) == 0
            || lowOnMemory()) {
        return 0;
    }

    qint64 photoId = photo["id"].toLongLong();

    QString relativePath = "Kutegram_photos/" + QString::number(photoId) + ".jpg";
    QString avatarFilePath = _client->sessionDirectory().absoluteFilePath(relativePath);

    if (!QFile::exists(avatarFilePath + ".thumbnail.jpg")) {
        qint64 loadingId = _client->downloadFile(avatarFilePath, photo).toLongLong();
        _requestsPhotos[loadingId] = photoId;
    } else {
#if QT_VERSION >= 0x050000
        emit photoDownloaded(photoId, "file:///" + avatarFilePath);
#else
        emit photoDownloaded(photoId, avatarFilePath);
#endif
    }

    return photoId;
}

qint64 AvatarDownloader::downloadSticker(TgObject document)
{
    QMutexLocker lock(&_mutex);

    if (!_mediaEnabled || !_client || !_client->isAuthorized() || GETID(document) == 0
            || lowOnMemory()) {
        return 0;
    }

    qint64 stickerId = document["id"].toLongLong();

    QString relativePath = "Kutegram_stickers/" + QString::number(stickerId) + ".webp";
    QString stickerFilePath = _client->sessionDirectory().absoluteFilePath(relativePath);

    // Вся цепочка стикера пишется в журнал: он теряется где-то между запросом
    // и картинкой на экране, и с этой стороны шага не видно.
    if (!QFile::exists(stickerFilePath)) {
        qint64 loadingId = _client->downloadFile(stickerFilePath, document).toLongLong();
        _requestsStickers[loadingId] = stickerId;
        kgLog("sticker " + QString::number(stickerId) + ": requested, file id "
              + QString::number(loadingId), false);
    } else {
        kgLog("sticker " + QString::number(stickerId) + ": already on disk", false);
        emit stickerDownloaded(stickerId, stickerFilePath);
    }

    return stickerId;
}

qint64 AvatarDownloader::downloadAvatar(TgObject peer)
{
    QMutexLocker lock(&_mutex);

    if (!_avatarsEnabled || !_client || !_client->isAuthorized()
            || TgClient::commonPeerType(peer) == 0 || lowOnMemory()) {
        return 0;
    }

    TgObject photo = peer["photo"].toMap();
    if (GETID(photo) == 0) {
        return 0;
    }

    qint64 photoId = photo["photo_id"].toLongLong();

    QString relativePath = "Kutegram_avatars/" + QString::number(photoId) + ".jpg";
    QString avatarFilePath = _client->sessionDirectory().absoluteFilePath(relativePath);

    // Whether it is here is a question about the file, not about a list that
    // remembers everything ever downloaded. The list could not survive the
    // cache being cleared, and it only ever grew.
    const QString readyPath = _roundAvatars ? avatarFilePath + ".png" : avatarFilePath;

    if (!QFile::exists(readyPath)) {
        qint64 loadingId = _client->downloadFile(avatarFilePath, peer).toLongLong();
        _requestsAvatars[loadingId] = photoId;
    } else {
#if QT_VERSION >= 0x050000
        emit avatarDownloaded(photoId, "file:///" + readyPath);
#else
        emit avatarDownloaded(photoId, readyPath);
#endif
    }

    return photoId;
}

void AvatarDownloader::fileDownloaded(TgLongVariant fileId, QString filePath)
{
    QMutexLocker lock(&_mutex);

    TgLongVariant photoId = _requestsAvatars.take(fileId.toLongLong());
    if (!photoId.isNull()) {
        if (!_roundAvatars) {
            // Отдаём как есть: круглой её сделает тот, кто рисует.
#if QT_VERSION >= 0x050000
            emit avatarDownloaded(photoId, "file:///" + filePath);
#else
            emit avatarDownloaded(photoId, filePath);
#endif
            return;
        }

        QFile file(filePath);
        if (!file.open(QFile::ReadOnly)) {
            return;
        }

        QImage roundedImage(160, 160, QImage::Format_ARGB32);
        roundedImage.fill(Qt::transparent);
        QPainter painter(&roundedImage);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QBrush(QImage::fromData(file.readAll())));
        painter.setPen(Qt::transparent);
        file.close();
        painter.drawRoundedRect(0, 0, 160, 160, 80, 80);
        filePath += ".png";
        if (!roundedImage.save(filePath)) {
            return;
        }

    #if QT_VERSION >= 0x050000
        emit avatarDownloaded(photoId, "file:///" + filePath);
    #else
        emit avatarDownloaded(photoId, filePath);
    #endif
        return;
    }

    TgLongVariant stickerId = _requestsStickers.take(fileId.toLongLong());
    if (!stickerId.isNull()) {
        kgLog("sticker " + stickerId.toString() + ": arrived, "
              + QString::number(QFile(filePath).size()) + " bytes", false);
        // Nothing is converted here: the WebP goes to the image provider as it
        // is, which decodes it straight to the size it will be drawn at.
        emit stickerDownloaded(stickerId, filePath);
        return;
    }

    photoId = _requestsPhotos.take(fileId.toLongLong());
    if (!photoId.isNull()) {
        QFile file(filePath);
        if (!file.open(QFile::ReadOnly)) {
            return;
        }

        QImage scaledImage = QImage::fromData(file.readAll());
        if (scaledImage.height() > scaledImage.width()) {
            scaledImage = scaledImage.scaledToHeight(280, Qt::SmoothTransformation);
        } else {
            scaledImage = scaledImage.scaledToWidth(280, Qt::SmoothTransformation);
        }
        file.close();
        if (!scaledImage.save(filePath + ".thumbnail.jpg")) {
            return;
        }

    #if QT_VERSION >= 0x050000
        emit photoDownloaded(photoId, "file:///" + filePath);
    #else
        emit photoDownloaded(photoId, filePath);
    #endif
        return;
    }
}

void AvatarDownloader::fileDownloadCanceled(TgLongVariant fileId, QString filePath)
{
    QMutexLocker lock(&_mutex);

    _requestsAvatars.remove(fileId.toLongLong());
    _requestsPhotos.remove(fileId.toLongLong());
    _requestsStickers.remove(fileId.toLongLong());
}

QString AvatarDownloader::getAvatarText(QString title)
{
    QStringList split = title.split(" ", QString::SkipEmptyParts);
    QString result;

    for (qint32 i = 0; i < split.size(); ++i) {
        QString item = split[i];
        for (qint32 j = 0; j < item.length(); ++j) {
            if (item[j].isLetterOrNumber()) {
                result += item[j].toUpper();
                break;
            }
        }

        if (result.size() > 1) {
            break;
        }
    }

    if (result.isEmpty() && !title.isEmpty())
        result += title[0].toUpper();

    return result;
}

QColor AvatarDownloader::userColor(TgLongVariant id)
{
   return QColor::fromHsl(id.toLongLong() % 360, 160, 120);
}
