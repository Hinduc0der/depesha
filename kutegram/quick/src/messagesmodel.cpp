#include "messagesmodel.h"
#include "kglog.h"

#include "tlschema.h"
#include <QMutexLocker>
#include <QColor>
#include <QDateTime>
#include <QUrl>
#include <QDomDocument>
#include "avatardownloader.h"
#include "cachemanager.h"
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include "messageutil.h"
#include "platformutils.h"

#if QT_VERSION >= 0x050000
#include <QStandardPaths>
#endif

using namespace TLType;

#define BATCH_SIZE 40

MessagesModel::MessagesModel(QObject *parent)
    : QAbstractListModel(parent)
    , _mutex(QMutex::Recursive)
    , _history()
    , _client(0)
    , _userId(0)
    , _peer()
    , _inputPeer()
    , _upRequestId(0)
    , _downRequestId(0)
    , _upOffset(0)
    , _downOffset(0)
    , _avatarDownloader(0)
    , _markAsRead(true)
    , _voiceUpload(false)
    , _mediaServerOn(false)
    , _lastReadId(0)
    , _replyToMsgId(0)
    , _typingActive(false)
    , _downloadRequests()
    , _uploadId(0)
    , _sentMessages()
    , _media()
{
#if QT_VERSION < 0x050000
    setRoleNames(roleNames());
#endif
}

QHash<int, QByteArray> MessagesModel::roleNames() const
{
    static QHash<int, QByteArray> roles;

    if (!roles.isEmpty())
        return roles;

    roles[PeerNameRole] = "peerName";
    roles[MessageTextRole] = "messageText";
    roles[MergeMessageRole] = "mergeMessage";
    roles[MessageTimeRole] = "messageTime";
    roles[SenderNameRole] = "senderName";
    roles[IsChannelRole] = "isChannel";
    roles[ThumbnailColorRole] = "thumbnailColor";
    roles[ThumbnailTextRole] = "thumbnailText";
    roles[AvatarRole] = "avatar";
    roles[HasMediaRole] = "hasMedia";
    roles[MediaImageRole] = "mediaImage";
    roles[MediaTitleRole] = "mediaTitle";
    roles[MediaTextRole] = "mediaText";
    roles[MediaDownloadableRole] = "mediaDownloadable";
    roles[MessageIdRole] = "messageId";
    roles[ForwardedFromRole] = "forwardedFrom";
    roles[MediaUrlRole] = "mediaUrl";
    roles[PhotoFileRole] = "photoFile";
    roles[PhotoPreviewRole] = "photoPreview";
    roles[HasPhotoRole] = "hasPhoto";
    roles[MediaSpoilerRole] = "mediaSpoiler";
    roles[PhotoSpoilerRole] = "photoSpoiler";
    roles[HasStickerRole] = "hasSticker";
    roles[StickerFileRole] = "stickerFile";
    roles[MessagePlainTextRole] = "messagePlainText";
    roles[MediaIsVoiceRole] = "mediaIsVoice";
    roles[MediaFilePathRole] = "mediaFilePath";
    roles[HasAnimationRole] = "hasAnimation";
    roles[AnimationFileRole] = "animationFile";

    return roles;
}

void MessagesModel::resetState()
{
    if (!_history.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, _history.size() - 1);
        _history.clear();
        endRemoveRows();
    }

    _peer = TgObject();
    _inputPeer = TgObject();
    _upRequestId = 0;
    _downRequestId = 0;
    _upOffset = 0;
    _downOffset = 0;

    _lastReadId = 0;
    _replyToMsgId = 0;
    _typingActive = false;
    _lastDraft = QString();
}

void MessagesModel::setClient(QObject *client)
{
    QMutexLocker lock(&_mutex);

    if (_client) {
        _client->disconnect(this);
    }

    _client = dynamic_cast<TgClient*>(client);
    _userId = 0;

    resetState();
    cancelUpload();

    if (!_client) return;

    connect(_client, SIGNAL(authorized(TgLongVariant)), this, SLOT(authorized(TgLongVariant)));
    connect(_client, SIGNAL(messagesMessagesResponse(TgObject,TgLongVariant)), this, SLOT(messagesGetHistoryResponse(TgObject,TgLongVariant)));
    connect(_client, SIGNAL(fileDownloaded(TgLongVariant,QString)), this, SLOT(fileDownloaded(TgLongVariant,QString)));
    connect(_client, SIGNAL(fileDownloadCanceled(TgLongVariant,QString)), this, SLOT(fileDownloadCanceled(TgLongVariant,QString)));
    connect(_client, SIGNAL(gotMessageUpdate(TgObject,TgLongVariant)), this, SLOT(gotMessageUpdate(TgObject,TgLongVariant)));
    connect(_client, SIGNAL(gotUpdate(TgObject,TgLongVariant,TgList,TgList,qint32,qint32,qint32)), this, SLOT(gotUpdate(TgObject,TgLongVariant,TgList,TgList,qint32,qint32,qint32)));
    connect(_client, SIGNAL(fileUploading(TgLongVariant,TgLongVariant,TgLongVariant,qint32)), this, SLOT(fileUploading(TgLongVariant,TgLongVariant,TgLongVariant,qint32)));
    connect(_client, SIGNAL(fileUploaded(TgLongVariant,TgObject)), this, SLOT(fileUploaded(TgLongVariant,TgObject)));
    connect(_client, SIGNAL(fileUploadCanceled(TgLongVariant)), this, SLOT(fileUploadCanceled(TgLongVariant)));
}

QObject* MessagesModel::client() const
{
    return _client;
}

void MessagesModel::setAvatarDownloader(QObject *avatarDownloader)
{
    QMutexLocker lock(&_mutex);

    if (_avatarDownloader) {
        _avatarDownloader->disconnect(this);
    }

    _avatarDownloader = dynamic_cast<AvatarDownloader*>(avatarDownloader);

    if (!_avatarDownloader) return;

    connect(_avatarDownloader, SIGNAL(avatarDownloaded(TgLongVariant,QString)), this, SLOT(avatarDownloaded(TgLongVariant,QString)));
    connect(_avatarDownloader, SIGNAL(photoDownloaded(TgLongVariant,QString)), this, SLOT(photoDownloaded(TgLongVariant,QString)));
    connect(_avatarDownloader, SIGNAL(stickerDownloaded(TgLongVariant,QString)), this, SLOT(stickerDownloaded(TgLongVariant,QString)));
}

QObject* MessagesModel::avatarDownloader() const
{
    return _avatarDownloader;
}

void MessagesModel::setPeer(QByteArray bytes)
{
    kgLog("chat opened " + kgMemoryInfo());

    QMutexLocker lock(&_mutex);

    resetState();
    _downloadRequests.clear();

    _peer = qDeserialize(bytes).toMap();
    _inputPeer = TgClient::toInputPeer(_peer);
    cancelUpload();

    _upOffset = _downOffset = qMax(_peer["read_inbox_max_id"].toInt(), _peer["read_outbox_max_id"].toInt());
    fetchMoreUpwards();
    fetchMoreDownwards();
}

QByteArray MessagesModel::peer() const
{
    return qSerialize(_peer);
}

int MessagesModel::rowCount(const QModelIndex &parent) const
{
    return _history.size();
}

