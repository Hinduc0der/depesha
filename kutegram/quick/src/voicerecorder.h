#ifndef VOICERECORDER_H
#define VOICERECORDER_H

#include <QFile>
#include <QObject>
#include <QString>

// Запись с микрофона. Нужна ради одного: отправлять голосовые, а не только
// слушать их.
//
// Phonon писать не умеет вовсе, поэтому здесь симбиановский
// CMdaAudioInputStream -- сырой поток с микрофона. Он отдаёт PCM прямо в
// память, и мы складываем его в WAV 16 кГц моно: это и Phonon играет сам
// (значит записанное можно сразу послушать), и кодировщику Opus потом нужно
// ровно это.
//
// Почему не CMdaAudioRecorderUtility, на котором работает штатный диктофон:
// он требует из mediaclientaudio.dll номера функций 87-112, а Symbian
// связывает библиотеки ПО НОМЕРАМ, и на телефоне 2007 года они могут
// означать другое -- так умерла Jellyfin 0.8 на SetVolumeL. Здесь же всё
// нужное лежит в номерах 3-15 крошечной mediaclientaudioinputstream.dll.
//
// ОСТОРОЖНО: это первый в проекте вызов записи звука, поэтому каждый шаг
// обрамлён записями в журнал.
class KgVoiceRecorder : public QObject
{
    Q_OBJECT

public:
    explicit KgVoiceRecorder(QObject *parent = 0);
    ~KgVoiceRecorder();

    bool isRecording() const { return _recording; }
    QString filePath() const { return _path; }

    // Сколько секунд уже записано (считается по байтам, а не по часам).
    int seconds() const;

public slots:
    // path -- куда писать, с расширением .wav.
    void start(QString path);
    void stop();

signals:
    // Микрофон открылся, пошла запись.
    void started();
    // Запись окончена. path пуст, если ничего не вышло.
    void finished(QString path, int seconds, int bytes);
    void failed(QString why);

public:
    // Зовутся из симбиановского наблюдателя, снаружи трогать не надо.
    void openComplete(int error);
    void bufferReady(int error, const char *data, int length);
    void recordComplete(int error);

private:
    void *_impl;            // поток и наблюдатель
    QString _path;
    QFile _file;
    qint64 _bytes;
    qint64 _skipped;        // сколько выброшено с начала (щелчок клавиши)
    bool _recording;
    bool _opening;
};

#endif // VOICERECORDER_H
