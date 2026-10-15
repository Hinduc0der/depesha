#include "webpimageprovider.h"

#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QUrl>

#include "kglog.h"

#include "src/webp/decode.h"

// Nothing on this screen is drawn larger than this, and a sticker that arrives
// at 512x512 would otherwise be decoded at full size before being shrunk.
static const int KG_WEBP_MAX_SIDE = 192;

// A sticker is tens of kilobytes; anything far past that is not one, and
// decoding it would be a way to run a phone out of memory.
static const int KG_WEBP_MAX_BYTES = 4 * 1024 * 1024;

// The decoder needs room for the full-size intermediate image, not just for the
// scaled result: a 512x512 sticker is a megabyte before it is shrunk.
//
// Measured against free SYSTEM memory, not against the free space inside the
// process heap. That was the earlier mistake: the heap grows on demand, so the
// room left inside it is a few dozen kilobytes at any moment -- the check never
// passed, and every sticker was silently refused. Stickers last worked in
// 1.5.0, the version before this guard was added.
static const int KG_WEBP_MIN_FREE_RAM = 4 * 1024 * 1024;

WebpImageProvider::WebpImageProvider()
#if QT_VERSION >= 0x050000
    : QQuickImageProvider(QQuickImageProvider::Image)
#else
    : QDeclarativeImageProvider(QDeclarativeImageProvider::Image)
#endif
{
}

QImage WebpImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QFile file(QUrl::fromPercentEncoding(id.toUtf8()));
    if (!file.open(QIODevice::ReadOnly)) {
        return QImage();
    }

    if (file.size() > KG_WEBP_MAX_BYTES) {
        file.close();
        return QImage();
    }

    QByteArray data = file.readAll();
    file.close();

    int width = 0;
    int height = 0;
    if (!WebPGetInfo((const quint8*) data.constData(), data.size(), &width, &height)
            || width <= 0 || height <= 0) {
        // Не WebP. Сюда попадает миниатюра анимированного стикера: без
        // медиа-сервера вместо анимации показывается она, а приходит она
        // такой, какой её отдал Telegram, -- бывает и JPEG. Читает его сам
        // Qt, уже уменьшенным.
        QBuffer buffer(&data);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);

        const QSize full = reader.size();
        const int side = requestedSize.width() > 0 || requestedSize.height() > 0
                ? qMin(KG_WEBP_MAX_SIDE, qMax(requestedSize.width(), requestedSize.height()))
                : KG_WEBP_MAX_SIDE;

        if (full.isValid() && (full.width() > side || full.height() > side)) {
            QSize scaled = full;
            scaled.scale(side, side, Qt::KeepAspectRatio);
            reader.setScaledSize(scaled);
        }

        const QImage plain = reader.read();
        if (!plain.isNull() && size) {
            *size = plain.size();
        }
        return plain;
    }

    int maxSide = KG_WEBP_MAX_SIDE;
    if (requestedSize.width() > 0 || requestedSize.height() > 0) {
        maxSide = qMin(maxSide, qMax(requestedSize.width(), requestedSize.height()));
    }

    int targetWidth = width;
    int targetHeight = height;

    if (qMax(width, height) > maxSide) {
        if (width >= height) {
            targetWidth = maxSide;
            targetHeight = qMax(1, height * maxSide / width);
        } else {
            targetHeight = maxSide;
            targetWidth = qMax(1, width * maxSide / height);
        }
    }

    // Bracketing the decode: if the phone dies here, the log stops after the
    // "begin" line and never reaches "done".
    kgLog("webp decode begin " + QString::number(width) + "x" + QString::number(height)
          + " -> " + QString::number(targetWidth) + "x" + QString::number(targetHeight)
          + " bytes=" + QString::number(data.size()) + " " + kgMemoryInfo());

    // An allocation that fails inside the decoder would take the whole
    // application with it, and on this phone the whole phone with it. A sticker
    // is worth less than that: skip it and leave the room to whoever needs it.
    const int freeRam = kgFreeRam();
    if (freeRam > 0 && freeRam < KG_WEBP_MIN_FREE_RAM) {
        kgLog("webp decode skipped, only " + QString::number(freeRam / 1024)
              + "k of system memory left");
        return QImage();
    }

    // Premultiplied, because that is the format Qt can blit without converting
    // every frame.
    QImage image(targetWidth, targetHeight, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        return QImage();
    }

    WebPDecoderConfig config;
    if (!WebPInitDecoderConfig(&config)) {
        return QImage();
    }

    config.options.use_scaling = (targetWidth != width || targetHeight != height) ? 1 : 0;
    config.options.scaled_width = targetWidth;
    config.options.scaled_height = targetHeight;

    // MODE_bgrA lands in memory in exactly the byte order Format_ARGB32_Premultiplied
    // expects on a little-endian machine, which both ARM and x86 are here.
    config.output.colorspace = MODE_bgrA;
    config.output.is_external_memory = 1;
    config.output.u.RGBA.rgba = (quint8*) image.bits();
    config.output.u.RGBA.stride = image.bytesPerLine();
    config.output.u.RGBA.size = image.byteCount();

    const VP8StatusCode status = WebPDecode((const quint8*) data.constData(), data.size(), &config);
    WebPFreeDecBuffer(&config.output);

    kgLog("webp decode done status=" + QString::number((int) status));

    if (status != VP8_STATUS_OK) {
        return QImage();
    }

    if (size) {
        *size = QSize(targetWidth, targetHeight);
    }

    return image;
}
