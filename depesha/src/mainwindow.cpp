#include "mainwindow.h"
#include "chatpage.h"
#include "imagepage.h"
#include "settingspage.h"
#include "folderfilter.h"
#include "listview.h"
#include "loginpage.h"
#include "twolinedelegate.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QEvent>
#include <QKeyEvent>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QStackedWidget>
#include <QScrollBar>
#include <QStatusBar>
#include <QTimer>

#include "cachemanager.h"
#include "appsettings.h"
#ifdef KG_TEST_MEDIA_SERVER
#include "testdefaults.h"
#endif
#include "avatardownloader.h"
#include "dialogsmodel.h"
#include "foldersmodel.h"
#include "kglog.h"
#include "messagesmodel.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent)
{
    // Версия в заголовке: на снимке экрана с телефона иначе не видно, какая
    // сборка на нём стоит, и половина вопросов начинается с этого.
    setWindowTitle("Depesha " + QApplication::applicationVersion());

    _historyRetries = 0;
    _canSend = false;
    _returnTo = 0;
    _sendAction = 0;
    _openAction = 0;
    _replyAction = 0;
    _cancelReplyAction = 0;
    _silentAction = 0;
    _laterHourAction = 0;
    _laterDayAction = 0;
    _foldersMenu = 0;

    _client = new TgClient(this);
    _avatars = new AvatarDownloader(this);
    _chatMedia = new AvatarDownloader(this);
    _dialogs = new DialogsModel(this);
    _foldersModel = new FoldersModel(this);
    _messages = new MessagesModel(this);
    _cache = new CacheManager(this);
    _settings = new AppSettings(this);

#ifdef KG_TEST_MEDIA_SERVER
    // Отладочная сборка: адрес сервера подставляется, если поле пустое.
    // Убирается вместе с KG_TEST_MEDIA_SERVER в depesha.pro -- см.
    // testdefaults.h.
    if (_settings->mediaServerUrl().isEmpty()) {
        _settings->setMediaServerUrl(KG_TEST_MEDIA_URL);
        _settings->setMediaServerToken(KG_TEST_MEDIA_TOKEN);
        _settings->setMediaServer(true);
    }
#endif

    // Два загрузчика вместо одного, потому что спискам нужно разное.
    //
    // Списку чатов -- только аватарки: фотографии и стикеры он не рисует.
    // Переписке -- наоборот: картинки и стикеры рисуются, а аватарки нет, и
    // качать их на каждого участника каждой страницы истории (да ещё и
    // перерисовывать каждую в круглый PNG) -- чистая трата.
    _avatars->setClient(_client);
    _avatars->setMediaEnabled(false);
    // Круглый PNG 160x160 на каждую аватарку -- разворот JPEG, рисование со
    // сглаживанием и запись файла, и всё это в главном потоке. На сорок чатов
    // разом телефон вставал почти на минуту сразу после появления списка.
    // Круг рисует делегат, один раз на аватарку и уже в размере строки.
    _avatars->setRoundAvatars(false);

    _chatMedia->setClient(_client);
    _chatMedia->setAvatarsEnabled(false);

    _foldersModel->setClient(_client);
    _dialogs->setClient(_client);
    _dialogs->setAvatarDownloader(_avatars);
    // Модель папок только помечает строки: в какие папки попал этот чат.
    // Отбирает по этой пометке уже прокси ниже.
    _dialogs->setFolders(_foldersModel);

    // Две вещи, из-за которых телефон после входа минуты четыре было не
    // взять в руки.
    //
    // Первая: каждая пришедшая страница чатов тут же просила следующую, и так
    // до конца списка. Сотни чатов подряд -- и разбор, и расшифровка каждого
    // пакета в главном потоке. Страницы теперь догружаются по прокрутке,
    // и только они.
    _dialogs->setAutoLoadAll(false);
    // Вторая: аватарки скачивались для всех пришедших чатов разом. Теперь --
    // только для тех строк, которые видно.
    _dialogs->setLazyAvatars(true);
    _messages->setClient(_client);
    _messages->setAvatarDownloader(_chatMedia);
    _messages->setMarkAsRead(_settings->markAsRead());

    // Загрузчик аватарок моделям намеренно не отдан. Он тянет не только
    // аватарки: на каждую страницу истории он сам качает все фотографии и
    // стикеры -- по сорок штук за раз, в память, до конца загрузки. Депеша не
    // показывает ни того, ни другого, и платила за это чужой памятью на
    // телефоне, где её 56 МБ на всё.


    _login = new LoginPage(this);

    _folderFilter = new FolderFilter(_dialogs, this);

    _dialogsView = new ListView(this);
    _dialogsView->setModel(_folderFilter);
    // Делегату отдаётся исходная модель, а не прокси: номера ролей общие, а
    // имён ролей прокси в Qt 4 не пересылает.
    // У чата запасной роли нет: заголовок есть всегда, а вторая строка пустая
    // только у пустого чата.
    TwoLineDelegate *dialogsDelegate = new TwoLineDelegate(_dialogs, "title", "messageText",
                                                           "messageTime", QByteArray(),
                                                           _dialogsView);
    dialogsDelegate->setAvatarRoles(_dialogs, "avatar", "thumbnailColor", "thumbnailText");
    _dialogsView->setItemDelegate(dialogsDelegate);
    _dialogsView->setEmptyText(tr("No chats"));

    _chat = new ChatPage(_messages, _settings, this);
    _settingsPage = new SettingsPage(_settings, this);
    _imagePage = new ImagePage(this);

    _stack = new QStackedWidget(this);
    _stack->addWidget(_login);
    _stack->addWidget(_dialogsView);
    _stack->addWidget(_chat);
    _stack->addWidget(_settingsPage);
    _stack->addWidget(_imagePage);
    setCentralWidget(_stack);

    statusBar()->setSizeGripEnabled(false);

    // Правая софт-клавиша. На Symbian роль сама кладёт действие на нужную
    // клавишу; на десктопе это просто пункт меню.
    _backAction = new QAction(tr("Back"), this);
#ifdef Q_OS_SYMBIAN
    _backAction->setSoftKeyRole(QAction::NegativeSoftKey);
#endif
    connect(_backAction, SIGNAL(triggered()), this, SLOT(goBack()));

    // Действие «назад» висит на самом окне -- так, как было до 0.2.3.
    //
    // В 0.2.3 его перевесили на стопку страниц, чтобы из меню исчез раздел
    // «Actions», который Qt на Symbian составляет из действий окна. Раздел
    // действительно исчез -- вместе со всем меню: левая софт-клавиша
    // перестала открывать что-либо. Цена за лишний раздел оказалась
    // несоразмерной, и порядок возвращён обратно.
    addAction(_backAction);

    // Второе, обычное действие «назад» -- без роли софт-клавиши.
    //
    // Qt на Symbian собирает действия окна в раздел меню «Actions», но
    // действие с ролью софт-клавиши в него не кладёт -- раздел получался
    // пустым и по нажатию ничего не делал. Убрать сам раздел не вышло: без
    // действий у окна перестаёт строиться всё меню (0.2.3-0.2.6). Значит,
    // пусть в нём будет то, что работает.
    QAction *plainBackAction = new QAction(tr("Back"), this);
    connect(plainBackAction, SIGNAL(triggered()), this, SLOT(goBack()));
    addAction(plainBackAction);

    // Левая софт-клавиша: список папок. На Symbian меню главного окна кладётся
    // на неё системой само.
    _foldersMenu = menuBar()->addMenu(tr("Folders"));
    connect(_foldersMenu, SIGNAL(triggered(QAction*)), this, SLOT(folderSelected(QAction*)));
    connect(_foldersModel, SIGNAL(foldersChanged(QList<TgObject>)), this, SLOT(foldersChanged()));
    foldersChanged();

    // Отправка есть и в меню: центр джойстика делает то же самое, но меню
    // это видно, а клавишу надо знать.
    _sendAction = menuBar()->addAction(tr("Send"));
    _sendAction->setVisible(false);
    connect(_sendAction, SIGNAL(triggered()), _chat, SLOT(send()));
    connect(_chat, SIGNAL(canSendChanged(bool)), this, SLOT(canSendChanged(bool)));

    // Действия над перепиской.
    //
    // Сначала они были действиями окна -- в расчёте на то, что Qt сложит их в
    // свой раздел «Actions». В разделе они не появились: он собирается не так
    // и на смену видимости не отзывается. В нашем меню и то, и другое работает
    // проверенно -- на нём с 0.1.3 держатся «Folders» и «Send», и видно их
    // ровно там, где они нужны.
    _replyAction = menuBar()->addAction(tr("Reply"));
    _replyAction->setVisible(false);
    connect(_replyAction, SIGNAL(triggered()), this, SLOT(replyToMessage()));

    _cancelReplyAction = menuBar()->addAction(tr("Cancel reply"));
    _cancelReplyAction->setVisible(false);
    connect(_cancelReplyAction, SIGNAL(triggered()), this, SLOT(cancelReply()));

    // Это команда, а не настройка: «отправить без звука» отправляет набранное
    // прямо сейчас. Переключателем оно было ровно один выпуск и сбивало с
    // толку -- выбираешь пункт, а сообщение не уходит.
    _silentAction = menuBar()->addAction(tr("Send without sound"));
    _silentAction->setVisible(false);
    connect(_silentAction, SIGNAL(triggered()), this, SLOT(sendSilent()));

    // Отложенная отправка двумя готовыми сроками: выбирать час и минуты на
    // этой клавиатуре -- отдельное мучение, а эти два покрывают почти всё.
    _laterHourAction = menuBar()->addAction(tr("Send in an hour"));
    _laterHourAction->setVisible(false);
    connect(_laterHourAction, SIGNAL(triggered()), this, SLOT(sendInHour()));

    _laterDayAction = menuBar()->addAction(tr("Send tomorrow 9:00"));
    _laterDayAction->setVisible(false);
    connect(_laterDayAction, SIGNAL(triggered()), this, SLOT(sendTomorrow()));

    // Всё то же, что делает центр на выделенном сообщении: скачать, показать,
    // проиграть, открыть. В меню оно затем, что по метке в строке видно, что
    // произойдёт, а по клавише -- нет.
    _openAction = menuBar()->addAction(tr("Open message"));
    _openAction->setVisible(false);
    connect(_openAction, SIGNAL(triggered()), _chat, SLOT(activate()));

    QAction *settingsAction = menuBar()->addAction(tr("Settings"));
    connect(settingsAction, SIGNAL(triggered()), this, SLOT(showSettings()));

    // Отдельного «назад» в меню нет намеренно: он и так на правой
    // софт-клавише, а Qt показывает его же в разделе «Actions».

    connect(_chat, SIGNAL(status(QString)), this, SLOT(status(QString)));
    connect(_chat, SIGNAL(showImage(QString)), this, SLOT(showImage(QString)));
    connect(_chat, SIGNAL(replyStateChanged()), this, SLOT(updateChatActions()));
    connect(_settings, SIGNAL(markAsReadChanged()), this, SLOT(markAsReadChanged()));

    connect(_client, SIGNAL(initialized(bool)), this, SLOT(clientInitialized(bool)));
    connect(_client, SIGNAL(authorized(TgLongVariant)), this, SLOT(clientAuthorized(TgLongVariant)));
    connect(_client, SIGNAL(disconnected(bool)), this, SLOT(clientDisconnected(bool)));
    connect(_client, SIGNAL(socketError(qint32,QString)), this, SLOT(socketError(qint32,QString)));
    connect(_client, SIGNAL(rpcError(qint32,QString,TgLongVariant)),
            this, SLOT(rpcError(qint32,QString,TgLongVariant)));
    connect(_client, SIGNAL(authSentCodeResponse(TgObject,TgLongVariant)),
            this, SLOT(sentCode(TgObject,TgLongVariant)));
    connect(_client, SIGNAL(tfaRequired()), this, SLOT(tfaRequired()));
    connect(_client, SIGNAL(tfaProgress(qint32)), this, SLOT(tfaProgress(qint32)));

    connect(_login, SIGNAL(phoneEntered(QString)), this, SLOT(sendPhone(QString)));
    connect(_login, SIGNAL(codeEntered(QString)), this, SLOT(sendCode(QString)));
    connect(_login, SIGNAL(passwordEntered(QString)), this, SLOT(sendPassword(QString)));

    connect(_dialogsView, SIGNAL(activated(QModelIndex)), this, SLOT(openDialog(QModelIndex)));
    // activated() на кнопочном телефоне срабатывает не всегда: в режиме
    // клавиатурной навигации центральная клавиша у списка занята переключением
    // edit focus. Поэтому открытие чата ловится ещё и напрямую.
    _dialogsView->installEventFilter(this);
    connect(_dialogsView->verticalScrollBar(), SIGNAL(valueChanged(int)),
            this, SLOT(dialogsScrolled(int)));
    connect(_folderFilter, SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SLOT(updateVisibleAvatars()));

    showPage(_login);

    // Одна попытка переподключения на все причины. Раньше её заводили и обрыв,
    // и ошибка сокета, каждый раз новым одноразовым таймером: на телефоне они
    // приходят пачками, и попыток набиралось столько же, сколько ошибок.
    _reconnectTimer = new QTimer(this);
    _reconnectTimer->setSingleShot(true);
    _reconnectTimer->setInterval(4000);
    connect(_reconnectTimer, SIGNAL(timeout()), this, SLOT(reconnect()));

    // Уборка кэша -- обход всех файлов с сортировкой по времени, и на телефоне
    // он занимал 22 секунды, в которые не обслуживается и сокет. Через двадцать
    // секунд после запуска это приходилось ровно на разговор с сервером.
    QTimer::singleShot(120000, _cache, SLOT(trim()));

    // Строка памяти раз в минуту. После падения "мало памяти" это
    // единственный способ увидеть, росла память ровно или скакнула на чём-то
    // одном: журнал переживает и выдёргивание аккумулятора.
    QTimer *heartbeat = new QTimer(this);
    heartbeat->setInterval(60000);
    connect(heartbeat, SIGNAL(timeout()), this, SLOT(logMemory()));
    heartbeat->start();

    // Соединение поднимается сразу и всегда, есть сессия или нет. Без него
    // authSendCode уходит в никуда: отправлять нечем, и экран выглядит
    // мёртвым, хотя всё нажимается.
    status(tr("Connecting..."));
    _client->start();
}

