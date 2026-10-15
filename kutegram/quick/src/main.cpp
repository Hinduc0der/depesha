#include <QApplication>
#include <QTimer>
#include "qmlapplicationviewer.h"

#include <QFontDatabase>
#include <QTextCodec>
#include <QFont>
#include <QFontMetrics>
#include "tgclient.h"
#include "dialogsmodel.h"
#include "messagesmodel.h"
#include "systemname.h"
#include "avatardownloader.h"
#include "foldersmodel.h"
#include "currentuserinfo.h"
#include "platformutils.h"
#include "softkeys.h"
#include "keyrepeat.h"
#include "webpimageprovider.h"
#include "kglog.h"
#include "screenwatch.h"
#include "appsettings.h"

#include <QSystemSemaphore>
#include <QSharedMemory>

#if QT_VERSION < 0x050000
#include "voiceplayer.h"
#include "mediaconverter.h"
#include "cachemanager.h"
#include "filebrowser.h"
#endif

#if QT_VERSION >= 0x050000
#include <QQmlContext>
#include <QQmlEngine>
#else
#include <QtDeclarative>
#endif

#if QT_VERSION >= 0x040702
#include <QNetworkConfigurationManager>
#include <QNetworkSession>
#endif

// VERSION arrives quoted on most platforms and bare on symbian-abld
// (see kutegramquick.pro). Stringify, then strip any quotes that came along.
#define KG_STR2(x) #x
#define KG_STR(x) KG_STR2(x)

