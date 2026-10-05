#ifndef LOGINPAGE_H
#define LOGINPAGE_H

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

// Вход: номер, код, при необходимости пароль. Три шага на одном экране -- на
// 240 пикселях больше и не нужно, а переключать страницы ради двух полей
// незачем.
class LoginPage : public QWidget
{
    Q_OBJECT

public:
    enum Step {
        Phone,
        Code,
        Password
    };

    explicit LoginPage(QWidget *parent = 0);

    void setStep(Step step);
    // Фокус в поле ввода вместе с edit focus: без него на кнопочном телефоне
    // поле выделено, но не принимает символы.
    void focusInput();
    void setBusy(bool busy, const QString &what = QString());
    void setHint(const QString &hint);

signals:
    void phoneEntered(QString phone);
    void codeEntered(QString code);
    void passwordEntered(QString password);

private slots:
    void submit();

private:
    QLabel *_title;
    QLabel *_hint;
    QLineEdit *_input;
    QPushButton *_button;
    Step _step;
};

#endif // LOGINPAGE_H