QVariant MessagesModel::data(const QModelIndex &index, int role) const
{
    if (role == IsChannelRole) {
        return TgClient::isChannel(_peer);
    }

    if (role == MergeMessageRole) {
        if (index.row() < 1) {
            return false;
        }

        TgObject curr = _history[index.row()];
        TgObject prev = _history[index.row() - 1];

        if (!TgClient::peersEqual(curr["sender"].toMap(), prev["sender"].toMap())) {
            return false;
        }

        if (!curr["grouped_id"].isNull() && !prev["grouped_id"].isNull()
                && curr["grouped_id"].toLongLong() == prev["grouped_id"].toLongLong()) {
            return true;
        }

        if (!TgClient::isChannel(_peer) && curr["date"].toInt() - prev["date"].toInt() < 300) {
            return true;
        }

        return false;
    }

    return _history[index.row()][roleNames()[role]];
}

bool MessagesModel::canFetchMoreDownwards() const
{
    return _client && _client->isAuthorized() && TgClient::commonPeerType(_inputPeer) != 0 && !_downRequestId.toLongLong() && _downOffset != -1;
}

void MessagesModel::fetchMoreDownwards()
{
    QMutexLocker lock(&_mutex);

    _downRequestId = _client->messagesGetHistory(_inputPeer, _downOffset, 0, -BATCH_SIZE, BATCH_SIZE);
}

bool MessagesModel::canFetchMoreUpwards() const
{
    return _client && _client->isAuthorized() && TgClient::commonPeerType(_inputPeer) != 0 && !_upRequestId.toLongLong() && _upOffset != -1;
}

void MessagesModel::fetchMoreUpwards()
{
    QMutexLocker lock(&_mutex);

    _upRequestId = _client->messagesGetHistory(_inputPeer, _upOffset, 0, 0, BATCH_SIZE);
}

void MessagesModel::authorized(TgLongVariant userId)
{
    QMutexLocker lock(&_mutex);

    if (_userId != userId) {
        resetState();
        cancelUpload();
        _downloadRequests.clear();
        _userId = userId;
    }
}

void MessagesModel::messagesGetHistoryResponse(TgObject data, TgLongVariant messageId)
{
    QMutexLocker lock(&_mutex);

    if (messageId == _downRequestId) {
        handleHistoryResponse(data, messageId);
        _downRequestId = 0;
        return;
    }

    if (messageId == _upRequestId) {
        handleHistoryResponseUpwards(data, messageId);
        _upRequestId = 0;
        return;
    }
}

void MessagesModel::handleHistoryResponse(TgObject data, TgLongVariant messageId)
{
    kgLog("history page down, messages=" + QString::number(data["messages"].toList().size())
          + " peers=" + QString::number(globalUsers().size() + globalChats().size())
          + " " + kgMemoryInfo());

    TgList messages = data["messages"].toList();
    TgList chats = data["chats"].toList();
    TgList users = data["users"].toList();

    appendGlobalUsers(users);
    appendGlobalChats(chats);

    if (messages.isEmpty()) {
        _downOffset = -1;
        return;
    }

    QList<TgObject> messagesRows;
    messagesRows.reserve(messages.size());

    for (qint32 i = messages.size() - 1; i >= 0; --i) {
        TgObject message = messages[i].toMap();
        TgObject fromId = message["from_id"].toMap();
        TgObject sender;

        if (TgClient::isUser(fromId)) for (qint32 j = 0; j < users.size(); ++j) {
            TgObject peer = users[j].toMap();
            if (TgClient::peersEqual(peer, fromId)) {
                sender = peer;
                break;
            }
        }
        if (TgClient::isChat(fromId)) for (qint32 j = 0; j < chats.size(); ++j) {
            TgObject peer = chats[j].toMap();
            if (TgClient::peersEqual(peer, fromId)) {
                sender = peer;
                break;
            }
        }
        if (TgClient::commonPeerType(fromId) == 0) {
            sender = _peer;
        }

        messagesRows.append(createRow(message, sender, users, chats));
    }

    qint32 oldOffset = _downOffset;
    qint32 newOffset = messages.first().toMap()["id"].toInt();
    if (_downOffset != newOffset && messages.size() == BATCH_SIZE) {
        _downOffset = newOffset;
    } else {
        _downOffset = -1;
    }

    qint32 oldSize = _history.size();

    beginInsertRows(QModelIndex(), _history.size(), _history.size() + messagesRows.size() - 1);
    _history.append(messagesRows);
    endInsertRows();

    if (oldSize > 0) {
        emit dataChanged(index(oldSize - 1), index(oldSize - 1));
    }

    // aka it is the first time when history is loaded in chat
    if (qMax(_peer["read_inbox_max_id"].toInt(), _peer["read_outbox_max_id"].toInt()) == oldOffset) {
        emit scrollTo(_history.size() - 1);
    }


    if (_avatarDownloader) {
        for (qint32 i = 0; i < users.size(); ++i) {
            _avatarDownloader->downloadAvatar(users[i].toMap());
        }
        for (qint32 i = 0; i < chats.size(); ++i) {
            _avatarDownloader->downloadAvatar(chats[i].toMap());
        }
        for (qint32 i = 0; i < messagesRows.size(); ++i) {
            TgObject photo = messagesRows[i]["_photoToDownload"].toMap();
            _avatarDownloader->downloadPhoto(photo);
            _avatarDownloader->downloadSticker(messagesRows[i]["_stickerToDownload"].toMap());
        }
    }

    markReadIfNeeded();
}

void MessagesModel::handleHistoryResponseUpwards(TgObject data, TgLongVariant messageId)
{
    kgLog("history page up, messages=" + QString::number(data["messages"].toList().size())
          + " peers=" + QString::number(globalUsers().size() + globalChats().size())
          + " " + kgMemoryInfo());

    TgList messages = data["messages"].toList();
    TgList chats = data["chats"].toList();
    TgList users = data["users"].toList();

    appendGlobalUsers(users);
    appendGlobalChats(chats);

    if (messages.isEmpty()) {
        _upOffset = -1;
        return;
    }

    QList<TgObject> messagesRows;
    messagesRows.reserve(messages.size());

    for (qint32 i = messages.size() - 1; i >= 0; --i) {
        TgObject message = messages[i].toMap();
        TgObject fromId = message["from_id"].toMap();
        TgObject sender;

        if (TgClient::isUser(fromId)) for (qint32 j = 0; j < users.size(); ++j) {
            TgObject peer = users[j].toMap();
            if (TgClient::peersEqual(peer, fromId)) {
                sender = peer;
                break;
            }
        }
        if (TgClient::isChat(fromId)) for (qint32 j = 0; j < chats.size(); ++j) {
            TgObject peer = chats[j].toMap();
            if (TgClient::peersEqual(peer, fromId)) {
                sender = peer;
                break;
            }
        }
        if (TgClient::commonPeerType(fromId) == 0) {
            //This means that it is a channel feed or personal messages.
            //Authorized user is returned by API, so we don't need to put it manually.
            sender = _peer;
        }

        messagesRows.append(createRow(message, sender, users, chats));
    }

    qint32 oldOffset = _upOffset;
    qint32 newOffset = messages.last().toMap()["id"].toInt();
    if (_upOffset != newOffset && messages.size() == BATCH_SIZE) {
        _upOffset = newOffset;
    } else {
        _upOffset = -1;
    }

    qint32 oldSize = _history.size();

    beginInsertRows(QModelIndex(), 0, messagesRows.size() - 1);
    _history = messagesRows + _history;
    endInsertRows();

    if (oldSize > 0) {
        emit dataChanged(index(messagesRows.size()), index(messagesRows.size()));
    }

    // aka it is the first time when history is loaded in chat
    if (qMax(_peer["read_inbox_max_id"].toInt(), _peer["read_outbox_max_id"].toInt()) == oldOffset) {
        emit scrollTo(_history.size() - 1);
    } else {
        emit scrollTo(messagesRows.size());
    }

    if (_avatarDownloader) {
        for (qint32 i = 0; i < users.size(); ++i) {
            _avatarDownloader->downloadAvatar(users[i].toMap());
        }
        for (qint32 i = 0; i < chats.size(); ++i) {
            _avatarDownloader->downloadAvatar(chats[i].toMap());
        }
        for (qint32 i = 0; i < messagesRows.size(); ++i) {
            _avatarDownloader->downloadPhoto(messagesRows[i]["_photoToDownload"].toMap());
            _avatarDownloader->downloadSticker(messagesRows[i]["_stickerToDownload"].toMap());
        }
    }
}

