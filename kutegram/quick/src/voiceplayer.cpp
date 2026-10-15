#include "voiceplayer.h"
#include "appsettings.h"
#include "cachemanager.h"
#include "opusvoice.h"
#include "kglog.h"

#include <QFile>
#include <QTimer>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <phonon/audiooutput.h>
#include <phonon/mediaobject.h>

VoicePlayer::VoicePlayer(AppSettings *settings, QObject *parent) :
    QObject(parent),
    _reply(0),
    _decoder(0),
    _tick(0),
    _shownPercent(-1),
    _status("idle"),
    _settings(settings),
    _media(0),
    _output(0)
{
}

VoicePlayer::~VoicePlayer()
{
    // Недосчитанное надо выбросить руками: в деструкторе декодера стоит
    // удаление недописанного WAV.
    delete _decoder;
    _decoder = 0;
}

QString VoicePlayer::status() const
{
    return _status;
}

void VoicePlayer::setStatus(QString status, bool writeToLog)
{
    if (_status == status) {
        return;
    }

    _status = status;

    // Проценты расшифровки в журнал не идут: их десятки, а смысла в них для
    // разбора никакого -- скорость и так пишется отдельной строкой.
    if (writeToLog) {
        kgLog("voice: " + status);
    }

    emit statusChanged();
}

void VoicePlayer::play(QString path)
{
    if (path.startsWith("file:///")) {
        path = path.mid(8);
    }

    if (path.isEmpty() || !QFile::exists(path)) {
        setStatus("no file");
        return;
    }

    stop();

    // Already something the phone can play, or converted once before. The
    // transcoder prefers AMR -- the native format of the S60 voice recorder,
    // and less than half the size of an mp3 over EDGE -- and falls back to mp3
    // where its ffmpeg cannot encode AMR, so both are worth looking for.
    if (path.endsWith(".mp3", Qt::CaseInsensitive)
            || path.endsWith(".amr", Qt::CaseInsensitive)
            // WAV сюда попадает двумя путями: раскодированное нами голосовое
            // и запись с микрофона. И то, и другое телефон играет сам.
            || path.endsWith(".wav", Qt::CaseInsensitive)) {
        playFile(path);
        return;
    }

    if (QFile::exists(CacheManager::convertedPath(path, ".amr"))) {
        playFile(CacheManager::convertedPath(path, ".amr"));
        return;
    }

    if (QFile::exists(CacheManager::convertedPath(path, ".mp3"))) {
        playFile(CacheManager::convertedPath(path, ".mp3"));
        return;
    }

    const QString wav = CacheManager::convertedPath(path, ".wav");
    if (QFile::exists(wav)) {
        playFile(wav);
        return;
    }

    // Своими силами. Получится -- сервер не нужен вовсе; не получится --
    // уйдём на сервер из decodeDone(), и человек этого даже не заметит.
    //
    // Работа режется на куски по таймеру: на аппарате она идёт примерно
    // вчетверо быстрее реального времени, то есть минутное голосовое -- это
    // секунд четырнадцать, и всё это время телефон должен слушаться рук.
    kgLog("voice: decoding locally: " + QFileInfo(path).fileName());
    _pendingPath = path;
    _wavPath = wav;
    _shownPercent = -1;

    _decoder = new KgVoiceDecoder();
    if (!_decoder->begin(path, wav)) {
        delete _decoder;
        _decoder = 0;
        kgLog("voice: cannot start local decoding, asking the server");
        askServer(path);
        return;
    }

    if (!_tick) {
        _tick = new QTimer(this);
        // Десять миллисекунд передышки между кусками: за это время очередь
        // событий успевает разобрать нажатия и перерисовку, а на счёт всё
        // равно уходит почти всё время.
        _tick->setInterval(10);
        connect(_tick, SIGNAL(timeout()), this, SLOT(decodeTick()));
    }
    _tick->start();

    setStatus("decoding");
}

