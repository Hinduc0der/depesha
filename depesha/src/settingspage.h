#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>

class AppSettings;
class CacheManager;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

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
    explicit SettingsPage(AppSettings *settings, CacheManager *cache,
                          QWidget *parent = 0);

    // Поля заполняются из настроек при каждом показе страницы.
    void reload();
    void focusFirst();

private slots:
    void apply();
    void check();
    void serverStatus();
    // Кэш: куда складывать и как убрать. До этого у Депеши не было ни того,
    // ни другого -- только автоподрезка через две минуты после запуска, и
    // память телефона она забивала всё равно.
    void movePlace();
    void clearCache();
    void showCache();

private:
    AppSettings *_settings;
    CacheManager *_cache;
    QLabel *_cacheText;
    QPushButton *_cachePlace;
    QPushButton *_cacheClear;
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
