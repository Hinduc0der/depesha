import QtQuick 1.0
import "../dialog"
import "../message"
import "../control"
import "../auth"
import Kutegram 1.0

Item {
    id: mainScreen
    state: "MENU"

    states: [
        State {
            name: "MENU"
            PropertyChanges {
                target: messagePage
                anchors.leftMargin: mainScreen.width
            }
        },
        State {
            name: "CHAT"
            PropertyChanges {
                target: messagePage
                anchors.leftMargin: mainScreen.width < 600 * kgScaling ? 0 : dialogPage.width
            }
        }
    ]

    transitions: [
        Transition {
            NumberAnimation {
                properties: "anchors.leftMargin"
                easing.type: Easing.InOutQuad
                duration: 200
            }
        }
    ]

    onStateChanged: {
        topBar.currentState = state;
        messagePage.globalState = "NO_SELECT";

        focusCurrentPane();
    }

    // --- keypad routing ------------------------------------------------------
    //
    // main.qml sits above this component and cannot see the ids declared here,
    // so the softkey actions are exposed as functions instead. (The old
    // Keys.onPressed in main.qml called topBar.menuButtonClicked() directly,
    // which could never have resolved.)

    function focusCurrentPane() {
        if (state == "CHAT")
            messagePage.focusEdit();
        else
            dialogPage.focusList();
    }

    function menuPressed() {
        // In a chat, with a message picked out, the softkey means "what can I
        // do with this message"; everywhere else it opens the drawer.
        if (state == "CHAT" && messagePage.messagesView.activeFocus
                && messagePage.messagesView.currentIndex >= 0) {
            messagePage.openMessageMenu();
            return;
        }

        topBar.menuButtonClicked();
    }

    // Unwind whatever is on top, innermost first. Returns false only when the
    // main screen itself is the last thing left, so the caller can decide
    // whether the key should close the application.
    function openSettings() {
        settingsPage.open();
    }

    function pickFile() {
        filePicker.open();
    }

    function showText(path) {
        textViewer.open(path);
    }

    function backPressed() {
        if (textViewer.opened) {
            textViewer.close();
            focusCurrentPane();
            return true;
        }

        if (filePicker.opened) {
            // Внутри каталога "назад" поднимает на уровень выше, и только из
            // списка дисков закрывает выбор.
            if (!filePicker.up()) {
                focusCurrentPane();
            }

            return true;
        }

        if (settingsPage.opened) {
            settingsPage.close();
            focusCurrentPane();
            return true;
        }

        if (imageViewer.state != "CLOSED") {
            imageViewer.state = "CLOSED";
            focusCurrentPane();
            return true;
        }

        if (drawer.opened) {
            drawer.closeDrawer();
            focusCurrentPane();
            return true;
        }

        if (messagePage.messageMenuOpened()) {
            messagePage.closeMessageMenu();
            focusCurrentPane();
            return true;
        }

        if (state == "CHAT") {
            // Whatever was typed and not sent goes to the server as a draft, so
            // it is there on the other devices too.
            messagesModel.saveDraft(messagePage.messageEdit.messageText);
            messagesModel.replyToMsgId = 0;
            state = "MENU";
            return true;
        }

        return false;
    }

    Rectangle {
        color: "white"
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
    }

    FoldersModel {
        id: foldersModel
        client: telegramClient
        Component.onCompleted: dialogPage.refresh.connect(refresh)
    }

    DialogsModel {
        id: dialogsModel
        folders: foldersModel
        client: telegramClient
        avatarDownloader: globalAvatarDownloader

        // Каждая пришедшая страница чатов просила следующую, и так до конца
        // списка: сотни чатов подряд сразу после входа, с расшифровкой каждого
        // пакета в главном потоке. Телефон после запуска было не взять в руки
        // минуты три. Список и так догружает страницы по прокрутке -- см.
        // DialogPage.qml.
        autoLoadAll: false
        // Аватарки скачивались для всех пришедших чатов разом; теперь их
        // просит каждая строка, когда её создают, то есть только видимые --
        // см. DialogItem.qml.
        lazyAvatars: true
        Component.onCompleted: {
		    dialogPage.refresh.connect(refresh);
			dialogsModel.sendNotification.connect(platformUtils.gotNewMessage);
		}
    }

    Connections {
        target: mediaConverter
        onConverted: {
            messagesModel.setAnimationFile(index, path);
        }
    }

    MessagesModel {
        id: messagesModel
        client: telegramClient
        avatarDownloader: globalAvatarDownloader
        // The switch in the drawer decides whether the server is told that a
        // chat has been read.
        markAsRead: appSettings.markAsRead

        onScrollTo: {
            messagePage.messagesView.positionViewAtIndex(index, ListView.End);
        }

        onScrollForNew: {
            if (messagePage.messagesView.atYEnd) {
                messagePage.messagesView.positionViewAtIndex(messagePage.messagesView.count - 1, ListView.End);
            }
        }

        onDraftChanged: {
            messagePage.messageEdit.messageText = draft;
        }

        onUploadingProgress: {
            messagePage.messageEdit.uploadingProgress(progress);
        }

        Component.onCompleted: {
            messagesModel.sentMessageUpdate.connect(dialogsModel.gotMessageUpdate);
        }
    }

    DialogPage {
        id: dialogPage
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: parent.width < 600 * kgScaling ? parent.width : 300 * kgScaling
    }

    MessageIntroPage {
        id: messageIntroPage
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: dialogPage.width
        width: messagePage.width
    }

    MessagePage {
        id: messagePage
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: parent.width < 600 * kgScaling ? parent.width : parent.width - 300 * kgScaling
    }

    Rectangle {
        id: sidePanelSeparator
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: messageIntroPage.left
        width: 1 * kgScaling
        color: "#EEEEEE"
    }

    TopBar {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
    }

    Drawer {
        id: drawer
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }

    ImageViewer {
        id: imageViewer
        anchors.fill: parent
    }

    SettingsPage {
        id: settingsPage
        z: 60
    }

    TextViewer {
        id: textViewer
        z: 62
    }

    FilePicker {
        id: filePicker
        z: 61

        onPicked: {
            messagesModel.uploadFilePath(file);
            focusCurrentPane();
        }
    }
}
