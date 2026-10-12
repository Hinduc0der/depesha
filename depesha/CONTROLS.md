# Depesha: keyboard controls

**English** · [Русский](CONTROLS.ru.md)

This document describes how the application is driven on a Nokia E90, and why
it works that way. It was written before the code was changed, so that the
controls would not be fixed one key at a time: on a feature phone the controls
are a system, and a hole in one place (nothing to send a message with) appears
precisely when every key is bound on its own.

## 1. What the device actually has

**Outside (lid closed), 240x320 screen.** A numeric keypad with multi-tap, a
joystick (four directions and a centre press), two soft keys, `C` (clear), and
the green and red keys.

**Inside (lid open), 800x352 screen.** A full QWERTY keyboard with `Enter`,
`Backspace`, `Shift` and `Ctrl`, its own joystick with a centre press, and its
own two soft keys.

What Qt sees of all that:

| Key | Qt event |
|---|---|
| joystick centre | `Qt::Key_Select` |
| joystick directions | `Key_Up`, `Key_Down`, `Key_Left`, `Key_Right` |
| left soft key | the application menu (`QMenuBar`), or an action with the `PositiveSoftKey` role |
| right soft key | an action with the `NegativeSoftKey` role — "Back", here |
| `C` | `Key_Backspace` |
| `Enter` (inside) | `Key_Return` / `Key_Enter` |
| letters and digits | ordinary events with a non-empty `QKeyEvent::text()` |

**The key fact: `Key_Select` is not `Enter`.** A single-line Qt input field
does not react to `Key_Select` by itself and does not emit `returnPressed()`.
So there was **nothing to send** a message typed on the outside keypad with:
`Enter` only exists under the lid.

## 2. How Qt hands out keys on this phone

The application starts in `NavigationModeKeypadDirectional`. In that mode:

- the arrows **move the focus** between widgets rather than reaching inside a
  widget;
- for a widget to start receiving the arrows itself (to scroll, to move a
  cursor) it needs **edit focus** — given by a centre press, or by
  `QWidget::setEditFocus(true)` from code;
- without edit focus a list does not scroll at all, and an input field accepts
  no characters.

Two consequences follow, and both have already been walked into (sections 51
and 52 of the project log): the focus must not be given to a container page,
and edit focus has to be set by hand for input fields and lists.

## 3. Principles

1. **Centre = do the obvious thing here.** In a list, open what is selected;
   in the input field, send. One key, and the meaning depends on where the
   focus is.
2. **Right soft key = one step back.** Always and everywhere, right up to
   leaving the application.
3. **Typing starts immediately.** Wherever the focus happens to be in a
   conversation, a typed letter moves itself into the input field along with
   the focus. There is no need to walk down to the field first.
4. **The focus never reaches a dead end.** Down from the end of the list goes
   into the input field; up from the field goes back into the list.
5. **Heavy things only on a press.** A voice message, a GIF, a video note and
   a video are only marked in the list (`[voice]`, `[gif]`, `[video]`);
   downloading, converting and playing start with a centre press on the
   selected message, and by nothing else.
6. **Left soft key = menu.** Everything that does not fit on a single key
   lives there; what the menu contains depends on the screen.

## 4. Screen by screen

### 4.1 Sign-in (number, code, password)

| Key | Action |
|---|---|
| letters, digits | type into the field (focus and edit focus are set when the page is shown) |
| centre, `Enter` | submit what was typed (number → code → password) |
| up / down | between the field and the button |
| right soft | back (on the first step, exit) |

### 4.2 Chat list

| Key | Action |
|---|---|
| up / down | move through the list (needs edit focus, set when shown) |
| centre | open the selected chat |
| left soft | menu: **Folders** (all chats and Telegram folders), **Settings**, **Back** |
| right soft | leave the application |

Scrolling to the end of the list loads the next page of chats.

### 4.3 Conversation

The focus has two homes: the **message list** and the **input field**. Which
one has it is visible from the selection frame.

Focus in the message list:

| Key | Action |
|---|---|
| up / down | move through the messages; at the top, older ones are loaded |
| **down on the last message** | move into the input field |
| **centre** | act on the selected message: download, show, play, open |
| **any letter or digit** | move into the input field and type it there |
| right soft | back to the chat list |

The left soft key menu in a conversation: **Send** (only when there is text in
the field), **Open message** (the same as centre on the selected message),
**Settings**, **Back**, and the conversation-specific entries:

| Entry | When it is visible | What it does |
|---|---|---|
| Reply | in a conversation | reply to the selected message; who you are replying to is shown in the header |
| Cancel reply | when a reply is set | clear it |
| Send without sound | when there is something to send | **sends** what is typed, with no sound for the recipient |
| Send in an hour | when there is something to send | sends what is typed an hour from now |
| Send tomorrow 9:00 | when there is something to send | sends what is typed at nine tomorrow morning |

