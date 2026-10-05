#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModelIndex>

#include "tgclient.h"

class AppSettings;
class AvatarDownloader;
class CacheManager;
class ImagePage;
class ChatPage;
class DialogsModel;
class FolderFilter;
class FoldersModel;
class ListView;
class LoginPage;
class MessagesModel;
class QAction;
class QMenu;
class SettingsPage;
class QTimer;
class QStackedWidget;
class QStatusBar;

// Всё окно приложения: вход, список чатов, переписка.
//
// Depesha -- тот же клиент Telegram, что и Kutegram: библиотека протокола и
// модели данных общие, файл за файлом. Разное только одно -- здесь интерфейс
// собран из штатных виджетов Qt со стилем телефона, а не нарисован самим
// приложением на QML. На 332 МГц это и есть разница между "листается" и "не
// листается".
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);

protected:
    void keyPressEvent(QKeyEvent *event);
    bool eventFilter(QObject *watched, QEvent *event);

private slots:
    void clientInitialized(bool hasUserId);
    void clientAuthorized(TgLongVariant userId);
    void clientDisconnected(bool hasUserId);
    void socketError(qint32 errorCode, QString errorMessage);
    void rpcError(qint32 errorCode, QString errorMessage, TgLongVariant messageId);

    void sentCode(TgObject data, TgLongVariant messageId);
    void tfaRequired();
    void tfaProgress(qint32 percent);

    void sendPhone(QString phone);
    void sendCode(QString code);
    void sendPassword(QString password);

    void openDialog(const QModelIndex &index);
    void retryHistory();
    void logMemory();
    void foldersChanged();
    void folderSelected(QAction *action);
    void showSettings();
    void showImage(QString path);
    void canSendChanged(bool can);
    void replyToMessage();
    void cancelReply();
    void sendSilent();
    void sendInHour();
    void sendTomorrow();
    void updateChatActions();
    void markAsReadChanged();
    void goBack();
    void reconnect();
    void dialogsScrolled(int value);
    void updateVisibleAvatars();

private slots:
    // Слот, потому что состояние сообщает и страница переписки.
    void status(const QString &text);

private:
    void showPage(QWidget *page);

    TgClient *_client;
    AvatarDownloader *_avatars;
    AvatarDownloader *_chatMedia;
    DialogsModel *_dialogs;
    FoldersModel *_foldersModel;
    FolderFilter *_folderFilter;
    MessagesModel *_messages;
    CacheManager *_cache;
    AppSettings *_settings;

    QStackedWidget *_stack;
    LoginPage *_login;
    ListView *_dialogsView;
    ChatPage *_chat;
    SettingsPage *_settingsPage;
    ImagePage *_imagePage;
    // Куда возвращаться из настроек и из просмотра картинки.
    QWidget *_returnTo;

    QTimer *_reconnectTimer;
    QAction *_backAction;
    QAction *_sendAction;
    QAction *_openAction;
    // Эти четыре живут в разделе «Actions»: Qt собирает его из действий окна.
    QAction *_replyAction;
    QAction *_cancelReplyAction;
    QAction *_silentAction;
    QAction *_laterHourAction;
    QAction *_laterDayAction;
    QMenu *_foldersMenu;
    QString _phone;
    QString _phoneCodeHash;
    // Номер, введённый до того, как соединение поднялось: отправится сам,
    // как только клиент доложит, что готов.
    QString _pendingPhone;
    // Открытый чат: по нему повторяется запрос истории, если первый потерялся.
    QByteArray _chatPeer;
    int _historyRetries;
    bool _canSend;
};

#endif // MAINWINDOW_H