void MainWindow::showPage(QWidget *page)
{
    _stack->setCurrentWidget(page);

    // Меню показывает только то, что имеет смысл на этой странице. «Send»
    // вдобавок появляется лишь когда есть что отправлять: предлагать отправить
    // пустоту, стоя на чужом сообщении, -- это и сбивало с толку.
    if (_sendAction) {
        _sendAction->setVisible(page == _chat && _canSend);
    }
    if (_openAction) {
        _openAction->setVisible(page == _chat);
    }

    updateChatActions();
    if (_foldersMenu) {
        _foldersMenu->menuAction()->setVisible(page == _dialogsView);
    }

    // Фокус отдаётся не странице, а тому, что на ней вводит или листается.
    // Страница -- обычный QWidget: получив фокус, она его просто держит, и
    // экран перестаёт отзываться на клавиши вовсе.
    if (page == _login) {
        _login->focusInput();
    } else if (page == _chat) {
        _chat->focusInput();
    } else {
        page->setFocus();

#ifdef QT_KEYPAD_NAVIGATION
        // Списку нужен edit focus, иначе стрелки уходят на переход между
        // виджетами, а не на прокрутку.
        page->setEditFocus(true);
#endif
    }
}

void MainWindow::status(const QString &text)
{
    statusBar()->showMessage(text, text.isEmpty() ? 1 : 8000);
}

