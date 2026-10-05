#ifndef WEBPDECODER_H
#define WEBPDECODER_H

#include <QImage>
#include <QString>

// Обычный стикер Telegram -- это WebP, а Qt 4.7 его не читает. Декодер взят
// из Kutegram (там он был провайдером картинок для QML) и здесь сведён к
// обычной функции: виджетам провайдер не нужен.
//
// Разворачивается сразу в нужный размер, а не в полный: стикер приходит
// 512x512, и это мегабайт в памяти до уменьшения.
QImage kgDecodeWebp(const QString &path, int maxSide);

#endif // WEBPDECODER_H
