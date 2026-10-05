import QtQuick 1.0

Item {
    width: 240
    height: 40 * kgScaling

    // Своя аватарка просится тогда, когда строку создают. ListView создаёт
    // делегаты только для видимого (плюс cacheBuffer), поэтому качается ровно
    // то, на что смотрят, а не весь список чатов разом.
    Component.onCompleted: dialogsModel.requestAvatars(index, index)

    function openDialog() {
        messagesModel.peer = peerBytes;
        topBar.peerTitle = title;
        topBar.peerThumbnailColor = thumbnailColor;
        topBar.peerThumbnailText = thumbnailText;
        topBar.peerAvatar = avatar;
        topBar.peerTooltip = tooltip;
        mainScreen.state = "CHAT";
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            openDialog();
        }
    }

    property string avatarWatcher: avatar
    onAvatarWatcherChanged: {
        if (messagesModel.peer == peerBytes) {
            topBar.peerAvatar = avatar;
        }
    }

    Rectangle {
        id: avatarRect
        visible: avatar.length == 0 || avatarImage.status != Image.Ready

        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 5 * kgScaling

        width: 30 * kgScaling
        height: width
        smooth: true

        color: thumbnailColor
        radius: width / 2

        Text {
            anchors.fill: parent
            text: thumbnailText
            color: "#FFFFFF"
            font.bold: true
            font.pixelSize: 12 * kgScaling
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    Image {
        id: avatarImage
        visible: avatar.length != 0

        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: avatarRect.anchors.leftMargin

        width: 30 * kgScaling
        height: width
        smooth: true

        asynchronous: true
        source: avatar
    }

    Column {
        anchors.left: avatarRect.right
        anchors.right: parent.right
        anchors.verticalCenter: avatarRect.verticalCenter
        anchors.leftMargin: avatarRect.anchors.leftMargin
        anchors.rightMargin: anchors.leftMargin

        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            height: messageTimeText.height

            Text {
                text: title
                elide: Text.ElideRight
                font.pixelSize: 12 * kgScaling
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: messageTimeText.left
            }

            Text {
                id: messageTimeText
                text: messageTime
                // A pinned chat says so by the colour of its time, which costs
                // no width on a 240 pixel screen.
                color: pinned ? globalAccent : "#999999"
                font.pixelSize: 12 * kgScaling
                anchors.top: parent.top
                anchors.right: parent.right
            }
        }

        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            height: messageSenderLabel.height

            Text {
                id: messageSenderLabel
                text: messageSenderName
                color: messageSenderColor
                font.pixelSize: 12 * kgScaling
                anchors.top: parent.top
                anchors.left: parent.left
            }

            Rectangle {
                id: unreadBadge
                visible: unreadCount > 0
                anchors.right: parent.right
                anchors.top: parent.top
                width: visible ? Math.max(height, unreadText.paintedWidth + 8 * kgScaling) : 0
                height: 14 * kgScaling
                radius: height / 2
                color: globalAccent

                Text {
                    id: unreadText
                    anchors.centerIn: parent
                    text: unreadCount > 99 ? "99+" : unreadCount
                    color: "#FFFFFF"
                    font.bold: true
                    font.pixelSize: 10 * kgScaling
                }
            }

            Text {
                id: messageTextLabel
                text: messageText
                color: "#8D8D8D"
                elide: Text.ElideRight
                font.pixelSize: 12 * kgScaling
                anchors.top: parent.top
                anchors.left: messageSenderLabel.right
                anchors.right: unreadBadge.visible ? unreadBadge.left : parent.right

                onLinkActivated: {
                    openDialog();
                }
            }
        }
    }

    Rectangle {
        height: 1 * kgScaling
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: "#EEEEEE"
    }
}
