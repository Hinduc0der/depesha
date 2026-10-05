#include "voiceplayer.h"
#include "appsettings.h"
#include "cachemanager.h"
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
    _status("idle"),
    _settings(settings),
    _media(0),
    _output(0)
{
}

QString VoicePlayer::status() const
{
    return _status;
}

void VoicePlayer::setStatus(QString status)
{
    if (_status == status) {
        return;
    }

    _status = status;
    kgLog("voice: " + status);
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
            || path.endsWith(".amr", Qt::CaseInsensitive)) {
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

    if (_media) {
        _media->stop();
    }

    setStatus("idle");
}
