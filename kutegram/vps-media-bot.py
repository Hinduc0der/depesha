#!/usr/bin/env python3
# Бот-конвертер медиа для Kutegram на Nokia E90.
#
# Зачем нужен второй способ. Прямой (vps-media-convert.py) заставляет телефон
# скачать файл, отправить его целиком на сервер и скачать результат — три
# ходки. Для голосового это ерунда, для видео на несколько мегабайт — нет:
# отдача у E90 идёт по обычному UMTS, около 48 КБ/с.
#
# Здесь разворот обратный: файл забирает сам сервер. Клиент пересылает боту
# сообщение (это несколько байт), бот скачивает файл через Bot API, перегоняет
# и присылает результат ответом на пересланное сообщение. Телефон качает только
# результат — одна ходка вместо трёх.
#
# Пересылку делает сам клиент в момент нажатия «играть». Руками пересылать
# ничего не надо.
#
# Что во что превращается, решает сам бот по типу файла:
#   голосовое, аудио      -> AMR (телефон играет его штатно)
#   кружочек, видеостикер,
#   гифка, короткое видео -> анимированный GIF (рисуется прямо в переписке)
#   длинное видео         -> 3GP для полноэкранного плеера
#
# Само перегонянние делает vps-media-convert.py на 127.0.0.1:8760 — тот же
# сервис, что и для прямого способа. Наружу его выставлять при этом не нужно.
#
# ВАЖНО: у бота будет копия каждого пересланного файла, и она останется в
# Telegram в переписке с ботом. Это плата за экономию трафика; прямой способ
# такой копии не создаёт.
#
# Установка на VPS (рядом с ватсаповым бэкендом):
#   1. Создать бота у @BotFather, забрать токен.
#   2. Узнать свой Telegram id: написать боту /start, он ответит числом.
#      Вписать это число в KG_ALLOWED_USER, иначе ботом сможет пользоваться
#      кто угодно, кто его найдёт.
#   3. cp vps-media-bot.py /opt/kg-bot.py
#      cat > /etc/systemd/system/kg-bot.service <<'EOF'
#      [Unit]
#      Description=Kutegram media bot
#      After=network.target kg-media.service
#      [Service]
#      ExecStart=/usr/bin/python3 /opt/kg-bot.py
#      Environment=KG_BOT_TOKEN=123456:ABC...
#      Environment=KG_ALLOWED_USER=0
#      Restart=always
#      [Install]
#      WantedBy=multi-user.target
#      EOF
#      systemctl enable --now kg-bot
#   4. journalctl -u kg-bot -f  — посмотреть, что живой.
#
# Требуется работающий vps-media-convert.py: systemctl status kg-media

import io
import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

TOKEN = os.environ.get("KG_BOT_TOKEN", "")

# 0 значит «никого не пускать»: пока сюда не вписан ваш id, бот только
# отвечает на /start и называет id того, кто написал.
ALLOWED_USER = int(os.environ.get("KG_ALLOWED_USER", "0"))

CONVERT_URL = os.environ.get("KG_CONVERT_URL", "http://127.0.0.1:8760/convert")
CONVERT_TOKEN = os.environ.get("KG_VOICE_TOKEN", "")

API = "https://api.telegram.org/bot" + TOKEN
FILES = "https://api.telegram.org/file/bot" + TOKEN

# Bot API не отдаёт файлы больше 20 МБ.
MAX_FILE = 20 * 1024 * 1024

# Дольше этого видео не стоит превращать в GIF — получится файл, который
# телефон будет качать дольше, чем смотреть.
GIF_MAX_SECONDS = 20

TIMEOUT = 300


def log(text):
    # systemd сам поставит время, поэтому здесь только суть.
    print(text, flush=True)


def api(method, params=None):
    data = urllib.parse.urlencode(params or {}).encode("utf-8")

    try:
        with urllib.request.urlopen(API + "/" + method, data=data, timeout=70) as response:
            return json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as error:
        return json.loads(error.read().decode("utf-8"))
    except Exception as error:
        log("api %s failed: %s" % (method, error))
        return None


def send_message(chat_id, text, reply_to=None):
    params = {"chat_id": chat_id, "text": text}
    if reply_to:
        params["reply_to_message_id"] = reply_to

    api("sendMessage", params)


# Multipart руками: у сервера нет ничего кроме стандартной библиотеки, и
# заводить зависимость ради одной формы незачем.
def send_document(chat_id, reply_to, filename, content):
    boundary = "----kutegram" + str(int(time.time() * 1000))
    body = io.BytesIO()

    def field(name, value):
        body.write(("--%s\r\n" % boundary).encode())
        body.write(('Content-Disposition: form-data; name="%s"\r\n\r\n' % name).encode())
        body.write((str(value) + "\r\n").encode())

    field("chat_id", chat_id)
    if reply_to:
        field("reply_to_message_id", reply_to)

    body.write(("--%s\r\n" % boundary).encode())
    body.write(('Content-Disposition: form-data; name="document"; filename="%s"\r\n'
                % filename).encode())
    body.write(b"Content-Type: application/octet-stream\r\n\r\n")
    body.write(content)
    body.write(("\r\n--%s--\r\n" % boundary).encode())

    request = urllib.request.Request(
        API + "/sendDocument", data=body.getvalue(),
        headers={"Content-Type": "multipart/form-data; boundary=" + boundary})

    try:
        with urllib.request.urlopen(request, timeout=TIMEOUT) as response:
            return json.loads(response.read().decode("utf-8"))
    except Exception as error:
        log("sendDocument failed: %s" % error)
        return None


