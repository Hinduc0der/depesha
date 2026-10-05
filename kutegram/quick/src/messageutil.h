#ifndef MESSAGEUTIL_H
#define MESSAGEUTIL_H

#include "tgstream.h"

TgList& globalUsers();

// Use these rather than appending to the lists above. They keep one entry per
// peer instead of one per message received: the plain append grew the lists
// forever, which on a phone that stays connected for hours is a slow way to
// run the whole system out of memory.
void appendGlobalUsers(TgList users);
void appendGlobalChats(TgList chats);
TgList& globalChats();
QString prepareDialogItemMessage(QString text, TgList entities);
QString messageToHtml(QString text, TgList entities);
void handleMessageAction(TgObject &row, TgObject message, TgObject sender, TgList users, TgList chats);

#endif // MESSAGEUTIL_H
