#ifndef LISTVIEW_H
#define LISTVIEW_H

#include <QListView>

// Список с гарантированно читаемыми цветами и подписью на пустом месте.
//
// Тема телефона отдаёт палитру под свою картинку-подложку: фон светлый, а
// QPalette::Text при этом белый. Штатный стиль рисует текст сам и попадает в
// цвет, но нам он не подходит по скорости -- рисуем мы. Поэтому цвета здесь
// задаются явно, а не берутся у темы.
class ListView : public QListView
{
    Q_OBJECT

public:
    explicit ListView(QWidget *parent = 0);

    // Что написать, когда строк нет. Иначе пустой список неотличим от
    // сломанного.
    void setEmptyText(const QString &text);

protected:
    void paintEvent(QPaintEvent *event);

private:
    QString _emptyText;
};

#endif // LISTVIEW_H
