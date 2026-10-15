#ifndef OPUSENCODE_H
#define OPUSENCODE_H

#include <QByteArray>
#include <QFile>
#include <QString>

struct OpusEncoder;

// Обратное opusvoice.h: записанный с микрофона WAV (PCM 16 кГц моно) -> Opus
// в контейнере Ogg, то есть ровно то, что Telegram считает голосовым
// сообщением. Всё на телефоне, без сервера-прокладки.
//
// Ogg пишем сами, как и читаем: нужны страницы, таблица отрезков и своя
// контрольная сумма -- это полторы сотни строк, а libogg тянуть незачем.
//
// Работа режется на куски по той же причине, что и расшифровка: кодирование
// дороже неё в несколько раз, а телефон всё это время должен слушаться рук.
// Вызывающий дёргает step() по таймеру.
class KgVoiceEncoder
{
public:
    KgVoiceEncoder();
    ~KgVoiceEncoder();

    // wavPath -- то, что записал KgVoiceRecorder; oggPath -- куда сложить.
    bool begin(const QString &wavPath, const QString &oggPath);

    // Поработать не дольше msBudget. true -- работа ещё осталась.
    bool step(int msBudget);

    // Дописать последнюю страницу. false -- ничего не вышло, файл удалён.
    bool finish();

    int percent() const;
    int msSpent() const { return _msSpent; }
    // Длительность получившегося голосового в секундах -- её ждёт Telegram.
    int seconds() const;

private:
    void addPacket(const QByteArray &packet, int samples);
    void flushPage(bool last);

    QFile _wav;
    QFile _ogg;
    QString _oggPath;
    OpusEncoder *_encoder;

    qint64 _pcmLeft;        // сколько байт PCM осталось прочитать
    qint64 _pcmTotal;
    qint64 _samples;        // сколько отсчётов уже закодировано
    int _preSkip;           // «разгон» кодировщика, в отсчётах 48 кГц

    quint32 _serial;
    quint32 _sequence;
    bool _firstPage;

    QByteArray _lacing;     // таблица отрезков текущей страницы
    QByteArray _body;       // её содержимое
    qint64 _pageGranule;

    int _msSpent;
    bool _broken;
};

#endif // OPUSENCODE_H
