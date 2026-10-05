#!/usr/bin/env python3
# Медиа-конвертер для Kutegram на Nokia E90.
#
# Зачем: телефон 2007 года не декодирует ничего из того, чем Telegram
# пользуется сегодня — голосовые это Opus (2012), видеостикеры VP9 (2013),
# анимированные стикеры вообще векторная анимация. Декодера нет и не будет.
# Зато телефон отлично играет то, что для него уже перегнали: AMR, mp3, 3GP,
# а анимированный GIF Qt рисует сам, прямо в переписке.
#
# Ровно так же работают голосовые в тамошнем ватсапе: сервер-мост перегоняет
# их ffmpeg'ом, а телефон получает mp3.
#
# Телефон качает файл с серверов Telegram напрямую, шлёт байты сюда POST'ом и
# получает результат. Что именно вернуть, говорит параметр want:
#
#   POST /convert?want=audio      -> AMR (или mp3, если ffmpeg без AMR)
#   POST /convert?want=animation  -> анимированный GIF, до 15 секунд
#   POST /convert?want=image      -> один кадр PNG
#   POST /convert?want=video      -> 3GP для полноэкранного плеера
#   GET  /                        -> проверка связи из настроек клиента
#
# Расширение файла клиент берёт из Content-Type ответа.
#
# ВАЖНО: через сервис проходит содержимое личной переписки. Он должен стоять
# на своём VPS. Если он смотрит наружу, задайте KG_VOICE_TOKEN — тогда чужой
# им не воспользуется.
#
# Установка на VPS:
#   apt install ffmpeg
#   cp vps-media-convert.py /opt/kg-media.py
#   cat > /etc/systemd/system/kg-media.service <<'EOF'
#   [Unit]
#   Description=Kutegram media converter
#   After=network.target
#   [Service]
#   ExecStart=/usr/bin/python3 /opt/kg-media.py
#   Environment=KG_VOICE_TOKEN=придумайте-длинную-строку
#   Restart=always
#   [Install]
#   WantedBy=multi-user.target
#   EOF
#   systemctl enable --now kg-media
#
# nginx (рядом с уже раздающимся /srs/):
#   location /kg-media {
#       proxy_pass http://127.0.0.1:8760;
#       client_max_body_size 16m;
#       proxy_read_timeout 180s;
#   }
#
# В клиенте: шторка -> Media server... -> адрес http://ВАШ-СЕРВЕР:8089/kg-media
# и тот же токен.

import http.server
import os
import socketserver
import subprocess
import sys
import tempfile

PORT = 8760
HOST = "127.0.0.1"

# Голосовое — десятки килобайт, кружочек — единицы мегабайт. Всё, что сильно
# больше, телефон и не отправит.
MAX_BODY = 16 * 1024 * 1024

TIMEOUT = 180

# Необязательный общий секрет: сервис смотрит наружу через nginx, а перегонять
# чужое видео за свой счёт незачем. Клиент шлёт его в X-Kutegram-Token.
TOKEN = os.environ.get("KG_VOICE_TOKEN", "")

FFMPEG = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y"]


# Через файлы, а не через pipe: mp4 и webm держат оглавление в конце файла,
# и по неперематываемому потоку ffmpeg их просто не прочитает — проверено,
# "partial file" на первом же кружочке.
def audio_amr(src, dst):
    # AMR-NB 12.2 кбит/с — родной формат диктофона S60. 30 секунд это 48 КБ
    # против 120 КБ у mp3 32 кбит/с, то есть меньше самого оригинала.
    return FFMPEG + ["-i", src, "-vn", "-ac", "1", "-ar", "8000",
                     "-b:a", "12.2k", "-c:a", "libopencore_amrnb",
                     "-f", "amr", dst]


def audio_mp3(src, dst):
    # Запасной путь: кодировщик AMR есть не в каждой сборке ffmpeg, mp3 есть
    # везде.
    return FFMPEG + ["-i", src, "-vn", "-ac", "1", "-ar", "16000",
                     "-b:a", "16k", "-f", "mp3", dst]