void MainWindow::clientInitialized(bool hasUserId)
{
    kgLog("client initialized, hasUserId=" + QString::number(hasUserId));

    if (hasUserId) {
        return;
    }

    // Номер могли ввести, пока поднималось соединение -- тогда отправляем его
    // прямо сейчас, а не просим набрать заново.
    if (!_pendingPhone.isEmpty()) {
        const QString phone = _pendingPhone;
        _pendingPhone.clear();
        sendPhone(phone);
        return;
    }

    status(QString());
    _login->setStep(LoginPage::Phone);
    showPage(_login);
}

void MainWindow::clientAuthorized(TgLongVariant userId)
{
    Q_UNUSED(userId)

    status(QString());
    showPage(_dialogsView);

    kgLog("authorized");

    // Связь могла пропасть, пока чат был открыт: тогда запрос истории ушёл в
    // никуда, и после восстановления его надо повторить.
    if (_stack->currentWidget() == _chat && _messages->rowCount() == 0
            && !_chatPeer.isEmpty()) {
        _historyRetries = 0;
        _messages->setPeer(_chatPeer);
        return;
    }

    status(tr("Loading chats..."));

    // refresh() уже просит первую страницу; второй раз просить незачем.
    _dialogs->refresh();
    _foldersModel->refresh();
}

