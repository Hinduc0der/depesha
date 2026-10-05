#include "platformutils.h"
#include "appsettings.h"
#include <QFileInfo>
#include "kglog.h"

// Long enough to feel through a pocket, short enough not to be a ringtone.
#define KG_VIBRA_MS 400

// A short chirp to go with it. Not a real notification tone -- the SDK has no
// library for those -- but audible.
#define KG_BEEP_HZ 1200
#define KG_BEEP_US 120000

#include <QApplication>
#include <QClipboard>
#include "debug.h"

#if defined(Q_OS_WIN32) && QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
#include <QtWinExtras/QtWin>
#define DWM_FEATURES
#endif

#ifdef SYMBIAN3_READY
#include <akndiscreetpopup.h>
#endif

// Needed on every platform: openFile() converts the path with it.
#include <QDir>

#ifdef Q_OS_SYMBIAN
#include <apgcli.h>
#include <apgtask.h>
#include <eikenv.h>
#include <hwrmvibra.h>
#else
#include <QDesktopServices>
#endif

#include <crypto.h>

#ifdef Q_OS_SYMBIAN
// Symbian objects only inside the TRAP: a leave skips C++ destructors, so
// anything that owns memory has to live outside.
static void VibrateL(TInt milliseconds)
{
    CHWRMVibra *vibra = CHWRMVibra::NewLC();
    vibra->StartVibraL(milliseconds);
    CleanupStack::PopAndDestroy(vibra);
}
#endif


PlatformUtils::PlatformUtils(QObject *parent)
    : QObject(parent)
    , _settings(0)
    , window(dynamic_cast<QWidget*>(parent))
#if !defined(Q_OS_SYMBIAN) && !defined(Q_OS_WINPHONE)
    , trayIcon(this)
    , trayMenu()
#endif
    , unread()
#ifdef SYMBIAN3_READY
    , pigler()
    , piglerId(-1)
#endif
{
    if (window) {
        window->setAttribute(Qt::WA_DeleteOnClose, false);
        window->setAttribute(Qt::WA_QuitOnClose, false);
    }

#if !defined(Q_OS_SYMBIAN) && !defined(Q_OS_WINPHONE)
    connect(&trayIcon, SIGNAL(messageClicked()), this, SLOT(messageClicked()));
    connect(&trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)));
    connect(&trayMenu, SIGNAL(triggered(QAction*)), this, SLOT(menuTriggered(QAction*)));

    trayMenu.addAction("Open Kutegram");
    trayMenu.addAction("Exit");

    trayIcon.setContextMenu(&trayMenu);
    trayIcon.setIcon(QIcon(":/kutegramquick_small.png"));
    trayIcon.setToolTip("Kutegram");
    trayIcon.show();
#endif

#ifdef SYMBIAN3_READY
    qint32 response = pigler.init("Kutegram"); //TODO think about randomization
    if (response >= 0) {
        if (response > 0)
            piglerHandleTap(response);

        connect(&pigler, SIGNAL(handleTap(qint32)), this, SLOT(piglerHandleTap(qint32)));
        pigler.removeAllNotifications();
        piglerId = 0;
    }
#endif
}

#ifdef SYMBIAN3_READY
void PlatformUtils::piglerHandleTap(qint32 notificationId)
{
    //App should be opened automatically
    unread.clear();
}
#endif

void PlatformUtils::showAndRaise()
{
    //TODO remove notifications
    unread.clear();

    if (window) {
        window->show();
        window->activateWindow();
        window->raise();
    }
}

void PlatformUtils::quit()
{
    QApplication::exit();
}

void PlatformUtils::copyToClipboard(QString text)
{
    QApplication::clipboard()->setText(text);
}

#if !defined(Q_OS_SYMBIAN) && !defined(Q_OS_WINPHONE)
void PlatformUtils::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason != QSystemTrayIcon::Context) {
        showAndRaise();
    }
}

void PlatformUtils::messageClicked()
{
    showAndRaise();
}

void PlatformUtils::menuTriggered(QAction *action)
{
    if (action->text() == "Exit") {
        quit();
        return;
    }

    showAndRaise();
}
#endif

