#include "kglog.h"

#include "tgclient.h"

#include <QDir>
#include <QFile>
#include <QMutex>
#include <QStringList>
#include <QTextStream>
#include <QTime>

#ifdef Q_OS_SYMBIAN
#include <e32std.h>
#include <hal.h>
#endif

// Called from the GUI thread, from the image reader thread and from the
// network thread, so everything below is behind one mutex.
static QMutex kgLogMutex;
static QStringList kgLogBuffer;
static QTime kgLogClock;
static bool kgLogStarted = false;

QString kgLogPath()
{
#ifdef Q_OS_SYMBIAN
    // Phone memory, deliberately, even though the memory card is the half a PC
    // sees over USB. The log was on the card, written once a minute -- and the
    // card started dropping out of the phone during the very crashes it was
    // there to explain, needing a reboot to come back. Poking the card while
    // the system is out of memory is a bad way to investigate being out of
    // memory. The file is tens of kilobytes; copy it to the card with a file
    // manager when it needs to leave the phone.
    // Своя папка на клиента: Депеша и Kutegram стоят на телефоне вместе, и
    // один журнал на двоих читать было нельзя -- строки шли вперемешку, а
    // чьи они, в них не написано.
    const QString app = TgClient::appFolder();
    QDir("C:/Data").mkpath(app);
    return "C:/Data/" + app + "/" + app.toLower() + ".log";
#else
    return QDir::tempPath() + "/kutegram.log";
#endif
}

int kgHeapAvailable()
{
#ifdef Q_OS_SYMBIAN
    TInt biggestBlock = 0;
    return User::Heap().Available(biggestBlock);
#else
    return -1;
#endif
}

int kgFreeRam()
{
#ifdef Q_OS_SYMBIAN
    TInt freeRam = 0;
    if (HAL::Get(HALData::EMemoryRAMFree, freeRam) != KErrNone) {
        return -1;
    }

    return freeRam;
#else
    return -1;
#endif
}

QString kgMemoryInfo()
{
#ifdef Q_OS_SYMBIAN
    TInt biggestBlock = 0;
    // RHeap, not RAllocator: only the heap knows its own size.
    const TInt available = User::Heap().Available(biggestBlock);
    const TInt size = User::Heap().Size();

    TInt freeRam = 0;
    HAL::Get(HALData::EMemoryRAMFree, freeRam);

    return "heap=" + QString::number(size / 1024)
            + "k free=" + QString::number(available / 1024)
            + "k biggest=" + QString::number(biggestBlock / 1024)
            + "k ram=" + QString::number(freeRam / 1024) + "k";
#else
    return QString();
#endif
}

static void kgLogWriteLocked()
{
    if (kgLogBuffer.isEmpty()) {
        return;
    }

    QFile file(kgLogPath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        for (int i = 0; i < kgLogBuffer.size(); ++i) {
            stream << kgLogBuffer[i] << "\n";
        }
        // Closed every time on purpose: a battery pull keeps only what the file
        // server has already taken.
        file.close();
    }

    kgLogBuffer.clear();
}

void kgLog(const QString &line, bool flushNow)
{
    QMutexLocker locker(&kgLogMutex);

    if (!kgLogStarted) {
        kgLogClock.start();
        kgLogStarted = true;
    }

    kgLogBuffer.append(QString::number(kgLogClock.elapsed()) + " " + line);

    if (flushNow || kgLogBuffer.size() >= 20) {
        kgLogWriteLocked();
    }
}

void kgLogFlush()
{
    QMutexLocker locker(&kgLogMutex);
    kgLogWriteLocked();
}
