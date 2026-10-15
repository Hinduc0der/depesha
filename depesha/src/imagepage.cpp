#include "imagepage.h"
#include "webpdecoder.h"

#include <QImageReader>
#include <QPainter>

ImagePage::ImagePage(QWidget *parent) :
    QWidget(parent)
{
    setAutoFillBackground(true);

    QPalette colors = palette();
    colors.setColor(QPalette::Window, Qt::black);
    setPalette(colors);
}

bool ImagePage::showFile(const QString &path)
{
    _path = path;
    _scaled = QPixmap();

    rescale();

    return !_scaled.isNull();
}

void ImagePage::rescale()
{
    if (_path.isEmpty() || width() < 2 || height() < 2) {
        return;
    }

    // Стикер -- WebP, его Qt не читает; остальное читает сама, и сразу в
    // нужном размере: разворачивать мегапиксель ради экрана 240 точек не надо
    // ни здесь, ни в списке.
    // Миниатюра анимированного стикера лежит под тем же расширением, а
    // приходит такой, какой её отдал Telegram, -- бывает и JPEG. Поэтому если
    // WebP не разобрался, читаем обычным путём, а не показываем пустоту.
    if (_path.endsWith(".webp")) {
        const QImage image = kgDecodeWebp(_path, qMax(width(), height()));
        if (!image.isNull()) {
            _scaled = QPixmap::fromImage(image);
            return;
        }
    }

    QImageReader reader(_path);
    const QSize full = reader.size();

    if (full.isValid() && (full.width() > width() || full.height() > height())) {
        QSize target = full;
        target.scale(width(), height(), Qt::KeepAspectRatio);
        reader.setScaledSize(target);
    }

    const QImage image = reader.read();
    _scaled = image.isNull() ? QPixmap() : QPixmap::fromImage(image);
}

void ImagePage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    // Крышка открылась или закрылась -- экран другой, картинку надо прочитать
    // заново под него.
    rescale();
}

void ImagePage::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    if (_scaled.isNull()) {
        return;
    }

    QPainter painter(this);
    painter.drawPixmap((width() - _scaled.width()) / 2,
                       (height() - _scaled.height()) / 2, _scaled);
}
