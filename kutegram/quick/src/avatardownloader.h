#ifndef AVATARDOWNLOADER_H
#define AVATARDOWNLOADER_H

#include <QObject>

#include <QMutex>
#include <QColor>
#include <QSettings>
#include "tgclient.h"

class AvatarDownloader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* client READ client WRITE setClient)

private:
    QMutex _mutex;
    TgClient* _client;
    TgLongVariant _userId;
    QHash<qint64, TgLongVariant> _requestsAvatars;
    QHash<qint64, TgLongVariant> _requestsPhotos;
    QHash<qint64, TgLongVariant> _requestsStickers;

    // Фоновые загрузки -- единственное, что клиент делает без спроса. Когда
    // системе не хватает памяти, они первыми и должны замолчать.
    bool lowOnMemory();
    bool _lowMemoryLogged;

    // Фотографии и стикеры этот же загрузчик качает сам, без спроса -- по
    // сорок штук на страницу истории. Клиенту, который их не рисует, они
    // стоят только памяти.
    bool _mediaEnabled;

    // Аватарки нужны не всем спискам. В переписке Депеша их не рисует, а
    // качались они на каждого участника каждой страницы истории -- и каждая
    // ещё и перерисовывалась в круглый PNG.
    bool _avatarsEnabled;

    // Круглый PNG 160x160 на каждую аватарку -- это разворот JPEG, рисование
    // с сглаживанием и запись файла, всё в главном потоке. На сорок чатов
    // разом телефон встаёт на минуту.
    bool _roundAvatars;

public:
    explicit AvatarDownloader(QObject *parent = 0);
    void readDatabase();
    void saveDatabase();

    void setClient(QObject *client);
    QObject* client() const;

    // Аватарки качать, фотографии и стикеры -- нет. По умолчанию качается всё.
    void setMediaEnabled(bool enabled);
    // И наоборот: фотографии со стикерами качать, аватарки -- нет.
    void setAvatarsEnabled(bool enabled);
    // Перерисовывать ли скачанную аватарку в круглый PNG. Кому круг не нужен
    // (виджеты режут сами), тот платить за него не должен.
    void setRoundAvatars(bool enabled);

signals:
    void avatarDownloaded(TgLongVariant photoId, QString filePath);
    void photoDownloaded(TgLongVariant photoId, QString filePath);
    void stickerDownloaded(TgLongVariant stickerId, QString filePath);

public slots:
    void authorized(TgLongVariant userId);
    void fileDownloaded(TgLongVariant fileId, QString filePath);
    void fileDownloadCanceled(TgLongVariant fileId, QString filePath);

    qint64 downloadAvatar(TgObject peer);
    qint64 downloadPhoto(TgObject photo);
    // Stickers are cached next to avatars and photos rather than dropped in
    // the download folder: they arrive by themselves, not because anyone
    // asked for a file.
    qint64 downloadSticker(TgObject document);

    static QString getAvatarText(QString title);
    static QColor userColor(TgLongVariant id);

};

#endif // AVATARDOWNLOADER_H

