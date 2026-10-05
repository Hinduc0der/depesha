import QtQuick 1.0
import Kutegram 1.0

Item {
    property bool opened: false

    // Armed by the first press on "Log out", cleared whenever the drawer is
    // opened or closed so the confirmation never carries over.
    property bool confirmingLogout: false

    CurrentUserInfo {
        id: currentUserInfo
        client: telegramClient
        avatarDownloader: globalAvatarDownloader

        onUserAvatarDownloaded: {
            peerAvatar = avatar;
        }

        onUserInfoChanged: {
            peerThumbnailText = thumbnailText;
            peerThumbnailColor = thumbnailColor;
            peerName = name;
            peerUsername = username;
        }
    }

    property string peerAvatar: ""
    property string peerThumbnailText: ""
    property color peerThumbnailColor: "#00000000"
    property string peerName: ""
    property string peerUsername: ""

    id: drawerRoot
    width: 240
    height: 320

    state: opened ? "OPENED" : "CLOSED"

    states: [
        State {
            name: "OPENED"
            PropertyChanges {
                target: dimmBackground
                opacity: 0.5
            }
            PropertyChanges {
                target: drawerSlide
                currentIndex: 0
                anchors.leftMargin: 0
            }
        },
        State {
            name: "CLOSED"
            PropertyChanges {
                target: dimmBackground
                opacity: 0
            }
            PropertyChanges {
                target: drawerSlide
                anchors.leftMargin: -Math.max(240, drawerRoot.width)
            }
        }
    ]

    transitions: [
        Transition {
            NumberAnimation {
                properties: "opacity,anchors.leftMargin"
                easing.type: Easing.InOutQuad
                duration: 200
            }
        }
    ]

    Rectangle {
        id: dimmBackground
        anchors.fill: parent
        color: "#000000"
    }

    function closeDrawer() {
        topBar.currentState = "MENU"
        opened = false;
    }

    // The drawer covers the screen, so while it is open it owns the keypad;
    // when it closes, MainScreen takes the focus back.
    onOpenedChanged: {
        confirmingLogout = false;

        if (opened) {
            drawerListView.currentIndex = 0;
            drawerListView.forceActiveFocus();
            return;
        }

        // Фокус надо вернуть здесь, а не в обработчике клавиши «назад»:
        // шторку закрывают ещё и левой софт-клавишей, и нажатием по затемнению,
        // и пунктом меню. Во всех этих случаях фокус оставался на скрытом
        // списке, и после выхода не работало вообще ничего, кроме той же
        // софт-клавиши.
        mainScreen.focusCurrentPane();
    }

    ListView {
        id: drawerSlide
        anchors.left: parent.left
        width: drawerRoot.width
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        boundsBehavior: Flickable.StopAtBounds
        orientation: ListView.Horizontal
        snapMode: ListView.SnapOneItem
        highlightRangeMode: ListView.StrictlyEnforceRange
        highlightFollowsCurrentItem: true
        highlightMoveDuration: 200

        onCurrentItemChanged: {
            if (currentIndex == 1) {
                closeDrawer();
            }
        }

        model: VisualItemModel {
            Item {
                id: drawerContent
                width: drawerRoot.width
                height: drawerRoot.height

                MouseArea {
                    id: dimmMouseArea
                    anchors.fill: parent

                    onClicked: {
                        closeDrawer();
                    }
                }

                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    width: Math.min(Math.max(240, parent.width * 5 / 6), 280 * kgScaling)
                    color: "#FFFFFF"

                    Rectangle {
                        id: currentUserRect
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 90 * kgScaling
                        color: globalAccent

                        MouseArea {
                            anchors.fill: parent
                        }

                        Rectangle {
                            id: avatarRect
                            visible: peerAvatar.length == 0 || avatarImage.status != Image.Ready

                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.topMargin: 8 * kgScaling
                            anchors.leftMargin: anchors.topMargin

                            width: 40 * kgScaling
                            height: width
                            smooth: true

                            color: peerThumbnailColor
                            radius: width / 2

                            Text {
                                anchors.fill: parent
                                text: peerThumbnailText
                                color: "#FFFFFF"
                                font.bold: true
                                font.pixelSize: 12 * kgScaling
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        Image {
                            id: avatarImage
                            visible: peerAvatar.length != 0

                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.topMargin: avatarRect.anchors.topMargin
                            anchors.leftMargin: avatarRect.anchors.topMargin

                            width: 40 * kgScaling
                            height: width
                            smooth: true

                            asynchronous: true
                            source: peerAvatar
                        }

                        Text {
                            id: avatarName
                            text: peerName
                            color: "#FFFFFF"
                            font.bold: true
                            font.pixelSize: 12 * kgScaling

                            anchors.left: parent.left
                            anchors.top: avatarImage.bottom
                            anchors.topMargin: avatarRect.anchors.topMargin
                            anchors.leftMargin: avatarRect.anchors.topMargin
                            anchors.right: parent.right
                            anchors.rightMargin: avatarRect.anchors.topMargin
                        }

                        Text {
                            id: avatarUsername
                            text: peerUsername.length != 0 ? "@" + peerUsername : "no username"
                            color: "#FFFFFF"
                            font.pixelSize: 12 * kgScaling

                            anchors.left: parent.left
                            anchors.top: avatarName.bottom
                            anchors.topMargin: 0
                            anchors.leftMargin: avatarRect.anchors.topMargin
                            anchors.right: parent.right
                            anchors.rightMargin: avatarRect.anchors.topMargin
                        }
                    }

                    ListView {
                        id: drawerListView
                        anchors.top: currentUserRect.bottom
                        anchors.bottom: drawerBottom.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        boundsBehavior: Flickable.StopAtBounds
                        // Без этого делегаты рисуются за пределами списка и при
                        // прокрутке залезают на шапку с именем и на подвал.
                        clip: true

                        keyNavigationWraps: false
                        highlightFollowsCurrentItem: true
                        highlightMoveDuration: 0

                        highlight: Rectangle {
                            width: drawerListView.width
                            color: "#000000"
                            opacity: drawerListView.activeFocus ? 0.14 : 0.0
                        }

                        Keys.onPressed: {
                            if (event.key == Qt.Key_Select
                                    || event.key == Qt.Key_Return
                                    || event.key == Qt.Key_Enter) {
                                if (currentItem)
                                    currentItem.activate();
                                event.accepted = true;
                            }
                        }

//                        highlight: Rectangle {
//                            width: drawerListView.width
//                            height: 40 * kgScaling
//                            opacity: 0.1
//                            color: "#000000"
//                        }

                        // The switches live here because this is the only menu
                        // the phone has. Toggling one leaves the drawer open,
                        // so several can be changed in one visit.
                        model: ListModel {
                            ListElement {
                                icon: ""
                                name: "Mark chats as read"
                                action: "markAsRead"
                            }
                            ListElement {
                                icon: ""
                                name: "Notifications"
                                action: "notifications"
                            }
                            ListElement {
                                icon: ""
                                name: "Vibrate"
                                action: "vibrate"
                            }
                            ListElement {
                                icon: "../../img/web_black.png"
                                name: "Media server..."
                                action: "settings"
                            }
                            ListElement {
                                icon: "../../img/exit-to-app.png"
                                name: "Log out"
                                action: "logout"
                            }
                        }

                        delegate: DrawerButton {

                        }
                    }

                    Rectangle {
                        id: drawerBottom
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        color: "#FFFFFF"
                        height: versionRow.height + 16 * kgScaling

                        MouseArea {
                            anchors.fill: parent
                        }

                        Text {
                            id: versionRow
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.leftMargin: 12 * kgScaling
                            anchors.right: parent.right
                            anchors.rightMargin: 12 * kgScaling
                            anchors.bottomMargin: 8 * kgScaling

                            color: "#999999"
                            text: "Version " + kutegramVersion + " for " + kutegramPlatform
                            elide: Text.ElideRight
                            font.bold: true
                            font.pixelSize: 12 * kgScaling
                        }
                    }
                }
            }
            Item {
                id: drawerSpace
                width: drawerRoot.width
                height: drawerRoot.height
            }
        }
    }
}
