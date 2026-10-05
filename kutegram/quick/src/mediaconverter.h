#ifndef MEDIACONVERTER_H
#define MEDIACONVERTER_H

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class QNetworkReply;
class AppSettings;

// Turns what the phone cannot draw into something it can.
//
// A round video note, a video sticker and a GIF are all video that no codec in
// S60v3 can decode. The media server converts them into an animated GIF, and
// an animated GIF is something Qt draws by itself -- so the result goes back
// into the message list rather than into a separate player.
//
// The audio half of the same idea lives in voiceplayer.cpp: it also has to own
// the Phonon pipeline, and that path is already working, so it is left alone.
class MediaConverter : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit MediaConverter(AppSettings *settings, QObject *parent = 0);

    QString status() const;

public slots:
    // index identifies the row to hand the result back to; kind is what the
    // server should produce ("animation", "image", "video").
    void convert(int index, QString path, QString kind);

signals:
    void converted(int index, QString path);
    void statusChanged();

private slots:
    void requestFinished();

private:
    void setStatus(QString status);

    QNetworkAccessManager _network;
    // Several rows can be converting at once, and each reply has to find its
    // way back to the row that asked.
    QHash<QNetworkReply*, int> _rows;
    QHash<QNetworkReply*, QString> _paths;
    QString _status;
    AppSettings *_settings;
};

#endif // MEDIACONVERTER_H
