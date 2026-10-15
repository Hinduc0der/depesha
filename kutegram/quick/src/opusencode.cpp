#include "opusencode.h"

#include <string.h>

#include <QDateTime>
#include <QFileInfo>
#include <QTime>

#include "kglog.h"

#include "opus.h"

// Пишем в тех же 16 кГц моно, в которых записывает микрофон и в которых мы
// раскодируем чужие голосовые.
static const int KG_ENC_RATE = 16000;

// Кадр 20 мс -- то, чем меряет сам Telegram и что лучше всего жмётся для речи.
static const int KG_ENC_FRAME = KG_ENC_RATE / 50;       // 320 отсчётов

// 20 кбит/с для речи -- это уже разборчиво, а минута весит 150 КБ: по EDGE
// такое уходит за несколько секунд.
static const int KG_ENC_BITRATE = 20000;

// Самый дешёвый по счёту режим. На 332 МГц это главный рычаг: разница в
// качестве речи на телефонном динамике не слышна, а времени экономит в разы.
static const int KG_ENC_COMPLEXITY = 0;

static const int KG_WAV_HEADER = 44;

// Контрольная сумма Ogg -- своя: многочлен 0x04c11db7, без переворотов и без
// начальной единицы. Таблица считается один раз при первом обращении.
static quint32 kgOggCrcTable[256];
static bool kgOggCrcReady = false;

static void kgOggCrcInit()
{
    for (int i = 0; i < 256; ++i) {
        quint32 r = (quint32) i << 24;
        for (int j = 0; j < 8; ++j) {
            r = (r & 0x80000000u) ? ((r << 1) ^ 0x04c11db7u) : (r << 1);
        }
        kgOggCrcTable[i] = r;
    }
    kgOggCrcReady = true;
}

static quint32 kgOggCrc(const char *data, int length)
{
    if (!kgOggCrcReady) {
        kgOggCrcInit();
    }

    quint32 crc = 0;
    for (int i = 0; i < length; ++i) {
        crc = (crc << 8) ^ kgOggCrcTable[((crc >> 24) & 0xFF) ^ (quint8) data[i]];
    }

    return crc;
}

static void kgPut32(char *at, quint32 value)
{
    at[0] = (char)(value & 0xFF);
    at[1] = (char)((value >> 8) & 0xFF);
    at[2] = (char)((value >> 16) & 0xFF);
    at[3] = (char)((value >> 24) & 0xFF);
}

static void kgPut16(char *at, quint16 value)
{
    at[0] = (char)(value & 0xFF);
    at[1] = (char)((value >> 8) & 0xFF);
}

KgVoiceEncoder::KgVoiceEncoder() :
    _encoder(0),
    _pcmLeft(0),
    _pcmTotal(0),
    _samples(0),
    _preSkip(0),
    _serial(0),
    _sequence(0),
    _firstPage(true),
    _pageGranule(0),
    _msSpent(0),
    _broken(false)
{
}

KgVoiceEncoder::~KgVoiceEncoder()
{
    if (_encoder) {
        opus_encoder_destroy(_encoder);
        _encoder = 0;
    }

    if (_ogg.isOpen()) {
        _ogg.close();
        QFile::remove(_oggPath);
    }
}