void VoicePlayer::decodeTick()
{
    if (!_decoder) {
        if (_tick) {
            _tick->stop();
        }
        return;
    }

    // Кусок в 120 мс: меньше -- и накладные расходы таймера начинают
    // съедать выигрыш, больше -- и нажатия заметно запаздывают.
    if (_decoder->step(120)) {
        const int percent = _decoder->percent() / 10 * 10;
        if (percent != _shownPercent) {
            _shownPercent = percent;
            setStatus(QString("decoding %1%").arg(percent), false);
        }
        return;
    }

    _tick->stop();

    const bool ok = _decoder->finish();
    const int ms = _decoder->msSpent();
    const QString wav = _wavPath;

    delete _decoder;
    _decoder = 0;

    decodeDone(ok, ms, wav);
}

void VoicePlayer::decodeDone(bool ok, int ms, QString wav)
{
    if (ok) {
        kgLog(QString("voice: decoded locally in %1 ms").arg(ms));
        playFile(wav);
        return;
    }

    // Не вышло: битый файл, незнакомый контейнер или просто не Opus.
    // Сервер остаётся запасным путём -- у кого он есть, тот ничего не теряет.
    kgLog("voice: local decoding failed, asking the server");
    askServer(_pendingPath);
}

void VoicePlayer::askServer(QString path)
{
    const QString transcoderUrl = _settings ? _settings->activeMediaServerUrl() : QString();
    if (transcoderUrl.isEmpty()) {
        setStatus("media server is off");
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setStatus("cannot read");
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QNetworkRequest request((QUrl(transcoderUrl)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");

    const QString token = _settings->mediaServerToken();
    if (!token.isEmpty()) {
        request.setRawHeader("X-Kutegram-Token", token.toUtf8());
    }

    // The extension is decided by what comes back, not before.
    _pendingPath = path;
    _reply = _network.post(request, data);
    // Молчащий сервер не должен оставлять «converting» навсегда -- см.
    // mediaconverter.cpp.
    QTimer::singleShot(45000, _reply, SLOT(abort()));
    connect(_reply, SIGNAL(finished()), this, SLOT(transcodeFinished()));

    setStatus("converting");
}

void VoicePlayer::transcodeFinished()
{
    QNetworkReply *reply = _reply;
    _reply = 0;

    if (!reply) {
        return;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        setStatus("transcoder unreachable");
        return;
    }

    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        setStatus("transcoder returned nothing");
        return;
    }

    const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    // Into the cache, not next to the download: the downloads folder holds
    // what the user asked for, and clearing the cache must not touch it.
    const QString converted = CacheManager::convertedPath(
                _pendingPath, contentType.contains("amr") ? ".amr" : ".mp3");

    QFile file(converted);
    if (!file.open(QIODevice::WriteOnly)) {
        setStatus("cannot save");
        return;
    }

    file.write(data);
    file.close();

    playFile(converted);
}

void VoicePlayer::playFile(QString path)
{
    // Built on first use: on this phone the audio device is worth opening only
    // when something is actually going to be played.
    if (!_media) {
        _media = new Phonon::MediaObject(this);
        _output = new Phonon::AudioOutput(Phonon::MusicCategory, this);
        Phonon::createPath(_media, _output);
        connect(_media, SIGNAL(stateChanged(Phonon::State,Phonon::State)),
                this, SLOT(mediaStateChanged()));
    }

    _media->setCurrentSource(Phonon::MediaSource(path));
    _media->play();

    setStatus("playing");
}

void VoicePlayer::mediaStateChanged()
{
    if (!_media) {
        return;
    }

    switch (_media->state()) {
    case Phonon::PlayingState:
        setStatus("playing");
        break;
    case Phonon::ErrorState:
        setStatus("cannot play");
        break;
    case Phonon::StoppedState:
    case Phonon::PausedState:
        setStatus("idle");
        break;
    default:
        break;
    }
}

void VoicePlayer::stop()
{
    if (_reply) {
        _reply->abort();
        _reply = 0;
    }

    if (_decoder) {
        // Расшифровка идёт кусками, и бросить её можно между любыми двумя:
        // недописанный WAV убирает за собой сам декодер.
        if (_tick) {
            _tick->stop();
        }
        delete _decoder;
        _decoder = 0;
    }

    if (_media) {
        _media->stop();
    }

    setStatus("idle");
}
