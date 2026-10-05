#ifndef MESSAGEDELEGATE_H
#define MESSAGEDELEGATE_H

#include "twolinedelegate.h"

#include <QFontMetrics>
#include <QHash>
#include <QRect>

class AppSettings;
class QMovie;

// Строка переписки: те же две строки текста, а под ними -- картинка, если она
// у сообщения есть.
//
// Фотографии и обычные стикеры рисуются как есть. Кружки, видеостикеры и гифки
// телефон не декодирует вовсе: их превращает в анимированный GIF медиа-сервер,
// а GIF Qt рисует сам. Играет всегда только выделенная строка -- по одному
// QMovie на весь список, иначе на 332 МГц не хватит ни кадров, ни памяти.
class MessageDelegate : public TwoLineDelegate
{
    Q_OBJECT

public:
    // Что за медиа в строке. Решает и метку в списке, и то, что делает
    // нажатие -- поэтому не в делегате наедине, а наружу.
    enum MediaKind {
        NoMedia,
        VoiceMedia,      // Opus: сервер отдаёт mp3
        AnimationMedia,  // кружок, видеостикер, гифка: сервер отдаёт GIF
        VideoMedia       // обычное видео: сервер отдаёт 3GP штатному плееру
    };

    // Настройки нужны затем, что без медиа-сервера половина видов медиа на
    // этом телефоне невозможна в принципе, и обещать их нельзя.
    MessageDelegate(QAbstractItemModel *model, AppSettings *settings, QObject *parent = 0);

    MediaKind mediaKind(const QModelIndex &index) const;

    // Готовый (конвертированный) файл для этой строки уже лежит в кэше.
    // Вызывается из отрисовки, поэтому ответ запоминается: обращение к диску
    // на каждую перерисовку строки -- непозволительно.
    bool convertedReady(const QModelIndex &index) const;
    // Конвертация только что закончилась: ответ для этого файла поменялся.
    void markConverted(const QString &sourcePath);

    // Приписка к строке: «converting…», «no media server» и тому подобное.
    // Строка состояния на этом экране почти не видна, а здесь видно.
    void setNote(int row, const QString &note);

    // Какая строка сейчас играет и чем. Ноль -- никакая.
    void setAnimation(int row, QMovie *movie);

    // Есть ли у строки что рисовать: от этого зависит её высота.
    bool hasPicture(const QModelIndex &index) const;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;

private:
    QPixmap picture(const QModelIndex &index, int maxSide) const;
    // Размытая заглушка из самого сообщения, пока файл не скачан.
    QPixmap preview(const QModelIndex &index, int maxSide) const;
    // Ширину строки делегату никто не сообщает -- она берётся у списка.
    int textWidth(const QStyleOptionViewItem &option) const;
    QString bodyText(const QModelIndex &index) const;
    // [voice], [gif], [video] и подсказка, что с ними делать. Пусто для того,
    // что рисуется само -- фотографий и обычных стикеров.
    QString mediaTag(const QModelIndex &index) const;
    QRect bodyRect(const QFontMetrics &metrics, int width, const QString &text) const;

    int _hasPhotoRole;
    int _photoFileRole;
    int _hasStickerRole;
    int _stickerFileRole;
    int _hasAnimationRole;
    int _animationFileRole;
    int _mediaIsVoiceRole;
    int _mediaFilePathRole;
    int _mediaTitleRole;
    int _photoPreviewRole;
    int _messageIdRole;

    int _animationRow;
    QMovie *_movie;

    // Путь исходника -> есть ли рядом готовый файл. Заполняется по одному
    // обращению к диску на файл.
    mutable QHash<QString, bool> _convertedKnown;

    int _noteRow;
    QString _note;
    AppSettings *_settings;
};

#endif // MESSAGEDELEGATE_H
