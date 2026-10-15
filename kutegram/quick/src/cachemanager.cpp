#include "cachemanager.h"
#include "kglog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QMutex>
#include <QMutexLocker>
#include <QSettings>
#include <QTime>
#include <QTimer>

#if QT_VERSION >= 0x050000
#include <QStandardPaths>
#endif

#include "tgclient.h"

// Two limits, because there are two places the cache can live. In phone
// memory a few dozen megabytes free is normal on an E90, so the cache has to
// stay small enough to be forgettable; on a card there are gigabytes and
// re-fetching a picture costs more than keeping it. Either can be overridden
// in the settings file as CacheLimitMb.
#define KG_CACHE_LIMIT_MB 10
#define KG_CACHE_LIMIT_CARD_MB 60

// Подпапки кэша. Приставка -- имя клиента: папки теперь и так разные, но
// имя внутри избавляет от вопроса «чей это каталог», если файлы однажды
// окажутся рядом.
static QStringList kgCacheNames()
{
    const QString app = TgClient::appFolder();

    QStringList dirs;
    dirs << app + "_avatars"
         << app + "_photos"
         << app + "_stickers"
         << app + "_converted";
    return dirs;
}

// How often the cache is cut back to its limit while the application runs.
//
// It used to happen once, twenty seconds after start, and that left the whole
// session unbounded: the log off the phone shows the cache reaching 18-22 MB
// against a limit of 10. Ten minutes apart each pass has little to delete, so
// each pass is quick -- the slow one was the first, with megabytes to remove.
#define KG_TRIM_EVERY_MS (10 * 60 * 1000)

static const char *KG_PLACE_CARD = "card";
static const char *KG_PLACE_PHONE = "phone";
static const char *KG_PLACE_AUTO = "auto";

#ifdef Q_OS_SYMBIAN
// Карта, как её называет Symbian. Папка -- по имени клиента: до 15.10.2026
// Депеша и Kutegram делили и её, и папку в памяти телефона, так что чистка
// одного уносила картинки другого.
#define KG_CARD_DOWNLOADS "E:/Downloads"

static QString kgCardRoot()
{
    return "E:/Data/" + TgClient::appFolder();
}
#endif

// Settings are read on another thread as well (avatars and pictures are
// fetched off the GUI thread), so the answer is worked out once and kept.
static QMutex kgPlaceMutex;
static QString kgPlaceNow;
// Своя засада на проверку карты. Её спрашивают и из потока картинок, и из
// настроек, а проверка -- это запись пробного файла: столкнись два потока на
// нём, и одному открыть не удастся. Ответ был бы «карты нет», то есть кэш
// снова в памяти телефона -- ровно то, от чего мы уходим.
static QMutex kgCardMutex;

// Phone memory: the directory the client keeps its session in.
// settingsFilePath() is static, so this works without a TgClient instance.
static QString kgPhoneRoot()
{
    return QFileInfo(TgClient::settingsFilePath("cache")).absolutePath();
}

bool CacheManager::cardAvailable()
{
#ifdef Q_OS_SYMBIAN
    QMutexLocker lock(&kgCardMutex);

    static int answer = -1;
    if (answer >= 0) {
        return answer == 1;
    }

    answer = 0;
    if (QDir("E:/").exists()) {
        QDir("E:/").mkpath("Data/" + TgClient::appFolder());

        // Written, not merely looked at: an empty slot still answers, and a
        // card can be there and refuse to be written to.
        QFile probe(kgCardRoot() + "/probe.tmp");
        if (probe.open(QIODevice::WriteOnly)) {
            probe.write("1");
            probe.close();
            probe.remove();
            answer = 1;
        }
    }

    return answer == 1;
#else
    return false;
#endif
}

QString CacheManager::place()
{
    QMutexLocker lock(&kgPlaceMutex);

    if (!kgPlaceNow.isEmpty()) {
        return kgPlaceNow;
    }

    QSettings settings(TgClient::settingsFilePath("settings"), QSettings::IniFormat);
    const QString want = settings.value("CacheDrive", KG_PLACE_AUTO).toString();

    if (want == KG_PLACE_PHONE) {
        kgPlaceNow = KG_PLACE_PHONE;
    } else if (want == KG_PLACE_CARD && CacheManager::cardAvailable()) {
        kgPlaceNow = KG_PLACE_CARD;
    } else {
        // Default, and the point of the whole thing: the card if there is
        // one. Phone memory has a few dozen megabytes for everything on it,
        // and weeks of chatting used to fill it up with pictures nobody will
        // look at again.
        kgPlaceNow = CacheManager::cardAvailable() ? KG_PLACE_CARD : KG_PLACE_PHONE;
    }

    return kgPlaceNow;
}

