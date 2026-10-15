#include "messagedelegate.h"
#include "webpdecoder.h"

#include "appsettings.h"
#include "cachemanager.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QFile>
#include <QImageReader>
#include <QStringList>
#include <QFontMetrics>
#include <QMovie>
#include <QPainter>
#include <QPixmapCache>

// Больше этого картинка в строке не делается. Три таких строки помещаются на
// внешнем экране 240x320 -- дальше список перестаёт быть списком.
static const int KG_PICTURE_SIDE = 96;

// Длинное сообщение разворачивается не целиком: строка выше экрана не
// листается, а место занимает всё. Остальное видно в самом сообщении, когда
// понадобится.
static const int KG_MAX_LINES = 8;

MessageDelegate::MessageDelegate(QAbstractItemModel *model, AppSettings *settings, QObject *parent) :
    TwoLineDelegate(model, "senderName", "messageText", "messageTime", "mediaTitle", parent),
    _hasPhotoRole(roleByName(model, "hasPhoto")),
    _photoFileRole(roleByName(model, "photoFile")),
    _hasStickerRole(roleByName(model, "hasSticker")),
    _stickerFileRole(roleByName(model, "stickerFile")),
    _hasAnimationRole(roleByName(model, "hasAnimation")),
    _animationFileRole(roleByName(model, "animationFile")),
    _mediaIsVoiceRole(roleByName(model, "mediaIsVoice")),
    _mediaFilePathRole(roleByName(model, "mediaFilePath")),
    _mediaTitleRole(roleByName(model, "mediaTitle")),
    _photoPreviewRole(roleByName(model, "photoPreview")),
    _messageIdRole(roleByName(model, "messageId")),
    _animationRow(-1),
    _movie(0),
    _noteRow(-1),
    _settings(settings)
{
}

void MessageDelegate::setNote(int row, const QString &note)
{
    _noteRow = row;
    _note = note;
}

void MessageDelegate::setAnimation(int row, QMovie *movie)
{
    _animationRow = row;
    _movie = movie;
}

bool MessageDelegate::hasPicture(const QModelIndex &index) const
{
    if (_hasPhotoRole >= 0 && index.data(_hasPhotoRole).toBool()) {
        return true;
    }

    if (_hasStickerRole >= 0 && index.data(_hasStickerRole).toBool()) {
        return true;
    }

    // Анимация занимает место только когда она уже превращена в GIF: до этого
    // рисовать нечего, и держать под неё пустую полосу незачем.
    return _animationFileRole >= 0 && !index.data(_animationFileRole).toString().isEmpty();
}

QPixmap MessageDelegate::picture(const QModelIndex &index, int maxSide) const
{
    QString path;
    bool sticker = false;

    if (_animationFileRole >= 0) {
        path = index.data(_animationFileRole).toString();
    }

    if (path.isEmpty() && _stickerFileRole >= 0
            && index.data(_hasStickerRole).toBool()) {
        path = index.data(_stickerFileRole).toString();
        sticker = true;
    }

    if (path.isEmpty() && _photoFileRole >= 0) {
        path = index.data(_photoFileRole).toString();
    }

    if (path.isEmpty()) {
        // Настоящей картинки ещё нет -- рисуется то крошечное размытое, что
        // пришло вместе с сообщением. Лучше, чем белое поле на все те минуты,
        // пока файл ползёт по 3G.
        return preview(index, maxSide);
    }

    const QString key = "kgpic" + QString::number(maxSide) + path;

    QPixmap result;
    if (QPixmapCache::find(key, &result)) {
        return result;
    }

    QImage image;

    if (sticker) {
        image = kgDecodeWebp(path, maxSide);
    }

    // Не WebP -- значит обычная картинка, и читает её сам Qt. Сюда попадает
    // не только фотография: миниатюра анимированного стикера приходит и
    // JPEG'ом, по тому, что отдал Telegram.
    if (image.isNull()) {
        // Разворачивать фотографию целиком, чтобы потом ужать до девяноста
        // шести точек, -- это мегабайты и секунды на каждую строку при
        // прокрутке. QImageReader умеет отдать сразу уменьшенное: для JPEG это
        // делает сам libjpeg, на четверть или восьмую часть работы.
        QImageReader reader(path);
        const QSize full = reader.size();

        if (full.isValid() && (full.width() > maxSide || full.height() > maxSide)) {
            QSize scaled = full;
            scaled.scale(maxSide, maxSide, Qt::KeepAspectRatio);
            reader.setScaledSize(scaled);
        }

        image = reader.read();
    }

    if (!image.isNull()) {
        result = QPixmap::fromImage(image);
    }

    if (!result.isNull()) {
        QPixmapCache::insert(key, result);
    }

    return result;
}

