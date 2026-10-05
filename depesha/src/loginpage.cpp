#include "loginpage.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

LoginPage::LoginPage(QWidget *parent) :
    QWidget(parent),
    _step(Phone)
{
    _title = new QLabel(this);
    _title->setWordWrap(true);

    _hint = new QLabel(this);
    _hint->setWordWrap(true);
    _hint->setEnabled(false);

    _input = new QLineEdit(this);
    _button = new QPushButton(tr("Next"), this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(6);
    layout->addWidget(_title);
    layout->addWidget(_input);
    layout->addWidget(_button);
    layout->addWidget(_hint);
    layout->addStretch();

    connect(_button, SIGNAL(clicked()), this, SLOT(submit()));
    // Джойстик на поле ввода тоже отправляет: тянуться до кнопки на кнопочном
    // телефоне -- лишнее движение.
    connect(_input, SIGNAL(returnPressed()), this, SLOT(submit()));

    setStep(Phone);
}

void LoginPage::setStep(Step step)
{
    _step = step;
    _input->clear();
    _input->setEchoMode(QLineEdit::Normal);
    _hint->clear();

    switch (step) {
    case Phone:
        _title->setText(tr("Phone number with country code"));
        _input->setInputMethodHints(Qt::ImhDialableCharactersOnly);
        break;
    case Code:
        _title->setText(tr("Code from Telegram"));
        _input->setInputMethodHints(Qt::ImhDigitsOnly);
        break;
    case Password:
        _title->setText(tr("Two-step verification password"));
        _input->setInputMethodHints(Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText);
        _input->setEchoMode(QLineEdit::Password);
        break;
    }

    setBusy(false);
    focusInput();
}

void LoginPage::focusInput()
{
    _input->setFocus();

#ifdef QT_KEYPAD_NAVIGATION
    // В режиме клавиатурной навигации виджет сначала просто выделяется, а
    // символы принимает только после нажатия центра. Поле ввода -- ровно то
    // место, где ждать этого нажатия незачем.
    _input->setEditFocus(true);
#endif
}

void LoginPage::setBusy(bool busy, const QString &what)
{
    // Кнопка не выключается, а только меняет надпись: выключенная кнопка на
    // кнопочном телефоне теряет фокус, и экран становится мёртвым -- этим мы
    // уже обжигались в Kutegram.
    _button->setText(busy ? (what.isEmpty() ? tr("Please wait...") : what) : tr("Next"));
    _input->setEnabled(!busy);
}

void LoginPage::setHint(const QString &hint)
{
    _hint->setText(hint);
}

void LoginPage::submit()
{
    const QString value = _input->text().trimmed();

    if (value.isEmpty()) {
        return;
    }

    switch (_step) {
    case Phone:
        emit phoneEntered(value);
        break;
    case Code:
        emit codeEntered(value);
        break;
    case Password:
        emit passwordEntered(value);
        break;
    }
}
