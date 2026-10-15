#include "voicerecorder.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "kglog.h"
#include "opusvoice.h"      // заголовок WAV пишется там же, где и читается

// Те же 16 кГц моно, в которые раскодируются чужие голосовые: кодировщику
// Opus потом нужно ровно это, а большего телефонный микрофон не даст.
static const int KG_REC_RATE = 16000;

#ifdef Q_OS_SYMBIAN
#include <mdaaudioinputstream.h>
#include <mda/common/audio.h>

// Кусок в 4 КБ -- это 128 мс звука. Их два: пока один уходит в файл, второй
// уже набирается, и между ними ничего не теряется.
static const int KG_REC_CHUNK = 4096;

// Щелчки клавиш. Микрофон открывается сразу после нажатия «записать», а
// останавливают запись тоже клавишей -- и оба щелчка слышны. Начало и конец
// просто выбрасываются: 300 мс в начале (туда попадает закрытие меню) и
// 200 мс в конце (там щелчок «стоп», он физически раньше самого события).
static const qint64 KG_REC_SKIP_HEAD = (qint64) 16000 * 2 * 300 / 1000;
static const qint64 KG_REC_SKIP_TAIL = (qint64) 16000 * 2 * 200 / 1000;

// Почему не CMdaAudioRecorderUtility (на нём работает штатный диктофон):
// он тянет из mediaclientaudio.dll номера функций 87-112, а Symbian связывает
// библиотеки ПО НОМЕРАМ, и на телефоне 2007 года они могут значить другое --
// так умерла Jellyfin 0.8 на SetVolumeL. Здесь же вся нужная нам работа
// лежит в номерах 3-15 из крошечной mediaclientaudioinputstream.dll
// (28 функций, и последние шесть дописаны в конец -- значит порядок не
// переставляли). Заодно поток отдаёт сырой PCM прямо в память, а не файл
// неизвестного формата, и это ровно то, что нужно кодировщику.
class KgRecorderWatcher : public MMdaAudioInputStreamCallback
{
public:
    KgRecorderWatcher(KgVoiceRecorder *owner)
        : stream(0), _owner(owner), _which(0) {}

    void MaiscOpenComplete(TInt aError)
    {
        _owner->openComplete(aError);
    }

    void MaiscBufferCopied(TInt aError, const TDesC8 &aBuffer)
    {
        _owner->bufferReady(aError, (const char *) aBuffer.Ptr(), aBuffer.Length());
    }

    void MaiscRecordComplete(TInt aError)
    {
        _owner->recordComplete(aError);
    }

    // Просит следующий кусок, чередуя два буфера.
    void readNext()
    {
        if (!stream) {
            return;
        }

        _which = _which ? 0 : 1;
        buffer[_which].Zero();
        TRAP_IGNORE(stream->ReadL(buffer[_which]));
    }

    CMdaAudioInputStream *stream;
    TBuf8<KG_REC_CHUNK> buffer[2];

private:
    KgVoiceRecorder *_owner;
    int _which;
};
#endif

KgVoiceRecorder::KgVoiceRecorder(QObject *parent) :
    QObject(parent),
    _impl(0),
    _bytes(0),
    _skipped(0),
    _recording(false),
    _opening(false)
{
}

KgVoiceRecorder::~KgVoiceRecorder()
{
    stop();

#ifdef Q_OS_SYMBIAN
    KgRecorderWatcher *watcher = (KgRecorderWatcher *) _impl;
    if (watcher) {
        delete watcher->stream;
        delete watcher;
        _impl = 0;
    }
#endif
}

int KgVoiceRecorder::seconds() const
{
    return (int)(_bytes / (KG_REC_RATE * 2));
}

void KgVoiceRecorder::start(QString path)
{
#ifdef Q_OS_SYMBIAN
    if (_recording || _opening) {
        return;
    }

    _path = path;
    _bytes = 0;
    _skipped = 0;

    _file.setFileName(path);
    if (!_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        kgLog("rec: cannot write " + path);
        emit failed("cannot write file");
        return;
    }
    _file.write(QByteArray(44, '\0'));      // место под заголовок WAV

    KgRecorderWatcher *watcher = (KgRecorderWatcher *) _impl;
    if (!watcher) {
        watcher = new KgRecorderWatcher(this);
        _impl = watcher;
    }

    if (!watcher->stream) {
        // Парные метки: это первый в проекте вызов записи звука, и если
        // телефон встанет, в журнале останется последняя удавшаяся строка.
        kgLog("rec: creating input stream");
        TRAPD(err, watcher->stream = CMdaAudioInputStream::NewL(*watcher));
        if (err != KErrNone || !watcher->stream) {
            kgLog("rec: NewL failed, " + QString::number(err));
            _file.close();
            emit failed("no microphone");
            return;
        }
        kgLog("rec: input stream created");
    }

    _opening = true;

    kgLog("rec: opening stream");
    watcher->stream->Open(NULL);            // ответ придёт в openComplete()
    kgLog("rec: open requested");
#else
    Q_UNUSED(path)
    emit failed("recording is only on the phone");
#endif
}

