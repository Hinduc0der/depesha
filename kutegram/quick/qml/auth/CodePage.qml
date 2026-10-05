import QtQuick 1.0
import "../control"

Rectangle {

    function focusFirst() {
        codeEdit.forceActiveFocus();
    }

    Column {
        anchors.centerIn: parent
        width: parent.width * 2 / 3
        spacing: 5

        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            text: "Confirmation Code"
            font.bold: true
            wrapMode: Text.Wrap
            font.pixelSize: 12 * kgScaling
        }

        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            text: "Please, enter your confirmation code."
            wrapMode: Text.Wrap
            font.pixelSize: 12 * kgScaling
        }

        LineEdit {
            id: codeEdit
            anchors.left: parent.left
            anchors.right: parent.right
            inputMethodHints: Qt.ImhDigitsOnly
            KeyNavigation.down: nextButton
        }

        Button {
            id: nextButton
            anchors.left: parent.left
            anchors.right: parent.right
            busy: root.authProgress
            KeyNavigation.up: codeEdit
            onClicked: {
                if (codeEdit.text.length == 0) {
                    snackBar.text = "You have entered an invalid code.";
                    return;
                }

                snackBar.close();

                setAuthProgress(true);
                telegramClient.authSignIn(phonePage.phoneNumber, phonePage.phoneCodeHash, codeEdit.text);
            }
        }
    }
}
