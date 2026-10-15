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
class QTimer;
class AppSettings;
class KgVoiceDecoder;

// Plays Telegram voice messages, which arrive as Opus in an Ogg container --
// a codec from 2012, on a phone whose newest codec is older than that. S60v3
// has no Opus decoder of its own.
//
// Сначала пробуем раскодировать САМИ: в проект собран целочисленный декодер
// libopus (см. opusvoice.h), и телефону достаётся обычный WAV, который Phonon
// играет без чужой помощи. Это главное: без сервера-прокладки человек раньше
// не мог слушать голосовые вообще, а поднимать VPS ради них готов не каждый.
//
// Считается это не мгновенно -- на аппарате примерно вчетверо быстрее
// реального времени, то есть минутное голосовое это секунд четырнадцать.
// Поэтому работа идёт кусками по таймеру, между ними телефон живёт обычной
// жизнью. Отдельный поток был бы уместнее, но QThread на этом телефоне не
// стартует вовсе (проверено сборкой 0.4.3).
//
// Если своими силами не вышло, остаётся прежний путь: отдать .oga небольшой
// службе (kg-media), получить mp3/amr и играть его. Адрес службы живёт в
// настройках, так что его можно сменить без пересборки; голосовые -- дело
// частное, и указывать там стоит только свой сервер.
class VoicePlayer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    // The settings own the address: it can change while the client runs, and
    // an empty one means voice messages simply cannot be played here.
    explicit VoicePlayer(AppSettings *settings, QObject *parent = 0);
    ~VoicePlayer();

    QString status() const;

public slots:
    // Takes the path of the downloaded voice file. Converted files are kept
    // next to it, so the second listen costs nothing.
    void play(QString path);
    void stop();

signals:
    void statusChanged();

private slots:
    void decodeTick();
    void transcodeFinished();
    void mediaStateChanged();

private:
    void playFile(QString path);
    // Один и тот же разбор итога -- и когда получилось, и когда нет.
    void decodeDone(bool ok, int ms, QString wav);
    void askServer(QString path);
    void setStatus(QString status, bool writeToLog = true);

    QNetworkAccessManager _network;
    QNetworkReply *_reply;
    KgVoiceDecoder *_decoder;
    QTimer *_tick;
    QString _wavPath;
    int _shownPercent;
    QString _pendingPath;
    QString _status;
    AppSettings *_settings;

    Phonon::MediaObject *_media;
    Phonon::AudioOutput *_output;
};

#endif // VOICEPLAYER_H