bool KgVoiceEncoder::begin(const QString &wavPath, const QString &oggPath)
{
    _wav.setFileName(wavPath);
    if (!_wav.open(QIODevice::ReadOnly)) {
        kgLog("enc: cannot read " + wavPath);
        return false;
    }

    // Заголовок свой же, писанный kgWavHeader(): 44 байта, PCM 16 бит моно.
    QByteArray header = _wav.read(KG_WAV_HEADER);
    if (header.size() != KG_WAV_HEADER || !header.startsWith("RIFF")
            || header.mid(8, 4) != "WAVE") {
        kgLog("enc: not a wav file");
        return false;
    }

    _pcmTotal = _wav.size() - KG_WAV_HEADER;
    _pcmLeft = _pcmTotal;

    int error = OPUS_OK;
    _encoder = opus_encoder_create(KG_ENC_RATE, 1, OPUS_APPLICATION_VOIP, &error);
    if (!_encoder || error != OPUS_OK) {
        kgLog(QString("enc: opus_encoder_create: ") + opus_strerror(error));
        return false;
    }

    opus_encoder_ctl(_encoder, OPUS_SET_BITRATE(KG_ENC_BITRATE));
    opus_encoder_ctl(_encoder, OPUS_SET_COMPLEXITY(KG_ENC_COMPLEXITY));
    opus_encoder_ctl(_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));

    // «Разгон»: сколько отсчётов кодировщик съедает на раскачку. Его кладут в
    // OpusHead, чтобы проигрыватель их выбросил, и считают в 48 кГц.
    opus_int32 lookahead = 0;
    opus_encoder_ctl(_encoder, OPUS_GET_LOOKAHEAD(&lookahead));
    _preSkip = lookahead * (48000 / KG_ENC_RATE);

    _ogg.setFileName(oggPath);
    _oggPath = oggPath;
    if (!_ogg.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        kgLog("enc: cannot write " + oggPath);
        return false;
    }

    // Номер потока: любой, лишь бы не ноль и не совпадал каждый раз.
    _serial = (quint32) QDateTime::currentDateTime().toTime_t() ^ 0x5be90000u;
    if (_serial == 0) {
        _serial = 1;
    }

    // Первый пакет -- OpusHead, и он обязан лежать на своей странице один.
    QByteArray head(19, '\0');
    memcpy(head.data(), "OpusHead", 8);
    head[8] = 1;                                    // версия
    head[9] = 1;                                    // каналов
    kgPut16(head.data() + 10, (quint16) _preSkip);
    kgPut32(head.data() + 12, KG_ENC_RATE);         // частота исходника
    kgPut16(head.data() + 16, 0);                   // усиление
    head[18] = 0;                                   // раскладка каналов
    addPacket(head, 0);
    flushPage(false);

    // Второй -- OpusTags, тоже отдельной страницей.
    const QByteArray vendor("Depesha");
    QByteArray tags;
    tags.append("OpusTags", 8);
    QByteArray len(4, '\0');
    kgPut32(len.data(), (quint32) vendor.size());
    tags.append(len);
    tags.append(vendor);
    kgPut32(len.data(), 0);                         // своих подписей нет
    tags.append(len);
    addPacket(tags, 0);
    flushPage(false);

    return true;
}

// Пакет Opus в текущую страницу. samples -- сколько отсчётов 16 кГц он даёт
// (0 для служебных).
void KgVoiceEncoder::addPacket(const QByteArray &packet, int samples)
{
    // Отрезки: каждые 255 байт -- отрезок длиной 255, остаток -- последним.
    // 255 означает «пакет продолжается», поэтому пакет длиной ровно 255 байт
    // требует ещё и нулевого отрезка.
    const int need = packet.size() / 255 + 1;

    // В странице их не больше 255; не влезает -- закрываем страницу.
    if (_lacing.size() + need > 255) {
        flushPage(false);
    }

    int left = packet.size();
    while (left >= 255) {
        _lacing.append((char) 255);
        left -= 255;
    }
    _lacing.append((char) left);

    _body.append(packet);

    if (samples > 0) {
        _samples += samples;
        // Позиция в потоке считается в 48 кГц и включает «разгон».
        _pageGranule = _samples * (48000 / KG_ENC_RATE) + _preSkip;
    }
}