// mm:ss, and hh:mm:ss once an hour is on the clock.
static QString durationString(qint32 seconds)
{
    qint32 hours = seconds / 3600;
    qint32 minutes = (seconds % 3600) / 60;
    seconds = seconds % 60;

    QString result;
    if (hours > 0) {
        result += QString::number(hours) + ":";
        result += QString("%1").arg(minutes, 2, 10, QChar('0')) + ":";
    } else {
        result += QString::number(minutes) + ":";
    }

    result += QString("%1").arg(seconds, 2, 10, QChar('0'));
    return result;
}

// Сколько байт качать у выбранной миниатюры. У photoSizeProgressive своего
// размера нет -- вместо него список размеров по уровням качества, и нужен
// последний, самый полный.
static qint64 kgThumbBytes(TgObject thumb)
{
    qint64 bytes = thumb["size"].toLongLong();
    if (bytes != 0) {
        return bytes;
    }

    TgList steps = thumb["sizes"].toList();
    return steps.isEmpty() ? 0 : steps.last().toLongLong();
}

// Миниатюра анимированного стикера. Их у документа обычно несколько:
//
//   * photoStrippedSize -- несколько сотен байт прямо в сообщении, без
//     заголовка JPEG (его достраивает getJpegPayload) -- качать нечего;
//   * photoCachedSize -- готовый JPEG, тоже прямо в сообщении;
//   * photoSize/photoSizeProgressive -- картинка на сервере, десяток
//     килобайт, за ней нужен отдельный запрос;
//   * photoPathSize -- векторный контур, рисовать его нечем.
//
// Из «настоящих» берётся самая маленькая: в строке она всё равно не больше
// девяноста шести точек.
static TgObject kgStickerThumb(TgObject document, QByteArray &inlineBytes)
{
    TgList thumbs = document["thumbs"].toList();
    TgObject best;

    for (qint32 i = 0; i < thumbs.size(); ++i) {
        TgObject thumb = thumbs[i].toMap();

        switch (GETID(thumb)) {
        case TLType::PhotoStrippedSize:
            if (inlineBytes.isEmpty()) {
                inlineBytes = TgClient::getJpegPayload(thumb["bytes"].toByteArray());
            }
            break;
        case TLType::PhotoCachedSize:
            // Целый JPEG, достраивать нечего -- он лучше обрезанного.
            inlineBytes = thumb["bytes"].toByteArray();
            break;
        case TLType::PhotoSize:
        case TLType::PhotoSizeProgressive:
            if (ID(best) == 0 || thumb["w"].toInt() < best["w"].toInt()) {
                best = thumb;
            }
            break;
        default:
            break;
        }
    }

    return best;
}

// Enough of the types Telegram actually sends. The fallback takes the subtype
// as it is, which covers ogg, webp, webm and anything else that is already a
// plain word.
static QString extensionForMimeType(QString mimeType)
{
    if (mimeType.isEmpty())
        return QString();

    if (mimeType == "audio/ogg") return ".ogg";
    if (mimeType == "audio/mpeg") return ".mp3";
    if (mimeType == "audio/mp4") return ".m4a";
    if (mimeType == "video/mp4") return ".mp4";
    if (mimeType == "video/quicktime") return ".mov";
    if (mimeType == "image/jpeg") return ".jpg";
    if (mimeType == "image/svg+xml") return ".svg";
    if (mimeType == "application/x-tgsticker") return ".tgs";

    QString subtype = mimeType.section('/', 1);
    for (qint32 i = 0; i < subtype.length(); ++i) {
        if (!subtype[i].isLetterOrNumber())
            return QString();
    }

    return subtype.isEmpty() ? QString() : ("." + subtype);
}

