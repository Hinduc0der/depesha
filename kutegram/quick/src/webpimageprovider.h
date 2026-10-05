#ifndef WEBPIMAGEPROVIDER_H
#define WEBPIMAGEPROVIDER_H

#include <QtGlobal>

#if QT_VERSION >= 0x050000
#include <QQuickImageProvider>
#else
#include <QDeclarativeImageProvider>
#endif

// Draws the WebP files that Qt 4.7 cannot read on its own -- which is every
// ordinary Telegram sticker. Registered under the "webp" scheme, so QML asks
// for one as image://webp/<percent-encoded path>.
//
// Decoding happens on Qt's image-loading thread whenever the Image is
// asynchronous, and straight into the size the item will be drawn at: a
// 512x512 sticker scaled afterwards would cost this phone both the memory and
// the time twice over.
#if QT_VERSION >= 0x050000
class WebpImageProvider : public QQuickImageProvider
#else
class WebpImageProvider : public QDeclarativeImageProvider
#endif
{
public:
    WebpImageProvider();

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize);
};

#endif // WEBPIMAGEPROVIDER_H
