#ifndef CACHEMANAGER_H
#define CACHEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

// Looks after everything the client has written to the phone and can write
// again: avatars, photo thumbnails, stickers, and the files the media server
// converted. All of it is disposable -- delete any of it and it comes back the
// next time it is needed.
//
// Somebody has to say so, though. Nothing here was ever deleted before: on a
// phone with a few dozen megabytes of free memory, weeks of chatting fill it up
// and the only cure was to reinstall.
class CacheManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString sizeText READ sizeText NOTIFY changed)

public:
    explicit CacheManager(QObject *parent = 0);

    // Where a converted file belongs: in the cache, not next to the download.
    // The downloads folder holds what the user asked for, and nothing else.
    static QString convertedPath(QString sourcePath, QString extension);

    QString sizeText() const;

public slots:
    qint64 size();
    void clear();

    // Deletes the oldest files until the cache is under its limit. Cheap
    // enough to run at every start, which is when it runs.
    void trim();

signals:
    void changed();

private:
    QStringList cacheDirectories() const;
    qint64 totalSize() const;
};

#endif // CACHEMANAGER_H
