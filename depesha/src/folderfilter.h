#ifndef FOLDERFILTER_H
#define FOLDERFILTER_H

#include <QSortFilterProxyModel>

class DialogsModel;

// Папки Telegram поверх списка чатов.
//
// Сами папки считает DialogsModel: у каждой строки есть список номеров папок,
// в которые она попала. Здесь только отбор -- строки не той папки в список не
// попадают вовсе, а не прячутся после отрисовки.
//
// Роли прокси не пересылает (в Qt 4 roleNames() у него свой), поэтому делегату
// и коду открытия чата отдаётся исходная модель: номера ролей у них общие.
class FolderFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    FolderFilter(DialogsModel *dialogs, QObject *parent = 0);

    // -1 -- все чаты.
    void setFolderIndex(int index);
    int folderIndex() const;

public slots:
    // Список папок пришёл или изменился: отбор надо пересчитать целиком.
    void refilter();

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const;

private:
    DialogsModel *_dialogs;
    int _folderIndex;
};

#endif // FOLDERFILTER_H