void PlatformUtils::windowsExtendFrameIntoClientArea(int left, int top, int right, int bottom)
{
#ifdef DWM_FEATURES
    if (window) {
        window->setAttribute(Qt::WA_TranslucentBackground, true);
        window->setAttribute(Qt::WA_NoSystemBackground, false);
        window->setStyleSheet("background: transparent");
        QtWin::extendFrameIntoClientArea(window, left, top, right, bottom);
    }
#endif
}

bool PlatformUtils::windowsIsCompositionEnabled()
{
#ifdef DWM_FEATURES
    return QtWin::isCompositionEnabled();
#else
    return false;
#endif
}

QColor PlatformUtils::windowsRealColorizationColor()
{
#ifdef DWM_FEATURES
    return QtWin::realColorizationColor();
#else
    return Qt::white;
#endif
}

bool PlatformUtils::isWindows()
{
#ifdef DWM_FEATURES
    return true;
#else
    return false;
#endif
}

void PlatformUtils::gotNewMessage(qint64 peerId, QString peerName, QString senderName, QString text, bool silent)
{
    if (window && window->hasFocus()) {
        unread.clear();
        return;
    }

    QVariantMap info;
    info["id"] = peerId;
    info["peerName"] = peerName;
    info["senderName"] = senderName;
    info["text"] = text;

    unread.insert(peerId, info);

    QString title;
    QString message;

    title = peerName;
    message = senderName;
    message += text;

#if !defined(Q_OS_SYMBIAN) && !defined(Q_OS_WINPHONE)
    if (!silent) {
        kgDebug() << "Sending Windows notification";
        trayIcon.showMessage(title, message);
    }
#endif

    if (unread.size() != 1) {
        title = "New messages from " + QString::number(unread.size()) + " chats";
        foreach (qint32 pid, unread.keys()) {
            if (!message.isEmpty()) {
                message += ", ";
            }
            message += unread[pid]["peerName"].toString();
        }
    }

    title = title.left(63);
    message = message.left(63);

#ifdef SYMBIAN3_READY
    kgDebug() << "Sending Symbian notification";
    TUid symbianUid = {SYMBIAN_UID};
    //TODO: icon
    TRAP_IGNORE(CAknDiscreetPopup::ShowGlobalPopupL(TPtrC16(title.utf16()), TPtrC16(message.utf16()), KAknsIIDNone, KNullDesC, 0, 0, 1, 0, 0, symbianUid));
#endif

#ifdef SYMBIAN3_READY
    kgDebug() << "Sending Pigler notification";
    if (piglerId == 0) {
        piglerId = pigler.createNotification(title, message);
    } else if (piglerId > 0) {
        pigler.updateNotification(piglerId, title, message);
    } else {
        kgDebug() << "Pigler is not initialized";
    }

    if (piglerId > 0) {
        static QImage piglerImage(":/kutegramquick_pigler.png");
        pigler.setNotificationIcon(piglerId, piglerImage);
    }
#endif

#ifdef Q_OS_SYMBIAN
    // Everything above is behind SYMBIAN3_READY, and the E90 is S60v3: until
    // now a new message on this phone produced nothing whatsoever -- no sound,
    // no buzz, no popup. The function has already returned above when the
    // window has the focus, so what follows only happens while the user is
    // somewhere else.
    //
    // There is no visible notification here on purpose: CAknGlobalNote lives in
    // aknnotify.lib, and the Qt Symbian SDK does not ship that library at all
    // (no .dso exports the class). Showing a popup from the background would
    // need a full S60 3.1 SDK. What the platform does give us is the buzzer.
    if (!silent && (!_settings || _settings->notifications())) {
        if (!_settings || _settings->vibrate()) {
            TRAP_IGNORE(VibrateL(KG_VIBRA_MS));
        }

        User::Beep(KG_BEEP_HZ, TTimeIntervalMicroSeconds32(KG_BEEP_US));
    }
#endif

    //TODO: custom notification popup for Windows
    //TODO: android
    //TODO: sound
    //TODO: blink
}

void PlatformUtils::setSettings(AppSettings *settings)
{
    _settings = settings;
}

