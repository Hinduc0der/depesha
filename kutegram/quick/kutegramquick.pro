TARGET = Kutegram
APPNAME = Kutegram
VERSION = 1.9.14
PKG_VERSION = $$replace(VERSION, "\.", ",")
# symbian-abld: cpp.exe from the 2011 SDK chokes on escaped quotes inside
# a MACRO line ("unterminated string or character constant"), so on Symbian
# VERSION is passed bare and stringified in main.cpp via KG_STR().
symbian {
    DEFINES += VERSION=$$VERSION
} else {
    DEFINES += VERSION=\\\"$$VERSION\\\"
}
#DATE = $$system(date /t)
#DEFINES += BUILDDATE=\"\\\"$$DATE\\\"\"
#COMMIT_SHA = $$system(git log --pretty=format:%h -n 1);
#DEFINES += COMMIT_SHA=\"\\\"$$COMMIT_SHA\\\"\"

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += core widgets qml quick network xml
    win32:!winrt:QT += winextras
}
!greaterThan(QT_MAJOR_VERSION, 4) {
    QT += core declarative gui network xml phonon

    # Playing voice messages goes through Phonon, which Qt 5 dropped. Nothing
    # here builds against Qt 5 today; when something does, this needs its own
    # implementation rather than a broken one.
    SOURCES += src/voiceplayer.cpp src/opusvoice.cpp
    HEADERS += src/voiceplayer.h src/opusvoice.h

    # Декодер Opus: голосовое Telegram -- это Opus в Ogg, своего такого кодека
    # у телефона нет. С этой библиотекой телефон раскодирует голосовые сам, и
    # сервер-прокладка для них больше не нужен.
    include(libopus/libopus.pri)
}

winrt {
    WINRT_MANIFEST.background = lightSkyBlue
    WINRT_MANIFEST.description = "An unofficial Telegram client, written in Qt Quick and C++."
    WINRT_MANIFEST.logo_large = wpassets/logo_150x150.png
    WINRT_MANIFEST.logo_medium = wpassets/logo_71x71.png
    WINRT_MANIFEST.logo_small = wpassets/logo_44x44.png
    WINRT_MANIFEST.logo_splash = wpassets/logo_480x800.png
    WINRT_MANIFEST.logo_store = wpassets/logo_store.png
    WINRT_MANIFEST.logo_wide = wpassets/logo_310x150.png
    WINRT_MANIFEST.publisherid = "CN=curoviyxru"
    WINRT_MANIFEST.identity = "1dcfeda8-9772-4658-8c54-853732753c6f"
    WINRT_MANIFEST.publisher = "curoviyxru"
    WINRT_MANIFEST.version = $$VERSION".0"
}

DEFINES += QT_USE_FAST_CONCATENATION QT_USE_FAST_OPERATOR_PLUS
CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT KG_NO_DEBUG KG_NO_INFO

QML_IMPORT_PATH =

win32:RC_FILE = kutegramquick.rc
macx:ICON = kutegramquick.icns