void MainWindow::foldersChanged()
{
    const int current = _folderFilter->folderIndex();
    const int count = _foldersModel->rowCount();

    _foldersMenu->clear();

    // Свой пункт "все чаты" нужен только пока папок нет: сервер отдаёт их
    // списком, первым пунктом которого и так идёт папка "все чаты"
    // (DialogFilterDefault). Со своим их получалось два.
    if (count == 0) {
        QAction *all = _foldersMenu->addAction(tr("All chats"));
        all->setCheckable(true);
        all->setChecked(true);
        all->setData(-1);
    }

    for (int i = 0; i < count; ++i) {
        const QString title = _foldersModel->index(i, 0)
                .data(TwoLineDelegate::roleByName(_foldersModel, "title")).toString();

        QAction *action = _foldersMenu->addAction(title.isEmpty() ? tr("Folder") : title);
        action->setCheckable(true);
        action->setChecked(current == i);
        action->setData(i);
    }

    _folderFilter->refilter();
}

void MainWindow::folderSelected(QAction *action)
{
    if (!action) {
        return;
    }

    _folderFilter->setFolderIndex(action->data().toInt());
    foldersChanged();

    // Выделение после смены папки остаётся на старой строке, которой в новом
    // списке может не быть вовсе.
    if (_folderFilter->rowCount() > 0) {
        _dialogsView->setCurrentIndex(_folderFilter->index(0, 0));
    }
}

