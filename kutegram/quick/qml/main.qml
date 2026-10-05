import QtQuick 1.0
import "dialog"
import "message"
import "control"
import "auth"
import Kutegram 1.0

Item {
    //TODO: remove dynamically unused pages / components from memory
    id: root

    width: 320
    height: 240

    // Root of the key event chain: anything the focused item leaves unaccepted
    // bubbles up to here, which is where the softkeys are handled.
    focus: true

    property color globalAccent: platformUtils.isWindows() ? platformUtils.windowsRealColorizationColor() : "#54759E"

    state: "AUTH"

    onStateChanged: {
        // A screen that just slid into view cannot claim the focus by itself.
        if (state == "MAIN")
            mainScreen.focusCurrentPane();
        else
            authScreen.focusCurrentPage();
    }

    states: [
        State {
            name: "AUTH"
            PropertyChanges {
                target: mainScreen
                anchors.leftMargin: -root.width
                opacity: 0
            }
            PropertyChanges {
                target: authScreen
                anchors.leftMargin: 0
            }
        },
        State {
            name: "MAIN"
            PropertyChanges {
                target: mainScreen
                anchors.leftMargin: 0
                opacity: 1
            }
            PropertyChanges {
                target: authScreen
                anchors.leftMargin: -root.width
            }
        }
    ]

    transitions: [
        Transition {
            NumberAnimation {
                properties: "anchors.leftMargin,opacity"
                easing.type: Easing.InOutQuad
                duration: 200
            }
        }
    ]

    property int currentFolderIndex: 0
    property bool authProgress: false

    function setAuthProgress(visible) {
        authProgress = visible;
    }

    // A request that finishes -- successfully or not -- must hand the focus
    // back to the current page. Without this the joystick has nothing to act
    // on after the first error and the screen looks frozen.
    onAuthProgressChanged: {
        if (!authProgress && root.state == "AUTH")
            authScreen.focusCurrentPage();
    }

    // Called from the left softkey as well as from Keys.onPressed below.
    function menuPressed() {
        if (root.state == "MAIN")
            mainScreen.menuPressed();
    }

    Component.onCompleted: {
        if (telegramClient.hasSession()) {
            setAuthProgress(true);
            telegramClient.start();
        }
    }

    TgClient {
        id: telegramClient

        onInitialized: {
            if (hasUserId) {
                return;
            }

            setAuthProgress(false);
            authScreen.currentIndex = 1;

            helpGetCountriesList();
        }

        onDisconnected: {
            setAuthProgress(false);

            if (!hasUserId) {
                root.state = "AUTH";
                authScreen.currentIndex = 0;
                return;
            }

            // Через таймер, а не сразу: связь на телефоне пропадает пачками, и
            // немедленная повторная попытка обычно ложится там же, где легла
            // предыдущая.
            snackBar.text = "Reconnecting...";
            reconnectTimer.restart();
        }

        onAuthSentCodeResponse: {
            setAuthProgress(false);

            authScreen.phonePage.phoneCodeHash = data["phone_code_hash"];
            switch (data["type"]["_"]) {
                //TODO messages
//            case TLType::AuthSentCodeTypeApp:
//                codeNumberDescriptionLabel->setText("A code was sent via Telegram to your other\ndevices, if you have any connected.");
//                break;
//            case TLType::AuthSentCodeTypeSms:
//                codeNumberDescriptionLabel->setText("We've sent an activation code to your phone.\nPlease enter it below.");
//                break;
//            case TLType::AuthSentCodeTypeCall:
//                break;
//            case TLType::AuthSentCodeTypeFlashCall:
//                break;
//            case TLType::AuthSentCodeTypeMissedCall:
//                break;
//            case TLType::AuthSentCodeTypeEmailCode:
//                break;
//            case TLType::AuthSentCodeTypeSetUpEmailRequired:
//                //TODO: show error or implement it lol
//                break;
//            case TLType::AuthSentCodeTypeFragmentSms:
//                break;
//            case TLType::AuthSentCodeTypeFirebaseSms:
//                break;
            }

            authScreen.currentIndex = 2;
        }

        onAuthAuthorizationResponse: {
            setAuthProgress(false);

            if (data["_"] == 0x44747e9a) {
                //TODO sign up / registration support
                snackBar.text = "Sign up isn't supported now. Please, use official app for signing up.";
            }
        }

        onAuthorized: {
            setAuthProgress(false);
            //TODO hide reconnecting

            root.state = "MAIN";
        }

        onRpcError: {
            setAuthProgress(false);

            //TODO think how to improve it
            if (errorMessage == "PHONE_NUMBER_INVALID") {
                snackBar.text = "Invalid phone number. Please try again.";
            }
            else if (errorMessage == "PHONE_NUMBER_FLOOD") {
                snackBar.text = "Phone is used too many times recently.";
            }
            else if (errorMessage == "PHONE_CODE_INVALID") {
                snackBar.text = "You have entered an invalid code.";
            }
            else if (errorMessage == "PHONE_CODE_EXPIRED") {
                snackBar.text = "The code has expired. Please request a new one.";
                authScreen.currentIndex = 1;
            }
            else if (errorMessage == "PASSWORD_HASH_INVALID"
                     || errorMessage == "SRP_PASSWORD_INVALID") {
                snackBar.text = "Wrong password. Please try again.";
            }
            else if (errorMessage == "SRP_ID_INVALID") {
                snackBar.text = "The password check expired. Please try again.";
            }
            else if (errorCode == 0) {
                // Locally produced failure (SRP), already human readable.
                snackBar.text = errorMessage;
            }
            else {
                snackBar.text = "RPC error occured: " + errorMessage + " (" + errorCode + ")"
            }
        }

        onSocketError: {
            // Код и текст Qt о сокете пользователю ничего не говорят: "invalid
            // socket descriptor" означает всего лишь, что соединения ещё нет.
            // Важно другое -- что попытка повторится сама.
            setAuthProgress(false);
            snackBar.text = "No connection. Trying again...";
            reconnectTimer.restart();
        }

        onTfaRequired: {
            setAuthProgress(false);

            authScreen.currentIndex = 3;

            // SESSION_PASSWORD_NEEDED carries no hint, so ask for it separately
            // while the user is reading the page.
            telegramClient.accountGetPassword();
        }

        onAccountPasswordResponse: {
            authScreen.passwordPage.passwordHint = data["hint"] ? data["hint"] : "";
        }

        // Key derivation runs on a worker thread and takes tens of seconds on
        // the target hardware; keep the progress state up for the whole of it.
        onTfaChecking: {
            setAuthProgress(true);
            authScreen.passwordPage.checkStage = 1;
        }

        onTfaWorkerStarted: {
            authScreen.passwordPage.checkStage = 2;
        }

        onTfaProgress: {
            authScreen.passwordPage.checkProgress = percent;
        }

        onHelpCountriesListResponse: {
            //TODO country selector
        }

        //Debug only
//        onFileDownloadCanceled: {
//            console.log("[INFO] File " + fileId + " download canceled");
//        }

//        onFileDownloaded: {
//            console.log("[INFO] File " + fileId + " have been downloaded");
//        }

//        onFileDownloading: {
//            console.log("[INFO] File " + fileId + " download progress: " + processedLength + " / " + totalLength + " " + progressPercentage + " %");
//        }

//        onFileUploadCanceled: {
//            console.log("[INFO] File " + fileId + " upload canceled");
//        }

//        onFileUploaded: {
//            console.log("[INFO] File " + fileId + " have been uploaded");
//        }

//        onFileUploading: {
//            console.log("[INFO] File " + fileId + " upload progress: " + processedLength + " / " + totalLength + " " + progressPercentage + " %");
//        }
    }

    AvatarDownloader {
        id: globalAvatarDownloader
        client: telegramClient
    }

    AuthScreen {
        id: authScreen
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width
    }

    MainScreen {
        id: mainScreen
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width
    }

    SnackBar {
        id: snackBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }

    // Walk one step back out of whatever is currently open. Returns false when
    // there is nothing left to unwind, leaving the key unaccepted so Symbian
    // closes the application as the user expects.
    function goBack() {
        if (root.state == "AUTH")
            return authScreen.goBack();

        return mainScreen.backPressed();
    }

    // Вызывается из keyrepeat.cpp и только тогда, когда в сцене действительно
    // не осталось ничего с фокусом. Прошлый вариант жил прямо в Keys.onPressed
    // ниже и оказался вредным: софт-клавиши доходят до корня всегда, не потому
    // что фокус потерян, а потому что так устроена обработка. В чате это
    // перебрасывало фокус в поле ввода на каждое нажатие софт-клавиши.
    function restoreFocus() {
        if (root.state == "MAIN") {
            mainScreen.focusCurrentPane();
        }
    }

    // Единственное место, где соединение поднимается заново. Без него любая
    // потеря связи была тупиком до перезапуска приложения.
    Timer {
        id: reconnectTimer
        interval: 4000
        repeat: false
        onTriggered: telegramClient.start()
    }

    Keys.onPressed: {
        // Symbian softkeys: the left one arrives as Key_Context1, the right one
        // as Key_Back. Key_Escape covers the emulator and the desktop builds.
        switch (event.key) {
        case Qt.Key_Context1:
            if (root.state == "MAIN") {
                root.menuPressed();
                event.accepted = true;
            }
            break;
        case Qt.Key_Back:
        case Qt.Key_Context2:
        case Qt.Key_Escape:
            event.accepted = root.goBack();
            break;
        }
    }
}
