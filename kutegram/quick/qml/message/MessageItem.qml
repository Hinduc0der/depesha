import QtQuick 1.0

Item {
    property alias currentState: messageRoot.state
    property int messageIndex: index
    // The list above needs the id, and a role is not reachable from there.
    property int messageIdValue: messageId

    id: messageRoot
    width: ListView.view.width
    height: Math.max(textContainer.height, avatarImage.height) + 6 * kgScaling
    state: "NO_SELECT"

    // Called from the message list when the joystick centre is pressed on this
    // row. Everything it needs is a model role, so it does not depend on which
    // QML items happen to exist for this message: a photo opens in the viewer,
    // an attachment downloads, and a downloaded one opens in whatever
    // application the phone associates with it.
    function activate() {
        if (hasPhoto && photoFile.length != 0) {
            imageViewer.imageSource = photoFile;
            imageViewer.state = "OPENED";
            return;
        }

        if (!hasMedia) {
            return;
        }

        // A voice message is Opus, which this phone has no decoder for. The
        // player sends it out to be converted and plays what comes back; see
        // voiceplayer.h. Until it has been downloaded there is nothing to send,
        // so the first press still just downloads it.
        if (mediaIsVoice && mediaFilePath.length != 0) {
            voicePlayer.play(mediaFilePath);
            return;
        }

        // Same idea for a round video note, a video sticker or a GIF: once the
        // file is here it goes out to be turned into an animated GIF, and then
        // it is drawn in place instead of opening anything.
        if (hasAnimation && animationFile.length == 0 && mediaFilePath.length != 0) {
            mediaConverter.convert(messageIndex, mediaFilePath, "animation");
            return;
        }

        // Текст читаем сами: для телефона .md и подобное -- неизвестный тип,
        // и просьба открыть его заканчивалась зависанием.
        if (mediaFilePath.length != 0 && fileBrowser.isTextFile(mediaFilePath)) {
            mainScreen.showText(mediaFilePath);
            return;
        }

        messagesModel.activateMedia(messageIndex);
    }

    states: [
        State {
            name: "NO_SELECT"
            PropertyChanges {
                target: checkbox
                opacity: 0
                x: -15 * kgScaling
            }
        },
        State {
            name: "SHOW_SELECT"
            PropertyChanges {
                target: checkbox
                opacity: 1
                x: 5 * kgScaling
            }
        }
    ]

    transitions: [
        Transition {
            NumberAnimation {
                properties: "x,opacity"
                easing.type: Easing.InOutQuad
                duration: 200
            }
        }
    ]

//    MouseArea {
//        anchors.fill: parent
//        onClicked: {
//            if (ListView.view.parent.globalState == "SHOW_SELECT")
//                ListView.view.parent.globalState = "NO_SELECT"
//            else ListView.view.parent.globalState = "SHOW_SELECT"
//        }
//    }

    Image {
        id: checkbox
        source: "../../img/checkbox-blank-circle-outline.png"
        smooth: true
        width: 20 * kgScaling
        height: width
        x: 5 * kgScaling
        y: 3 * kgScaling
        asynchronous: true
    }

    Rectangle {
        id: messageAvatar
        visible: !mergeMessage && !isChannel && (avatar.length == 0 || avatarImage.status != Image.Ready)

        anchors.top: parent.top
        anchors.topMargin: 3 * kgScaling
        anchors.left: checkbox.right
        anchors.leftMargin: (isChannel ? -35 : 5) * kgScaling

        width: 30 * kgScaling
        height: visible ? width : 0
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
        visible: !mergeMessage && !isChannel && avatar.length != 0

        anchors.top: parent.top
        anchors.topMargin: 3 * kgScaling
        anchors.left: checkbox.right
        anchors.leftMargin: (isChannel ? -35 : 5) * kgScaling

        width: 30 * kgScaling
        height: visible ? width : 0
        smooth: true

        asynchronous: true
        source: avatar
    }

    Column {
        id: textContainer
        anchors.top: parent.top
        anchors.left: messageAvatar.right
        anchors.right: parent.right
        anchors.topMargin: 3 * kgScaling
        anchors.leftMargin: 5 * kgScaling
        anchors.rightMargin: 5 * kgScaling
        spacing: 2 * kgScaling

        Row {
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 4 * kgScaling

            Text {
                text: senderName
                font.bold: true
                font.pixelSize: 12 * kgScaling
                visible: !mergeMessage
            }

            Text {
                anchors.bottom: parent.bottom
                text: messageTime
                color: "#999999"
                visible: !mergeMessage
                font.pixelSize: 12 * kgScaling
            }
        }

        Row {
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 4 * kgScaling
            visible: forwardedFrom.length != 0

            Image {
                source: "../../img/share_forwarded.png"
                smooth: true
                width: 20 * kgScaling
                height: width
                asynchronous: true
            }

            Text {
                text: forwardedFrom
                font.bold: true
                font.pixelSize: 12 * kgScaling
                color: "#8D8D8D"
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Text {
            anchors.left: parent.left
            anchors.right: parent.right

            wrapMode: Text.Wrap
            text: messageText
            visible: messageText.length != 0
            color: "#000000"
            font.pixelSize: 12 * kgScaling

            onLinkActivated: {
                messagesModel.linkActivated(link, index);
            }
        }

        Repeater {
            id: photoRepeater
            model: hasPhoto

            MessageImage {
                id: photoRow
                state: currentState
                anchors.left: parent.left
                width: Math.min(280, parent.width)
            }
        }

        // A round video note, a video sticker or a GIF, once the media server
        // has turned it into an animated GIF. Qt draws those by itself, so it
        // ends up in the conversation rather than in a separate player.
        Repeater {
            model: hasAnimation && animationFile.length != 0

            AnimatedImage {
                anchors.left: parent.left
                width: 120 * kgScaling
                // По пропорциям самой картинки, а не квадратом: иначе вокруг
                // неё остаются широкие пустые поля, а на 240 пикселях это
                // половина экрана впустую.
                height: implicitWidth > 0
                        ? Math.round(width * implicitHeight / implicitWidth) : width
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                source: "file:///" + animationFile

                // Only the selected row animates. Several GIFs at once would
                // eat a 332 MHz processor alive, and the rest of the list has
                // to stay usable.
                playing: messageRoot.ListView.isCurrentItem
                paused: !playing
            }
        }

        // An ordinary sticker is a WebP file, which Qt cannot read: it goes
        // through the image provider in webpimageprovider.cpp instead.
        Repeater {
            model: hasSticker

            Image {
                anchors.left: parent.left
                width: 120 * kgScaling
                height: implicitWidth > 0
                        ? Math.round(width * implicitHeight / implicitWidth) : width
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                // No smoothing on purpose: sourceSize makes the decoder produce
                // exactly the size that is drawn, so there is nothing to scale.
                sourceSize.width: width
                sourceSize.height: height
                source: stickerFile.length == 0
                        ? "" : ("image://webp/" + encodeURIComponent(stickerFile))
            }
        }

        Repeater {
            id: mediaRepeater
            model: hasMedia && !hasSticker && animationFile.length == 0

            MessageDocument {
                id: mediaRow
                rowIndex: messageIndex
                anchors.left: parent.left
                anchors.right: parent.right
                state: currentState
            }
        }
    }

//    Rectangle {
//        id: debugRectangle
//        anchors.fill: parent
//        color: Qt.hsla((messageId % 37) / 36, 0.5, 0.5, 0.8)
//    }
}