TgObject MessagesModel::createRow(TgObject message, TgObject sender, TgList users, TgList chats)
{
    TgObject row;
    row["messageId"] = message["id"];

    if (TgClient::isUser(sender)) {
        row["senderName"] = QString(sender["first_name"].toString() + " " + sender["last_name"].toString());
    } else {
        row["senderName"] = sender["title"].toString();
    }

    //TODO post author

    row["thumbnailColor"] = AvatarDownloader::userColor(sender["id"].toLongLong());
    row["thumbnailText"] = AvatarDownloader::getAvatarText(row["senderName"].toString());
    row["avatar"] = "";
    row["photoId"] = sender["photo"].toMap()["photo_id"];

    row["senderName"] = QString("<html><span style=\"color: "
            + AvatarDownloader::userColor(sender["id"]).name()
            + "\">"
            + row["senderName"].toString()
            + "</span></html>");

    row["date"] = message["date"];
    row["grouped_id"] = message["grouped_id"];
    //TODO 12-hour format
    row["messageTime"] = QDateTime::fromTime_t(qMax(message["date"].toInt(), message["edit_date"].toInt())).toString("hh:mm");
    //TODO replies support
    row["messageText"] = messageToHtml(message["message"].toString(), message["entities"].toList());
    row["messagePlainText"] = message["message"].toString();
    row["sender"] = TgClient::toInputPeer(sender);

    TgObject fwdFrom = message["fwd_from"].toMap();
    row["forwardedFrom"] = "";
    if (EXISTS(fwdFrom)) {
        QString forwardedFrom = fwdFrom["from_name"].toString();

        if (forwardedFrom.isEmpty()) {
            TgObject fwdPeer = fwdFrom["from_id"].toMap();

            if (TgClient::isUser(fwdPeer)) for (qint32 i = 0; i < users.size(); ++i) {
                TgObject realPeer = users[i].toMap();
                if (TgClient::peersEqual(fwdPeer, realPeer)) {
                    forwardedFrom = QString(realPeer["first_name"].toString() + " " + realPeer["last_name"].toString());
                    break;
                }
            }
            if (TgClient::isChat(fwdPeer)) for (qint32 i = 0; i < chats.size(); ++i) {
                TgObject realPeer = chats[i].toMap();
                if (TgClient::peersEqual(fwdPeer, realPeer)) {
                    forwardedFrom = realPeer["title"].toString();
                    break;
                }
            }
        }

        row["forwardedFrom"] = forwardedFrom;
    }

    TgObject media = message["media"].toMap();
    row["hasMedia"] = GETID(media) != 0;
    row["hasSticker"] = false;
    row["stickerFile"] = "";
    row["mediaIsVoice"] = false;
    row["mediaFilePath"] = "";
    row["hasAnimation"] = false;
    row["animationFile"] = "";
    row["mediaDownloadable"] = false;
    row["mediaSpoiler"] = false;
    row["photoSpoiler"] = false;
    row["mediaUrl"] = "";

    switch (GETID(media)) {
    case MessageMediaPhoto:
    {
        row["hasMedia"] = false;
        row["photoFile"] = "";
        row["_photoToDownload"] = media["photo"].toMap();

        // Крошечная размытая версия приходит вместе с самим сообщением, ещё до
        // всякой загрузки: это несколько сотен байт без заголовка JPEG,
        // getJpegPayload() достраивает его обратно. На медленной связи это
        // разница между «картинка появилась сразу» и «белое поле на три
        // минуты».
        TgList photoSizes = media["photo"].toMap()["sizes"].toList();
        for (qint32 i = 0; i < photoSizes.size(); ++i) {
            TgObject size = photoSizes[i].toMap();

            if (GETID(size) == TLType::PhotoStrippedSize) {
                row["photoPreview"] = TgClient::getJpegPayload(size["bytes"].toByteArray());
                break;
            }
        }

        row["photoFileId"] = media["photo"].toMap()["id"].toLongLong();
        row["hasPhoto"] = row["photoFileId"].toLongLong() != 0;
        row["photoSpoiler"] = media["spoiler"].toBool();
        break;
    }
    case MessageMediaContact:
    {
        row["mediaImage"] = "../../img/media/account.png";
        QString contactName;

        contactName += media["first_name"].toString();
        contactName += " ";
        contactName += media["last_name"].toString();

        row["mediaTitle"] = contactName;
        row["mediaText"] = media["phone_number"].toString();
        break;
    }
    case MessageMediaUnsupported:
        row["mediaImage"] = "../../img/media/file.png";
        row["mediaTitle"] = "Unsupported media";
        row["mediaText"] = "update your app";
        break;
    case MessageMediaDocument:
    {
        row["mediaImage"] = "../../img/media/file.png";
        row["mediaDownloadable"] = true;
        row["mediaDownload"] = media;

        TgObject document = media["document"].toMap();

        // A file name is the exception, not the rule: voice messages, stickers
        // and GIFs arrive with no DocumentAttributeFilename at all, which is why
        // every one of them used to show up as "Unknown file". Everything the
        // attributes do say is collected first, then turned into a name.
        QString fileName;
        QString audioTitle;
        QString audioPerformer;
        QString stickerAlt;
        qint32 duration = 0;
        bool isVoice = false;
        bool isRound = false;
        bool isSticker = false;
        bool isAnimated = false;
        bool isVideo = false;
        bool isAudio = false;

        TgList attributes = document["attributes"].toList();
        for (qint32 i = 0; i < attributes.size(); ++i) {
            TgObject attribute = attributes[i].toMap();

            switch (GETID(attribute)) {
            case DocumentAttributeFilename:
                fileName = attribute["file_name"].toString();
                break;
            case DocumentAttributeAudio:
                isAudio = true;
                isVoice = attribute["voice"].toBool();
                duration = attribute["duration"].toInt();
                audioTitle = attribute["title"].toString();
                audioPerformer = attribute["performer"].toString();
                break;
            case DocumentAttributeVideo:
                isVideo = true;
                isRound = attribute["round_message"].toBool();
                duration = (qint32) attribute["duration"].toDouble();
                break;
            case DocumentAttributeAnimated:
                isAnimated = true;
                break;
            case DocumentAttributeSticker:
                isSticker = true;
                stickerAlt = attribute["alt"].toString();
                break;
            default:
                break;
            }
        }

        QString mimeType = document["mime_type"].toString();

        // Анимированный стикер: Lottie (.tgs) или VP9 (.webm). Ни того, ни
        // другого телефон не рисует.
        const bool animatedSticker = isSticker
                && (mimeType == "application/x-tgsticker" || mimeType == "video/webm");

        // Everything here is video that no codec in this phone can decode, and
        // all of it is short. Converted to an animated GIF by the media server
        // it becomes something Qt draws by itself, in the message list.
        //
        // Без сервера обещать анимацию нечестно: вместо неё ниже появится
        // миниатюра, а она картинка, а не анимация.
        if (isRound || isAnimated || (isSticker && mimeType == "video/webm")) {
            row["hasAnimation"] = _mediaServerOn || !animatedSticker;
        }

        QString title;
        if (isVoice) {
            title = "Voice message";
            row["mediaIsVoice"] = true;
        } else if (isRound) {
            title = "Video message";
        } else if (isSticker) {
            title = stickerAlt.isEmpty() ? QString("Sticker") : ("Sticker " + stickerAlt);
            row["mediaImage"] = "../../img/media/image.png";

            // Only plain WebP stickers can be drawn. Animated ones are Lottie
            // (.tgs) or VP9 (.webm) and stay ordinary attachments.
            //
            // Off unless the build asks for them (qmake CONFIG+=stickers).
            // Drawing them is what the E90 started freezing on -- the whole
            // phone, not just this application -- and until the log from such a
            // freeze says why, a client that works is worth more than a picture.
#ifdef KG_STICKERS
            if (mimeType == "image/webp") {
                row["hasSticker"] = true;
                row["stickerId"] = document["id"];
                row["_stickerToDownload"] = document;
            } else if (animatedSticker && !_mediaServerOn) {
                // Анимацию без сервера не сделать, но у каждого стикера
                // Telegram есть готовая миниатюра -- обычная картинка. Пусть
                // лучше будет видно, что именно прислали, чем строка
                // «Sticker» и ничего.
                //
                // С сервером этого не делаем: там из стикера получается GIF,
                // и подкладывать под него картинку незачем.
                QByteArray inlineThumb;
                TgObject thumb = kgStickerThumb(document, inlineThumb);

                if (!inlineThumb.isEmpty()) {
                    // Это уже пришло вместе с сообщением -- видно сразу,
                    // ничего не качая.
                    row["photoPreview"] = inlineThumb;
                    row["hasSticker"] = true;
                }

                if (ID(thumb) != 0) {
                    TgObject withThumb = document;
                    withThumb["_thumbType"] = thumb["type"];
                    withThumb["_thumbSize"] = kgThumbBytes(thumb);

                    row["hasSticker"] = true;
                    row["stickerId"] = document["id"];
                    row["_stickerToDownload"] = withThumb;
                }
            }
#endif
        } else if (isAnimated) {
            title = "GIF";
            row["mediaImage"] = "../../img/media/image.png";
        } else if (isVideo) {
            title = "Video";
            row["mediaImage"] = "../../img/media/image.png";
        } else if (isAudio && !audioTitle.isEmpty()) {
            title = audioPerformer.isEmpty() ? audioTitle : (audioPerformer + " - " + audioTitle);
        } else if (!fileName.isEmpty()) {
            title = fileName;
        } else if (!mimeType.isEmpty()) {
            title = mimeType;
        } else {
            title = "File";
        }

        // The name a file is saved under decides which application the phone
        // offers for it, so one is built from the MIME type when the document
        // carries no name of its own. Without an extension nothing can open it.
        if (fileName.isEmpty()) {
            QString base;
            if (isVoice) base = "voice";
            else if (isRound) base = "video_message";
            else if (isSticker) base = "sticker";
            else if (isAnimated) base = "animation";
            else if (isVideo) base = "video";
            else if (isAudio) base = "audio";
            else base = "file";

            fileName = base + "_" + QString::number(message["id"].toLongLong())
                    + extensionForMimeType(mimeType);
        }

        qint64 size = document["size"].toLongLong();
        QString sizeString;

        if (size > 1073741824) {
            sizeString = QString::number((long double) size / 1073741824L, 'f', 2);
            sizeString += " GB";
        } else if (size > 1048576) {
            sizeString = QString::number((long double) size / 1048576L, 'f', 2);
            sizeString += " MB";
        } else if (size > 1024) {
            sizeString = QString::number((long double) size / 1024L, 'f', 2);
            sizeString += " KB";
        } else {
            sizeString = QString::number((long double) size, 'f', 2);
            sizeString += " B";
        }

        if (duration > 0) {
            sizeString = durationString(duration) + ", " + sizeString;
        }

        row["mediaTitle"] = title;
        row["mediaFileName"] = fileName;
        row["mediaText"] = sizeString;
        row["mediaSpoiler"] = media["spoiler"].toBool();
        break;
    }
    case MessageMediaWebPage:
    {
        TgObject webpage = media["webpage"].toMap();

        row["mediaImage"] = "../../img/media/web.png";

        // The page's own title, not the word "Webpage": the subtitle already
        // had it, and the row said nothing useful at a glance.
        QString pageTitle = webpage["title"].toString();
        if (pageTitle.isEmpty()) pageTitle = webpage["site_name"].toString();
        if (pageTitle.isEmpty()) pageTitle = "Webpage";

        QString pageText = webpage["description"].toString();
        if (pageText.isEmpty()) pageText = webpage["url"].toString();

        row["mediaTitle"] = pageTitle;
        row["mediaText"] = pageText;
        row["mediaUrl"] = webpage["url"].toString();
        break;
    }
    case MessageMediaVenue:
        row["mediaImage"] = "../../img/media/map-marker.png";
        row["mediaTitle"] = "Venue";
        row["mediaText"] = media["title"].toString();
        break;
    case MessageMediaGame:
        row["mediaImage"] = "../../img/media/gamepad-square.png";
        row["mediaTitle"] = "Game";
        row["mediaText"] = media["game"].toMap()["title"].toString();
        break;
    case MessageMediaInvoice:
        row["mediaImage"] = "../../img/media/receipt-text.png";
        row["mediaTitle"] = media["title"].toString();
        row["mediaText"] = media["description"].toString();
        break;
    case MessageMediaGeo:
    case MessageMediaGeoLive:
    {
        row["mediaImage"] = "../../img/media/map-marker.png";
        row["mediaTitle"] = GETID(media) == MessageMediaGeoLive ? "Live geolocation" : "Geolocation";

        TgObject geo = media["geo"].toMap();
        QString geoText;
        geoText += geo["long"].toString();
        geoText += ", ";
        geoText += geo["lat"].toString();

        row["mediaText"] = geoText;

        // Gives the row something to do when it is opened: the browser knows
        // this form, and so does the phone's own map application.
        row["mediaUrl"] = QString("https://maps.google.com/?q=" + geo["lat"].toString()
                                  + "," + geo["long"].toString());
        break;
    }
    case MessageMediaPoll:
        row["mediaImage"] = "../../img/media/poll.png";
        row["mediaTitle"] = "Poll";
        row["mediaText"] = media["poll"].toMap()["public_voters"].toBool() ? "public" : "anonymous";
        break;
    case MessageMediaDice:
        row["mediaImage"] = "../../img/media/dice-multiple.png";
        row["mediaTitle"] = "Dice";
        row["mediaText"] = media["value"].toString();
        break;
    }

    //TODO special bubble for service messages
    handleMessageAction(row, message, sender, users, chats);

    return row;
}

