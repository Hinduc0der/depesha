#ifndef VOICEPLAYER_H
#define VOICEPLAYER_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>

namespace Phonon
{
    class MediaObject;
    class AudioOutput;
}

class QNetworkReply;
class AppSettings;

// Plays Telegram voice messages, which arrive as Opus in an Ogg container --
// a codec from 2012, on a phone whose newest codec is older than that. There
// is no Opus decoder in S60v3 and there is no way to add one cheaply.
//
// The way around it is the way the WhatsApp client on this same phone already
// works: something else does the decoding. The .oga goes to a small service
// (vps-voice-transcode.py) that hands back an mp3, and the phone plays that
// with its own hardware codec through Phonon.
//
// The address lives in the settings file under VoiceTranscoder, so it can be
// pointed somewhere else without rebuilding. Voice messages are private, so it
// should only ever point at a server the user owns.
class VoicePlayer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    // The settings own the address: it can change while the client runs, and
    // an empty one means voice messages simply cannot be played here.
    explicit VoicePlayer(AppSettings *settings, QObject *parent = 0);

    QString status() const;

public slots:
    // Takes the path of the downloaded voice file. Converted files are kept
    // next to it, so the second listen costs nothing.
    void play(QString path);
    void stop();

signals:
    void statusChanged();

private slots:
    void transcodeFinished();
    void mediaStateChanged();

private:
    void playFile(QString path);
    void setStatus(QString status);

    QNetworkAccessManager _network;
    QNetworkReply *_reply;
    QString _pendingPath;
    QString _status;
    AppSettings *_settings;

    Phonon::MediaObject *_media;
    Phonon::AudioOutput *_output;
};

#endif // VOICEPLAYER_H
