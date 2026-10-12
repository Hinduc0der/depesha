#ifndef CHATPAGE_H
#define CHATPAGE_H

#include <QModelIndex>
#include <QWidget>

class AppSettings;
class ListView;
class MediaConverter;
class MessageDelegate;
class MessagesModel;
class QLabel;
class QLineEdit;
class QListView;
class QMovie;
class VoicePlayer;

// Переписка: список сообщений и строка ввода. Никаких пузырей и анимаций
// оформления -- на этом телефоне за них платят кадрами.
//
// Медиа здесь ровно то же, что в Kutegram, и тем же способом: картинки и
// стикеры рисуются в строке, а всё, что телефон декодировать не умеет
// (кружки, видеостикеры, гифки, голосовые), уходит на медиа-сервер и
// возвращается тем, что телефон умеет -- GIF или mp3.
class ChatPage : public QWidget
{
    Q_OBJECT

public:
    ChatPage(MessagesModel *model, AppSettings *settings, QWidget *parent = 0);

    void setTitle(const QString &title);
    // Есть ли сейчас сообщение, на которое отвечаем.
    bool hasReply() const;
    // Фокус в список или в строку ввода -- с этого начинается всё остальное
    // управление с клавиатуры.
    void focusInput();
    void focusMessages();

    QListView *view() const;

public slots:
    // Отправка и действие над сообщением нужны и меню на левой софт-клавише --
    // см. CONTROLS.ru.md.
    void send();
    void activate();

    // Ответить на выделенное сообщение и отменить ответ.
    void replyToCurrent();
    void cancelReply();
    // Отправить набранное без звука у получателя -- одним действием, а не
    // переключателем: в меню это читается как команда, ею и должно быть.
    void sendSilent();
    // Отправить набранное отложенно: секунды от текущего момента.
    void sendLater(int secondsFromNow);

signals:
    // Строка состояния живёт в главном окне, а рассказать есть что и отсюда.
    void status(QString text);
    // Есть ли что отправлять: от этого зависит пункт меню «Send».
    void canSendChanged(bool can);
    // Открыть картинку во весь экран: страницами заведует главное окно.
    void showImage(QString path);
    // Ответ назначен или снят: меню показывает «Cancel reply» только когда
    // есть что отменять.
    void replyStateChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event);

private slots:
    void inputChanged();
    void replyChanged();
    void scrolled(int value);
    void scrollTo(qint32 index);
    void scrollForNew();
    void currentChanged(QModelIndex current, QModelIndex previous);
    void animationFrame();
    void converted(int index, QString path);
    void converterStatus();
    void playerStatus();

private:
    void updateTitle();
    QString plainSender(const QModelIndex &index) const;
    void note(int row, const QString &text);
    void startAnimation(const QModelIndex &index);
    void stopAnimation();

    MessagesModel *_model;
    AppSettings *_settings;
    QLabel *_title;
    ListView *_view;
    QLineEdit *_input;
    MessageDelegate *_delegate;
    MediaConverter *_converter;
    VoicePlayer *_player;
    QMovie *_movie;
    int _animationRow;
    int _noteRow;
    bool _messagesCanLoad;
    QString _peerTitle;
    QString _replyTo;
};

#endif // CHATPAGE_H
