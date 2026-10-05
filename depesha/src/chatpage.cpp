#include "chatpage.h"
#include "listview.h"
#include "messagedelegate.h"

#include <QApplication>
#include <QDateTime>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMovie>
#include <QScrollBar>
#include <QVBoxLayout>

#include "appsettings.h"
#include "kglog.h"
#include "mediaconverter.h"
#include "messagesmodel.h"
#include "platformutils.h"
#include "voiceplayer.h"

ChatPage::ChatPage(MessagesModel *model, AppSettings *settings, QWidget *parent) :
    QWidget(parent),
    _model(model),
    _settings(settings),
    _movie(0),
    _animationRow(-1),
    _noteRow(-1),
    _messagesCanLoad(true)
{
    _title = new QLabel(this);
    _title->setTextFormat(Qt::PlainText);
    _title->setAutoFillBackground(true);

    // Заголовок красится по тому же правилу, что и строки списка: у темы цвет
    // текста не в тон нашему фону.
    QPalette titleColors = _title->palette();
    titleColors.setColor(QPalette::Window, Qt::white);
    titleColors.setColor(QPalette::WindowText, Qt::black);
    _title->setPalette(titleColors);

    QFont titleFont = _title->font();
    titleFont.setBold(true);
    _title->setFont(titleFont);

    _view = new ListView(this);
    _view->setModel(_model);
    _delegate = new MessageDelegate(_model, _settings, _view);
    _view->setItemDelegate(_delegate);
    // Строки с картинками выше остальных, значит одинаковыми они больше не
    // являются.
    _view->setUniformItemSizes(false);
    _view->setEmptyText(tr("No messages yet"));
    _view->installEventFilter(this);

    _input = new QLineEdit(this);
    _input->setPlaceholderText(tr("Message"));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(2);
    layout->setSpacing(2);
    layout->addWidget(_title);
    layout->addWidget(_view, 1);
    layout->addWidget(_input);

    _converter = new MediaConverter(_settings, this);
    _player = new VoicePlayer(_settings, this);

    // Центр джойстика -- это Key_Select, а не Enter, и однострочное поле ввода
    // на него не реагирует вовсе. Снаружи, с крышкой закрытой, Enter'а нет --
    // значит без этого фильтра набранное сообщение отправить было нечем.
    // См. УПРАВЛЕНИЕ.md.
    _input->installEventFilter(this);

    connect(_input, SIGNAL(returnPressed()), this, SLOT(send()));
    connect(_input, SIGNAL(textChanged(QString)), this, SLOT(inputChanged()));
    connect(_view->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(scrolled(int)));
    connect(_view->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            this, SLOT(currentChanged(QModelIndex,QModelIndex)));
    connect(_model, SIGNAL(scrollTo(qint32)), this, SLOT(scrollTo(qint32)));
    connect(_model, SIGNAL(scrollForNew()), this, SLOT(scrollForNew()));
    connect(_converter, SIGNAL(converted(int,QString)), this, SLOT(converted(int,QString)));
    connect(_converter, SIGNAL(statusChanged()), this, SLOT(converterStatus()));
    connect(_player, SIGNAL(statusChanged()), this, SLOT(playerStatus()));
    connect(_model, SIGNAL(replyToMsgIdChanged()), this, SLOT(replyChanged()));
}

void ChatPage::setTitle(const QString &title)
{
    _peerTitle = title;
    _replyTo.clear();

    // Приписка вроде «converting...» относилась к сообщению прошлого чата, а
    // строки нумеруются заново -- иначе она всплывёт на чужом сообщении.
    note(-1, QString());

    updateTitle();
    stopAnimation();
}

// В заголовке видно и собеседника, и то, на что отвечаем, и выключенный звук.
// Другого места под это на 240 точках нет.
void ChatPage::updateTitle()
{
    QString text = _peerTitle;

    if (!_replyTo.isEmpty()) {
        text += "  " + tr("reply: ") + _replyTo;
    }

    _title->setText(text);
}

