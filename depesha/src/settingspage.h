#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>

class AppSettings;
class QCheckBox;
class QLabel;
class QLineEdit;

// Настройки: то немногое, что можно включить и выключить.
//
// Медиа-сервер здесь потому, что без него телефон не сыграет голосовое и не
// покажет ни кружок, ни гифку: Opus, VP9 и Lottie он не декодирует и никогда
// не научится. Адрес пустой -- значит этих вещей просто нет, а текст и
// картинки остаются на месте.
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(AppSettings *settings, QWidget *parent = 0);

    // Поля заполняются из настроек при каждом показе страницы.
    void reload();
    void focusFirst();

private slots:
    void apply();
    void check();
    void serverStatus();

private:
    AppSettings *_settings;
    QCheckBox *_markAsRead;
    QCheckBox *_notifications;
    QCheckBox *_vibrate;
    QCheckBox *_mediaServer;
    QLineEdit *_url;
    QLineEdit *_token;
    QLabel *_status;
    bool _loading;
};

#endif // SETTINGSPAGE_H
