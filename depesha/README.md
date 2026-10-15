# Depesha

**English** · [Русский](README.ru.md)

A Telegram client for Symbian S60v3 phones — the ones released around 2007.
Built on plain Qt 4.7 widgets, no QML.

Made for the **Nokia E90**: 332 MHz, 56 MB of free memory, two screens
(240×320 on the outside, 800×352 under the lid), a joystick and a keyboard.

![icon](depesha.svg)

## How it differs from Kutegram

Depesha grew out of [Kutegram](https://github.com/Kutegram/Kutegram) and uses
its protocol library (`libkg`) and data models file for file. Only the
interface was rewritten.

The reason is simple: on a 332 MHz phone, QML 1 draws every list row as a tree
of a dozen or more elements with their own bindings, and the result does not
scroll. In Depesha a list row is one text draw over a fill, and the QML engine
is never loaded at all.

What came out of that, besides speed:

- **the chat list is not fetched in full** at sign-in, and avatars are
  downloaded only for visible rows — before that the phone was busy for
  minutes after startup;
- **photos** show immediately as the blurred preview that arrives with the
  message, then load at a sensible size rather than the largest one;
- **heavy things are not downloaded on their own**: voice messages, video
  notes, GIFs and video are marked `[voice]`, `[gif]`, `[video]` in the list,
  and nothing happens until you press.

## What it can do

Text, photos, ordinary stickers, Telegram folders, replying to a message,
sending silently and scheduled sending, read receipts, viewing a picture
full-screen, downloading and opening files.

**Voice messages — both ways, on the phone itself.** Incoming ones are decoded
by the phone: a fixed-point Opus decoder is built into the project. To send
one, pick **"Record voice"** from the menu in a conversation; while recording,
the left soft key stops, the right one cancels, and the joystick centre stops
as well. What you record is compressed to Opus and sent as an ordinary voice
message. No media server is needed for voice any more.

Video notes, video stickers and GIFs still go through your own media server —
see below. Without a server, animated and video stickers show the still
preview picture instead of the animation.

**Depesha has its own folder.** Settings, the session, the log and the cache
live in `C:/Data/Depesha` (`E:/Data/Depesha` on the memory card), downloads in
`Downloads/Depesha`. All of it used to be shared with Kutegram: one log file
for both clients and a common cache each of them trimmed its own way. On the
first run of this version the session and settings move themselves; files
downloaded earlier stay where they are. The log is now
`C:/Data/Depesha/depesha.log`.

## Media server (optional)

A phone from 2007 does not decode VP9 (video stickers)
or Lottie (animated stickers), and it never will: there are no libraries for
them on S60v3, and a software decoder at 332 MHz would not keep up with real
time.

So the conversion lives on a server: it takes a file and returns something the
phone can handle — mp3, GIF or 3GP. The server is **your own**: your voice
messages go through it, and you should not hand them to somebody else's
address.

**Without a server** the application works fully apart from those types: text,
photos and ordinary stickers behave as usual, while voice messages, GIFs and
video are marked `no media server, press to save` in the list — the file can
be downloaded and picked up on a computer.

The address and token are entered on the phone: left soft key → **Settings** →
Media server. To avoid typing them after every reinstall, copy
`testdefaults.h.example` to `src/testdefaults.h` and fill in your own — the
build will use them on first run. That file is deliberately not in the
repository.

The server side is in `../kutegram/vps-media-convert.py` and `deploy-vps.sh`.

## Controls

Documented separately: [CONTROLS.md](CONTROLS.md) — which keys the phone
offers, what Qt turns them into, and what each one does on each screen. In
short:

| Key | What it does |
|---|---|
| joystick | moves through the list; **centre** opens a chat, does whatever the selected message calls for, and in the input field sends |
| any letter, in a conversation | moves both the focus and the letter itself into the input field |
| left soft key | menu: folders, sending, settings |
| right soft key | back |

## Building

Step by step, including installing the SDK and getting keys:
**[BUILDING.md](BUILDING.md)**.

In short: you need **Nokia Qt SDK 1.1** (Qt 4.7.3 for Symbian, GCCE, abld),
your own Telegram API keys in `../kutegram/quick/libkg/apisecrets.h`, and
`build-symbian.bat`. The `kutegram/quick` and `depesha` directories must sit
side by side.

**There are no ready-made packages, on purpose.** Every built `.sis` has
Telegram API keys baked into it, and a key belongs to whoever issued it — who
also answers for anyone else's use of it. Issuing your own at my.telegram.org
takes two minutes.

## Credits

The code was written by **Claude** (Anthropic's Claude Code); the repository
owner set the tasks and tested every build on a live Nokia E90 — without that
testing, half the decisions here would have been wrong.

## License

GPL-3.0 — same as Kutegram, which the protocol library and the models come
from. License text in [COPYING](COPYING).
