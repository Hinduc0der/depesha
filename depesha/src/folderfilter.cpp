#include "folderfilter.h"

#include "dialogsmodel.h"

FolderFilter::FolderFilter(DialogsModel *dialogs, QObject *parent) :
    QSortFilterProxyModel(parent),
    _dialogs(dialogs),
    _folderIndex(-1)
{
    setSourceModel(dialogs);
}

void FolderFilter::setFolderIndex(int index)
{
    if (_folderIndex == index) {
        return;
    }

    _folderIndex = index;
    invalidateFilter();
}

int FolderFilter::folderIndex() const
{
    return _folderIndex;
}

void FolderFilter::refilter()
{
    invalidateFilter();
}

bool FolderFilter::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    if (_folderIndex < 0 || !_dialogs) {
        return true;
    }

    // inFolder берёт мьютекс модели и потому не const. Здесь это чтение.
    return const_cast<DialogsModel*>(_dialogs)->inFolder(row, _folderIndex);
}
