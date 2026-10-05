#ifndef IMAGEPAGE_H
#define IMAGEPAGE_H

#include <QPixmap>
#include <QString>
#include <QWidget>

// Просмотр одной картинки во весь экран: фотография или стикер из переписки.
//
// Нажатие центра на строке с картинкой раньше не делало ничего -- картинка
// была видна миниатюрой, и всё. Здесь она открывается целиком; на внутреннем
// экране это 800 точек по ширине, ради чего файл и качается.
class ImagePage : public QWidget
{
    Q_OBJECT

public:
    explicit ImagePage(QWidget *parent = 0);

    // false, если файл не читается как картинка: тогда страницу показывать
    // незачем.
    bool showFile(const QString &path);

protected:
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event);

private:
    void rescale();

    QString _path;
    QPixmap _scaled;
};

#endif // IMAGEPAGE_H