void MessagesModel::linkActivated(QString link, qint32 listIndex)
{
    QMutexLocker lock(&_mutex);

    QUrl url(link);

    TgObject listItem = _history[listIndex];
    QDomDocument dom;

    if (!dom.setContent(listItem["messageText"].toString(), false)) {
        return;
    }

    QDomNodeList list = dom.elementsByTagName("a");
    for (qint32 i = 0; i < list.count(); ++i) {
        QDomNode parent = list.at(i).toElement();

        if (parent.toElement().attribute("href") != link) {
            continue;
        }

        while (!parent.isNull()) {
            if (parent.toElement().attribute("href").startsWith("kutegram://spoiler/")) {
                parent.toElement().removeAttribute("href");

                QList<QDomNode> list;
                list << parent;
                while (!list.isEmpty()) {
                    QDomNode parent = list.takeLast();
                    QDomNodeList nodeList = parent.childNodes();
                    for (qint32 j = 0; j < nodeList.count(); ++j)
                        list << nodeList.at(j);
                    parent.toElement().removeAttribute("color");
                }

                listItem["messageText"] = dom.toString(-1);
                _history[listIndex] = listItem;

                emit dataChanged(index(listIndex), index(listIndex));
                return;
            }

            parent = parent.parentNode();
        }
    }

    if (url.scheme().isEmpty()) {
        openUrl(QUrl("http://" + link));
    } else {
        openUrl(QUrl(link));
    }
}

void MessagesModel::avatarDownloaded(TgLongVariant photoId, QString filePath)
{
    QMutexLocker lock(&_mutex);

    for (qint32 i = 0; i < _history.size(); ++i) {
        TgObject message = _history[i];

        if (message["photoId"] != photoId) {
            continue;
        }

        message["avatar"] = filePath;
        _history[i] = message;

        emit dataChanged(index(i), index(i));
    }
}

void MessagesModel::stickerDownloaded(TgLongVariant stickerId, QString filePath)
{
    QMutexLocker lock(&_mutex);

    qint32 matched = 0;

    for (qint32 i = 0; i < _history.size(); ++i) {
        TgObject message = _history[i];

        if (message["stickerId"] != stickerId) {
            continue;
        }

        message["stickerFile"] = filePath;
        _history[i] = message;
        ++matched;

        emit dataChanged(index(i), index(i));
    }

    // Ноль строк здесь означает, что файл скачался, но не нашёл, к какому
    // сообщению относится -- а на экране это выглядит просто пустым местом.
    kgLog("sticker " + stickerId.toString() + ": " + QString::number(matched)
          + " row(s) of " + QString::number(_history.size()), false);
}

void MessagesModel::photoDownloaded(TgLongVariant photoId, QString filePath)
{
    QMutexLocker lock(&_mutex);

    for (qint32 i = 0; i < _history.size(); ++i) {
        TgObject message = _history[i];

        if (message["photoFileId"] != photoId) {
            continue;
        }

        message["photoFile"] = filePath;
        _history[i] = message;

        emit dataChanged(index(i), index(i));
    }
}

// QML called messagesModel.openUrl(), which does not exist on this class: the
// click threw and nothing ever opened. These two are the missing halves -- one
// for a web link, one for a file that has finished downloading.
bool MessagesModel::markAsRead() const
{
    return _markAsRead;
}

void MessagesModel::setMarkAsRead(bool value)
{
    _markAsRead = value;

    if (_markAsRead) {
        markReadIfNeeded();
    }
}

bool MessagesModel::mediaServerOn() const
{
    return _mediaServerOn;
}

void MessagesModel::setMediaServerOn(bool value)
{
    // Учитывается при сборке строки, значит на уже показанную переписку не
    // влияет. Это осознанно: перестраивать историю из-за настройки, которую
    // меняют раз в жизни, дороже, чем открыть чат заново.
    _mediaServerOn = value;
}

int MessagesModel::replyToMsgId() const
{
    return _replyToMsgId;
}

void MessagesModel::setReplyToMsgId(int messageId)
{
    if (_replyToMsgId == messageId) {
        return;
    }

    _replyToMsgId = messageId;
    emit replyToMsgIdChanged();
}

// Sent when the chat is opened and again whenever a message arrives while it is
// on screen. Without it the unread counter never cleared -- here or on any
// other device signed into the same account.
void MessagesModel::markReadIfNeeded()
{
    QMutexLocker lock(&_mutex);

    if (!_markAsRead || !_client || !_client->isAuthorized()
            || TgClient::commonPeerType(_inputPeer) == 0) {
        return;
    }

    qint32 maxId = 0;
    for (qint32 i = 0; i < _history.size(); ++i) {
        maxId = qMax(maxId, _history[i]["messageId"].toInt());
    }

    if (maxId == 0 || maxId <= _lastReadId) {
        return;
    }

    _lastReadId = maxId;
    _client->messagesReadHistory(_inputPeer, maxId);
}

