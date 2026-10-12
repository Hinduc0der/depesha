# Depesha — a Telegram client for Symbian S60v3

**English** · [Русский](README.ru.md)

> Idea, requirements and all testing on real hardware — [@Hinduc0der](https://github.com/Hinduc0der).
> Code written by Claude (Anthropic) under his direction.

Phones from around 2007: Nokia E90, N95, E71 and the rest of the S60 3rd
edition family. Built and tested on an **E90**.

Two directories:

- **`depesha/`** — Depesha itself: the interface, built on plain Qt 4.7
  widgets, no QML. [README](depesha/README.md) ·
  [Building](depesha/BUILDING.md) · [Controls](depesha/CONTROLS.md)
- **`kutegram/`** — [Kutegram](https://github.com/Kutegram/Kutegram), which
  this takes the protocol library (`libkg`) and the data models from, plus the
  server side of the media converter.

## Build it in one go

1. Install **Nokia Qt SDK 1.1** into `C:\QtSDK`. It has to be installed
   separately — it is Nokia's toolchain and cannot be redistributed here. The
   installer is called `Nokia_Qt_SDK_Win_offline_v1_1_2_en.exe`.
2. Get your own Telegram API keys at **my.telegram.org** →
   *API development tools*.
3. Run **`СОБРАТЬ.bat`** — "build" in Russian, the only batch file in the
   root. It asks for the keys, saves them and builds the `.sis`.
   **Its prompts are in Russian**; if that is in the way, do the same three
   steps by hand from [depesha/BUILDING.md](depesha/BUILDING.md) and run
   `depesha\build-symbian.bat`, which prints nothing you need to read.

The long version, with every detail and the phone settings you need, is in
[depesha/BUILDING.md](depesha/BUILDING.md).

## Why there is no ready-made package

Every built `.sis` has **Telegram API keys baked into it**. A key belongs to
whoever issued it, and that person answers for everything done with it:
Telegram restricts keys it sees being used en masse from nowhere, together with
the owner's account. Issuing your own takes two minutes, and it stays yours.

## How this was made

The code was written by **Claude** (Anthropic's Claude Code). The repository
owner set the tasks, tested every build on a live Nokia E90, and found the
things a computer cannot show you: interface stalls, the memory card dropping
out, menu entries nobody could understand, messages sent to the wrong place.
Almost every decision in this project came out of that loop — the development
log is mostly "made it → tried it on the phone → turned out otherwise".

## License

GPL-3.0, same as Kutegram — text in [COPYING](COPYING). Bundled libraries:
mbedtls (Apache-2.0), zlib, libwebp (BSD-3-Clause), qt-json (MIT), each under
its own license in its own directory.