symbian {
    # hwrmvibraclient: the buzz on a new message. avkon carries CAknGlobalNote.
    LIBS += -lavkon -lapgrfx -lcone -leikcore -lapmime -lhwrmvibraclient

    contains(SYMBIAN_VERSION, Symbian3) {
        DEFINES += SYMBIAN3_READY=1
        include(pigler/qt-library/pigler.pri)
    }

    ICON = kutegramquick.svg
    TARGET.UID3 = 0xE0713D51
    DEFINES += SYMBIAN_UID=$$TARGET.UID3

    # SwEvent is not a user-grantable capability: a self-signed SIS carrying it
    # is rejected at install time on a stock phone. It is needed by exactly one
    # call -- TApaTask::BringToForeground() in platformutils.cpp -- and calling
    # that without the capability panics the process with KERN-EXEC 46, so the
    # call is guarded by KG_HAS_SWEVENT and both are switched together.
    # Build with CONFIG+=swevent when signing with a developer certificate.
    TARGET.CAPABILITY += ReadUserData WriteUserData UserEnvironment NetworkServices LocalServices
    contains(CONFIG, swevent) {
        TARGET.CAPABILITY += SwEvent
        DEFINES += KG_HAS_SWEVENT
    }

    # Fall back to the virtual mouse cursor instead of QML keypad navigation.
    contains(CONFIG, cursornav): DEFINES += KG_CURSOR_NAV
    # Left at Qt's default 32 MB ceiling on purpose. Raising it to 64 MB was an
    # attempt to survive the 800x352 screen plus the WebP decoder, and it did
    # not stop the freeze -- but a phone with about 64 MB of RAM in total is
    # safer when this process hits its own ceiling and dies alone, rather than
    # taking the window server down with it.
    #TARGET.EPOCHEAPSIZE = 0x400000 0x4000000
    #TARGET.EPOCSTACKSIZE = 0x14000

    # Stickers are WebP and Qt cannot draw them without the decoder in
    # libwebp/. They were switched off while the phone was freezing; the cause
    # turned out to be the peer lists in messageutil.cpp growing forever, so
    # they are back on. CONFIG+=nostickers switches them off again.
    !contains(CONFIG, nostickers): DEFINES += KG_STICKERS

    # HAL::Get() for the free-memory line in the log.
    LIBS += -lhal

    supported_platforms = \
            "[0x1028315F],0,0,0,{\"S60ProductID\"}" \ # Symbian^1
            "[0x20022E6D],0,0,0,{\"S60ProductID\"}" \ # Symbian^3
            "[0x102032BE],0,0,0,{\"S60ProductID\"}" \ # Symbian 9.2
            "[0x102752AE],0,0,0,{\"S60ProductID\"}" \ # Symbian 9.3
            "[0x2003A678],0,0,0,{\"S60ProductID\"}"   # Symbian Belle

    default_deployment.pkg_prerules -= pkg_platform_dependencies
    supported_platforms_deployment.pkg_prerules += supported_platforms
    DEPLOYMENT += supported_platforms_deployment

    vendor_info = \
        " " \
        "; Localised Vendor name" \
        "%{\"curoviyxru\"}" \
        " " \
        "; Unique Vendor name" \
        ":\"curoviyxru\"" \
        " "
    package.pkg_prerules += vendor_info

    header = "$${LITERAL_HASH}{\"Kutegram\"},(0xE0713D51),$$PKG_VERSION,TYPE=SA,RU"
    package.pkg_prerules += header

    DEPLOYMENT += package
    DEPLOYMENT.installer_header = "$${LITERAL_HASH}{\"Kutegram Installer\"},(0xE5E0AFB2),$$PKG_VERSION"
}

INCLUDEPATH += src

SOURCES +=  \
    src/main.cpp \
    src/dialogsmodel.cpp \
    src/messagesmodel.cpp \
    src/avatardownloader.cpp \
    src/foldersmodel.cpp \
    src/currentuserinfo.cpp \
    src/messageutil.cpp \
    src/platformutils.cpp \
    src/softkeys.cpp \
    src/keyrepeat.cpp \
    src/webpimageprovider.cpp \
    src/kglog.cpp \
    src/screenwatch.cpp \
    src/appsettings.cpp \
    src/mediaconverter.cpp \
    src/cachemanager.cpp \
    src/filebrowser.cpp

HEADERS += \
    src/dialogsmodel.h \
    src/messagesmodel.h \
    src/avatardownloader.h \
    src/foldersmodel.h \
    src/currentuserinfo.h \
    src/messageutil.h \
    src/platformutils.h \
    src/softkeys.h \
    src/keyrepeat.h \
    src/webpimageprovider.h \
    src/kglog.h \
    src/screenwatch.h \
    src/appsettings.h \
    src/mediaconverter.h \
    src/cachemanager.h \
    src/filebrowser.h

OTHER_FILES += \
    qtc_packaging/debian_harmattan/rules \
    qtc_packaging/debian_harmattan/README \
    qtc_packaging/debian_harmattan/copyright \
    qtc_packaging/debian_harmattan/control \
    qtc_packaging/debian_harmattan/compat \
    qtc_packaging/debian_harmattan/changelog

RESOURCES += \
    resources.qrc

include(libwebp/libwebp.pri)

include(libkg/libkg.pri)

include(qmlapplicationviewer/qmlapplicationviewer.pri)
qtcAddDeployment()

DISTFILES += \
    android/AndroidManifest.xml \
    android/gradle/wrapper/gradle-wrapper.jar \
    android/gradlew \
    android/res/values/libs.xml \
    android/build.gradle \
    android/gradle/wrapper/gradle-wrapper.properties \
    android/gradlew.bat

ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