bool ChatPage::hasReply() const
{
    return !_replyTo.isEmpty();
}

void ChatPage::replyToCurrent()
{
    const QModelIndex index = _view->currentIndex();

    if (!index.isValid()) {
        return;
    }

    const int messageId = index.data(TwoLineDelegate::roleByName(_model, "messageId")).toInt();

    if (messageId == 0) {
        return;
    }

    _model->setReplyToMsgId(messageId);

    _replyTo = plainSender(index);
    updateTitle();
    emit replyStateChanged();

    focusInput();
}

QString ChatPage::plainSender(const QModelIndex &index) const
{
    const QString sender =
            TwoLineDelegate::plainText(index.data(TwoLineDelegate::roleByName(_model, "senderName")));

    return sender.isEmpty() ? tr("message") : sender;
}

void ChatPage::cancelReply()
{
    _model->setReplyToMsgId(0);

    _replyTo.clear();
    updateTitle();
    emit replyStateChanged();
}

void ChatPage::replyChanged()
{
    // Модель сбрасывает ответ сама, когда сообщение ушло.
    if (_model->replyToMsgId() == 0 && !_replyTo.isEmpty()) {
        _replyTo.clear();
        updateTitle();
        emit replyStateChanged();
    }
}

void ChatPage::sendSilent()
{
    const QString text = _input->text();

    if (text.trimmed().isEmpty()) {
        emit status(tr("Nothing to send"));
        return;
    }

    _model->sendMessageWith(text, true, 0);
    _input->clear();

    emit status(tr("Sent without sound"));
}

void ChatPage::sendLater(int secondsFromNow)
{
    const QString text = _input->text();

    if (text.trimmed().isEmpty()) {
        emit status(tr("Nothing to send"));
        return;
    }

    // Срок отдаётся секундами: абсолютное время модель считает по часам
    // сервера, а не телефона.
    _model->sendMessageWith(text, false, secondsFromNow);
    _input->clear();

    emit status(tr("Scheduled"));
}

QListView *ChatPage::view() const
{
    return _view;
}

void ChatPage::focusInput()
{
    _input->setFocus();

#ifdef QT_KEYPAD_NAVIGATION
    _input->setEditFocus(true);
#endif

    // Жалоба «центр всё равно попадает в сообщение» означает, что фокус
    // остался в списке. Это надо видеть, а не угадывать.
    const QWidget *focused = QApplication::focusWidget();
    kgLog(QString("focus -> ") + (focused ? focused->metaObject()->className() : "none"));
}

void ChatPage::focusMessages()
{
    _view->setFocus();

#ifdef QT_KEYPAD_NAVIGATION
    // Без edit focus список стрелками не листается вовсе: Qt в режиме
    // клавиатурной навигации отдаёт стрелки не ему, а переходу между
    // виджетами.
    _view->setEditFocus(true);
#endif

    if (!_view->currentIndex().isValid() && _model->rowCount() > 0) {
        _view->setCurrentIndex(_model->index(_model->rowCount() - 1, 0));
    }
}

