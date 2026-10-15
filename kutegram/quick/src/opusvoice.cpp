#include "opusvoice.h"

#include <string.h>

#include "kglog.h"

#include "opus.h"

// Раскодируем в 16 кГц, а не в родные для Opus 48: втрое меньше работы и
// втрое меньше файл, а речь в телефонном динамике от этого не меняется.
static const int KG_VOICE_RATE = 16000;

// Самый длинный кадр Opus -- 120 мс.
static const int KG_VOICE_MAX_FRAME = KG_VOICE_RATE / 1000 * 120;

// Голосовое -- это десятки килобайт. Всё, что сильно больше, голосовым не
// является, а разворачивание такого -- способ остаться без памяти (ровно та
// же оговорка, что у декодера стикеров).
static const int KG_VOICE_MAX_OGG_BYTES = 4 * 1024 * 1024;

// Телеграм даёт на голосовое не больше часа, но нам хватит и десяти минут:
// это 19 МБ WAV, и дальше упрёмся в карту памяти, а не в кодек.
static const qint64 KG_VOICE_MAX_SAMPLES = (qint64) KG_VOICE_RATE * 60 * 10;

static const int KG_WAV_HEADER = 44;

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

QByteArray kgWavHeader(qint64 samples, int rate)
{
    const quint32 bytes = (quint32)(samples * 2);
    QByteArray header(KG_WAV_HEADER, '\0');
    char *h = header.data();

    memcpy(h, "RIFF", 4);
    kgPut32(h + 4, bytes + 36);
    memcpy(h + 8, "WAVEfmt ", 8);
    kgPut32(h + 16, 16);                        // длина блока fmt
    kgPut16(h + 20, 1);                         // PCM без сжатия
    kgPut16(h + 22, 1);                         // каналов
    kgPut32(h + 24, rate);
    kgPut32(h + 28, rate * 2);                  // байт в секунду
    kgPut16(h + 32, 2);                         // байт на кадр
    kgPut16(h + 34, 16);                        // бит на отсчёт
    memcpy(h + 36, "data", 4);
    kgPut32(h + 40, bytes);

    return header;
}

KgVoiceDecoder::KgVoiceDecoder() :
    _decoder(0),
    _at(0),
    _headers(0),
    _preSkip(0),
    _preSkipTotal(0),
    _lastGranule(-1),
    _written(0),
    _reported(0),
    _msSpent(0),
    _broken(false)
{
}

KgVoiceDecoder::~KgVoiceDecoder()
{
    if (_decoder) {
        opus_decoder_destroy(_decoder);
        _decoder = 0;
    }

    // Бросили на полпути -- недописанный WAV никому не нужен: в нём нет даже
    // честного заголовка, а Phonon на таком спотыкается.
    if (_wav.isOpen()) {
        _wav.close();
        QFile::remove(_wavPath);
    }
}

