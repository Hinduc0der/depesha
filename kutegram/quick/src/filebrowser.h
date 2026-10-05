#ifndef FILEBROWSER_H
#define FILEBROWSER_H

#include <QObject>
#include <QString>
#include <QVariantList>

// Даёт QML список дисков и файлов, чтобы выбор файла можно было нарисовать
// самим.
//
// Раньше тут был QFileDialog. Он виджетный, и это оказалось тремя бедами
// сразу: по нему нельзя ходить джойстиком (ради QML в приложении выключена
// клавиатурная навигация по виджетам), в нём не переключиться на карту памяти,
// а его exec() запускает вложенный цикл событий прямо посреди обработчика
// сигнала QML — на телефоне это заканчивалось вылетом.
class FileBrowser : public QObject
{
    Q_OBJECT

public:
    explicit FileBrowser(QObject *parent = 0);

public slots:
    // Пустой путь — список дисков. Иначе содержимое каталога: сначала папки,
    // потом файлы, у каждой записи name, path, isDir и size.
    QVariantList list(QString path);

    // Родительский каталог или пустая строка, если выше только список дисков.
    QString parentOf(QString path);

    QString fileName(QString path);

    // Текст ли это -- по расширению. Нужно, чтобы решить, показывать файл
    // самим или отдавать системе.
    bool isTextFile(QString path);

    // Содержимое текстового файла, обрезанное до разумного размера: на этом
    // телефоне мегабайтный текст в одном элементе не нарисуется.
    QString readText(QString path);
    qint64 fileSize(QString path);
};

#endif // FILEBROWSER_H