void KgVoiceEncoder::flushPage(bool last)
{
    if (_lacing.isEmpty() && !last) {
        return;
    }

    if (_lacing.isEmpty()) {
        // Последняя страница обязана быть, даже если писать нечего: именно на
        // ней стоит пометка «конец потока».
        _lacing.append((char) 0);
    }

    QByteArray page(27, '\0');
    char *h = page.data();

    memcpy(h, "OggS", 4);
    h[4] = 0;                                       // версия
    h[5] = (char)((_firstPage ? 0x02 : 0x00) | (last ? 0x04 : 0x00));

    // Позиция -- 64 бита, но дольше суток голосовых не бывает.
    kgPut32(h + 6, (quint32)(_pageGranule & 0xFFFFFFFF));
    kgPut32(h + 10, (quint32)((_pageGranule >> 32) & 0xFFFFFFFF));
    kgPut32(h + 14, _serial);
    kgPut32(h + 18, _sequence++);
    kgPut32(h + 22, 0);                             // место под сумму
    h[26] = (char) _lacing.size();

    page.append(_lacing);
    page.append(_body);

    const quint32 crc = kgOggCrc(page.constData(), page.size());
    kgPut32(page.data() + 22, crc);

    _ogg.write(page);

    _firstPage = false;
    _lacing.clear();
    _body.clear();
}

bool KgVoiceEncoder::step(int msBudget)
{
    if (_broken || !_encoder) {
        return false;
    }

    QTime clock;
    clock.start();

    const int frameBytes = KG_ENC_FRAME * 2;
    unsigned char packet[1275];                     // предел кадра Opus
    opus_int16 pcm[KG_ENC_FRAME];

    while (_pcmLeft > 0) {
        QByteArray chunk = _wav.read(frameBytes);
        if (chunk.isEmpty()) {
            _pcmLeft = 0;
            break;
        }

        _pcmLeft -= chunk.size();

        // Последний кадр добиваем тишиной: Opus кодирует только целые кадры.
        memset(pcm, 0, sizeof(pcm));
        memcpy(pcm, chunk.constData(), chunk.size());

        const int wrote = opus_encode(_encoder, pcm, KG_ENC_FRAME, packet, sizeof(packet));
        if (wrote < 0) {
            kgLog(QString("enc: opus_encode: ") + opus_strerror(wrote));
            _broken = true;
            break;
        }

        addPacket(QByteArray((const char *) packet, wrote), KG_ENC_FRAME);

        // Страницу закрываем примерно раз в секунду -- так делают и обычные
        // кодировщики: мельче дробить незачем, крупнее некуда.
        if (_lacing.size() >= 50) {
            flushPage(false);
        }

        if (clock.elapsed() >= msBudget) {
            break;
        }
    }

    _msSpent += clock.elapsed();

    return !_broken && _pcmLeft > 0;
}

bool KgVoiceEncoder::finish()
{
    if (!_broken) {
        // Последний кадр добит тишиной, и без этого голосовое выходило бы
        // чуть длиннее записанного. Истинная длина ставится позицией на
        // последней странице -- по ней проигрыватель и обрежет добивку.
        const qint64 real = _pcmTotal / 2;
        if (real > 0) {
            _pageGranule = real * (48000 / KG_ENC_RATE) + _preSkip;
        }

        flushPage(true);
    }

    const bool ok = !_broken && _samples > 0;

    _wav.close();
    _ogg.close();

    if (_encoder) {
        opus_encoder_destroy(_encoder);
        _encoder = 0;
    }

    if (!ok) {
        QFile::remove(_oggPath);
        return false;
    }

    kgLog("enc: " + QString::number(seconds()) + " s -> "
          + QString::number((int) QFileInfo(_oggPath).size()) + " bytes in "
          + QString::number(_msSpent) + " ms");

    return true;
}

int KgVoiceEncoder::percent() const
{
    if (_pcmTotal <= 0) {
        return 0;
    }

    const int done = (int)((_pcmTotal - _pcmLeft) * 100 / _pcmTotal);
    return done > 100 ? 100 : done;
}

int KgVoiceEncoder::seconds() const
{
    return (int)(_samples / KG_ENC_RATE);
}