QString CacheManager::storageRoot()
{
#ifdef Q_OS_SYMBIAN
    if (place() == KG_PLACE_CARD) {
        return kgCardRoot();
    }
#endif
    return kgPhoneRoot();
}

QString CacheManager::otherRoot()
{
#ifdef Q_OS_SYMBIAN
    if (place() == KG_PLACE_CARD) {
        return kgPhoneRoot();
    }

    return cardAvailable() ? kgCardRoot() : QString();
#else
    return QString();
#endif
}

QDir CacheManager::baseDirectory()
{
    QDir directory(storageRoot());
    directory.mkpath(".");
    return directory;
}

QDir CacheManager::downloadDirectory()
{
    QString root;

#ifdef Q_OS_SYMBIAN
    if (place() == KG_PLACE_CARD) {
        root = KG_CARD_DOWNLOADS;
    }
#endif

    if (root.isEmpty()) {
#if QT_VERSION < 0x050000
        root = QDir::homePath() + "/Downloads";
#else
        root = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
#endif
    }

    // Папка загрузок -- тоже своя: это файлы человека, и путать, из какого
    // клиента что скачано, незачем. Скачанное раньше остаётся там, где
    // лежало, -- чужие файлы мы не двигаем.
    QDir directory(root);
    directory.mkpath(TgClient::appFolder());
    return QDir(directory.absoluteFilePath(TgClient::appFolder()));
}

QString CacheManager::placeText() const
{
    return place() == KG_PLACE_CARD ? QString("memory card") : QString("phone memory");
}

CacheManager::CacheManager(QObject *parent) :
    QObject(parent)
{
    // Trimming on a timer, not only at start. See KG_TRIM_EVERY_MS.
    QTimer *clock = new QTimer(this);
    clock->setInterval(KG_TRIM_EVERY_MS);
    connect(clock, SIGNAL(timeout()), this, SLOT(trim()));
    clock->start();
}

void CacheManager::setPlace(QString where)
{
    if (where != KG_PLACE_CARD && where != KG_PLACE_PHONE && where != KG_PLACE_AUTO) {
        return;
    }

    if (where == KG_PLACE_CARD && !cardAvailable()) {
        kgLog("no memory card to move the cache to");
        emit changed();
        return;
    }

    QSettings settings(TgClient::settingsFilePath("settings"), QSettings::IniFormat);
    settings.setValue("CacheDrive", where);
    settings.sync();

    {
        QMutexLocker lock(&kgPlaceMutex);
        kgPlaceNow.clear();
    }

    kgLog("cache goes to " + place() + " from now on");

    // The files left behind are disposable, and leaving them would defeat the
    // move: nothing else would ever look at them again.
    clearOtherPlace();
    emit changed();
}

QString CacheManager::convertedPath(QString sourcePath, QString extension)
{
    QDir directory = baseDirectory();
    const QString converted = TgClient::appFolder() + "_converted";
    directory.mkpath(converted);

    return directory.absoluteFilePath(converted + "/"
                                      + QFileInfo(sourcePath).fileName() + extension);
}

QStringList CacheManager::cacheDirectoriesIn(QString root)
{
    QStringList directories;

    if (root.isEmpty()) {
        return directories;
    }

    QDir base(root);
    const QStringList names = kgCacheNames();
    for (int i = 0; i < names.size(); ++i) {
        directories.append(base.absoluteFilePath(names[i]));
    }

    return directories;
}

QStringList CacheManager::cacheDirectories() const
{
    return cacheDirectoriesIn(storageRoot());
}

// Only the four disposable directories, never the root itself: the session
// and the settings files live in the root, and deleting those means signing
// in by SMS again.
void CacheManager::clearOtherPlace()
{
    const QString root = otherRoot();
    if (root.isEmpty()) {
        return;
    }

    qint64 removed = 0;
    QStringList directories = cacheDirectoriesIn(root);

    for (int i = 0; i < directories.size(); ++i) {
        QDir directory(directories[i]);
        QFileInfoList files = directory.entryInfoList(QDir::Files);

        for (int j = 0; j < files.size(); ++j) {
            const qint64 bytes = files[j].size();

            if (QFile::remove(files[j].absoluteFilePath())) {
                removed += bytes;
            }
        }

        directory.rmdir(directories[i]);
    }

    if (removed > 0) {
        kgLog("freed " + QString::number(removed / 1024) + "k: cache left over in "
              + root);
        emit changed();
    }
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
    const int byDefault = place() == KG_PLACE_CARD ? KG_CACHE_LIMIT_CARD_MB
                                                   : KG_CACHE_LIMIT_MB;
    const qint64 limit = settings.value("CacheLimitMb", byDefault).toLongLong() * 1048576;

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
