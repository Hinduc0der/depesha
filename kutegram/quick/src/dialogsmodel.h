#ifndef DIALOGSMODEL_H
#define DIALOGSMODEL_H

#include <QAbstractListModel>
#include <QVariant>
#include <QMutex>
#include "tgclient.h"
#include "avatardownloader.h"
#include "foldersmodel.h"

class DialogsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QObject* client READ client WRITE setClient)
    Q_PROPERTY(QObject* avatarDownloader READ avatarDownloader WRITE setAvatarDownloader)
    Q_PROPERTY(QObject* folders READ folders WRITE setFolders)
    // Оба выключателя нужны и из QML -- см. комментарии к сеттерам ниже.
    Q_PROPERTY(bool autoLoadAll READ autoLoadAll WRITE setAutoLoadAll)
    Q_PROPERTY(bool lazyAvatars READ lazyAvatars WRITE setLazyAvatars)

private:
    QMutex _mutex;
    QList<TgObject> _dialogs;

    TgClient* _client;
    TgLongVariant _userId;

    TgLongVariant _requestId;
    TgObject _offsets;

    AvatarDownloader* _avatarDownloader;

    FoldersModel* _folders;

    // Догружать ли следующую страницу чатов сразу, не дожидаясь, пока до неё
    // долистают.
    bool _autoLoadAll;
    // Качать ли аватарки всех пришедших чатов разом.
    bool _lazyAvatars;
    qint32 _lastPinnedIndex;

    enum DialogRoles {
        TitleRole = Qt::UserRole + 1,
        ThumbnailColorRole,
        ThumbnailTextRole,
        AvatarRole,
        MessageTimeRole,
        MessageTextRole,
        TooltipRole,
        PeerBytesRole,
        MessageSenderNameRole,
        MessageSenderColorRole,
        UnreadCountRole,
        PinnedRole
    };

public:
    explicit DialogsModel(QObject *parent = 0);
    void resetState();

    QHash<int, QByteArray> roleNames() const;

    void setClient(QObject *client);
    QObject* client() const;

    void setAvatarDownloader(QObject *client);

    // Список чатов на телефоне грузился целиком: каждая пришедшая страница
    // тут же просила следующую, и так до конца списка -- сотни чатов и сотни
    // аватарок сразу после входа, всё в главном потоке. Клиенту, который
    // догружает страницы по прокрутке, это не нужно.
    void setAutoLoadAll(bool enabled);
    bool autoLoadAll() const;
    // Аватарки только тех строк, до которых долистали.
    void setLazyAvatars(bool enabled);
    bool lazyAvatars() const;
    QObject* avatarDownloader() const;

    void setFolders(QObject *model);
    QObject* folders() const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;

    TgObject createRow(TgObject dialog, TgObject peer, TgObject message, TgObject messageSender, QList<TgObject> folders, TgList users, TgList chats);
    void handleDialogMessage(TgObject &row, TgObject message, TgObject messageSender, TgList users, TgList chats);
    void prepareNotification(TgObject row);

signals:
    void sendNotification(qint64 peerId, QString peerName, QString senderName, QString text, bool silent);

public slots:
    void authorized(TgLongVariant userId);
    void messagesGetDialogsResponse(TgObject data, TgLongVariant messageId);
    void avatarDownloaded(TgLongVariant photoId, QString filePath);

    void refresh();

    // Аватарки для строк с first по last включительно. Нужна при ленивой
    // загрузке: список сам сообщает, что сейчас видно.
    void requestAvatars(qint32 first, qint32 last);

    bool canFetchMoreDownwards() const;
    void fetchMoreDownwards();

    void foldersChanged(QList<TgObject> folders);
    bool inFolder(qint32 index, qint32 folderIndex);

    void gotUpdate(TgObject update, TgLongVariant messageId, TgList users, TgList chats, qint32 date, qint32 seq, qint32 seqStart);
    void gotMessageUpdate(TgObject update, TgLongVariant messageId);

};

#endif // DIALOGSMODEL_H
