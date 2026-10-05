#ifndef KGLOG_H
#define KGLOG_H

#include <QString>

// A black box on the phone, because a freeze that takes the whole device down
// leaves nothing else behind: no console, no crash dump, and the battery comes
// out before anything can be asked.
//
// Written to the memory card when there is one (E:/Kutegram/kutegram.log), so
// it can be picked up over USB, and to C:/Data/Kutegram otherwise. Every line
// carries the milliseconds since start.
//
// Rare events are written through immediately -- after a battery pull only
// what reached the file survives. Key presses are batched, because a file
// write per keystroke would be felt on this hardware.
void kgLog(const QString &line, bool flushNow = true);
void kgLogFlush();

QString kgLogPath();

// Heap and system memory as one line for the log. Empty where the platform
// does not tell us.
QString kgMemoryInfo();

// Free bytes left in the process heap, or -1 when unknown. The heap has a hard
// ceiling on Symbian (EPOCHEAPSIZE in the .pro), and hitting it is fatal.
int kgHeapAvailable();

// Свободная память всей системы в байтах, или -1. На Symbian кончается именно
// она: процесс умирает не один, а вместе с телефоном.
int kgFreeRam();

#endif // KGLOG_H