void MessagesModel::setTyping(bool typing)
{
    QMutexLocker lock(&_mutex);

    if (!_client || !_client->isAuthorized() || TgClient::commonPeerType(_inputPeer) == 0) {
        return;
    }

    if (typing) {
        // The server holds the state for about six seconds, so repeating it any
        // faster is pure traffic on a phone that pays for every byte.
        if (_typingActive && _typingSent.elapsed() < 5000) {
            return;
        }

        _typingActive = true;
        _typingSent.start();
    } else {
        if (!_typingActive) {
            return;
        }

        _typingActive = false;
    }

    _client->messagesSetTyping(_inputPeer, typing);
}

void MessagesModel::saveDraft(QString text)
{
    QMutexLocker lock(&_mutex);

    if (!_client || !_client->isAuthorized() || TgClient::commonPeerType(_inputPeer) == 0) {
        return;
    }

    // Only when it actually changed: leaving a chat without typing anything
    // should not send a request, and should not clear a draft made elsewhere.
    if (text == _lastDraft) {
        return;
    }

    _lastDraft = text;
    _client->messagesSaveDraft(_inputPeer, text);
}

void MessagesModel::deleteMessage(qint32 index)
{
    QMutexLocker lock(&_mutex);

    if (!_client || !_client->isAuthorized() || index < 0 || index >= _history.size()) {
        return;
    }

    qint32 messageId = _history[index]["messageId"].toInt();
    if (messageId == 0) {
        return;
    }

    TgVector ids;
    ids.append(messageId);

    // revoke: take it back from the other side as well. The server ignores the
    // flag where it does not apply.
    _client->messagesDeleteMessages(_inputPeer, ids, true);

    // Taken out here as well rather than waiting for the update to come back:
    // on this connection that wait is visible.
    beginRemoveRows(QModelIndex(), index, index);
    _history.removeAt(index);
    endRemoveRows();
}

QString MessagesModel::plainText(qint32 index)
{
    QMutexLocker lock(&_mutex);

    if (index < 0 || index >= _history.size()) {
        return QString();
    }

    return _history[index]["messagePlainText"].toString();
}

void MessagesModel::setAnimationFile(int index, QString path)
{
    QMutexLocker lock(&_mutex);

    if (index < 0 || index >= _history.size()) {
        return;
    }

    TgObject message = _history[index];
    message["animationFile"] = path;
    _history[index] = message;

    emit dataChanged(this->index(index), this->index(index));
}

void MessagesModel::activateMedia(qint32 index)
{
    QMutexLocker lock(&_mutex);

    if (index < 0 || index >= _history.size()) {
        return;
    }

    TgObject message = _history[index];

    QString filePath = message["mediaFilePath"].toString();
    if (!filePath.isEmpty()) {
        // Анимация рисуется в переписке после конвертации; отдавать её
        // системному плееру означает уйти из приложения ради того, ради чего
        // конвертация и существует.
        if (message["hasAnimation"].toBool()) {
            return;
        }

        openDownloadedFile(filePath);
        return;
    }

    QString url = message["mediaUrl"].toString();
    if (!url.isEmpty()) {
        openLink(url);
        return;
    }

    if (!message["mediaDownloadable"].toBool()) {
        return;
    }

    // A second press while it is coming down means "stop", the same as clicking
    // the spinner does.
    if (_downloadRequests.values().contains(message["messageId"].toInt())) {
        cancelDownload(index);
    } else {
        downloadFile(index);
    }
}

void MessagesModel::openLink(QString url)
{
    if (url.isEmpty())
        return;

    ::openUrl(QUrl(url));
}

void MessagesModel::openDownloadedFile(QString filePath)
{
    if (filePath.isEmpty())
        return;

    QString path = filePath;
    if (path.startsWith("file:///"))
        path = path.mid(8);

    ::openFile(path);
}

void MessagesModel::downloadFile(qint32 index)
{
    QMutexLocker lock(&_mutex);

    if (!_client || !_client->isAuthorized() || TgClient::commonPeerType(_peer) == 0 || index == -1) {
        return;
    }

    cancelDownload(index);

    QString fileName = _history[index]["mediaFileName"].toString();
    if (fileName.isEmpty()) fileName = QString::number(QDateTime::currentDateTime().toMSecsSinceEpoch());

    QStringList split = fileName.split('.');
    QString fileNameBefore;
    QString fileNameAfter;

    if (split.length() == 1) {
        fileNameBefore = split.first();
    } else {
        fileNameBefore = QStringList(split.mid(0, split.length() - 1)).join(".");
        fileNameAfter = split.last();
    }

    if (!fileNameAfter.isEmpty()) {
        fileNameAfter = "." + fileNameAfter;
    }

    // The downloads folder follows the same choice as the cache: on a phone
    // with a card, this is the single biggest thing worth moving off internal
    // memory, because nothing ever deletes from here. Every attachment the
    // user opens lands in it, and names are numbered rather than reused.
    QDir dir = CacheManager::downloadDirectory();

    QString indexedFileName = fileName;
    QString indexedFilePath = dir.absoluteFilePath(indexedFileName);
    qint32 fileIndex = 0;

    while (QFile(indexedFilePath).exists()) {
        ++fileIndex;
        indexedFileName = fileNameBefore + " (" + QString::number(fileIndex) + ")" + fileNameAfter;
        indexedFilePath = dir.absoluteFilePath(indexedFileName);
    }

    qint32 messageId = _history[index]["messageId"].toInt();
    qint64 requestId = _client->downloadFile(indexedFilePath, _history[index]["mediaDownload"].toMap()).toLongLong();
    _downloadRequests.insert(requestId, messageId);
    emit downloadUpdated(messageId, 0, "");
}

void MessagesModel::cancelDownload(qint32 index)
{
    QMutexLocker lock(&_mutex);

    if (!_client) {
        return;
    }

    qint32 messageId = _history[index]["messageId"].toInt();
    qint64 requestId = _downloadRequests.key(messageId);
    _downloadRequests.remove(requestId);
    _client->cancelDownload(requestId);
    emit downloadUpdated(messageId, -1, "");
}

void MessagesModel::fileDownloaded(TgLongVariant fileId, QString filePath)
{
    QMutexLocker lock(&_mutex);

    TgVariant messageId = _downloadRequests.take(fileId.toLongLong());

    if (messageId.isNull()) {
        return;
    }

    // Kept on the row as well, so activateMedia() can open the file without
    // asking the QML item that happens to be drawing it.
    for (qint32 i = 0; i < _history.size(); ++i) {
        if (_history[i]["messageId"].toInt() != messageId.toInt()) {
            continue;
        }

        TgObject message = _history[i];
        message["mediaFilePath"] = filePath;
        _history[i] = message;

        // Без этого QML не узнаёт, что файл появился: роль остаётся пустой,
        // нажатие уходит по ветке «открыть чем-нибудь», и гифка открывается
        // плеером телефона -- ровно то, ради чего всё и затевалось.
        emit dataChanged(index(i), index(i));
        break;
    }

    emit downloadUpdated(messageId.toInt(), 1, "file:///" + filePath);
}

void MessagesModel::fileDownloadCanceled(TgLongVariant fileId, QString filePath)
{
    QMutexLocker lock(&_mutex);

    TgVariant messageId = _downloadRequests.take(fileId.toLongLong());

    if (messageId.isNull()) {
        return;
    }

    emit downloadUpdated(messageId.toInt(), -1, "");
}