void KgVoiceRecorder::openComplete(int error)
{
#ifdef Q_OS_SYMBIAN
    kgLog("rec: open complete, " + QString::number(error));

    KgRecorderWatcher *watcher = (KgRecorderWatcher *) _impl;
    if (!watcher || !watcher->stream) {
        return;
    }

    _opening = false;

    if (error != KErrNone) {
        _file.close();
        QFile::remove(_path);
        emit failed("cannot open microphone (" + QString::number(error) + ")");
        return;
    }

    // 16 кГц моно. Не получится -- останется то, что телефон даёт по
    // умолчанию (обычно 8 кГц), и это видно будет по размеру файла.
    TRAPD(e1, watcher->stream->SetAudioPropertiesL(TMdaAudioDataSettings::ESampleRate16000Hz,
                                                   TMdaAudioDataSettings::EChannelsMono));
    kgLog("rec: 16 kHz mono, " + QString::number(e1));

    // Усиление микрофона: начальное не определено.
    watcher->stream->SetGain(watcher->stream->MaxGain());

    _recording = true;
    emit started();

    kgLog("rec: reading");
    watcher->readNext();
#else
    Q_UNUSED(error)
#endif
}

void KgVoiceRecorder::bufferReady(int error, const char *data, int length)
{
#ifdef Q_OS_SYMBIAN
    KgRecorderWatcher *watcher = (KgRecorderWatcher *) _impl;

    if (!_recording) {
        return;         // уже остановились, этот кусок лишний
    }

    if (error != KErrNone && error != KErrCancel) {
        kgLog("rec: buffer error " + QString::number(error));
    }

    if (length > 0 && _file.isOpen()) {
        // Голова: пока не набралось KG_REC_SKIP_HEAD байт, пишем не всё.
        int from = 0;
        if (_skipped < KG_REC_SKIP_HEAD) {
            const qint64 left = KG_REC_SKIP_HEAD - _skipped;
            from = left < length ? (int) left : length;
            _skipped += from;
        }

        if (length > from) {
            _file.write(data + from, length - from);
            _bytes += length - from;
        }
    }

    if (watcher) {
        watcher->readNext();
    }
#else
    Q_UNUSED(error)
    Q_UNUSED(data)
    Q_UNUSED(length)
#endif
}

void KgVoiceRecorder::recordComplete(int error)
{
#ifdef Q_OS_SYMBIAN
    kgLog("rec: record complete, " + QString::number(error));
#else
    Q_UNUSED(error)
#endif
}

void KgVoiceRecorder::stop()
{
#ifdef Q_OS_SYMBIAN
    if (!_recording && !_opening) {
        return;
    }

    KgRecorderWatcher *watcher = (KgRecorderWatcher *) _impl;

    _recording = false;
    _opening = false;

    if (watcher && watcher->stream) {
        kgLog("rec: stopping");
        watcher->stream->Stop();
        kgLog("rec: stopped");
    }

    const int howLong = seconds();
    const qint64 bytes = _bytes;

    if (_file.isOpen()) {
        // Хвост со щелчком «стоп» отрезаем -- но только если после этого
        // что-то останется: короткую запись лучше отдать с щелчком, чем
        // пустой.
        qint64 keep = bytes;
        if (keep > KG_REC_SKIP_TAIL * 2) {
            keep -= KG_REC_SKIP_TAIL;
            _file.resize(44 + keep);
        }

        // Заголовок дописывается в конце: до этого длина неизвестна.
        _file.seek(0);
        _file.write(kgWavHeader(keep / 2, KG_REC_RATE));
        _file.close();
        _bytes = keep;
    }

    kgLog("rec: " + QString::number(howLong) + " s, " + QString::number((int) bytes) + " bytes");

    if (bytes <= 0) {
        QFile::remove(_path);
        emit finished(QString(), 0, 0);
        return;
    }

    emit finished(_path, howLong, (int) bytes);
#endif
}
