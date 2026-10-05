#include "filebrowser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QVariantMap>

FileBrowser::FileBrowser(QObject *parent) :
    QObject(parent)
{
}

static QVariantMap entryFor(const QFileInfo &info, QString name)
{
    QVariantMap entry;

    entry["name"] = name.isEmpty() ? info.fileName() : name;
    entry["path"] = info.absoluteFilePath();
    entry["isDir"] = info.isDir();
    entry["size"] = info.isDir() ? (qint64) 0 : info.size();

    return entry;
}

QVariantList FileBrowser::list(QString path)
{
    QVariantList result;

    // Верхний уровень — диски. На телефоне это C: (память аппарата) и E:
    // (карта), и без этого списка до карты было не добраться.
    if (path.isEmpty()) {
        QFileInfoList drives = QDir::drives();

        for (int i = 0; i < drives.size(); ++i) {
            result.append(entryFor(drives[i], drives[i].absoluteFilePath()));
        }

        return result;
    }

    QDir directory(path);
    if (!directory.exists()) {
        return result;
    }

    // Папки сверху, дальше файлы, и то и другое по имени: на списке из сотни
    // файлов порядок важнее скорости.
    QFileInfoList entries = directory.entryInfoList(
                QDir::AllEntries | QDir::NoDotAndDotDot,
                QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    for (int i = 0; i < entries.size(); ++i) {
        result.append(entryFor(entries[i], QString()));
    }

    return result;
}

QString FileBrowser::parentOf(QString path)
{
    if (path.isEmpty()) {
        return QString();
    }

    QDir directory(path);

    // cdUp() у корня диска возвращает false — это и есть сигнал вернуться к
    // списку дисков.
    if (!directory.cdUp()) {
        return QString();
    }

    return directory.absolutePath();
}

// Форматы, которые заведомо являются текстом. Список короткий нарочно: всё,
// чего тут нет, уходит системе, а она сама разберётся -- или честно откажет.
static const char *KG_TEXT_EXTENSIONS[] = {
    ".txt", ".md", ".log", ".json", ".xml", ".csv", ".ini", ".cfg", ".conf",
    ".sh", ".py", ".js", ".html", ".htm", ".css", ".srt", 0
};

// Полмегабайта текста -- это уже больше, чем можно прочитать с экрана 240x320,
// и заметно больше, чем стоит держать в одном элементе QML.
#define KG_TEXT_LIMIT (512 * 1024)

bool FileBrowser::isTextFile(QString path)
{
    const QString lower = path.toLower();

    for (int i = 0; KG_TEXT_EXTENSIONS[i]; ++i) {
        if (lower.endsWith(KG_TEXT_EXTENSIONS[i])) {
            return true;
        }
    }

    return false;
}

QString FileBrowser::readText(QString path)
{
    if (path.startsWith("file:///")) {
        path = path.mid(8);
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QByteArray data = file.read(KG_TEXT_LIMIT);
    const bool truncated = file.size() > KG_TEXT_LIMIT;
    file.close();

    QString text = QString::fromUtf8(data);
    if (truncated) {
        text += "\n\n[...]";
    }

    return text;
}

QString FileBrowser::fileName(QString path)
{
    return QFileInfo(path).fileName();
}

qint64 FileBrowser::fileSize(QString path)
{
    return QFileInfo(path).size();
}
