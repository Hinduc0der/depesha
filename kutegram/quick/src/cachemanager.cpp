#include "cachemanager.h"
#include "kglog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QSettings>
#include <QTime>

#include "tgclient.h"

// Phone memory, not the memory card: a few dozen megabytes free is normal on
// an E90, so the cache has to stay small enough to be forgettable. Overridable
// in the settings file as CacheLimitMb for anyone who keeps everything on a
// card.
#define KG_CACHE_LIMIT_MB 10

static const char *KG_CACHE_DIRS[] = {
    "Kutegram_avatars",
    "Kutegram_photos",
    "Kutegram_stickers",
    "Kutegram_converted",
    0
};

// The same directory the client keeps its session in; settingsFilePath() is
// static, so this works without a TgClient instance.
static QDir cacheBaseDirectory()
{
    return QDir(QFileInfo(TgClient::settingsFilePath("cache")).absolutePath());
}

CacheManager::CacheManager(QObject *parent) :
    QObject(parent)
{
}

QString CacheManager::convertedPath(QString sourcePath, QString extension)
{
    QDir directory = cacheBaseDirectory();
    directory.mkpath("Kutegram_converted");

    return directory.absoluteFilePath("Kutegram_converted/"
                                      + QFileInfo(sourcePath).fileName() + extension);
}

QStringList CacheManager::cacheDirectories() const
{
    QDir base = cacheBaseDirectory();
    QStringList directories;

    for (int i = 0; KG_CACHE_DIRS[i]; ++i) {
        directories.append(base.absoluteFilePath(KG_CACHE_DIRS[i]));
    }

    return directories;
}

qint64 CacheManager::totalSize() const
{
    qint64 total = 0;

    QStringList directories = cacheDirectories();
    for (int i = 0; i < directories.size(); ++i) {
        QFileInfoList files = QDir(directories[i]).entryInfoList(QDir::Files);

        for (int j = 0; j < files.size(); ++j) {
            total += files[j].size();
        }
    }

    return total;
}

qint64 CacheManager::size()
{
    return totalSize();
}

QString CacheManager::sizeText() const
{
    const qint64 bytes = totalSize();

    if (bytes > 1048576) {
        return QString::number((double) bytes / 1048576.0, 'f', 1) + " MB";
    }

    if (bytes > 1024) {
        return QString::number(bytes / 1024) + " KB";
    }

    return QString::number(bytes) + " B";
}

void CacheManager::clear()
{
    qint64 removed = 0;

    QStringList directories = cacheDirectories();
    for (int i = 0; i < directories.size(); ++i) {
        QDir directory(directories[i]);
        QFileInfoList files = directory.entryInfoList(QDir::Files);

        for (int j = 0; j < files.size(); ++j) {
            const qint64 bytes = files[j].size();

            if (QFile::remove(files[j].absoluteFilePath())) {
                removed += bytes;
            }
        }
    }

    kgLog("cache cleared, " + QString::number(removed / 1024) + "k freed");
    emit changed();
}

void CacheManager::trim()
{
    QTime spent;
    spent.start();

    QSettings settings(TgClient::settingsFilePath("settings"), QSettings::IniFormat);
    const qint64 limit = settings.value("CacheLimitMb", KG_CACHE_LIMIT_MB).toLongLong() * 1048576;

    qint64 total = totalSize();
    if (total <= limit) {
        kgLog("cache " + QString::number(total / 1024) + "k, under the limit");
        return;
    }

    // Oldest first. Nothing here is precious: whatever goes is fetched again
    // when it is next looked at.
    QFileInfoList all;
    QStringList directories = cacheDirectories();
    for (int i = 0; i < directories.size(); ++i) {
        all.append(QDir(directories[i]).entryInfoList(QDir::Files, QDir::Time | QDir::Reversed));
    }

    qint64 removed = 0;
    for (int i = 0; i < all.size() && total - removed > limit; ++i) {
        const qint64 bytes = all[i].size();

        if (QFile::remove(all[i].absoluteFilePath())) {
            removed += bytes;
        }
    }

    kgLog("cache was " + QString::number(total / 1024) + "k, freed "
          + QString::number(removed / 1024) + "k in "
          + QString::number(spent.elapsed()) + " ms");
    emit changed();
}