// Сколько места есть под текст. Ширину строки делегату никто не сообщает:
// в списке QStyledItemDelegate::sizeHint зовут с пустым прямоугольником,
// поэтому она берётся у самого списка.
int MessageDelegate::textWidth(const QStyleOptionViewItem &option) const
{
    if (option.rect.width() > 16) {
        return option.rect.width() - 8;
    }

    const QAbstractItemView *view = qobject_cast<QAbstractItemView*>(parent());

    if (view && view->viewport()->width() > 16) {
        return view->viewport()->width() - 8;
    }

    return 200;
}

// Текст сообщения, разложенный по строкам, и его высота. Длинные сообщения
// обрезаются по числу строк: сообщение выше экрана листать нечем, а место в
// списке оно занимает всё.
// Голосовое, кружок, гифка и видео ничего не качают и не конвертируют сами:
// это минуты 3G и мегабайты памяти на то, мимо чего чаще всего просто
// прокручивают. В списке от них остаётся метка и подсказка, а работа начинается
// нажатием на выделенном сообщении.
MessageDelegate::MediaKind MessageDelegate::mediaKind(const QModelIndex &index) const
{
    if (_mediaIsVoiceRole >= 0 && index.data(_mediaIsVoiceRole).toBool()) {
        return VoiceMedia;
    }

    // Кружок, видеостикер и гифка помечены моделью: их сервер превращает в GIF
    // и они рисуются прямо в строке.
    if (_hasAnimationRole >= 0 && index.data(_hasAnimationRole).toBool()) {
        return AnimationMedia;
    }

    // Обычное видео в GIF гнать незачем -- оно длинное. Сервер делает из него
    // 3GP, и его телефон играет сам, своим кодеком.
    const QString title = _mediaTitleRole >= 0
            ? plainText(index.data(_mediaTitleRole)) : QString();

    if (title == "Video" || title == "Video message") {
        return VideoMedia;
    }

    return NoMedia;
}

bool MessageDelegate::convertedReady(const QModelIndex &index) const
{
    const QString path = _mediaFilePathRole >= 0
            ? index.data(_mediaFilePathRole).toString() : QString();

    if (path.isEmpty()) {
        return false;
    }

    const QHash<QString, bool>::const_iterator known = _convertedKnown.find(path);
    if (known != _convertedKnown.end()) {
        return known.value();
    }

    const MediaKind kind = mediaKind(index);

    QStringList candidates;
    if (kind == VideoMedia) {
        candidates << ".3gp" << ".png";
    } else if (kind == AnimationMedia) {
        candidates << ".gif" << ".png";
    } else if (kind == VoiceMedia) {
        candidates << ".mp3" << ".amr";
    }

    bool ready = false;
    for (int i = 0; i < candidates.size() && !ready; ++i) {
        ready = QFile::exists(CacheManager::convertedPath(path, candidates[i]));
    }

    _convertedKnown.insert(path, ready);

    return ready;
}

void MessageDelegate::markConverted(const QString &sourcePath)
{
    if (!sourcePath.isEmpty()) {
        _convertedKnown.insert(sourcePath, true);
    }
}