void openUrl(QUrl url)
{
#ifdef Q_OS_SYMBIAN
    static TUid KUidBrowser = {0x10008D39};
    _LIT(KBrowserPrefix, "4 ");

    // convert url to encoded version of QString
    QString encUrl(QString::fromUtf8(url.toEncoded()));
    // using qt_QString2TPtrC() based on
    // <http://qt.gitorious.org/qt/qt/blobs/4.7/src/corelib/kernel/qcore_symbian_p.h#line102>
    TPtrC tUrl(TPtrC16(static_cast<const TUint16*>(encUrl.utf16()), encUrl.length()));

    // Following code based on
    // <http://www.developer.nokia.com/Community/Wiki/Launch_default_web_browser_using_Symbian_C%2B%2B>

    // create a session with apparc server
    RApaLsSession appArcSession;
    User::LeaveIfError(appArcSession.Connect());
    CleanupClosePushL<RApaLsSession>(appArcSession);

    // get the default application uid for application/x-web-browse
    TDataType mimeDatatype(_L8("application/x-web-browse"));
    TUid handlerUID;
    appArcSession.AppForDataType(mimeDatatype, handlerUID);

    // if UiD not found, use the native browser
    if (handlerUID.iUid == 0 || handlerUID.iUid == -1)
        handlerUID = KUidBrowser;

    // Following code based on
    // <http://qt.gitorious.org/qt/qt/blobs/4.7/src/gui/util/qdesktopservices_s60.cpp#line213>

    HBufC* buf16 = HBufC::NewLC(tUrl.Length() + KBrowserPrefix.iTypeLength);
    buf16->Des().Copy(KBrowserPrefix); // Prefix used to launch correct browser view
    buf16->Des().Append(tUrl);

#ifdef KG_HAS_SWEVENT
    // BringToForeground() and SendMessage() below require the SwEvent capability;
    // without it the kernel panics the process (KERN-EXEC 46). See kutegramquick.pro.
    TApaTaskList taskList(CCoeEnv::Static()->WsSession());
    TApaTask task = taskList.FindApp(handlerUID);
    if (task.Exists()) {
        // Switch to existing browser instance
        task.BringToForeground();
        HBufC8* param8 = HBufC8::NewLC(buf16->Length());
        param8->Des().Append(buf16->Des());
        task.SendMessage(TUid::Uid(0), *param8); // Uid is not used
        CleanupStack::PopAndDestroy(param8);
    } else {
#endif
        // Start a new browser instance
        TThreadId id;
        appArcSession.StartDocument(*buf16, handlerUID, id);
#ifdef KG_HAS_SWEVENT
    }
#endif

    CleanupStack::PopAndDestroy(buf16);
    CleanupStack::PopAndDestroy(&appArcSession);
#else
    QDesktopServices::openUrl(url);
#endif
}

// Типы, для которых на телефоне заведомо есть приложение. Остальное просить
// систему открыть бесполезно и, как выяснилось, опасно: скачанный .md (для
// Symbian тип неизвестный) закончился зависанием телефона и вынутым
// аккумулятором.
bool canOpenFile(QString path)
{
    static const char *known[] = {
        ".jpg", ".jpeg", ".png", ".gif", ".bmp",
        ".mp3", ".aac", ".m4a", ".amr", ".wav", ".mid",
        ".mp4", ".3gp", ".3g2", ".avi", ".rm", ".wmv",
        ".pdf", ".doc", ".xls", ".ppt", ".zip", ".sis", ".sisx", ".vcf",
        0
    };

    const QString lower = path.toLower();

    for (int i = 0; known[i]; ++i) {
        if (lower.endsWith(known[i])) {
            return true;
        }
    }

    return false;
}

void openFile(QString path)
{
#ifdef Q_OS_SYMBIAN
    if (!canOpenFile(path)) {
        kgLog("not opening " + path + ": no application for this type");
        return;
    }

    // openUrl() above is a browser launcher: it prefixes the string with "4 "
    // and starts the handler for application/x-web-browse. A local file needs
    // the other half of the API -- StartDocument() resolves the application by
    // the file itself, so an .mp4 goes to the video player and a .jpg to the
    // gallery.
    QString native = QDir::toNativeSeparators(path);
    TPtrC tPath(TPtrC16(static_cast<const TUint16*>(native.utf16()), native.length()));

    RApaLsSession appArcSession;
    if (appArcSession.Connect() != KErrNone)
        return;

    TThreadId id;
    const TInt result = appArcSession.StartDocument(tPath, id);
    appArcSession.Close();

    kgLog("StartDocument(" + QFileInfo(path).fileName() + ") -> "
          + QString::number(result));
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
#endif
}
