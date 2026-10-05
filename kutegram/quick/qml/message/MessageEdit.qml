import QtQuick 1.0
import Kutegram 1.0
import "../control"

Rectangle {
    height: 40 * kgScaling
    width: 240 * kgScaling
    color: "#FFFFFF"
    id: editRoot

    property alias messageText: innerEdit.text

    // Shown above the input while a reply is being composed. Cleared by the
    // model when the message goes, or by the back key.
    property string replyText: ""

    // --- keypad entry points -------------------------------------------------

    function focusEdit() {
        innerEdit.forceActiveFocus();
    }

    function send() {
        if (sendButton.state != "NOT_EMPTY")
            return;

        messagesModel.sendMessage(messageText);
        messageText = "";
        replyText = "";
        messagesModel.setTyping(false);
    }

    function toggleAttachment() {
        if (attachButton.state == "NOT_UPLOADING") {
            mainScreen.pickFile();
        } else {
            messagesModel.cancelUpload();
        }
    }

    function uploadingProgress(progress) {
        if (progress != -1 && progress != 100) {
            attachButton.state = "UPLOADING";
            uploadBar.width = progress * editRoot.width / 100;
        } else {
            attachButton.state = progress != 100 ? "NOT_UPLOADING" : "UPLOADED";
            uploadBar.width = 0;
        }
    }

    MouseArea {
        anchors.fill: parent
    }

    Item {
        id: attachButton
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: height
        state: "NOT_UPLOADING"

        Keys.onPressed: {
            if (event.key == Qt.Key_Select || event.key == Qt.Key_Return
                    || event.key == Qt.Key_Enter) {
                editRoot.toggleAttachment();
                event.accepted = true;
            }
        }

        KeyNavigation.right: innerEdit

        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: 0.1
            visible: attachButton.activeFocus
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                attachButton.forceActiveFocus();
                editRoot.toggleAttachment();
            }
        }

        Image {
            id: attachmentImage
            anchors.centerIn: parent
            width: 20 * kgScaling
            height: width
            smooth: true
            source: "../../img/attachment.png"
            rotation: 135
            asynchronous: true
        }

        Image {
            id: deleteImage
            anchors.centerIn: parent
            width: 20 * kgScaling
            height: width
            smooth: true
            source: "../../img/delete.png"
            asynchronous: true
        }

        Spinner {
            id: uploadSpinner
            anchors.centerIn: parent

            Image {
                anchors.centerIn: parent
                width: 20 * kgScaling
                height: width
                smooth: true
                source: "../../img/close-circle-outline_inner.png"
                asynchronous: true
            }
        }

        states: [
            State {
                name: "UPLOADING"
                PropertyChanges {
                    target: attachmentImage
                    opacity: 0
                    scale: 0
                }
                PropertyChanges {
                    target: deleteImage
                    opacity: 0
                    scale: 0
                }
                PropertyChanges {
                    target: uploadSpinner
                    opacity: 1
                    scale: 1
                }
            },
            State {
                name: "NOT_UPLOADING"
                PropertyChanges {
                    target: attachmentImage
                    opacity: 1
                    scale: 1
                }
                PropertyChanges {
                    target: deleteImage
                    opacity: 0
                    scale: 0
                }
                PropertyChanges {
                    target: uploadSpinner
                    opacity: 0
                    scale: 0
                }
            },
            State {
                name: "UPLOADED"
                PropertyChanges {
                    target: attachmentImage
                    opacity: 0
                    scale: 0
                }
                PropertyChanges {
                    target: deleteImage
                    opacity: 1
                    scale: 1
                }
                PropertyChanges {
                    target: uploadSpinner
                    opacity: 0
                    scale: 0
                }
            }
        ]

        transitions: [
            Transition {
                NumberAnimation {
                    properties: "opacity,scale"
                    easing.type: Easing.InOutQuad
                    duration: 200
                }
            }
        ]
    }

    Item {
        id: sendButton
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: height
        state: messageText.length == 0 && attachButton.state != "UPLOADED" ? "EMPTY" : "NOT_EMPTY"

        Keys.onPressed: {
            if (event.key == Qt.Key_Select || event.key == Qt.Key_Return
                    || event.key == Qt.Key_Enter) {
                editRoot.send();
                event.accepted = true;
            }
        }

        KeyNavigation.left: innerEdit

        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: 0.1
            visible: sendButton.activeFocus
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                sendButton.forceActiveFocus();
                editRoot.send();
            }
        }

        Image {
            id: sendImage
            anchors.centerIn: parent
            width: 20 * kgScaling
            height: width
            smooth: true
            source: "../../img/send_accent.png"
            asynchronous: true
        }

        Image {
            id: micImage
            anchors.centerIn: parent
            width: 20 * kgScaling
            height: width
            smooth: true
            //source: "../../img/microphone.png"
            source: "../../img/send.png"
            asynchronous: true
        }

        states: [
            State {
                name: "EMPTY"
                PropertyChanges {
                    target: sendImage
                    opacity: 0
                    scale: 0
                }
                PropertyChanges {
                    target: micImage
                    opacity: 1
                    scale: 1
                }
            },
            State {
                name: "NOT_EMPTY"
                PropertyChanges {
                    target: sendImage
                    opacity: 1
                    scale: 1
                }
                PropertyChanges {
                    target: micImage
                    opacity: 0
                    scale: 0
                }
            }
        ]

        transitions: [
            Transition {
                NumberAnimation {
                    properties: "opacity,scale"
                    easing.type: Easing.InOutQuad
                    duration: 200
                }
            }
        ]
    }

    Flickable {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: attachButton.right
        anchors.right: sendButton.left
        anchors.margins: 2 * kgScaling
        clip: true
        id: innerFlick

        function ensureVisible(r)
        {
            if (contentX >= r.x)
                contentX = r.x;
            else if (contentX+width <= r.x+r.width)
                contentX = r.x+r.width-width;
            if (contentY >= r.y)
                contentY = r.y;
            else if (contentY+height <= r.y+r.height)
                contentY = r.y+r.height-height;
        }

        Item {
            id: editContainer
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: parent.height
            anchors.topMargin: Math.max((parent.height - innerEdit.paintedHeight) / 2, 0)

            TextEdit {
                id: innerEdit
                anchors.fill: parent

                wrapMode: TextEdit.Wrap
                font.pixelSize: 12 * kgScaling
                onCursorRectangleChanged: innerFlick.ensureVisible(cursorRectangle)

                // The model throttles this down to one request every few
                // seconds; the server forgets after about six.
                onTextChanged: messagesModel.setTyping(text.length != 0)

                // Keys.priority defaults to BeforeItem, so this runs before
                // TextEdit's own caret handling and can claim keys the editor
                // would otherwise swallow.
                //
                // An empty compose box means the user is reading rather than
                // typing, and the caret has nowhere to move anyway -- that is
                // the only moment it is safe to repurpose the arrows for
                // navigation. Once there is text, they belong to the caret.
                Keys.onPressed: {
                    if (event.key == Qt.Key_Select) {
                        editRoot.send();
                        event.accepted = true;
                        return;
                    }

                    if (innerEdit.text.length != 0)
                        return;

                    if (event.key == Qt.Key_Up) {
                        messagePageRoot.focusMessages();
                        event.accepted = true;
                    } else if (event.key == Qt.Key_Left) {
                        attachButton.forceActiveFocus();
                        event.accepted = true;
                    } else if (event.key == Qt.Key_Right) {
                        sendButton.forceActiveFocus();
                        event.accepted = true;
                    }
                }
            }
        }

        Text {
            id: messageTip
            anchors.top: editContainer.top
            anchors.left: parent.left
            anchors.right: parent.right
            color: "#8D8D8D"
            text: "Write a message..."
            font.pixelSize: 12 * kgScaling
            state: messageText.length == 0 ? "EMPTY" : "NOT_EMPTY"

            states: [
                State {
                    name: "EMPTY"
                    PropertyChanges {
                        target: messageTip
                        opacity: 1
                        anchors.leftMargin: 0
                    }
                },
                State {
                    name: "NOT_EMPTY"
                    PropertyChanges {
                        target: messageTip
                        opacity: 0
                        anchors.leftMargin: -10 * kgScaling
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
        }
    }

    Rectangle {
        id: uploadBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        height: 2 * kgScaling
        color: globalAccent
        width: 0

        Behavior on width {
            NumberAnimation {
                easing.type: Easing.InOutQuad
            }
        }
    }

    Rectangle {
        height: 1 * kgScaling
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: "#EEEEEE"
    }
}
