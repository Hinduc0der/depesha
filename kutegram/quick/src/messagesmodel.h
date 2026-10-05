#ifndef MESSAGESMODEL_H
#define MESSAGESMODEL_H

#include <QAbstractListModel>
#include <QVariant>
#include <QMutex>
#include <QTime>
#include "tgclient.h"
#include "avatardownloader.h"

class MessagesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QObject* client READ client WRITE setClient)
    Q_PROPERTY(QObject* avatarDownloader READ avatarDownloader WRITE setAvatarDownloader)
    Q_PROPERTY(QByteArray peer READ peer WRITE setPeer)
    // Bound to the switch in the drawer. Telling Telegram a chat has been read
    // cannot be taken back, so it stays the user's decision.
    Q_PROPERTY(bool markAsRead READ markAsRead WRITE setMarkAsRead)
    Q_PROPERTY(int replyToMsgId READ replyToMsgId WRITE setReplyToMsgId NOTIFY replyToMsgIdChanged)

private:
    QMutex _mutex;
    QList<TgObject> _history;

    TgClient* _client;
    TgLongVariant _userId;

    TgObject _peer;
    TgObject _inputPeer;

    TgLongVariant _upRequestId;
    TgLongVariant _downRequestId;

    qint32 _upOffset;
    qint32 _downOffset;

    AvatarDownloader* _avatarDownloader;

    QHash<qint64, TgVariant> _downloadRequests;

    TgLongVariant _uploadId;

    bool _markAsRead;
    qint32 _lastReadId;
    qint32 _replyToMsgId;
    QTime _typingSent;
    bool _typingActive;
    QString _lastDraft;

    QHash<TgLong, QString> _sentMessages;
    TgObject _media;

    enum MessageRoles {
        PeerNameRole = Qt::UserRole + 1,
        MessageTextRole,
        MergeMessageRole,
        SenderNameRole,
        MessageTimeRole,
        IsChannelRole,
        ThumbnailColorRole,
        ThumbnailTextRole,
        AvatarRole,
        HasMediaRole,
        MediaImageRole,
        MediaTitleRole,
        MediaTextRole,
        MediaDownloadableRole,
        MessageIdRole,
        ForwardedFromRole,
        MediaUrlRole,
        PhotoFileRole,
        PhotoPreviewRole,
        HasPhotoRole,
        PhotoSpoilerRole,
        MediaSpoilerRole,
        HasStickerRole,
        StickerFileRole,
        MessagePlainTextRole,
        MediaIsVoiceRole,
        MediaFilePathRole,
        HasAnimationRole,
        AnimationFileRole
    };

public:
    explicit MessagesModel(QObject *parent = 0);
    void resetState();

    QHash<int, QByteArray> roleNames() const;

    void setClient(QObject *client);
    QObject* client() const;

    void setAvatarDownloader(QObject *client);
    QObject* avatarDownloader() const;

    void setPeer(QByteArray bytes);
    QByteArray peer() const;

    bool markAsRead() const;
    void setMarkAsRead(bool value);

    int replyToMsgId() const;
    void setReplyToMsgId(int messageId);

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;

    TgObject createRow(TgObject message, TgObject sender, TgList users, TgList chats);

    void handleHistoryResponse(TgObject data, TgLongVariant messageId);
    void handleHistoryResponseUpwards(TgObject data, TgLongVariant messageId);

signals:
    void scrollTo(qint32 index);
    void downloadUpdated(qint32 messageId, qint32 state, QString filePath);
    void draftChanged(QString draft);
    void uploadingProgress(qint32 progress);
    void scrollForNew();
    void sentMessageUpdate(TgObject update, TgLongVariant messageId);
    void replyToMsgIdChanged();

public slots:
    void authorized(TgLongVariant userId);
    void messagesGetHistoryResponse(TgObject data, TgLongVariant messageId);
    void avatarDownloaded(TgLongVariant photoId, QString filePath);
    void photoDownloaded(TgLongVariant photoId, QString filePath);
    void stickerDownloaded(TgLongVariant stickerId, QString filePath);

    void fileDownloaded(TgLongVariant fileId, QString filePath);
    void fileDownloadCanceled(TgLongVariant fileId, QString filePath);

    void fileUploading(TgLongVariant fileId, TgLongVariant processedLength, TgLongVariant totalLength, qint32 progressPercentage);
    void fileUploaded(TgLongVariant fileId, TgObject inputFile);
    void fileUploadCanceled(TgLongVariant fileId);

    void sendMessage(QString message);
    // То же самое, но без звука и/или отложенно. Срок задаётся **в секундах
    // от текущего момента**, а не абсолютным временем: считать его надо по
    // часам сервера, а не телефона, и модель это делает сама.
    void sendMessageWith(QString message, bool silent, qint32 secondsFromNow);
    void uploadFile();
    // Путь приходит из своего выбора файла в QML; см. filebrowser.h о том,
    // почему системный диалог убран.
    void uploadFilePath(QString path);
    void cancelUpload();

    void gotMessageUpdate(TgObject update, TgLongVariant messageId);
    void gotUpdate(TgObject update, TgLongVariant messageId, TgList users, TgList chats, qint32 date, qint32 seq, qint32 seqStart);

    bool canFetchMoreDownwards() const;
    void fetchMoreDownwards();

    bool canFetchMoreUpwards() const;
    void fetchMoreUpwards();

    void linkActivated(QString link, qint32 index);
    // The single action of the selected row, so the keypad can reach media
    // without having to find the QML item that draws it.
    void activateMedia(qint32 index);
    // Everything the message menu can do to the selected row.
    void markReadIfNeeded();
    void setTyping(bool typing);
    void saveDraft(QString text);
    void deleteMessage(qint32 index);
    // Called when the media server hands back a converted animation.
    void setAnimationFile(int index, QString path);
    QString plainText(qint32 index);

    void openLink(QString url);
    void openDownloadedFile(QString filePath);
    void downloadFile(qint32 index);
    void cancelDownload(qint32 index);
};

#endif // MESSAGESMODEL_H
