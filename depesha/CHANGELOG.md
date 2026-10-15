# Changelog

**English** · [Русский](CHANGELOG.ru.md)

The version is shown in the window title and in the first line of the log
(`C:/Data/Depesha/depesha.log`). There are no ready-made packages here: a
`.sis` carries the Telegram API keys in plain text and everyone needs their
own — build it with `СОБРАТЬ.bat` in the root.

## 0.5.3

* **Sending voice messages.** "Record voice" in the conversation menu: the
  phone records from the microphone, compresses to Opus itself and sends an
  ordinary voice message. While recording, the left soft key stops, the right
  one cancels, the joystick centre stops as well; the key clicks at both ends
  are trimmed off.

## 0.4.4

* **Receiving voice messages without a server.** The phone decodes Opus
  itself, about four times faster than real time — a one-minute message takes
  some fourteen seconds. The work is sliced up, so the phone stays responsive.
* **Still previews for animated and video stickers** when the media server is
  off: a picture instead of the word "Sticker".
* **Depesha has its own folder**: settings, session, log and cache are no
  longer shared with Kutegram. The session and settings move themselves on
  first run.
* The version in the title stopped lying: a package used to call itself
  whatever version `main.cpp` was last changed in.
* When the server ends the session (`401`), the reason now reaches the log.

## 0.3.8

* Cache and downloads can live on the memory card, trimming runs on a timer,
  and settings got a "clear cache" button.

## 0.3.7

* First published version.
