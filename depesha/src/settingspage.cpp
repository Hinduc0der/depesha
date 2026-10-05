#include "settingspage.h"

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "appsettings.h"

SettingsPage::SettingsPage(AppSettings *settings, QWidget *parent) :
    QWidget(parent),
    _settings(settings),
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

    _loading = false;
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