int main(int argc, char *argv[])
{
    //TODO OpenGL acceleration
    //Causes some crashes on Windows, Symbian 9.2-9.3?, research it
    //Requires custom fonts, but they can't be install on iOS
#if QT_VERSION < 0x050000
    //QApplication::setGraphicsSystem("opengl");
#endif

    QApplication app(argc, argv);

    QSystemSemaphore sema("Kutegram_semaphone", 1);
    bool isRunning;
    sema.acquire();

    {
        QSharedMemory shmem("Kutegram_shared");
        shmem.attach();
    }

    QSharedMemory shmem("Kutegram_shared");
    if (shmem.attach())
    {
        isRunning = true;
    }
    else
    {
        shmem.create(1);
        isRunning = false;
    }

    sema.release();
    if (isRunning) {
        //TODO raise Kutegram window
        return 1;
    }

#ifdef Q_OS_SYMBIAN
    QApplication::setAttribute(Qt::AA_S60DisablePartialScreenInputMode, false);

    // Keypad navigation for non-touch phones (Nokia E90 and the rest of S60v3).
    //
    // The whole UI is QML, so there is exactly one widget on screen: the
    // QDeclarativeView. Qt's default mode on a non-touch Symbian device is
    // NavigationModeKeypadTabOrder, which consumes the D-pad to walk a widget
    // tab order that has a single entry -- so the joystick appears dead and
    // nothing can be reached. NavigationModeNone keeps Qt out of the way and
    // lets the arrow keys and Key_Select reach the scene, where the QML items
    // carry their own KeyNavigation chains and Keys handlers.
    //
    // KG_CURSOR_NAV (qmake CONFIG+=cursornav) falls back to the virtual mouse
    // cursor, which needs no QML support but is far clumsier to drive.
#ifdef KG_CURSOR_NAV
    QApplication::setNavigationMode(Qt::NavigationModeCursorAuto);
#else
    QApplication::setNavigationMode(Qt::NavigationModeNone);
#endif
#endif

#if QT_VERSION >= 0x050300
    QApplication::setAttribute(Qt::AA_UseOpenGLES, true);
#endif

    QApplication::setApplicationVersion(QString(KG_STR(VERSION)).replace("\"", ""));
    QApplication::setApplicationName("Kutegram");
    QApplication::setOrganizationName("Kutegram");
    QApplication::setOrganizationDomain("kg.crx.moe");

    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
#if QT_VERSION < 0x050000
    QTextCodec::setCodecForTr(codec);
    QTextCodec::setCodecForCStrings(codec);
#endif
    QTextCodec::setCodecForLocale(codec);

    TgClient::registerQML();
    qmlRegisterType<DialogsModel>("Kutegram", 1, 0, "DialogsModel");
    qmlRegisterType<MessagesModel>("Kutegram", 1, 0, "MessagesModel");
    qmlRegisterType<AvatarDownloader>("Kutegram", 1, 0, "AvatarDownloader");
    qmlRegisterType<FoldersModel>("Kutegram", 1, 0, "FoldersModel");
    qmlRegisterType<CurrentUserInfo>("Kutegram", 1, 0, "CurrentUserInfo");
    qmlRegisterUncreatableType<PlatformUtils>("Kutegram", 1, 0, "PlatformUtils", "PlatformUtils is uncreatable. Use platformUtils root property.");

    //TODO show status pane without button group on Symbian
    QmlApplicationViewer viewer;
    viewer.setOrientation(QmlApplicationViewer::ScreenOrientationAuto);
    viewer.rootContext()->setContextProperty("kutegramVersion", QApplication::applicationVersion());
    viewer.rootContext()->setContextProperty("kutegramPlatform", systemName());
    AppSettings *appSettings = new AppSettings(&viewer);
    PlatformUtils *platformUtils = new PlatformUtils(&viewer);
    // Notifications and vibration are switchable from the drawer, and this is
    // how the platform side gets to see the switches.
    platformUtils->setSettings(appSettings);

    CacheManager *cacheManager = new CacheManager(&viewer);
    // Avatars, thumbnails, stickers and converted media pile up forever
    // otherwise; the phone has tens of megabytes, not gigabytes.
    //
    // Deliberately not right here: the first trim had 10 MB to delete and took
    // 14.6 seconds of phone flash, all of it before the window appeared. It
    // waits until the client is up and the user is reading something.
    QTimer::singleShot(20000, cacheManager, SLOT(trim()));
    // И подмести то место, где кэш лежал раньше. Само оно не исчезнет:
    // подрезка видит только текущую папку, а брошенные мегабайты в памяти
    // телефона -- ровно то, из-за чего всё это и затевалось.
    QTimer::singleShot(30000, cacheManager, SLOT(clearOtherPlace()));

    viewer.rootContext()->setContextProperty("cacheManager", cacheManager);
    viewer.rootContext()->setContextProperty("fileBrowser", new FileBrowser(&viewer));
    viewer.rootContext()->setContextProperty("appSettings", appSettings);
#if QT_VERSION < 0x050000
    // Voice messages are Opus, which this phone cannot decode; see voiceplayer.h.
    viewer.rootContext()->setContextProperty("voicePlayer", new VoicePlayer(appSettings, &viewer));
    // Round video notes, video stickers and GIFs come back as animated GIFs,
    // which Qt can draw inside the message list; see mediaconverter.h.
    viewer.rootContext()->setContextProperty("mediaConverter", new MediaConverter(appSettings, &viewer));
#endif
    viewer.rootContext()->setContextProperty("platformUtils", platformUtils);
    viewer.rootContext()->setContextProperty("kgScaling", QFontMetrics(app.font()).height() / 14.0f);
    // Qt 4.7 has no WebP reader, and every ordinary Telegram sticker is WebP.
    viewer.engine()->addImageProvider(QLatin1String("webp"), new WebpImageProvider());

    viewer.setMainQmlFile(QLatin1String("qrc:///qml/Main.qml"));
#if QT_VERSION >= 0x050000
    viewer.setTitle("Kutegram");
#else
    viewer.setWindowTitle("Kutegram");
#endif
    // Gives the two keys under the screen a real Back / Options binding; see
    // softkeys.h for why the QML back arrow alone was not reachable.
    new SoftKeys(&viewer);

    // One push of the joystick must be one step. Installed on the view rather
    // than on the application: the view forwards key events into the scene by
    // re-sending the same event object, which would run an application-wide
    // filter a second time and swallow every key.
    viewer.installEventFilter(new KeyRepeatFilter(&viewer));

    kgLog("started, version " + QApplication::applicationVersion() + " " + kgMemoryInfo());

    // The E90 has a second display under the lid; see screenwatch.h.
    new ScreenWatch(&viewer);

    viewer.showExpanded();
    // Without this the view can come up without keyboard focus, and then no key
    // reaches the QML scene at all.
    viewer.setFocus(Qt::OtherFocusReason);

#if QT_VERSION >= 0x040702
    QNetworkConfigurationManager manager;
    if (manager.capabilities() & QNetworkConfigurationManager::NetworkSessionRequired) {
        //TODO save network selection
        QNetworkConfiguration config = manager.defaultConfiguration();
        QNetworkSession* networkSession = new QNetworkSession(config);
        networkSession->open(); //TODO reset network selection
    }
#endif

    return app.exec();
}
