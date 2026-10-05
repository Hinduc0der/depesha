#include "listview.h"

#include <QAbstractItemModel>
#include <QPainter>

ListView::ListView(QWidget *parent) :
    QListView(parent)
{
    QPalette colors = palette();
    colors.setColor(QPalette::Base, Qt::white);
    colors.setColor(QPalette::Text, Qt::black);
    setPalette(colors);

    // Без этого под строками остаётся картинка темы, и белый фон строки
    // оказывается заплаткой на чужой подложке.
    viewport()->setAutoFillBackground(true);

    setUniformItemSizes(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::SingleSelection);
}

void ListView::setEmptyText(const QString &text)
{
    _emptyText = text;
    viewport()->update();
}

void ListView::paintEvent(QPaintEvent *event)
{
    QListView::paintEvent(event);

    if (_emptyText.isEmpty() || !model() || model()->rowCount() > 0) {
        return;
    }

    QPainter painter(viewport());
    painter.setPen(QColor(128, 128, 128));
    painter.drawText(viewport()->rect(), Qt::AlignCenter, _emptyText);
}
