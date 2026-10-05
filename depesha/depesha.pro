TARGET = Depesha
APPNAME = Depesha
VERSION = 0.3.7
PKG_VERSION = $$replace(VERSION, "\.", ",")

symbian {
    DEFINES += VERSION=$$VERSION
} else {
    DEFINES += VERSION=\\\"$$VERSION\\\"
}

# Declarative нет: интерфейс собран из обычных виджетов, и весь смысл проекта
# в том, чтобы движок QML не загружался вовсе.
QT += core gui network xml

# Phonon -- ради голосовых: Qt Multimedia в этом SDK нет, а Phonon есть и mp3
# играет через свой же кодек телефона.
QT += phonon

DEFINES += QT_USE_FAST_CONCATENATION QT_USE_FAST_OPERATOR_PLUS

# Свой медиа-сервер можно вбить в сборку, чтобы не набирать адрес на телефоне
# при каждой переустановке: создайте src/testdefaults.h по образцу
# testdefaults.h.example. Файла нет -- и подстановки нет, поле адреса в
# настройках открывается пустым.
#
# Файл намеренно не в репозитории (см. .gitignore): через медиа-сервер идут
# ваши голосовые и видео, и адрес с токеном у каждого свои.
exists($$PWD/src/testdefaults.h): DEFINES += KG_TEST_MEDIA_SERVER

# Обычные стикеры -- это WebP, и без этого флага модель их даже не помечает.
# В Kutegram он включается из командной строки; здесь нужен всегда, декодер
# WebP собран в проект.
DEFINES += KG_STICKERS
CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT KG_NO_DEBUG KG_NO_INFO

# Общее с Kutegram: библиотека протокола и модели данных. Они наследуют
# стандартный список Qt, поэтому подключаются к виджетам напрямую -- ради
# этого ничего переписывать не пришлось.
KUTEGRAM = $$PWD/../kutegram/quick

INCLUDEPATH += src $$KUTEGRAM/src

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/loginpage.cpp \
    src/chatpage.cpp \
    src/imagepage.cpp \
    src/messagedelegate.cpp \
    src/settingspage.cpp \
    src/webpdecoder.cpp \
    src/listview.cpp \
    src/folderfilter.cpp \
    src/twolinedelegate.cpp \
    $$KUTEGRAM/src/dialogsmodel.cpp \
    $$KUTEGRAM/src/messagesmodel.cpp \
    $$KUTEGRAM/src/avatardownloader.cpp \
    $$KUTEGRAM/src/foldersmodel.cpp \
    $$KUTEGRAM/src/messageutil.cpp \
    $$KUTEGRAM/src/platformutils.cpp \
    $$KUTEGRAM/src/appsettings.cpp \
    $$KUTEGRAM/src/cachemanager.cpp \
    $$KUTEGRAM/src/mediaconverter.cpp \
    $$KUTEGRAM/src/voiceplayer.cpp \
    $$KUTEGRAM/src/kglog.cpp

HEADERS += \
    src/mainwindow.h \
    src/loginpage.h \
    src/chatpage.h \
    src/imagepage.h \
    src/messagedelegate.h \
    src/settingspage.h \
    src/webpdecoder.h \
    src/listview.h \
    src/folderfilter.h \
    src/twolinedelegate.h \
    $$KUTEGRAM/src/dialogsmodel.h \
    $$KUTEGRAM/src/messagesmodel.h \
    $$KUTEGRAM/src/avatardownloader.h \
    $$KUTEGRAM/src/foldersmodel.h \
    $$KUTEGRAM/src/messageutil.h \
    $$KUTEGRAM/src/platformutils.h \
    $$KUTEGRAM/src/appsettings.h \
    $$KUTEGRAM/src/cachemanager.h \
    $$KUTEGRAM/src/mediaconverter.h \
    $$KUTEGRAM/src/voiceplayer.h \
    $$KUTEGRAM/src/kglog.h

# Декодер WebP: обычный стикер Telegram -- это WebP, а Qt 4.7 его не читает.
include($$PWD/libwebp.pri)

include($$KUTEGRAM/libkg/libkg.pri)

symbian {
    # hal: свободная память системы для журнала; hwrmvibraclient: вибрация
    # на новое сообщение. Остальное -- запуск приложений и уведомления.
    LIBS += -lavkon -lapgrfx -lcone -leikcore -lapmime -lhwrmvibraclient -lhal

    ICON = depesha.svg

    # Свой идентификатор, не Kutegram: с одинаковым установка одного сносила бы
    # другой, а они должны жить на телефоне вместе.
    TARGET.UID3 = 0xE0713D53
    DEFINES += SYMBIAN_UID=$$TARGET.UID3

    TARGET.CAPABILITY += ReadUserData WriteUserData UserEnvironment NetworkServices LocalServices

    supported_platforms = \
            "[0x102032BE],0,0,0,{\"S60ProductID\"}" \ # Symbian 9.2
            "[0x102752AE],0,0,0,{\"S60ProductID\"}" \ # Symbian 9.3
            "[0x1028315F],0,0,0,{\"S60ProductID\"}" \ # Symbian^1
            "[0x20022E6D],0,0,0,{\"S60ProductID\"}" \ # Symbian^3
            "[0x2003A678],0,0,0,{\"S60ProductID\"}"   # Symbian Belle

    default_deployment.pkg_prerules -= pkg_platform_dependencies
    supported_platforms_deployment.pkg_prerules += supported_platforms
    DEPLOYMENT += supported_platforms_deployment

    vendor_info = \
        " " \
        "; Localised Vendor name" \
        "%{\"Depesha\"}" \
        " " \
        "; Unique Vendor name" \
        ":\"Depesha\"" \
        " "
    package.pkg_prerules += vendor_info

    header = "$${LITERAL_HASH}{\"Depesha\"},(0xE0713D53),$$PKG_VERSION,TYPE=SA,RU"
    package.pkg_prerules += header

    DEPLOYMENT += package
    DEPLOYMENT.installer_header = "$${LITERAL_HASH}{\"Depesha Installer\"},(0xE5E0AFB3),$$PKG_VERSION"
}