def animation_gif(src, dst):
    # Кружочки, видеостикеры и гифки. 128 пикселей, 8 кадров в секунду, не
    # длиннее 15 секунд: дальше размер растёт быстрее пользы. Палитра считается
    # по самому ролику, иначе GIF выходит и больше, и грязнее.
    return FFMPEG + ["-i", src, "-t", "15", "-filter_complex",
                     "fps=8,scale=128:-1:flags=lanczos,split[a][b];"
                     "[a]palettegen[p];[b][p]paletteuse",
                     "-f", "gif", dst]


def single_frame(src, dst):
    # Обложка для длинного видео и запасной путь, когда анимация не вышла.
    return FFMPEG + ["-i", src, "-frames:v", "1", "-vf", "scale=192:-1",
                     "-f", "image2", "-c:v", "png", dst]


def video_3gp(src, dst):
    # Не H.264: MPEG-4 SP в 3GP E90 играет наверняка, H.264 — как повезёт.
    return FFMPEG + ["-i", src, "-c:v", "mpeg4", "-vf", "scale=320:-2",
                     "-r", "15", "-b:v", "256k",
                     "-c:a", "aac", "-ac", "1", "-ar", "16000", "-b:a", "24k",
                     "-f", "3gp", dst]


# По одному варианту на строку: если первый не получился, берётся следующий.
WANTED = {
    "audio": [(audio_amr, "audio/amr", ".amr"), (audio_mp3, "audio/mpeg", ".mp3")],
    "animation": [(animation_gif, "image/gif", ".gif"), (single_frame, "image/png", ".png")],
    "image": [(single_frame, "image/png", ".png")],
    "video": [(video_3gp, "video/3gpp", ".3gp"), (single_frame, "image/png", ".png")],
}


class Handler(http.server.BaseHTTPRequestHandler):
    # По умолчанию BaseHTTPRequestHandler пишет в stderr на каждый запрос,
    # в journal это лишний шум.
    def log_message(self, fmt, *args):
        pass

    def reply(self, code, content_type, body):
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def fail(self, code, text):
        self.reply(code, "text/plain; charset=utf-8", text.encode("utf-8"))

    def authorized(self):
        if not TOKEN:
            return True

        if self.headers.get("X-Kutegram-Token", "") == TOKEN:
            return True

        self.fail(403, "bad token")
        return False

    def wanted(self):
        if "?" not in self.path:
            return "audio"

        for part in self.path.split("?", 1)[1].split("&"):
            if part.startswith("want="):
                return part[5:]

        return "audio"

    def do_POST(self):
        if not self.authorized():
            return

        try:
            length = int(self.headers.get("Content-Length", 0))
        except ValueError:
            length = 0

        if length <= 0:
            self.fail(400, "empty body")
            return

        if length > MAX_BODY:
            self.fail(413, "too large")
            return

        want = self.wanted()
        if want not in WANTED:
            self.fail(400, "unknown want=" + want)
            return

        data = self.rfile.read(length)
        last_error = "no attempt succeeded"

        with tempfile.TemporaryDirectory(prefix="kgmedia") as workdir:
            source = os.path.join(workdir, "in")
            with open(source, "wb") as handle:
                handle.write(data)

            for build, content_type, extension in WANTED[want]:
                target = os.path.join(workdir, "out" + extension)

                try:
                    result = subprocess.run(
                        build(source, target), stdout=subprocess.DEVNULL,
                        stderr=subprocess.PIPE, timeout=TIMEOUT)
                except subprocess.TimeoutExpired:
                    self.fail(504, "ffmpeg timeout")
                    return
                except FileNotFoundError:
                    self.fail(500, "ffmpeg not installed")
                    return

                if result.returncode == 0 and os.path.exists(target) \
                        and os.path.getsize(target) > 0:
                    with open(target, "rb") as handle:
                        self.reply(200, content_type, handle.read())
                    return

                last_error = result.stderr.decode("utf-8", "replace")[:300]

        self.fail(422, "cannot convert: " + last_error)

    def do_GET(self):
        # Проверка связи из настроек клиента: жив ли сервис и подходит ли токен.
        if not self.authorized():
            return

        self.fail(200, "media server is running")


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


if __name__ == "__main__":
    try:
        with Server((HOST, PORT), Handler) as httpd:
            httpd.serve_forever()
    except KeyboardInterrupt:
        sys.exit(0)
