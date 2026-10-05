#include "twolinedelegate.h"

#include "kglog.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QPixmapCache>
#include <QTextDocument>

TwoLineDelegate::TwoLineDelegate(QAbstractItemModel *model,
                                 const QByteArray &titleRole,
                                 const QByteArray &subtitleRole,
                                 const QByteArray &sideRole,
                                 const QByteArray &fallbackRole,
                                 QObject *parent) :
    QStyledItemDelegate(parent),
    _titleRole(roleByName(model, titleRole)),
    _subtitleRole(roleByName(model, subtitleRole)),
    _sideRole(roleByName(model, sideRole)),
    _fallbackRole(roleByName(model, fallbackRole)),
    _avatarRole(-1),
    _colorRole(-1),
    _avatarTextRole(-1)
{
}

void TwoLineDelegate::setAvatarRoles(const QAbstractItemModel *model,
                                     const QByteArray &avatarRole,
                                     const QByteArray &colorRole,
                                     const QByteArray &textRole)
{
    _avatarRole = roleByName(model, avatarRole);
    _colorRole = roleByName(model, colorRole);
    _avatarTextRole = roleByName(model, textRole);

    kgLog("avatar roles: avatar=" + QString::number(_avatarRole)
          + " color=" + QString::number(_colorRole)
          + " text=" + QString::number(_avatarTextRole));
}

// Пока аватарка не скачалась (или её нет вовсе) -- кружок с буквами, как в
// самом Telegram. Цвет и буквы считает модель, здесь только заливка.
static void drawInitials(QPainter *painter, const QRect &rect,
                         const QColor &color, const QString &text)
{
    painter->setPen(Qt::NoPen);
    painter->setBrush(color.isValid() ? color : QColor(140, 140, 140));
    painter->drawEllipse(rect);
    painter->setBrush(Qt::NoBrush);

    if (text.isEmpty()) {
        return;
    }

    QFont font = painter->font();
    font.setBold(true);
    font.setPixelSize(qMax(8, rect.height() / 2));

    painter->setFont(font);
    painter->setPen(Qt::white);
    painter->drawText(rect, Qt::AlignCenter, text);
}

int TwoLineDelegate::roleByName(const QAbstractItemModel *model, const QByteArray &name)
{
    if (!model) {
        return -1;
    }

    const QHash<int, QByteArray> roles = model->roleNames();

    for (QHash<int, QByteArray>::const_iterator it = roles.begin(); it != roles.end(); ++it) {
        if (it.value() == name) {
            return it.key();
        }
    }

    return -1;
}

QString TwoLineDelegate::plainText(const QVariant &value)
{
    QString text = value.toString();

    if (text.indexOf('<') < 0) {
        return text.simplified();
    }

    QString result;
    result.reserve(text.size());
    bool inTag = false;

    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);

        if (c == '<') {
            inTag = true;
        } else if (c == '>') {
            inTag = false;
        } else if (!inTag) {
            result += c;
        }
    }

    return result.simplified();
}

// Чёрным по светлому или белым по тёмному -- по тому, что под текстом на
// самом деле. Брать цвет текста из темы нельзя: тема отдаёт его под свою
// картинку-подложку, а рисуем мы по своему фону, и белое по белому
// получалось именно так.
QColor TwoLineDelegate::readableOn(const QColor &background)
{
    return qGray(background.rgb()) > 140 ? QColor(Qt::black) : QColor(Qt::white);
}

void TwoLineDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const
{
    QStyleOptionViewItemV4 opt = option;
    initStyleOption(&opt, index);

    // Фон рисуется своей заливкой, а не стилем телефона: у стиля это картинка
    // со скруглениями и градиентом, и на 332 МГц она стоит заметно дороже
    // прямоугольника.
    const bool selected = (opt.state & QStyle::State_Selected) != 0;
    const QColor background = selected ? opt.palette.color(QPalette::Highlight)
                                       : opt.palette.color(QPalette::Base);
    painter->fillRect(opt.rect, background);

    const QString title = _titleRole >= 0 ? TwoLineDelegate::plainText(index.data(_titleRole)) : QString();
    QString subtitle = _subtitleRole >= 0 ? TwoLineDelegate::plainText(index.data(_subtitleRole)) : QString();
    if (subtitle.isEmpty() && _fallbackRole >= 0) {
        subtitle = TwoLineDelegate::plainText(index.data(_fallbackRole));
    }
    const QString side = _sideRole >= 0 ? TwoLineDelegate::plainText(index.data(_sideRole)) : QString();

    QRect rect = opt.rect.adjusted(4, 2, -4, -2);
    const QFontMetrics metrics(opt.font);
    const int lineHeight = metrics.height();

    const QColor textColor = readableOn(background);

    painter->save();

    if (_avatarRole >= 0) {
        const int size = rect.height();
        const QRect avatarRect(rect.left(), rect.top(), size, size);
        const QString path = index.data(_avatarRole).toString();

        QPixmap avatar;

        if (!path.isEmpty()) {
            const QString key = "kgav" + QString::number(size) + path;

            if (!QPixmapCache::find(key, &avatar)) {
                const QPixmap full(path);

                if (!full.isNull()) {
                    const QPixmap scaled = full.scaled(size, size, Qt::KeepAspectRatio,
                                                       Qt::SmoothTransformation);

                    // Круг рисуется здесь, один раз на аватарку и уже в
                    // размере строки. Раньше его делал загрузчик -- картинкой
                    // 160x160 на каждую скачанную аватарку, в главном потоке,
                    // и на сорока чатах это стоило телефону минуты.
                    avatar = QPixmap(size, size);
                    avatar.fill(Qt::transparent);

                    QPainter shaper(&avatar);
                    shaper.setRenderHint(QPainter::Antialiasing);
                    shaper.setPen(Qt::NoPen);
                    shaper.setBrush(QBrush(scaled));
                    shaper.drawEllipse(0, 0, size, size);
                    shaper.end();

                    QPixmapCache::insert(key, avatar);
                }
            }
        }

        if (avatar.isNull()) {
            painter->setRenderHint(QPainter::Antialiasing);
            drawInitials(painter, avatarRect,
                         _colorRole >= 0 ? qvariant_cast<QColor>(index.data(_colorRole)) : QColor(),
                         _avatarTextRole >= 0 ? index.data(_avatarTextRole).toString() : QString());
            painter->setRenderHint(QPainter::Antialiasing, false);
        } else {
            painter->drawPixmap(avatarRect, avatar);
        }

        rect.setLeft(rect.left() + size + 4);
    }

    painter->setFont(opt.font);
    painter->setPen(textColor);

    int sideWidth = 0;
    if (!side.isEmpty()) {
        sideWidth = metrics.width(side) + 6;
        painter->drawText(QRect(rect.right() - sideWidth + 6, rect.top(), sideWidth, lineHeight),
                          Qt::AlignRight | Qt::AlignVCenter, side);
    }

    QFont titleFont = opt.font;
    titleFont.setBold(true);
    painter->setFont(titleFont);
    painter->drawText(QRect(rect.left(), rect.top(), rect.width() - sideWidth, lineHeight),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      metrics.elidedText(title, Qt::ElideRight, rect.width() - sideWidth));

    if (!subtitle.isEmpty()) {
        QFont subtitleFont = opt.font;
        subtitleFont.setBold(false);
        painter->setFont(subtitleFont);

        // Вторая строка глуше первой, но того же цвета: отдельный цвет из
        // темы снова оказался бы не в тон нашему фону.
        QColor subtitleColor = textColor;
        subtitleColor.setAlpha(150);
        painter->setPen(subtitleColor);

        painter->drawText(QRect(rect.left(), rect.top() + lineHeight, rect.width(), lineHeight),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          metrics.elidedText(subtitle, Qt::ElideRight, rect.width()));
    }

    painter->restore();
}

QSize TwoLineDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index)

    // Две строки текста и немного воздуха. Высота фиксированная: переменная
    // заставила бы список измерять каждую строку, а их могут быть сотни.
    const QFontMetrics metrics(option.font);
    return QSize(option.rect.width(), metrics.height() * 2 + 6);
}
