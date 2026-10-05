#ifndef TWOLINEDELEGATE_H
#define TWOLINEDELEGATE_H

#include <QAbstractItemModel>
#include <QByteArray>
#include <QColor>
#include <QVariant>
#include <QStyledItemDelegate>

// Строка списка: заголовок, под ним серая вторая строка, справа время.
//
// Ровно то, чего не хватало в Kutegram: там каждая строка переписки была
// деревом из полутора десятков элементов QML со своими привязками и
// анимациями, и на 332 МГц это не рисовалось. Здесь одна отрисовка текста на
// строку, поверх штатного фона выделения от стиля телефона.
class TwoLineDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    // Роли задаются по именам из модели: и DialogsModel, и MessagesModel
    // объявляют их в roleNames(), а номера у них свои.
    TwoLineDelegate(QAbstractItemModel *model,
                    const QByteArray &titleRole,
                    const QByteArray &subtitleRole,
                    const QByteArray &sideRole,
                    // Чем заполнить вторую строку, когда основная роль пуста:
                    // у сообщения с вложением текста нет, и строка выглядела
                    // пустой.
                    const QByteArray &fallbackRole = QByteArray(),
                    QObject *parent = 0);

    // Аватарка слева. Без неё строка рисуется во всю ширину, как раньше.
    //
    // Сами файлы качает AvatarDownloader и кладёт на диск готовыми круглыми
    // PNG 160x160. Здесь они уменьшаются один раз на аватарку и живут в
    // QPixmapCache: 160x160 ARGB -- это 100 КБ, а нужная нам тридцатка -- 4.
    void setAvatarRoles(const QAbstractItemModel *model,
                        const QByteArray &avatarRole,
                        const QByteArray &colorRole,
                        const QByteArray &textRole);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;

    // -1, если такой роли в модели нет: рисуется тогда пустая строка, а не
    // мусор.
    static int roleByName(const QAbstractItemModel *model, const QByteArray &name);

    // Чёрное или белое -- по яркости того, что под текстом. Пригодилось и
    // делегату сообщений, поэтому здесь, а не в безымянном пространстве.
    static QColor readableOn(const QColor &background);

    // Текст моделей местами размечен под HTML. Здесь разметка не нужна и
    // стоила бы дорого -- снимается посимвольно, без разбора документа.
    static QString plainText(const QVariant &value);

protected:
    int _titleRole;
    int _subtitleRole;
    int _sideRole;
    int _fallbackRole;
    int _avatarRole;
    int _colorRole;
    int _avatarTextRole;
};

#endif // TWOLINEDELEGATE_H
