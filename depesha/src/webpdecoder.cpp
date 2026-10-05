#include "webpdecoder.h"

#include <QFile>

#include "kglog.h"

#include "src/webp/decode.h"

// Стикер -- десятки килобайт; всё, что сильно больше, стикером не является, а
// его разворачивание -- способ оставить телефон без памяти.
static const int KG_WEBP_MAX_BYTES = 4 * 1024 * 1024;

// Декодеру нужно место и под промежуточную картинку в полный размер. Меряется
// свободная память СИСТЕМЫ, а не свободное место внутри кучи процесса: куча
// растёт по требованию, и внутри неё свободными всегда числятся десятки
// килобайт -- на этом в Kutegram однажды молча отключились все стикеры.
static const int KG_WEBP_MIN_FREE_RAM = 4 * 1024 * 1024;

QImage kgDecodeWebp(const QString &path, int maxSide)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QImage();
    }

    if (file.size() > KG_WEBP_MAX_BYTES) {
        file.close();
        return QImage();
    }

    const QByteArray data = file.readAll();
    file.close();

    int width = 0;
    int height = 0;
    if (!WebPGetInfo((const quint8*) data.constData(), data.size(), &width, &height)
            || width <= 0 || height <= 0) {
        return QImage();
    }

    int targetWidth = width;
    int targetHeight = height;

    if (maxSide > 0 && qMax(width, height) > maxSide) {
        if (width >= height) {
            targetWidth = maxSide;
            targetHeight = qMax(1, height * maxSide / width);
        } else {
            targetHeight = maxSide;
            targetWidth = qMax(1, width * maxSide / height);
        }
    }

    // Неудачное выделение внутри декодера утащило бы всё приложение, а на этом
    // телефоне и телефон. Стикер того не стоит.
    const int freeRam = kgFreeRam();
    if (freeRam > 0 && freeRam < KG_WEBP_MIN_FREE_RAM) {
        kgLog("webp skipped, only " + QString::number(freeRam / 1024) + "k left");
        return QImage();
    }

    // Premultiplied -- тот формат, который Qt кладёт на экран без пересчёта.
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

    // MODE_bgrA ложится в память ровно тем порядком байт, которого ждёт
    // Format_ARGB32_Premultiplied на little-endian, а тут он и есть.
    config.output.colorspace = MODE_bgrA;
    config.output.is_external_memory = 1;
    config.output.u.RGBA.rgba = (quint8*) image.bits();
    config.output.u.RGBA.stride = image.bytesPerLine();
    config.output.u.RGBA.size = image.byteCount();

    const VP8StatusCode status = WebPDecode((const quint8*) data.constData(), data.size(), &config);
    WebPFreeDecBuffer(&config.output);

    if (status != VP8_STATUS_OK) {
        kgLog("webp decode failed, status=" + QString::number((int) status));
        return QImage();
    }

    return image;
}