void MessagesModel::gotMessageUpdate(TgObject update, TgLongVariant messageId)
{
    QMutexLocker lock(&_mutex);

    if (_downOffset != -1) {
        return;
    }

    TgObject peerId;
    TgObject fromId;
    qint64 fromIdNumeric;

    //TODO this should be handled by singleton object with data from DB
    if (ID(update) == TLType::UpdateShortSentMessage) { //TODO what if we don't know about message?
        if (!_sentMessages.contains(messageId.toLongLong())) {
            return;
        }

        if (TgClient::isChannel(_peer)) {
            ID_PROPERTY(peerId) = TLType::PeerChannel;
            peerId["channel_id"] = TgClient::getPeerId(_peer);
        } else if (TgClient::isChat(_peer)) {
            ID_PROPERTY(peerId) = TLType::PeerChat;
            peerId["chat_id"] = TgClient::getPeerId(_peer);
        } else {
            ID_PROPERTY(peerId) = TLType::PeerUser;
            peerId["user_id"] = TgClient::getPeerId(_peer);
        }

        fromIdNumeric = _client->getUserId().toLongLong();

        update["message"] = _sentMessages.take(messageId.toLongLong());

        update["peer_id"] = peerId;
        emit sentMessageUpdate(update, messageId);
    } else if (update["user_id"].toLongLong() == TgClient::getPeerId(_peer) && TgClient::isUser(_peer)) {
        ID_PROPERTY(peerId) = TLType::PeerUser;
        peerId["user_id"] = update["user_id"];

        fromIdNumeric = update["out"].toBool() ? _client->getUserId().toLongLong() : update["user_id"].toLongLong();
    } else if (update["chat_id"].toLongLong() == TgClient::getPeerId(_peer) && TgClient::isChat(_peer)) {
        ID_PROPERTY(peerId) = TLType::PeerChat;
        peerId["chat_id"] = update["chat_id"];

        fromIdNumeric = update["from_id"].toLongLong();
    } else {
        return;
    }

    TgObject sender;
    for (qint32 j = 0; j < globalUsers().size(); ++j) {
        TgObject peer = globalUsers()[j].toMap();
        if (TgClient::getPeerId(peer) == fromIdNumeric) {
            sender = peer;
            break;
        }
    }
    if (ID(sender) == 0) for (qint32 j = 0; j < globalChats().size(); ++j) {
        TgObject peer = globalChats()[j].toMap();
        if (TgClient::getPeerId(peer) == fromIdNumeric) {
            sender = peer;
            break;
        }
    }

    if (TgClient::isChannel(sender)) {
        ID_PROPERTY(fromId) = TLType::PeerChannel;
        fromId["channel_id"] = TgClient::getPeerId(sender);
    } else if (TgClient::isChat(sender)) {
        ID_PROPERTY(fromId) = TLType::PeerChat;
        fromId["chat_id"] = TgClient::getPeerId(sender);
    } else if (TgClient::isUser(sender)) {
        ID_PROPERTY(fromId) = TLType::PeerUser;
        fromId["user_id"] = TgClient::getPeerId(sender);
    }

    update["peer_id"] = peerId;
    update["from_id"] = fromId;

    qint32 oldSize = _history.size();

    beginInsertRows(QModelIndex(), _history.size(), _history.size());
    TgObject messageRow = createRow(update, sender, globalUsers(), globalChats());
    _history.append(messageRow);
    endInsertRows();

    if (oldSize > 0) {
        emit dataChanged(index(oldSize - 1), index(oldSize - 1));
    }

    if (_avatarDownloader) {
        _avatarDownloader->downloadAvatar(sender);
        _avatarDownloader->downloadPhoto(messageRow["_photoToDownload"].toMap());
        _avatarDownloader->downloadSticker(messageRow["_stickerToDownload"].toMap());
    }

    // The chat is open and the message is on screen, so it has been read.
    markReadIfNeeded();

    emit scrollForNew();
}

void MessagesModel::gotUpdate(TgObject update, TgLongVariant messageId, TgList users, TgList chats, qint32 date, qint32 seq, qint32 seqStart)
{
    QMutexLocker lock(&_mutex);

    //We should avoid duplicates. (implement DB)
//    _globalUsers.append(users);
//    _globalChats.append(chats);

    switch (ID(update)) {
    case TLType::UpdateNewMessage:
    case TLType::UpdateNewChannelMessage:
    {
        if (_downOffset != -1) {
            return;
        }

        TgObject message = update["message"].toMap();

        if (!TgClient::peersEqual(_peer, message["peer_id"].toMap())) {
            return;
        }

        TgObject fromId = message["from_id"].toMap();
        TgObject sender;
        if (TgClient::isUser(fromId)) for (qint32 j = 0; j < users.size(); ++j) {
            TgObject peer = users[j].toMap();
            if (TgClient::peersEqual(peer, fromId)) {
                sender = peer;
                break;
            }
        }
        if (TgClient::isChat(fromId)) for (qint32 j = 0; j < chats.size(); ++j) {
            TgObject peer = chats[j].toMap();
            if (TgClient::peersEqual(peer, fromId)) {
                sender = peer;
                break;
            }
        }
        if (TgClient::commonPeerType(fromId) == 0) {
            //This means that it is a channel feed or personal messages.
            //Authorized user is returned by API, so we don't need to put it manually.
            sender = _peer;
        }

        message["out"] = TgClient::getPeerId(sender) == _client->getUserId();

        qint32 oldSize = _history.size();

        beginInsertRows(QModelIndex(), _history.size(), _history.size());
        TgObject messageRow = createRow(message, sender, users, chats);
        _history.append(messageRow);
        endInsertRows();

        if (oldSize > 0) {
            emit dataChanged(index(oldSize - 1), index(oldSize - 1));
        }

        if (_avatarDownloader) {
            _avatarDownloader->downloadAvatar(sender);
            _avatarDownloader->downloadPhoto(messageRow["_photoToDownload"].toMap());
            _avatarDownloader->downloadSticker(messageRow["_stickerToDownload"].toMap());
        }

        markReadIfNeeded();
        emit scrollForNew();
        break;
    }
    case TLType::UpdateEditMessage:
    case TLType::UpdateEditChannelMessage:
    {
        TgObject message = update["message"].toMap();

        if (!TgClient::peersEqual(_peer, message["peer_id"].toMap())) {
            return;
        }

        qint32 rowIndex = -1;
        for (qint32 i = 0; i < _history.size(); ++i) {
            if (_history[i]["messageId"] == message["id"]) {
                rowIndex = i;
                break;
            }
        }

        if (rowIndex == -1) {
            return;
        }

        TgObject fromId = message["from_id"].toMap();
        TgObject sender;
        if (TgClient::isUser(fromId)) for (qint32 j = 0; j < users.size(); ++j) {
            TgObject peer = users[j].toMap();
            if (TgClient::peersEqual(peer, fromId)) {
                sender = peer;
                break;
            }
        }
        if (TgClient::isChat(fromId)) for (qint32 j = 0; j < chats.size(); ++j) {
            TgObject peer = chats[j].toMap();
            if (TgClient::peersEqual(peer, fromId)) {
                sender = peer;
                break;
            }
        }
        if (TgClient::commonPeerType(fromId) == 0) {
            //This means that it is a channel feed or personal messages.
            //Authorized user is returned by API, so we don't need to put it manually.
            sender = _peer;
        }

        message["out"] = TgClient::getPeerId(sender) == _client->getUserId();

        TgObject messageRow = createRow(message, sender, users, chats);
        _history.replace(rowIndex, messageRow);

        emit dataChanged(index(rowIndex), index(rowIndex));

        if (_avatarDownloader) {
            _avatarDownloader->downloadAvatar(sender);
            _avatarDownloader->downloadPhoto(messageRow["_photoToDownload"].toMap());
            _avatarDownloader->downloadSticker(messageRow["_stickerToDownload"].toMap());
        }
        break;
    }
    case TLType::UpdateDeleteChannelMessages:
        if (!TgClient::isChat(_peer) || TgClient::getPeerId(_peer) != update["channel_id"].toLongLong()) {
            return;
        }

        //fallthrough
    case TLType::UpdateDeleteMessages:
        TgList ids = update["messages"].toList();
        for (qint32 i = 0; i < _history.size(); ++i) {
            if (ids.removeOne(_history[i]["messageId"])) {
                beginRemoveRows(QModelIndex(), i, i);
                _history.removeAt(i);
                endRemoveRows();
                --i;
            }
        }

        break;
    }
}

