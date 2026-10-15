#include "settingspage.h"

#include <QCheckBox>
#include <QFont>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "appsettings.h"
#include "cachemanager.h"

SettingsPage::SettingsPage(AppSettings *settings, CacheManager *cache,
                           QWidget *parent) :
    QWidget(parent),
    _settings(settings),
    _cache(cache),
    _loading(false)
{
    _markAsRead = new QCheckBox(tr("Mark chats as read"), this);
    _notifications = new QCheckBox(tr("Notifications"), this);
    _vibrate = new QCheckBox(tr("Vibrate"), this);
    _mediaServer = new QCheckBox(tr("Media server"), this);

    QLabel *urlLabel = new QLabel(tr("Address"), this);
    _url = new QLineEdit(this);
    _url->setInputMethodHints(Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText);

    QLabel *tokenLabel = new QLabel(tr("Token"), this);
    _token = new QLineEdit(this);
    _token->setInputMethodHints(Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText);

    QPushButton *checkButton = new QPushButton(tr("Check server"), this);

    _status = new QLabel(this);
    _status->setWordWrap(true);

    QLabel *hint = new QLabel(tr("Without a server the client still shows text and "
                                 "pictures. It cannot play voice messages, round "
                                 "videos or animated stickers: the phone has no "
                                 "decoder for them."), this);
    hint->setWordWrap(true);
    hint->setEnabled(false);

    // --- Кэш ---
    //
    // Два действия, которых у Депеши не было вовсе: куда складывать и как
    // убрать. Память телефона -- несколько десятков мегабайт на всё, и
    // аватарки с картинками её забивали за недели переписки. Сессия и
    // настройки при переносе остаются в памяти телефона: они должны пережить
    // вынутую карту, а их потеря -- это вход по SMS заново.
    QLabel *cacheTitle = new QLabel(tr("Cache"), this);
    QFont bold = cacheTitle->font();
    bold.setBold(true);
    cacheTitle->setFont(bold);

    _cacheText = new QLabel(this);
    _cacheText->setWordWrap(true);
    _cacheText->setEnabled(false);

    _cachePlace = new QPushButton(this);
    _cacheClear = new QPushButton(tr("Clear cache now"), this);

    // Полей больше, чем строк на внешнем экране, поэтому страница прокручивается.
    QWidget *inner = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(inner);
    layout->setMargin(4);
    layout->addWidget(_markAsRead);
    layout->addWidget(_notifications);
    layout->addWidget(_vibrate);
    layout->addWidget(_mediaServer);
    layout->addWidget(urlLabel);
    layout->addWidget(_url);
    layout->addWidget(tokenLabel);
    layout->addWidget(_token);
    layout->addWidget(checkButton);
    layout->addWidget(_status);
    layout->addWidget(hint);
    layout->addWidget(cacheTitle);
    layout->addWidget(_cacheText);
    layout->addWidget(_cachePlace);
    layout->addWidget(_cacheClear);
    layout->addStretch();

    QScrollArea *area = new QScrollArea(this);
    area->setWidget(inner);
    area->setWidgetResizable(true);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Область прокрутки фокус себе не берёт: он должен доставаться полям.
    area->setFocusPolicy(Qt::NoFocus);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setMargin(0);
    outer->addWidget(area);

    connect(_markAsRead, SIGNAL(toggled(bool)), this, SLOT(apply()));
    connect(_notifications, SIGNAL(toggled(bool)), this, SLOT(apply()));
    connect(_vibrate, SIGNAL(toggled(bool)), this, SLOT(apply()));
    connect(_mediaServer, SIGNAL(toggled(bool)), this, SLOT(apply()));
    connect(_url, SIGNAL(editingFinished()), this, SLOT(apply()));
    connect(_token, SIGNAL(editingFinished()), this, SLOT(apply()));
    connect(checkButton, SIGNAL(clicked()), this, SLOT(check()));
    connect(_settings, SIGNAL(mediaServerStatusChanged()), this, SLOT(serverStatus()));
    connect(_cachePlace, SIGNAL(clicked()), this, SLOT(movePlace()));
    connect(_cacheClear, SIGNAL(clicked()), this, SLOT(clearCache()));
    if (_cache) {
        connect(_cache, SIGNAL(changed()), this, SLOT(showCache()));
    }

    reload();
}

void SettingsPage::reload()
{
    // Пока поля заполняются, их сигналы не должны записывать настройки
    // обратно: иначе первое же появление страницы перезапишет их же.
    _loading = true;

    _markAsRead->setChecked(_settings->markAsRead());
    _notifications->setChecked(_settings->notifications());
    _vibrate->setChecked(_settings->vibrate());
    _mediaServer->setChecked(_settings->mediaServer());
    _url->setText(_settings->mediaServerUrl());
    _token->setText(_settings->mediaServerToken());
    _status->setText(_settings->mediaServerStatus());
    showCache();

    _loading = false;
}

// Размер кэша и куда он идёт. Считается при показе страницы, а не постоянно:
// обход папок на телефоне стоит дорого.
void SettingsPage::showCache()
{
    if (!_cache) {
        return;
    }

    QString line = tr("Avatars, pictures, stickers and converted media, %1 in all, "
                      "kept on the %2. Deleting them frees memory; anything still "
                      "needed is fetched again.")
                       .arg(_cache->sizeText()).arg(_cache->placeText());

    if (!CacheManager::cardAvailable()) {
        line += QLatin1Char(' ');
        line += tr("No memory card found, so everything has to stay in phone memory.");
    } else {
        line += QLatin1Char(' ');
        line += tr("Downloaded attachments follow the same choice, and they are the "
                   "biggest of all: nothing ever deletes them. Files already "
                   "downloaded stay where they are.");
    }

    _cacheText->setText(line);

    const bool onCard = CacheManager::place() == QLatin1String("card");
    _cachePlace->setText(onCard ? tr("Move cache to phone memory")
                                : tr("Move cache to memory card"));
    _cachePlace->setEnabled(onCard || CacheManager::cardAvailable());
    _cacheClear->setText(tr("Clear cache now"));
}

void SettingsPage::movePlace()
{
    if (!_cache) {
        return;
    }

    const bool onCard = CacheManager::place() == QLatin1String("card");
    _cache->setPlace(onCard ? QLatin1String("phone") : QLatin1String("card"));
    showCache();
}

void SettingsPage::clearCache()
{
    if (!_cache) {
        return;
    }

    _cache->clear();
    showCache();
    // После showCache(), а не до: она возвращает кнопке обычную подпись, а
    // человеку нужно видеть, что нажатие сработало.
    _cacheClear->setText(tr("Cache cleared"));
}

void SettingsPage::focusFirst()
{
    _markAsRead->setFocus();

#ifdef QT_KEYPAD_NAVIGATION
    _markAsRead->setEditFocus(true);
#endif
}

void SettingsPage::apply()
{
    if (_loading) {
        return;
    }

    _settings->setMarkAsRead(_markAsRead->isChecked());
    _settings->setNotifications(_notifications->isChecked());
    _settings->setVibrate(_vibrate->isChecked());
    _settings->setMediaServer(_mediaServer->isChecked());
    _settings->setMediaServerUrl(_url->text().trimmed());
    _settings->setMediaServerToken(_token->text().trimmed());
}

void SettingsPage::check()
{
    apply();
    _settings->checkMediaServer();
}

void SettingsPage::serverStatus()
{
    _status->setText(_settings->mediaServerStatus());
}
