#!/bin/sh
# Разворачивает на VPS два сервиса для Kutegram. Уже развёрнуто на
# ВАШ-СЕРВЕР; скрипт нужен, чтобы повторить это после правок в .py или на
# новом сервере.
#
#   kg-media  — конвертер: принимает файл, отдаёт AMR/mp3, GIF, PNG или 3GP.
#               Слушает только 127.0.0.1:8760, наружу его пускает nginx.
#   kg-bot    — бот @SymbianMediaBot: забирает файл из Telegram сам, отдаёт
#               его kg-media и присылает результат ответом на сообщение.
#
# Ватсаповый бэкенд не трогается: свой порт (8089), свой сайт nginx, свои
# юниты. Сайты wrp (80/443 по умолчанию) и whatsapp-nokia (8088) не меняются.
#
# Запуск с этой машины (Git Bash), из каталога симбиан/kutegram:
#
#   KG_BOT_TOKEN=123456:ABC... KG_SSH_KEY=~/.ssh/ваш_ключ \
#       sh deploy-vps.sh root@ВАШ-СЕРВЕР
#
# Токен бота кладётся в /etc/kg-media.env с правами 600, а не в юнит, который
# читают все. Токен конвертера генерируется при первом разворачивании и потом
# сохраняется: он вбит в телефоне, и менять его на каждом обновлении нельзя.

set -e

TARGET="$1"
PORT="${2:-22}"

if [ -z "$TARGET" ]; then
    echo "Usage: KG_BOT_TOKEN=... sh deploy-vps.sh user@host [port]" >&2
    exit 1
fi

if [ -z "$KG_BOT_TOKEN" ]; then
    printf "Telegram bot token: "
    read KG_BOT_TOKEN
fi

# Свой Telegram id: пока он 0, бот отказывает всем и только отвечает на /start,
# называя id написавшего.
KG_ALLOWED_USER="${KG_ALLOWED_USER:-0}"

IDENTITY=""
if [ -n "$KG_SSH_KEY" ]; then
    IDENTITY="-i $KG_SSH_KEY"
fi

SSH="ssh $IDENTITY -p $PORT $TARGET"

echo "=== копирую файлы ==="
scp $IDENTITY -P "$PORT" vps-media-convert.py vps-media-bot.py "$TARGET:/tmp/"

echo "=== ставлю сервисы ==="
$SSH KG_BOT_TOKEN="$KG_BOT_TOKEN" KG_ALLOWED_USER="$KG_ALLOWED_USER" 'bash -s' <<'REMOTE'
set -e

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "--- ставлю ffmpeg ---"
    apt-get update -qq
    apt-get install -y -qq ffmpeg
fi

install -m 755 /tmp/vps-media-convert.py /opt/kg-media.py
install -m 755 /tmp/vps-media-bot.py /opt/kg-bot.py
rm -f /tmp/vps-media-convert.py /tmp/vps-media-bot.py

# Токен конвертера переживает обновление: он вбит в телефоне.
CONV_TOKEN=""
if [ -f /etc/kg-media.env ]; then
    CONV_TOKEN=$(grep '^KG_VOICE_TOKEN=' /etc/kg-media.env | cut -d= -f2-)
fi
if [ -z "$CONV_TOKEN" ]; then
    CONV_TOKEN=$(openssl rand -hex 8)
    echo "новый токен конвертера: $CONV_TOKEN"
fi

umask 077
cat > /etc/kg-media.env <<ENV
KG_BOT_TOKEN=$KG_BOT_TOKEN
KG_ALLOWED_USER=$KG_ALLOWED_USER
KG_VOICE_TOKEN=$CONV_TOKEN
ENV
chmod 600 /etc/kg-media.env
umask 022

cat > /etc/systemd/system/kg-media.service <<'UNIT'
[Unit]
Description=Kutegram media converter
After=network.target

[Service]
ExecStart=/usr/bin/python3 /opt/kg-media.py
EnvironmentFile=/etc/kg-media.env
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
UNIT

cat > /etc/systemd/system/kg-bot.service <<'UNIT'
[Unit]
Description=Kutegram media bot
After=network.target kg-media.service

[Service]
ExecStart=/usr/bin/python3 /opt/kg-bot.py
EnvironmentFile=/etc/kg-media.env
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
UNIT

# Свой сайт на своём порту: ничего из существующего не задевается.
cat > /etc/nginx/sites-available/kg-media <<'CONF'
server {
    listen 8089;
    listen [::]:8089;
    server_name _;

    location /kg-media {
        proxy_pass http://127.0.0.1:8760;
        proxy_set_header X-Kutegram-Token $http_x_kutegram_token;
        client_max_body_size 16m;
        proxy_read_timeout 180s;
        proxy_send_timeout 180s;
    }

    location / {
        return 404;
    }
}
CONF
ln -sf /etc/nginx/sites-available/kg-media /etc/nginx/sites-enabled/kg-media
nginx -t >/dev/null 2>&1 && systemctl reload nginx

systemctl daemon-reload
systemctl enable --now kg-media >/dev/null 2>&1
systemctl restart kg-media
systemctl enable --now kg-bot >/dev/null 2>&1
systemctl restart kg-bot
sleep 3

echo "kg-media: $(systemctl is-active kg-media)"
echo "kg-bot:   $(systemctl is-active kg-bot)"
echo "адрес для телефона: http://$(hostname -I | awk '{print $1}'):8089/kg-media"
echo "токен для телефона: $CONV_TOKEN"
REMOTE

echo
echo "=== готово ==="
echo "Дальше: написать боту /start, он ответит вашим id."
echo "Вписать id в KG_ALLOWED_USER в /etc/kg-media.env и:"
echo "  $SSH 'systemctl restart kg-bot'"
echo
echo "Логи:  $SSH 'journalctl -u kg-bot -u kg-media -n 50 --no-pager'"