QString MessageDelegate::mediaTag(const QModelIndex &index) const
{
    const MediaKind kind = mediaKind(index);

    if (kind == NoMedia) {
        return QString();
    }

    const QString title = _mediaTitleRole >= 0
            ? plainText(index.data(_mediaTitleRole)) : QString();

    QString tag;
    if (kind == VoiceMedia) {
        tag = "[voice]";
    } else if (kind == VideoMedia) {
        tag = "[video]";
    } else if (title == "GIF") {
        tag = "[gif]";
    } else if (title == "Video message") {
        tag = "[video]";
    } else {
        tag = "[sticker]";
    }

    const QString file = _mediaFilePathRole >= 0
            ? index.data(_mediaFilePathRole).toString() : QString();

    // Кружок, видеостикер, гифку и видео этот телефон не декодирует вовсе --
    // их переводит медиа-сервер. Без него обещать «нажмите, чтобы проиграть»
    // нельзя: единственное, что здесь можно сделать, -- сохранить файл и
    // забрать его на компьютер.
    //
    // Голосовые -- исключение: Opus раскодирует сам телефон (см. opusvoice.h),
    // и сервер для них больше не нужен.
    const bool hasServer = _settings && !_settings->activeMediaServerUrl().isEmpty();

    if (!hasServer && kind != VoiceMedia) {
        return tag + " " + (file.isEmpty() ? QObject::tr("no media server, press to save")
                                           : QObject::tr("saved to phone"));
    }

    if (file.isEmpty()) {
        return tag + " " + QObject::tr("press to load");
    }

    if (kind == AnimationMedia) {
        const QString shown = _animationFileRole >= 0
                ? index.data(_animationFileRole).toString() : QString();

        if (!shown.isEmpty()) {
            return tag + " " + QObject::tr("press to play");
        }

        // Готовое с прошлого запуска лежит в кэше, хотя строка об этом ещё не
        // знает: конвертация не повторится, значит и обещать её незачем.
        return tag + " " + (convertedReady(index) ? QObject::tr("press to play")
                                                  : QObject::tr("press to show"));
    }

    if (kind == VideoMedia) {
        return tag + " " + (convertedReady(index) ? QObject::tr("press to play")
                                                  : QObject::tr("press to convert"));
    }

    return tag + " " + QObject::tr("press to play");
}

QString MessageDelegate::bodyText(const QModelIndex &index) const
{
    QString text = _subtitleRole >= 0 ? plainText(index.data(_subtitleRole)) : QString();

    QString tag = mediaTag(index);

    // Приписка о том, что прямо сейчас происходит с этим сообщением.
    if (index.row() == _noteRow && !_note.isEmpty()) {
        tag = (tag.isEmpty() ? QString() : tag + " ") + "-- " + _note;
    }

    if (!tag.isEmpty()) {
        return text.isEmpty() ? tag : (tag + " " + text);
    }

    if (text.isEmpty() && _fallbackRole >= 0) {
        text = plainText(index.data(_fallbackRole));
    }

    return text;
}

QRect MessageDelegate::bodyRect(const QFontMetrics &metrics, int width,
                                const QString &text) const
{
    QRect rect = metrics.boundingRect(QRect(0, 0, width, 0),
                                      Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, text);

    const int maxHeight = metrics.height() * KG_MAX_LINES;

    if (rect.height() > maxHeight) {
        rect.setHeight(maxHeight);
    }

    return rect;
}

QPixmap MessageDelegate::preview(const QModelIndex &index, int maxSide) const
{
    if (_photoPreviewRole < 0) {
        return QPixmap();
    }

    const QByteArray bytes = index.data(_photoPreviewRole).toByteArray();
    if (bytes.isEmpty()) {
        return QPixmap();
    }

    const QString key = "kgpre" + QString::number(maxSide) + "_"
            + (_messageIdRole >= 0 ? index.data(_messageIdRole).toString()
                                   : QString::number(index.row()));

    QPixmap result;
    if (QPixmapCache::find(key, &result)) {
        return result;
    }

    QImage image;
    if (!image.loadFromData(bytes, "JPG")) {
        return QPixmap();
    }

    // Само превью -- десятки точек; растягивать его до коробки незачем, пусть
    // будет маленьким и честным.
    if (image.width() > maxSide || image.height() > maxSide) {
        image = image.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::FastTransformation);
    }

    result = QPixmap::fromImage(image);
    QPixmapCache::insert(key, result);

    return result;
}

void MessageDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const
{
    QStyleOptionViewItemV4 opt = option;
    initStyleOption(&opt, index);

    const bool selected = (opt.state & QStyle::State_Selected) != 0;
    const QColor background = selected ? opt.palette.color(QPalette::Highlight)
                                       : opt.palette.color(QPalette::Base);
    painter->fillRect(opt.rect, background);

    const QColor textColor = readableOn(background);
    const QFontMetrics metrics(opt.font);
    const int lineHeight = metrics.height();
    const int width = textWidth(opt);

    const QString sender = _titleRole >= 0 ? plainText(index.data(_titleRole)) : QString();
    const QString side = _sideRole >= 0 ? plainText(index.data(_sideRole)) : QString();
    const QString body = bodyText(index);

    painter->save();
    painter->setPen(textColor);

    int sideWidth = 0;
    if (!side.isEmpty()) {
        sideWidth = metrics.width(side) + 6;
        painter->drawText(QRect(opt.rect.right() - sideWidth - 2, opt.rect.top() + 2,
                                sideWidth, lineHeight),
                          Qt::AlignRight | Qt::AlignVCenter, side);
    }

    QFont senderFont = opt.font;
    senderFont.setBold(true);
    painter->setFont(senderFont);
    painter->drawText(QRect(opt.rect.left() + 4, opt.rect.top() + 2,
                            width - sideWidth, lineHeight),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      metrics.elidedText(sender, Qt::ElideRight, width - sideWidth));

    int textHeight = lineHeight + 4;

    if (!body.isEmpty()) {
        painter->setFont(opt.font);

        // Текст сообщения глуше имени, но того же цвета.
        QColor bodyColor = textColor;
        bodyColor.setAlpha(190);
        painter->setPen(bodyColor);

        const QRect wrapped = bodyRect(metrics, width, body);

        painter->drawText(QRect(opt.rect.left() + 4, opt.rect.top() + textHeight,
                                width, wrapped.height()),
                          Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, body);

        textHeight += wrapped.height() + 2;
    }

    painter->restore();

    if (!hasPicture(index)) {
        return;
    }

    QPixmap pixmap;

    // Выделенная строка показывает текущий кадр, остальные -- первый: одна
    // анимация на список, и та только там, где на неё смотрят.
    if (index.row() == _animationRow && _movie) {
        pixmap = _movie->currentPixmap();

        if (!pixmap.isNull()
                && (pixmap.width() > KG_PICTURE_SIDE || pixmap.height() > KG_PICTURE_SIDE)) {
            pixmap = pixmap.scaled(KG_PICTURE_SIDE, KG_PICTURE_SIDE,
                                   Qt::KeepAspectRatio, Qt::FastTransformation);
        }
    }

    if (pixmap.isNull()) {
        pixmap = picture(index, KG_PICTURE_SIDE);
    }

    if (pixmap.isNull()) {
        return;
    }

    painter->drawPixmap(opt.rect.left() + 4, opt.rect.top() + textHeight, pixmap);
}

QSize MessageDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const QFontMetrics metrics(option.font);
    const int width = textWidth(option);

    int height = metrics.height() + 6;

    const QString body = bodyText(index);
    if (!body.isEmpty()) {
        height += bodyRect(metrics, width, body).height() + 2;
    }

    // Высота под картинку -- всегда одна и та же, и не спрашивает саму
    // картинку: иначе список мерил бы каждую строку разворачиванием стикера.
    if (hasPicture(index)) {
        height += KG_PICTURE_SIDE + 4;
    }

    return QSize(width + 8, height);
}