void MainWindow::clientDisconnected(bool hasUserId)
{
    if (!hasUserId) {
        _login->setStep(LoginPage::Phone);
        showPage(_login);
        return;
    }

    // Через таймер: связь на телефоне пропадает пачками, и немедленная
    // повторная попытка обычно ложится там же, где легла предыдущая.
    kgLog("disconnected, hasUserId=" + QString::number(hasUserId));
    status(tr("Reconnecting..."));
    _reconnectTimer->start();
}

void MainWindow::reconnect()
{
    _client->start();
}

void MainWindow::socketError(qint32 errorCode, QString errorMessage)
{
    // Ни код, ни текст Qt о сокете пользователю ничего не говорят. Важно
    // только то, что попытка повторится.
    kgLog("socket error " + QString::number(errorCode) + ": " + errorMessage);

    _login->setBusy(false);
    status(tr("No connection. Trying again..."));
    _reconnectTimer->start();
}

void MainWindow::rpcError(qint32 errorCode, QString errorMessage, TgLongVariant messageId)
{
    Q_UNUSED(messageId)

    kgLog("rpc error " + QString::number(errorCode) + ": " + errorMessage);

    _login->setBusy(false);

    if (errorMessage == "SESSION_PASSWORD_NEEDED") {
        tfaRequired();
        return;
    }

    if (errorMessage == "PHONE_CODE_INVALID") {
        status(tr("Wrong code"));
        return;
    }

    if (errorMessage == "PASSWORD_HASH_INVALID" || errorMessage == "SRP_PASSWORD_INVALID") {
        status(tr("Wrong password"));
        return;
    }

    status(errorMessage + " (" + QString::number(errorCode) + ")");
}

void MainWindow::sentCode(TgObject data, TgLongVariant messageId)
{
    Q_UNUSED(messageId)

    _phoneCodeHash = data["phone_code_hash"].toString();

    _login->setBusy(false);
    _login->setStep(LoginPage::Code);
}