All three are **commands, not settings**: pick the entry and the message is
gone. The scheduled time is counted **by the server's clock**, not the
phone's.

Picking an hour and a minute on this keyboard is its own kind of misery, which
is why the delays are fixed ones.

> The **"Actions"** section in the menu is not ours: on Symbian, Qt builds it
> out of **window**-level actions, and ours is the "back" action for the right
> soft key. Trying to get rid of that section (0.2.3: the action was moved
> from the window to the page stack) **broke the whole left soft key** — the
> menu stopped opening at all. In 0.2.6 the original arrangement came back:
> the "back" action lives on the window, the "Actions" section exists, and the
> menu works.
>
> Hence the rule: **on Symbian, the left soft key depends on the window having
> both a menu and actions of its own.** Do not touch that pairing for
> cosmetics.
>
> The section itself used to be empty, which is why it "did nothing": Qt does
> not put an action with a soft-key role into the menu. Since 0.2.9 the window
> has a **second**, ordinary "back" action with no role — and that is what
> sits in the section.
>
> **Putting anything of your own in there is pointless** (tried in 0.3.1): not
> all window actions end up in the section, and it does not respond to
> visibility changes. Everything of ours belongs in our own menu entries,
> where per-page visibility works properly.

Focus in the input field:

| Key | Action |
|---|---|
| letters, digits | typing |
| **centre**, `Enter` | **send the message** |
| **up** | move into the message list |
| `C` | delete a character |
| right soft | back to the chat list |

What the centre press does to a message, in the order it is checked:

| State of the message | Action |
|---|---|
| any media, file not downloaded yet | download |
| voice message, file present | play it (the server returns mp3) |
| GIF / video note / video sticker, not converted yet | send for conversion |
| the same, animation ready | play it right in the row |
| ordinary video | send for conversion; the server returns **3GP**, which the built-in player opens — it cannot handle mp4 itself |
| text file | open in our own viewer |
| link | open the browser |
| any other file | download; press again to cancel |

Anything already converted stays in the cache: a second press does not push
megabytes to the server again.

**Without a media server** (empty address field, or the switch turned off)
voice messages, video notes, video stickers and GIFs do not play at all: the
phone decodes neither Opus nor VP9. They are marked `no media server, press to
save` in the list — a press downloads the file, and it can then be picked up
on a computer; a downloaded one is marked `saved to phone`. Video is still
handed to the built-in player: it plays 3GP from an old phone, but not
Telegram's H.264. Text, photos and ordinary stickers work fully without a
server.

### 4.4 Settings

| Key | Action |
|---|---|
| up / down | between fields |
| centre | toggle a checkbox; in an input field, start or finish typing |
| right soft | back **to wherever you came from** (to the conversation, if that is where you opened it) |

## 5. What of this already works, and what is still to do

| Item | State as of 0.2.0 |
|---|---|
| typing from anywhere in a conversation | done (0.1.9) |
| down from the last message into the field | done (0.1.9) |
| up from the field into the list | done |
| centre on a message | done |
| edit focus for lists and fields | done (0.1.2) |
| right soft key as back | done |
| menu on the left soft key | done (folders, settings) |
| centre in the input field sends | done (0.2.1) |
| `Send` in the menu as a fallback | done (0.2.1); since 0.2.2 the entry is visible **only when there is something to send** |
| video opens in something that plays it | done (0.2.2): via the server as 3GP, not raw mp4 into the built-in player |
| menu with no foreign sections | done (0.2.3): "Actions" removed, "Back" and "Open message" are explicit entries |
| the media mark is honest after conversion | done (0.2.3): a ready file in the cache is taken into account |
| photos do not slow scrolling down | done (0.2.3): a size with some headroom is fetched rather than the largest; it opens scaled down straight away |
| text files (.txt, .md) | **NO**: nothing to open them with; at least it now says so out loud |
| animated .tgs stickers (Lottie) | **NO**: neither the phone nor the server decodes them |
| it is clear where the focus is | indirectly (the frame); there is no separate hint |

## 6. Done because of this document

1. **Centre in the input field sends the message.** An event filter on the
   field: `Key_Select`, `Key_Enter`, `Key_Return` → send. `Enter` still works
   through `returnPressed()` as well, but the filter makes both paths one.
2. **`Send` appears in the conversation menu** — in case the centre press
   turns out to be taken by the system, and as visible proof that sending
   exists at all.
3. **The log records where the focus went** when moving into the input field:
   a complaint that "the centre press still lands on the message" means the
   focus stayed in the list, and that is something to see rather than guess.

Principle 5 (heavy things only on a press) and the `[voice]` / `[gif]` /
`[video]` marks were done in 0.2.0 and are only written down here.