bool ChatPage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == _input && event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent*>(event)->key();

        if (key == Qt::Key_Select || key == Qt::Key_Enter || key == Qt::Key_Return) {
            send();
            return true;
        }
    }

    if (watched == _view && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        const int key = keyEvent->key();

        if (key == Qt::Key_Select || key == Qt::Key_Enter || key == Qt::Key_Return) {
            activate();
            return true;
        }

        // Вниз с последнего сообщения уводит в строку ввода: это край списка,
        // и дальше вниз идти всё равно некуда.
        if (key == Qt::Key_Down && _view->currentIndex().row() >= _model->rowCount() - 1) {
            focusInput();
            return true;
        }

        // А вообще спускаться до низу ради ответа незачем: любая напечатанная
        // буква сама переносит и фокус, и себя в строку ввода -- так ведёт
        // себя любой телефонный мессенджер, и так это ожидается.
        const QString typed = keyEvent->text();

        if (!typed.isEmpty() && typed.at(0).isPrint()
                && !(keyEvent->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {
            focusInput();
            _input->insert(typed);
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void ChatPage::scrolled(int value)
{
    // Ближе к началу списка -- подгрузить страницу старых сообщений. Qt-шные
    // canFetchMore/fetchMore тут не подходят: у модели свои, отдельно вверх и
    // вниз.
    if (value <= _view->verticalScrollBar()->minimum() + 2
            && _messagesCanLoad && _model->canFetchMoreUpwards()) {
        _model->fetchMoreUpwards();
    }
}

void ChatPage::inputChanged()
{
    // Пункт «Send» в меню не должен предлагать отправить пустоту.
    emit canSendChanged(!_input->text().trimmed().isEmpty());
}

void ChatPage::send()
{
    const QString text = _input->text();

    if (text.trimmed().isEmpty()) {
        // Пустое поле и нажатый центр -- значит хотели не отправить, а
        // вернуться к сообщениям.
        focusMessages();
        return;
    }

    _model->sendMessage(text);
    _input->clear();
}

void ChatPage::scrollTo(qint32 index)
{
    if (index < 0 || index >= _model->rowCount()) {
        return;
    }

    _view->scrollTo(_model->index(index, 0), QAbstractItemView::PositionAtBottom);
}

void ChatPage::scrollForNew()
{
    if (_model->rowCount() > 0) {
        _view->scrollToBottom();
    }
}

void ChatPage::currentChanged(QModelIndex current, QModelIndex previous)
{
    Q_UNUSED(current)
    Q_UNUSED(previous)

    // Ушли со строки -- анимация останавливается. Сама она не заводится:
    // проигрывание, как и загрузка с конвертацией, начинается нажатием.
    // Иначе прокрутка мимо десятка гифок запускала бы их все по очереди.
    stopAnimation();
}

void ChatPage::activate()
{
    const QModelIndex index = _view->currentIndex();

    if (!index.isValid()) {
        return;
    }

    const int row = index.row();
    const MessageDelegate::MediaKind kind = _delegate->mediaKind(index);
    const QString path = index.data(TwoLineDelegate::roleByName(_model, "mediaFilePath")).toString();

    // Фотография и обычный стикер уже скачаны и нарисованы миниатюрой --
    // нажатие открывает их целиком.
    const QString photo = index.data(TwoLineDelegate::roleByName(_model, "photoFile")).toString();
    const QString sticker = index.data(TwoLineDelegate::roleByName(_model, "stickerFile")).toString();

    if (kind == MessageDelegate::NoMedia && !(photo.isEmpty() && sticker.isEmpty())) {
        emit showImage(photo.isEmpty() ? sticker : photo);
        return;
    }

    // Пока файла нет, любое из этого начинается с загрузки -- отправлять на
    // сервер нечего.
    if (kind != MessageDelegate::NoMedia && path.isEmpty()) {
        _model->activateMedia(row);
        return;
    }

    // Без медиа-сервера ни голосовое, ни кружок, ни гифка на этом телефоне не
    // проигрываются: ни Opus, ни VP9 он не декодирует. Файл уже скачан --
    // значит всё, что можно, уже сделано, и честнее сказать это, чем молча
    // ничего не сделать.
    if (kind != MessageDelegate::NoMedia
            && _settings->activeMediaServerUrl().isEmpty()) {
        if (kind == MessageDelegate::VideoMedia) {
            // У видео есть шанс: снятое старым телефоном 3GP штатный плеер
            // играет сам. Телеграмное H.264 -- нет, и скажет об этом.
            _model->activateMedia(row);
            return;
        }

        emit status(tr("Needs a media server: see Settings"));
        return;
    }

    switch (kind) {
    case MessageDelegate::VoiceMedia:
        // Opus, декодера которому на этом телефоне нет: файл уходит на сервер,
        // играется то, что вернулось.
        _player->play(path);
        return;

    case MessageDelegate::AnimationMedia: {
        // Кружок, видеостикер и гифка превращаются в GIF и рисуются прямо в
        // строке, а не открывают ничего.
        const QString animation =
                index.data(TwoLineDelegate::roleByName(_model, "animationFile")).toString();

        if (animation.isEmpty()) {
            note(row, tr("converting..."));
            _converter->convert(row, path, "animation");
        } else {
            startAnimation(index);
        }
        return;
    }

    case MessageDelegate::VideoMedia:
        // Обычное видео в GIF гнать незачем -- оно длинное. Сервер делает 3GP,
        // и его телефон играет сам. Раньше сюда же попадал исходный mp4, и
        // штатный плеер честно отвечал «воспроизведение невозможно».
        note(row, tr("converting..."));
        _converter->convert(row, path, "video");
        return;

    default:
        break;
    }

    // Файл на месте, но открывать его телефону нечем -- лучше сказать об этом,
    // чем промолчать: раньше нажатие на такой файл просто ничего не делало.
    if (!path.isEmpty() && !canOpenFile(path)) {
        emit status(tr("No application for this file type"));
        return;
    }

    _model->activateMedia(row);
}

void ChatPage::converted(int index, QString path)
{
    const QModelIndex row = _model->index(index, 0);

    // Метка строки обещает конвертацию, пока готового файла нет. Он появился --
    // значит обещание пора поменять на «press to play».
    _delegate->markConverted(row.data(TwoLineDelegate::roleByName(_model, "mediaFilePath")).toString());
    note(-1, QString());
    _view->viewport()->update(_view->visualRect(row));

    // Что вернулось, видно по расширению: 3GP играет штатный плеер, остальное
    // (GIF, а если сервер не осилил -- PNG) рисуется в строке.
    if (path.endsWith(".3gp") || path.endsWith(".mp4")) {
        _model->openDownloadedFile(path);
        return;
    }

    _model->setAnimationFile(index, path);

    if (_view->currentIndex().row() == index) {
        startAnimation(_view->currentIndex());
    }
}

void ChatPage::converterStatus()
{
    const QString text = _converter->status();

    emit status(text);

    // «converting», «media server is off», «too large to convert» -- всё это
    // надо видеть там, куда смотрят, а не в строке состояния под меню.
    if (text != "idle") {
        note(_noteRow, text);
    }
}

// Приписка живёт у делегата, но обновлять строку должен список.
void ChatPage::note(int row, const QString &text)
{
    _noteRow = row;
    _delegate->setNote(row, text);

    if (row >= 0 && row < _model->rowCount()) {
        _view->viewport()->update(_view->visualRect(_model->index(row, 0)));
    }
}

void ChatPage::playerStatus()
{
    emit status(_player->status());
}

void ChatPage::startAnimation(const QModelIndex &index)
{
    stopAnimation();

    if (!index.isValid()) {
        return;
    }

    const QString path = index.data(TwoLineDelegate::roleByName(_model, "animationFile")).toString();

    if (path.isEmpty()) {
        return;
    }

    _movie = new QMovie(path, QByteArray(), this);

    if (!_movie->isValid()) {
        delete _movie;
        _movie = 0;
        return;
    }

    // Кадры приходят мелко и часто, а перерисовывать весь список на каждый --
    // непозволительно: обновляется только своя строка.
    _animationRow = index.row();
    _delegate->setAnimation(_animationRow, _movie);

    connect(_movie, SIGNAL(frameChanged(int)), this, SLOT(animationFrame()));
    _movie->start();
}

void ChatPage::stopAnimation()
{
    if (!_movie) {
        return;
    }

    _movie->stop();
    _movie->deleteLater();
    _movie = 0;
    _animationRow = -1;
    _delegate->setAnimation(-1, 0);
}

void ChatPage::animationFrame()
{
    if (_animationRow < 0) {
        return;
    }

    _view->viewport()->update(_view->visualRect(_model->index(_animationRow, 0)));
}