void MainWindow::tfaRequired()
{
    _login->setBusy(false);
    _login->setStep(LoginPage::Password);
    _login->setHint(tr("Checking the password takes about a minute on this phone."));
    showPage(_login);
}

void MainWindow::tfaProgress(qint32 percent)
{
    _login->setBusy(true, tr("Checking: %1%").arg(percent));
}

void MainWindow::sendPhone(QString phone)
{
    _phone = phone;
    _login->setBusy(true);

    if (!_client->isInitialized()) {
        // Соединения ещё нет. Запрос сейчас просто утонул бы молча.
        kgLog("phone entered before the connection was up, queued");
        _pendingPhone = phone;
        status(tr("Connecting..."));
        return;
    }

    status(tr("Sending the number..."));
    kgLog("authSendCode");
    _client->authSendCode(phone);
}

void MainWindow::sendCode(QString code)
{
    _login->setBusy(true);
    _client->authSignIn(_phone, _phoneCodeHash, code);
}

void MainWindow::sendPassword(QString password)
{
    _login->setBusy(true, tr("Checking..."));
    _client->authCheckPassword(password);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == _dialogsView && event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent*>(event)->key();

        if (key == Qt::Key_Select || key == Qt::Key_Enter || key == Qt::Key_Return) {
            openDialog(_dialogsView->currentIndex());
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::openDialog(const QModelIndex &index)
{
    // Сюда приходят и activated(), и наш перехват клавиши: открывать чат
    // дважды незачем.
    if (!index.isValid() || _stack->currentWidget() == _chat) {
        return;
    }

    const int peerRole = TwoLineDelegate::roleByName(_dialogs, "peerBytes");
    const int titleRole = TwoLineDelegate::roleByName(_dialogs, "title");

    if (peerRole < 0) {
        return;
    }

    const QByteArray peer = index.data(peerRole).toByteArray();

    _chat->setTitle(index.data(titleRole).toString());


    _messages->setPeer(peer);

    // Чат открылся пустым, и по коду не видно, на каком шаге теряется история.
    // canFetchMoreUpwards() здесь -- показание прибора: false сразу после
    // setPeer означает либо негодный peer, либо что запрос уже ушёл.
    _chatPeer = peer;
    _historyRetries = 0;

    kgLog("open dialog: peer=" + QString::number(peer.size())
          + "b canFetchUp=" + QString::number(_messages->canFetchMoreUpwards())
          + " connected=" + QString::number(_client->isConnected())
          + " initialized=" + QString::number(_client->isInitialized())
          + " authorized=" + QString::number(_client->isAuthorized()));

    showPage(_chat);
    _chat->focusInput();

    // Если ответ на первый запрос истории потерялся, второй заход через пару
    // секунд обходится дешевле, чем пустой чат.
    QTimer::singleShot(3000, this, SLOT(retryHistory()));
}

void MainWindow::logMemory()
{
    kgLog("heartbeat " + kgMemoryInfo());
}

void MainWindow::retryHistory()
{
    if (_stack->currentWidget() != _chat || _messages->rowCount() > 0
            || _chatPeer.isEmpty() || _historyRetries >= 3) {
        return;
    }

    ++_historyRetries;

    kgLog("chat still empty (" + QString::number(_historyRetries)
          + "), canFetchUp=" + QString::number(_messages->canFetchMoreUpwards())
          + " authorized=" + QString::number(_client->isAuthorized()));

    if (_messages->canFetchMoreUpwards()) {
        _messages->fetchMoreUpwards();
    } else {
        // Запрос уже числится отправленным, а ответа нет -- значит он потерян
        // вместе с соединением, на котором ушёл. Повторная установка того же
        // собеседника обнуляет номера запросов и посылает их заново.
        _messages->setPeer(_chatPeer);
    }

    QTimer::singleShot(4000, this, SLOT(retryHistory()));
}

void MainWindow::updateVisibleAvatars()
{
    // Аватарки просятся для того, что сейчас на экране, плюс пара строк
    // вперёд: пролистнуть успеют раньше, чем она приедет.
    const QModelIndex top = _dialogsView->indexAt(QPoint(4, 4));
    const QModelIndex bottom = _dialogsView->indexAt(
                QPoint(4, _dialogsView->viewport()->height() - 4));

    const int first = top.isValid() ? _folderFilter->mapToSource(top).row() : 0;
    const int last = bottom.isValid() ? _folderFilter->mapToSource(bottom).row() : first + 8;

    _dialogs->requestAvatars(first, last + 2);
}

void MainWindow::dialogsScrolled(int value)
{
    updateVisibleAvatars();

    // Докрутили до конца списка -- запросить следующую страницу чатов.
    if (value >= _dialogsView->verticalScrollBar()->maximum() - 2
            && _dialogs->canFetchMoreDownwards()) {
        _dialogs->fetchMoreDownwards();
    }
}

void MainWindow::showSettings()
{
    // Настройки открываются откуда угодно, значит и возвращать должны туда же.
    // Раньше «назад» из них всегда вело в список чатов, и открытая переписка
    // на этом закрывалась.
    _returnTo = _stack->currentWidget();

    _settingsPage->reload();
    showPage(_settingsPage);
    _settingsPage->focusFirst();
}

void MainWindow::canSendChanged(bool can)
{
    _canSend = can;

    if (_sendAction) {
        _sendAction->setVisible(_stack->currentWidget() == _chat && can);
    }

    updateChatActions();
}

// В меню показывается только то, что сейчас имеет смысл: отвечать -- когда
// выделено сообщение, отменять ответ -- когда он есть, откладывать -- когда
// есть что отправлять.
void MainWindow::updateChatActions()
{
    if (!_replyAction) {
        return;
    }

    const bool inChat = _stack->currentWidget() == _chat;

    _replyAction->setVisible(inChat);
    _cancelReplyAction->setVisible(inChat && _chat->hasReply());
    // Отправить без звука можно только когда есть что отправлять -- как и
    // отложенную отправку.
    _silentAction->setVisible(inChat && _canSend);
    _laterHourAction->setVisible(inChat && _canSend);
    _laterDayAction->setVisible(inChat && _canSend);
}

void MainWindow::replyToMessage()
{
    _chat->replyToCurrent();
    updateChatActions();
}

void MainWindow::cancelReply()
{
    _chat->cancelReply();
    updateChatActions();
}

void MainWindow::sendSilent()
{
    _chat->sendSilent();
}

void MainWindow::sendInHour()
{
    _chat->sendLater(60 * 60);
}

void MainWindow::sendTomorrow()
{
    // Завтра в девять утра -- по часам телефона: сколько до них осталось.
    // Само время отправки модель отсчитает от серверного.
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime target(now.date().addDays(1), QTime(9, 0));

    _chat->sendLater(now.secsTo(target));
}

void MainWindow::showImage(QString path)
{
    if (_imagePage->showFile(path)) {
        _returnTo = _stack->currentWidget();
        showPage(_imagePage);
    } else {
        status(tr("Cannot show this picture"));
    }
}

void MainWindow::markAsReadChanged()
{
    _messages->setMarkAsRead(_settings->markAsRead());
}

void MainWindow::goBack()
{
    if (_stack->currentWidget() == _imagePage || _stack->currentWidget() == _settingsPage) {
        QWidget *back = _returnTo ? _returnTo : _dialogsView;
        _returnTo = 0;

        showPage(back);

        if (back == _chat) {
            _chat->focusMessages();
        }
        return;
    }

    if (_stack->currentWidget() == _chat) {
        showPage(_dialogsView);

        // История прочитанного чата больше не нужна, а держать её -- держать
        // сотни строк с разметкой. На 32 МБ потолка это заметно.
        _messages->resetState();
        _chatPeer.clear();
        return;
    }

    // Из списка чатов выход закрывает приложение -- как и везде в системе.
    if (_stack->currentWidget() == _dialogsView) {
        close();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // В переписке вверх из строки ввода уводит в список сообщений, а не в
    // никуда: иначе до сообщений с клавиатуры не добраться.
    if (_stack->currentWidget() == _chat && event->key() == Qt::Key_Up) {
        _chat->focusMessages();
        event->accept();
        return;
    }

    QMainWindow::keyPressEvent(event);
}
