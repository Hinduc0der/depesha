# Building Depesha yourself

**English** · [Русский](BUILDING.ru.md)

There are no ready-made packages, and here is why: every built `.sis` has
**Telegram API keys** baked into it, and everyone needs their own (the reason
is below). So Depesha is built from source. That takes longer than downloading
a file, but it is done once, and following the steps below it costs an evening,
most of which is waiting for an installer.

You need a **Windows** machine. The toolchain is from 2011, and there is
nothing else left for Symbian.

---

## 1. Install the Nokia Qt SDK

You need **Nokia Qt SDK 1.1**, which contains Qt 4.7.3 for Symbian and the
GCCE compiler. Nokia stopped handing it out long ago; live mirrors can be
found by the installer's file name,
`Nokia_Qt_SDK_Win_offline_v1_1_2_en.exe` — on archive.org, for instance.

When installing:

- install into **a path with no spaces and no non-Latin characters**; the
  default `C:\QtSDK` is what the build scripts expect;
- tick **Symbian: Qt 4.7.3** and the **Symbian^1 SDK** (in the wizard:
  "Symbian" → "Qt 4.7.3 for Symbian Anna");
- it takes about 3 GB.

To check: the file
`C:\QtSDK\Symbian\SDKs\Symbian1Qt473\epoc32\tools\abld.pl` must exist.

If the SDK landed somewhere else, the paths are at the top of
`build-symbian.bat` (`SDKPREFIX`, `SYMBIANDIR`, `EPOCROOT`).

## 2. Get your own Telegram API keys

1. Open **my.telegram.org** and sign in with your phone number.
2. **API development tools** → fill in the short form (any application name;
   platform Other).
3. You get an **App api_id** (a number) and an **App api_hash** (32
   characters).

Now copy `kutegram/quick/libkg/apisecrets.h.empty` to
`kutegram/quick/libkg/apisecrets.h` and put them in:

```c
#define KUTEGRAM_API_ID 12345678
#define KUTEGRAM_API_HASH "0123456789abcdef0123456789abcdef"
```

Without that file the build stops with a clear error: `Please, specify an
API id.`

> **Why you cannot use somebody else's.** A key belongs to whoever issued it,
> and that person answers for everything done with it. Telegram restricts and
> blocks keys it sees being used en masse from nowhere — together with the
> owner's account. Issuing your own takes two minutes, and it is exactly why
> **you should not pass a built `.sis` to anyone**: the keys are inside it.

## 3. Build

Put the two directories side by side, the way they sit in the repository:

```
...\kutegram\quick\      shared code: protocol and models
...\depesha\             Depesha itself
```

and run:

```
depesha\build-symbian.bat
```

The script copies the sources to `C:\dpbuild` on its own (the 2011 toolchain
cannot cope with non-Latin characters in paths), builds, and leaves
`Depesha-<version>.sis` next to itself. The first build takes about ten
minutes; later ones are faster.

If something goes wrong, the compiler's full output stays in
`C:\dpbuild\build.log`.

To wipe the working tree: `build-symbian.bat clean`.

## 4. Install it on the phone

The `.sis` is **self-signed**, so the phone will ask for permission.

Once, on the phone:

- **Tools → App. mgr. → Options → Settings**:
  - **Software installation: All**;
  - **Online certif. check: Off**.

Then copy the `.sis` to the phone (cable, Bluetooth, memory card) and open it
in the file manager. The installer asks for a drive; any of them will do.

## 5. Media server (optional)

This phone does not decode voice messages, video notes, video stickers or
GIFs, and it never will: no Opus, no VP9, no Lottie. **Your own server** turns
them into something the phone understands (mp3, GIF, 3GP) — any machine with
`ffmpeg`; the script and its deployment are in
`kutegram/vps-media-convert.py` and `kutegram/deploy-vps.sh`.

The address and token are entered on the phone: left soft key → **Settings** →
Media server. To avoid typing them after every reinstall, copy
`depesha/testdefaults.h.example` to `depesha/src/testdefaults.h` and fill in
your own — the build picks them up by itself.

**Without a server everything else works fully**: text, photos, ordinary
stickers, folders, replies, silent and scheduled sending. Voice messages and
video are marked `no media server, press to save` in the list — the file can
be downloaded and picked up on a computer.

---

## If you build often

- **Bump `VERSION` in `depesha.pro` before every build.** The script puts it
  in the file name, packages are installed by hand and they pile up — without
  it you cannot tell what is actually on the phone. The version is visible in
  the application's title.
- `build-desktop.bat` builds the same thing for Windows (the mingw compiler
  from the same SDK). Nothing phone-specific runs there, but compile errors
  show up in half a minute instead of ten.
- **Do not run two builds at once**: both work in `C:\dpbuild` and overwrite
  each other's intermediate files.