bool KgVoiceDecoder::begin(const QString &oggPath, const QString &wavPath)
{
    QFile in(oggPath);
    if (!in.open(QIODevice::ReadOnly)) {
        return false;
    }
    if (in.size() > KG_VOICE_MAX_OGG_BYTES) {
        kgLog("voice: ogg is too big to be a voice message");
        return false;
    }
    _ogg = in.readAll();
    in.close();

    int error = OPUS_OK;
    _decoder = opus_decoder_create(KG_VOICE_RATE, 1, &error);
    if (!_decoder || error != OPUS_OK) {
        kgLog(QString("voice: opus_decoder_create: ") + opus_strerror(error));
        return false;
    }

    _wavPath = wavPath;
    _wav.setFileName(wavPath);
    if (!_wav.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    _wav.write(QByteArray(KG_WAV_HEADER, '\0'));    // место под заголовок

    return true;
}

// Один собранный пакет Ogg: либо служебный заголовок Opus, либо кадр звука.
// false -- беда, дальше идти незачем.
bool KgVoiceDecoder::packet(const QByteArray &data)
{
    if (data.isEmpty()) {
        return true;
    }

    // Служебные пакеты узнаём по подписи, а не по порядковому номеру: мало ли
    // чем нас угостят.
    if (_headers < 2) {
        if (data.startsWith("OpusHead")) {
            if (data.size() >= 12) {
                // «Разгон» (pre-skip): первые отсчёты после запуска декодера --
                // мусор, и кодировщик пишет, сколько их выбросить. Считан он в
                // 48 кГц, а мы раскодируем в 16 -- значит делим на три. Без
                // этого голосовое начинается со щелчка.
                const int raw = (uchar) data.at(10) | ((uchar) data.at(11) << 8);
                _preSkip = raw / (48000 / KG_VOICE_RATE);
                _preSkipTotal = _preSkip;
            }
            ++_headers;
            return true;
        }
        if (data.startsWith("OpusTags")) {
            ++_headers;
            return true;
        }
        _headers = 2;
    }

    opus_int16 pcm[KG_VOICE_MAX_FRAME];
    const int got = opus_decode(_decoder, (const unsigned char *) data.constData(),
                                data.size(), pcm, KG_VOICE_MAX_FRAME, 0);
    if (got < 0) {
        kgLog(QString("voice: opus_decode: ") + opus_strerror(got));
        return false;
    }

    int from = 0;
    if (_preSkip > 0) {
        from = _preSkip < got ? _preSkip : got;
        _preSkip -= from;
    }

    if (got > from) {
        if (_wav.write((const char *)(pcm + from), (got - from) * 2) < 0) {
            kgLog("voice: wav write failed");
            return false;
        }
        _written += got - from;
    }

    return _written <= KG_VOICE_MAX_SAMPLES;
}

bool KgVoiceDecoder::step(int msBudget)
{
    if (_broken || !_decoder) {
        return false;
    }

    QTime clock;
    clock.start();

    const uchar *data = (const uchar *) _ogg.constData();
    const int size = _ogg.size();

    // Ogg разбираем сами: libogg нужен ради страниц и пакетов, а это полсотни
    // строк. Весь файл уже в памяти, поэтому пакет, переползший на следующую
    // страницу, просто продолжает набираться в _packet.
    while (_at + 27 <= size) {
        if (memcmp(data + _at, "OggS", 4) != 0) {
            ++_at;                                  // не на границе страницы
            continue;
        }

        const int segments = data[_at + 26];
        if (_at + 27 + segments > size) {
            break;
        }

        qint64 granule = 0;
        for (int i = 7; i >= 0; --i) {
            granule = (granule << 8) | data[_at + 6 + i];
        }

        int body = 0;
        for (int i = 0; i < segments; ++i) {
            body += data[_at + 27 + i];
        }
        if (_at + 27 + segments + body > size) {
            break;
        }

        const uchar *lengths = data + _at + 27;
        const uchar *payload = lengths + segments;
        int offset = 0;
        for (int i = 0; i < segments; ++i) {
            _packet.append((const char *) payload + offset, lengths[i]);
            offset += lengths[i];
            if (lengths[i] == 255) {
                continue;                           // продолжение следует
            }
            if (!packet(_packet)) {
                _broken = true;
            }
            _packet.clear();
            if (_broken) {
                break;
            }
        }

        // -1 во всех восьми байтах означает, что на этой странице не
        // кончается ни один пакет, и считать по ней длину нельзя.
        if (granule != (qint64) -1) {
            _lastGranule = granule;
        }

        _at += 27 + segments + body;

        if (_broken) {
            break;
        }

        // Отработали свой кусок -- отдаём телефон обратно интерфейсу.
        if (clock.elapsed() >= msBudget) {
            break;
        }
    }

    _msSpent += clock.elapsed();

    if (_written - _reported >= KG_VOICE_RATE * 5) {
        _reported = _written;
        kgLog(QString("voice: %1 s of audio in %2 ms")
              .arg(_written / KG_VOICE_RATE).arg(_msSpent));
    }

    return !_broken && _at + 27 <= size;
}

bool KgVoiceDecoder::finish()
{
    // Хвост. Кодировщик добивает последний кадр тишиной, а истинную длину
    // пишет в granulepos последней страницы -- в тех же 48 кГц и вместе с
    // «разгоном». Без обрезки в конце остаётся лишнее.
    if (!_broken && _lastGranule > 0) {
        const qint64 expected = _lastGranule / (48000 / KG_VOICE_RATE) - _preSkipTotal;
        if (expected > 0 && expected < _written) {
            _wav.resize(KG_WAV_HEADER + expected * 2);
            _written = expected;
        }
    }

    const bool ok = !_broken && _written > 0;

    if (ok) {
        _wav.seek(0);
        _wav.write(kgWavHeader(_written, KG_VOICE_RATE));
    }

    _wav.close();
    if (!ok) {
        QFile::remove(_wavPath);
    }

    if (_decoder) {
        opus_decoder_destroy(_decoder);
        _decoder = 0;
    }

    return ok;
}

int KgVoiceDecoder::percent() const
{
    const int size = _ogg.size();
    if (size <= 0) {
        return 0;
    }

    const int done = (int)((qint64) _at * 100 / size);
    return done > 100 ? 100 : done;
}

int KgVoiceDecoder::secondsDone() const
{
    return (int)(_written / KG_VOICE_RATE);
}

bool kgDecodeVoiceToWav(const QString &oggPath, const QString &wavPath, int *msSpent)
{
    KgVoiceDecoder decoder;

    if (!decoder.begin(oggPath, wavPath)) {
        return false;
    }

    // Без нарезки: кусок во весь файл.
    while (decoder.step(100000)) {
    }

    const bool ok = decoder.finish();

    if (msSpent) {
        *msSpent = decoder.msSpent();
    }

    return ok;
}
