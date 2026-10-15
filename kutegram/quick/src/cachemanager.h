#ifndef CACHEMANAGER_H
#define CACHEMANAGER_H

#include <QDir>
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
    // Where the disposable files go, and a line about it for the settings
    // page: "memory card" or "phone memory".
    Q_PROPERTY(QString place READ place NOTIFY changed)
    Q_PROPERTY(QString placeText READ placeText NOTIFY changed)
    Q_PROPERTY(bool cardAvailable READ cardAvailable NOTIFY changed)

public:
    explicit CacheManager(QObject *parent = 0);

    // Where a converted file belongs: in the cache, not next to the download.
    // The downloads folder holds what the user asked for, and nothing else.
    static QString convertedPath(QString sourcePath, QString extension);

    // The one place that answers "where do disposable files live".
    //
    // Everything that can be fetched again -- avatars, pictures, stickers,
    // converted media -- hangs off this, so moving it moves all of it at
    // once. The session and the settings deliberately do NOT: they are small,
    // they must survive a card being pulled out, and losing them means
    // signing in by SMS again.
    static QDir baseDirectory();
    static QString storageRoot();
    // The same, for whichever place is not in use -- so leftovers there can be
    // swept up instead of sitting in phone memory forever.
    static QString otherRoot();

    // Where a file the user asked for is saved. Follows the same choice: the
    // downloads folder is the biggest eater of all, because nothing ever
    // deletes from it.
    static QDir downloadDirectory();

    // Is there a memory card, and can we write to it? Answered by actually
    // writing: an empty card slot still answers, and read-only cards exist.
    static bool cardAvailable();

    // "card" or "phone" -- where things are going right now.
    static QString place();
    QString placeText() const;

    QString sizeText() const;

public slots:
    qint64 size();
    void clear();

    // "card", "phone" or "auto". Switching does not carry the old files
    // over: they are disposable, so the old place is swept instead. Copying
    // ten megabytes on a 332 MHz phone would take longer than fetching the
    // handful that are actually looked at again.
    void setPlace(QString where);

    // Sweeps the cache in the place that is NOT in use. Called at start, and
    // it is what frees phone memory after the move to the card.
    void clearOtherPlace();

    // Deletes the oldest files until the cache is under its limit. Cheap
    // enough to run at every start, which is when it runs -- and from now on
    // every ten minutes as well, because inside one long session nothing used
    // to stop the cache from growing.
    void trim();

signals:
    void changed();

private:
    QStringList cacheDirectories() const;
    static QStringList cacheDirectoriesIn(QString root);
    qint64 totalSize() const;
};

#endif // CACHEMANAGER_H