def download(file_id):
    answer = api("getFile", {"file_id": file_id})
    if not answer or not answer.get("ok"):
        return None, "getFile failed"

    info = answer["result"]
    if info.get("file_size", 0) > MAX_FILE:
        return None, "file is larger than the 20 MB the Bot API will give out"

    try:
        with urllib.request.urlopen(FILES + "/" + info["file_path"], timeout=TIMEOUT) as response:
            return response.read(), None
    except Exception as error:
        return None, "download failed: %s" % error


def convert(content, want):
    request = urllib.request.Request(
        CONVERT_URL + "?want=" + want, data=content,
        headers={"Content-Type": "application/octet-stream"})

    if CONVERT_TOKEN:
        request.add_header("X-Kutegram-Token", CONVERT_TOKEN)

    try:
        with urllib.request.urlopen(request, timeout=TIMEOUT) as response:
            return response.read(), response.headers.get("Content-Type", ""), None
    except urllib.error.HTTPError as error:
        return None, None, error.read().decode("utf-8", "replace")[:200]
    except Exception as error:
        return None, None, "converter unreachable: %s" % error


# Что за файл прислали и во что его превращать.
def classify(message):
    if "voice" in message:
        return message["voice"]["file_id"], "audio", "voice"

    if "audio" in message:
        return message["audio"]["file_id"], "audio", "audio"

    if "video_note" in message:
        note = message["video_note"]
        want = "animation" if note.get("duration", 0) <= GIF_MAX_SECONDS else "video"
        return note["file_id"], want, "video note"

    if "animation" in message:
        animation = message["animation"]
        want = "animation" if animation.get("duration", 0) <= GIF_MAX_SECONDS else "video"
        return animation["file_id"], want, "animation"

    if "sticker" in message:
        sticker = message["sticker"]
        if sticker.get("is_video"):
            return sticker["file_id"], "animation", "video sticker"
        # Обычный стикер клиент рисует сам (libwebp), анимированный .tgs
        # ffmpeg не читает — это Lottie, а не видео.
        return None, None, "sticker"

    if "video" in message:
        video = message["video"]
        want = "animation" if video.get("duration", 0) <= GIF_MAX_SECONDS else "video"
        return video["file_id"], want, "video"

    if "document" in message:
        document = message["document"]
        mime = document.get("mime_type", "")

        if mime.startswith("audio/"):
            return document["file_id"], "audio", "audio file"
        if mime.startswith("video/"):
            return document["file_id"], "video", "video file"

        return None, None, "document"

    return None, None, "message"


EXTENSIONS = {
    "audio/amr": ".amr",
    "audio/mpeg": ".mp3",
    "image/gif": ".gif",
    "image/png": ".png",
    "video/3gpp": ".3gp",
}


def handle(message):
    chat_id = message["chat"]["id"]
    message_id = message["message_id"]
    user_id = message.get("from", {}).get("id", 0)

    text = message.get("text", "")
    if text.startswith("/start") or text.startswith("/id"):
        send_message(chat_id, "Kutegram media bot.\nYour Telegram id is %d.\n"
                              "Put it into KG_ALLOWED_USER and restart the service."
                     % user_id, message_id)
        return

    if ALLOWED_USER == 0 or user_id != ALLOWED_USER:
        log("ignoring a message from %d" % user_id)
        send_message(chat_id, "This bot only works for its owner.", message_id)
        return

    file_id, want, what = classify(message)
    if not file_id:
        send_message(chat_id, "Nothing here to convert (%s)." % what, message_id)
        return

    log("converting a %s for %d" % (what, user_id))

    content, error = download(file_id)
    if error:
        send_message(chat_id, error, message_id)
        return

    converted, content_type, error = convert(content, want)
    if error:
        send_message(chat_id, "Conversion failed: " + error, message_id)
        return

    extension = EXTENSIONS.get(content_type.split(";")[0].strip(), ".bin")

    # Ответом на пересланное сообщение: по этой связи клиент и находит, к какой
    # строке в переписке относится результат.
    result = send_document(chat_id, message_id, "kutegram" + extension, converted)

    if not result or not result.get("ok"):
        send_message(chat_id, "Could not send the result back.", message_id)
        return

    log("sent back %d bytes as %s" % (len(converted), extension))


def main():
    if not TOKEN:
        log("KG_BOT_TOKEN is not set")
        return 1

    me = api("getMe")
    if not me or not me.get("ok"):
        log("the token is not working")
        return 1

    log("started as @%s" % me["result"].get("username", "?"))

    if ALLOWED_USER == 0:
        log("KG_ALLOWED_USER is not set: write /start to the bot to learn your id")

    offset = 0
    while True:
        updates = api("getUpdates", {"offset": offset, "timeout": 50})

        if not updates or not updates.get("ok"):
            # Сеть моргнула или Telegram не в духе: подождать и продолжить,
            # а не падать -- сервис должен пережить это сам.
            time.sleep(5)
            continue

        for update in updates["result"]:
            offset = update["update_id"] + 1

            message = update.get("message")
            if not message:
                continue

            try:
                handle(message)
            except Exception as error:
                log("failed to handle a message: %s" % error)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(0)
