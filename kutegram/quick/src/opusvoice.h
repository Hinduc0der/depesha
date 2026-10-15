#ifndef OPUSVOICE_H
#define OPUSVOICE_H

#include <QByteArray>
#include <QFile>
#include <QString>
#include <QTime>

struct OpusDecoder;

// Голосовое Telegram -- это Opus в контейнере Ogg: кодек 2012 года на
// телефоне, чей самый новый кодек старше. До сих пор голосовые раскодировал
// сервер-прокладка и присылал mp3/amr; здесь это делает сам телефон.
//
// Зачем: без сервера человек не мог слушать голосовые вообще, а поднимать
// VPS ради них готов не каждый. Декодер libopus собран в проект целочисленным
// (см. libopus/libopus.pri) -- плавающей точки в железе этого ARM11 нет.
//
// Получается WAV PCM 16 кГц моно: Phonon играет локальный файл своими силами,
// а 16 кГц вместо родных для Opus 48 кГц -- это втрое меньше счёта и втрое
// меньше файл, и на телефонном динамике разницы не слышно.
//
// РАБОТА ДЕЛИТСЯ НА КУСКИ. Замерено на аппарате: пять секунд звука
// раскодируются за 1169 мс, то есть минутное голосовое -- это четырнадцать
// секунд счёта. Столько держать интерфейс мёртвым нельзя, а отдельный поток
// на этом телефоне не заводится вовсе (QThread::start() молча не стартует,
// проверено сборкой 0.4.3). Поэтому вызывающий дёргает step() по таймеру,
// отдавая по сотне миллисекунд за раз, и между кусками телефон живёт
// обычной жизнью.
class KgVoiceDecoder
{
public:
    KgVoiceDecoder();
    ~KgVoiceDecoder();

    // Открыть голосовое и приготовиться. false -- не вышло даже начать.
    bool begin(const QString &oggPath, const QString &wavPath);

    // Поработать НЕ ДОЛЬШЕ msBudget и вернуть управление.
    // true -- работа ещё осталась, зовите снова.
    bool step(int msBudget);

    // Дописать хвост и заголовок WAV. false -- ничего путного не вышло,
    // файл удалён.
    bool finish();

    // Сколько прошли, в процентах: по месту в файле, а не по звуку --
    // длительности мы заранее не знаем.
    int percent() const;

    // Сколько миллисекунд ушло на счёт (без ожидания между кусками).
    int msSpent() const { return _msSpent; }

    // Сколько секунд звука уже получилось.
    int secondsDone() const;

private:
    bool packet(const QByteArray &data);

    QByteArray _ogg;
    QFile _wav;
    QString _wavPath;
    OpusDecoder *_decoder;

    int _at;                // где идём по файлу
    QByteArray _packet;     // пакет, который может переползать со страницы
    int _headers;
    int _preSkip;
    int _preSkipTotal;
    qint64 _lastGranule;
    qint64 _written;
    qint64 _reported;
    int _msSpent;
    bool _broken;
};

// Заголовок WAV: 16 бит, моно, 44 байта. Размеры известны только в конце,
// поэтому сначала пишется пустышка, а потом этот же заголовок поверх неё.
// Общий и для расшифровки, и для записи с микрофона.
QByteArray kgWavHeader(qint64 samples, int rate);

// Разом, без нарезки: для настольной проверки и на случай короткого файла.
// msSpent (если не ноль) -- сколько миллисекунд заняла работа.
bool kgDecodeVoiceToWav(const QString &oggPath, const QString &wavPath, int *msSpent = 0);

#endif // OPUSVOICE_H