void MessagesModel::sendMessage(QString message)
{
    sendMessageWith(message, false, 0);
}

void MessagesModel::sendMessageWith(QString message, bool silent, qint32 secondsFromNow)
{
    if (!_client || !_client->isAuthorized() || TgClient::commonPeerType(_inputPeer) == 0 || _uploadId.toLongLong() || (message.isEmpty() && GETID(_media["file"].toMap()) == 0)) {
        return;
    }

    // Отложенная отправка считается по времени сервера: часы телефона 2007
    // года врут, а сервер сравнивает срок со своими. Срок в прошлом он
    // понимает как «отправить сейчас» -- именно так это и выглядело.
    const qint32 scheduleDate = secondsFromNow > 0
            ? _client->serverTime() + secondsFromNow : 0;

    if (silent || scheduleDate) {
        kgLog("sending: silent=" + QString::number(silent)
              + " in=" + QString::number(secondsFromNow)
              + "s at=" + QString::number(scheduleDate)
              + " (server now " + QString::number(_client->serverTime())
              + ", phone now " + QString::number(QDateTime::currentDateTimeUtc().toTime_t()) + ")");
    }

    _sentMessages.insert(_client->messagesSendMessage(_inputPeer, message, _media,
                                                      randomLong(), _replyToMsgId,
                                                      silent, scheduleDate).toLongLong(), message);
    cancelUpload();

    // The reply belongs to the message that has just gone; the draft is spent
    // too, and typing has obviously stopped.
    if (_replyToMsgId != 0) {
        _replyToMsgId = 0;
        emit replyToMsgIdChanged();
    }

    _lastDraft = "";
    _typingActive = false;
}

void MessagesModel::uploadFilePath(QString path)
{
    cancelUpload();

    if (path.isEmpty()) {
        return;
    }

    QFileInfo info(path);
    if (!info.exists() || !info.isReadable() || info.size() <= 0) {
        kgLog("upload refused: " + path);
        return;
    }

    const QString lower = path.toLower();
    if (lower.endsWith(".jpg") || lower.endsWith(".jpeg") || lower.endsWith(".png")) {
        ID_PROPERTY(_media) = TLType::InputMediaUploadedPhoto;
    } else {
        ID_PROPERTY(_media) = TLType::InputMediaUploadedDocument;

        TGOBJECT(TLType::DocumentAttributeFilename, fileName);
        fileName["file_name"] = info.fileName();

        TgList attributes;
        attributes << fileName;
        _media["attributes"] = attributes;
    }

    kgLog("uploading " + info.fileName() + ", " + QString::number(info.size()) + " bytes");

    _uploadId = _client->uploadFile(path);

    if (_uploadId.toLongLong() == 0) {
        kgLog("upload did not start");
        return;
    }

    emit uploadingProgress(0);
}

void MessagesModel::sendVoice(QString path, int duration)
{
    cancelUpload();

    QFileInfo info(path);
    if (!info.exists() || info.size() <= 0) {
        kgLog("voice upload refused: " + path);
        return;
    }

    // Голосовое отличается от обычного вложения одним атрибутом: voice=true.
    // По нему Telegram и рисует волну с кнопкой «играть», а не строку файла.
    // Волну (waveform) не шлём -- клиенты рисуют тогда ровную линию, а считать
    // её на этом телефоне незачем.
    ID_PROPERTY(_media) = TLType::InputMediaUploadedDocument;
    _media["mime_type"] = "audio/ogg";

    TGOBJECT(TLType::DocumentAttributeAudio, audio);
    audio["voice"] = true;
    audio["duration"] = duration;

    TgList attributes;
    attributes << audio;
    _media["attributes"] = attributes;

    kgLog("sending voice " + info.fileName() + ", " + QString::number(info.size())
          + " bytes, " + QString::number(duration) + " s");

    _voiceUpload = true;
    _uploadId = _client->uploadFile(path);

    if (_uploadId.toLongLong() == 0) {
        kgLog("voice upload did not start");
        _voiceUpload = false;
        return;
    }

    emit uploadingProgress(0);
}

void MessagesModel::uploadFile()
{
    cancelUpload();

#ifdef Q_OS_SYMBIAN
    // Диалог выбора файла -- обычный виджет, а не QML. Ради QML в main.cpp
    // выключена клавиатурная навигация Qt по виджетам (NavigationModeNone), и
    // без неё по диалогу невозможно двигаться джойстиком: он открывается и не
    // реагирует. На время диалога навигация включается обратно.
    QApplication::setNavigationMode(Qt::NavigationModeKeypadDirectional);
#endif

    QFileDialog fileDialog;
    fileDialog.setAttribute(Qt::WA_DeleteOnClose, false);
    fileDialog.setAttribute(Qt::WA_QuitOnClose, false);
    fileDialog.setFileMode(QFileDialog::ExistingFile);
    fileDialog.exec();

#ifdef Q_OS_SYMBIAN
    QApplication::setNavigationMode(Qt::NavigationModeNone);
#endif

    QStringList selectedFiles = fileDialog.selectedFiles();
    if (selectedFiles.isEmpty()) {
        return;
    }

    QString selected = selectedFiles.first();
    if (selected.isEmpty()) {
        return;
    }

    if (selected.endsWith(".jpg") || selected.endsWith(".jpeg") || selected.endsWith(".png")) {
        ID_PROPERTY(_media) = TLType::InputMediaUploadedPhoto;
    } else {
        ID_PROPERTY(_media) = TLType::InputMediaUploadedDocument;

        TGOBJECT(TLType::DocumentAttributeFilename, fileName);
        fileName["file_name"] = selected.split('/').last();

        TgList attributes;
        attributes << fileName;
        _media["attributes"] = attributes;
    }

    _uploadId = _client->uploadFile(selected);

    emit uploadingProgress(0);
}

void MessagesModel::cancelUpload()
{
    _media = TgObject();
    if (_uploadId.toLongLong()) {
        _client->cancelUpload(_uploadId);
    }
    _uploadId = 0;

    emit uploadingProgress(-1);
}

void MessagesModel::fileUploadCanceled(TgLongVariant fileId)
{
    if (_uploadId != fileId) {
        return;
    }

    _uploadId = 0;
    cancelUpload();
}

void MessagesModel::fileUploaded(TgLongVariant fileId, TgObject inputFile)
{
    if (_uploadId != fileId) {
        return;
    }

    _uploadId = 0;
    _media["file"] = inputFile;

    emit uploadingProgress(100);

    // Голосовое уходит само: текста к нему нет, и ждать от человека нечего.
    if (_voiceUpload) {
        _voiceUpload = false;
        sendMessageWith(QString(), false, 0);
        emit voiceSent();
    }
}

void MessagesModel::fileUploading(TgLongVariant fileId, TgLongVariant processedLength, TgLongVariant totalLength, qint32 progressPercentage)
{
    if (_uploadId != fileId) {
        return;
    }

    emit uploadingProgress(progressPercentage);
}
