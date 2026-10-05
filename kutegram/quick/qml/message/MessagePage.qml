import QtQuick 1.0
import Kutegram 1.0

Rectangle {
    id: messagePageRoot
    property string globalState: "NO_SELECT"
    property alias messageEdit: messageEdit
    property alias messagesView: messagesView

    // --- keypad entry points -------------------------------------------------

    // Opening a chat puts the focus in the compose box: on a phone that is what
    // the user came here to do, and it is the one place the joystick centre has
    // an obvious meaning.
    function focusEdit() {
        messageEdit.focusEdit();
    }

    // --- message menu --------------------------------------------------------

    function openMessageMenu() {
        var item = messagesView.currentItem;
        messageMenu.open(messagesView.currentIndex,
                         item ? item.messageIdValue : 0);
    }

    function messageMenuOpened() {
        return messageMenu.opened;
    }

    function closeMessageMenu() {
        messageMenu.close();
    }

    function focusMessages() {
        messagesView.forceActiveFocus();
        if (messagesView.currentIndex < 0)
            messagesView.currentIndex = messagesView.count - 1;
    }

    ListView {
        id: messagesView
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: voiceStrip.top
        clip: true

        boundsBehavior: Flickable.StopAtBounds

        anchors.topMargin: Math.max(0, parent.height - messageEdit.height - childrenRect.height)

        cacheBuffer: Math.max(parent.height / 6, 0)

        currentIndex: -1
        keyNavigationWraps: false
        highlightFollowsCurrentItem: true
        highlightMoveDuration: 0

        highlight: Rectangle {
            width: messagesView.width
            color: "#000000"
            opacity: messagesView.activeFocus ? 0.14 : 0.0
        }

        // Runs before ListView's own key handling, so anything left unaccepted
        // still gets the built-in up/down walk.
        Keys.onPressed: {
            if (event.key == Qt.Key_Down && currentIndex >= count - 1) {
                messagePageRoot.focusEdit();
                event.accepted = true;
                return;
            }

            if (event.key == Qt.Key_Select || event.key == Qt.Key_Return
                    || event.key == Qt.Key_Enter) {
                // Attachments used to be reachable by mouse only, which on this
                // phone means not at all.
                if (currentItem)
                    currentItem.activate();
                event.accepted = true;
            }
        }

        // Nothing here is ever flicked with a finger, so onMovementEnded never
        // fired and the history stopped at the first page of 40. Coming close to
        // either end of what is loaded is the signal instead.
        onCurrentIndexChanged: {
            if (currentIndex < 0)
                return;

            if (currentIndex <= 2 && messagesModel.canFetchMoreUpwards())
                messagesModel.fetchMoreUpwards();
            else if (currentIndex >= count - 3 && messagesModel.canFetchMoreDownwards())
                messagesModel.fetchMoreDownwards();
        }

        onMovementEnded: {
            if (atYBeginning && messagesModel.canFetchMoreUpwards()) {
                messagesModel.fetchMoreUpwards();
            }
            // Was canFetchMoreDownwards() -- the question, asked twice, instead
            // of the answer.
            if (atYEnd && messagesModel.canFetchMoreDownwards()) {
                messagesModel.fetchMoreDownwards();
            }
        }

        model: messagesModel

        delegate: MessageItem {
            state: globalState
        }
    }

    // Says what the voice player is doing: converting takes a few seconds over
    // EDGE and silence would look like nothing happened.
    Rectangle {
        id: voiceStrip
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: replyStrip.top
        height: voicePlayer.status == "idle" ? 0 : 18 * kgScaling
        color: "#DDEEFF"
        clip: true

        Text {
            anchors.fill: parent
            anchors.leftMargin: 4 * kgScaling
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            font.pixelSize: 10 * kgScaling
            color: "#33556F"
            text: "Voice: " + voicePlayer.status
        }

        MouseArea {
            anchors.fill: parent
            onClicked: voicePlayer.stop()
        }
    }

    // What is being replied to, if anything. Zero height when there is nothing,
    // so the list simply takes the space back.
    Rectangle {
        id: replyStrip
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: messageEdit.top
        height: messageEdit.replyText.length != 0 ? 20 * kgScaling : 0
        color: "#EEEEEE"
        clip: true

        Text {
            anchors.fill: parent
            anchors.leftMargin: 4 * kgScaling
            anchors.rightMargin: 4 * kgScaling
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            font.pixelSize: 10 * kgScaling
            color: "#555555"
            text: "Reply: " + messageEdit.replyText
        }
    }

    //TODO Hide MessageEdit when user is restricted
    MessageEdit {
        id: messageEdit
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }

    MessageMenu {
        id: messageMenu
        z: 50

        onChosen: {
            switch (action) {
            case "reply":
                // text carries the message id, not the text: the menu only ever
                // needs to hand back what identifies the row.
                messagesModel.replyToMsgId = text;
                messageEdit.replyText = messagesModel.plainText(index);
                close();
                messagePageRoot.focusEdit();
                break;
            case "copy":
                platformUtils.copyToClipboard(messagesModel.plainText(index));
                snackBar.text = "Copied.";
                close();
                messagePageRoot.focusMessages();
                break;
            case "delete":
                if (!confirmingDelete) {
                    confirmingDelete = true;
                    return;
                }

                confirmingDelete = false;
                messagesModel.deleteMessage(index);
                close();
                messagePageRoot.focusMessages();
                break;
            }
        }
    }
}
