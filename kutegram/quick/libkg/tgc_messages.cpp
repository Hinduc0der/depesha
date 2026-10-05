#include "tgclient.h"

#include "tlschema.h"

TgLongVariant TgClient::messagesGetDialogs(qint32 offsetDate, qint32 offsetId, TgObject offsetPeer, qint32 limit, qint32 folderId, bool excludePinned, TgLongVariant hash)
{
    TGOBJECT(TLType::MessagesGetDialogsMethod, method);

    if (excludePinned) method["exclude_pinned"] = true;
    method["folder_id"] = folderId == 1 ? 1 : 0;
    method["offset_date"] = offsetDate;
    method["offset_id"] = offsetId;
    method["offset_peer"] = toInputPeer(offsetPeer);
    method["limit"] = limit;
    method["hash"] = hash;

    return sendObject<&writeTLMethodMessagesGetDialogs>(method);
}

TgLongVariant TgClient::messagesGetDialogsWithOffsets(TgObject offsets, qint32 limit, qint32 folderId, bool excludePinned, TgLongVariant hash)
{
    return messagesGetDialogs(offsets["offset_date"].toInt(), offsets["offset_id"].toInt(), offsets["offset_peer"].toMap(), limit, folderId, excludePinned, hash);
}

TgLongVariant TgClient::messagesGetHistory(TgObject inputPeer, qint32 offsetId, qint32 offsetDate, qint32 addOffset, qint32 limit, qint32 maxId, qint32 minId, TgLongVariant hash)
{
    TGOBJECT(TLType::MessagesGetHistoryMethod, method);

    method["peer"] = toInputPeer(inputPeer);
    method["offset_id"] = offsetId;
    method["offset_date"] = offsetDate;
    method["add_offset"] = addOffset;
    method["limit"] = limit;
    method["max_id"] = maxId;
    method["min_id"] = minId;
    method["hash"] = hash;

    return sendObject<&writeTLMethodMessagesGetHistory>(method);
}

TgLongVariant TgClient::messagesSendMessage(TgObject inputPeer, QString message, TgObject media, TgLongVariant randomId, qint32 replyToMsgId, bool silent, qint32 scheduleDate)
{
    TGOBJECT(GETID(media) != 0 ? TLType::MessagesSendMediaMethod : TLType::MessagesSendMessageMethod, method);

    method["peer"] = toInputPeer(inputPeer);
    method["message"] = message;
    method["media"] = media;
    method["random_id"] = randomId;

    if (replyToMsgId != 0) {
        TGOBJECT(TLType::InputReplyToMessage, replyTo);
        replyTo["reply_to_msg_id"] = replyToMsgId;
        method["reply_to"] = replyTo;
    }

    // Поля-флаги: схема смотрит на само наличие ключа, а не на его значение,
    // поэтому ставятся они только когда нужны.
    if (silent) {
        method["silent"] = true;
    }

    if (scheduleDate > 0) {
        method["schedule_date"] = scheduleDate;
    }

    if (GETID(media) != 0) {
        return sendObject<&writeTLMethodMessagesSendMedia>(method);
    } else {
        return sendObject<&writeTLMethodMessagesSendMessage>(method);
    }
}

TgLongVariant TgClient::messagesSendMedia(TgObject inputPeer, TgObject media, QString message, TgLongVariant randomId)
{
    return messagesSendMessage(inputPeer, message, media, randomId);
}

TgLongVariant TgClient::messagesGetDialogFilters()
{
    TGOBJECT(TLType::MessagesGetDialogFiltersMethod, method);

    return sendObject<&writeTLMethodMessagesGetDialogFilters>(method);
}

TgLongVariant TgClient::messagesReadHistory(TgObject inputPeer, qint32 maxId)
{
    TgObject peer = toInputPeer(inputPeer);

    // Channels keep their read marker in a different method than everything
    // else; sending the wrong one is simply ignored by the server.
    if (ID(peer) == TLType::InputPeerChannel) {
        TGOBJECT(TLType::ChannelsReadHistoryMethod, method);

        method["channel"] = toInputChannel(inputPeer);
        method["max_id"] = maxId;

        return sendObject<&writeTLMethodChannelsReadHistory>(method);
    }

    TGOBJECT(TLType::MessagesReadHistoryMethod, method);

    method["peer"] = peer;
    method["max_id"] = maxId;

    return sendObject<&writeTLMethodMessagesReadHistory>(method);
}

TgLongVariant TgClient::messagesSetTyping(TgObject inputPeer, bool typing)
{
    TGOBJECT(TLType::MessagesSetTypingMethod, method);

    TGOBJECT(typing ? TLType::SendMessageTypingAction : TLType::SendMessageCancelAction, action);

    method["peer"] = toInputPeer(inputPeer);
    method["action"] = action;

    return sendObject<&writeTLMethodMessagesSetTyping>(method);
}

TgLongVariant TgClient::messagesSaveDraft(TgObject inputPeer, QString message)
{
    TGOBJECT(TLType::MessagesSaveDraftMethod, method);

    method["peer"] = toInputPeer(inputPeer);
    method["message"] = message;

    return sendObject<&writeTLMethodMessagesSaveDraft>(method);
}

TgLongVariant TgClient::messagesDeleteMessages(TgObject inputPeer, TgVector ids, bool revoke)
{
    TgObject peer = toInputPeer(inputPeer);

    if (ID(peer) == TLType::InputPeerChannel) {
        TGOBJECT(TLType::ChannelsDeleteMessagesMethod, method);

        method["channel"] = toInputChannel(inputPeer);
        method["id"] = ids;

        return sendObject<&writeTLMethodChannelsDeleteMessages>(method);
    }

    TGOBJECT(TLType::MessagesDeleteMessagesMethod, method);

    // Without this the message only disappears here; with it, it is taken back
    // from the other side as well, which is what "delete" means to a person.
    if (revoke) {
        method["revoke"] = true;
    }

    method["id"] = ids;

    return sendObject<&writeTLMethodMessagesDeleteMessages>(method);
}
