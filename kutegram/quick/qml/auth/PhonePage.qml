import QtQuick 1.0
import "../control"

Rectangle {
    property string phoneNumber;
    property string phoneCodeHash;

    function focusFirst() {
        phoneEdit.forceActiveFocus();
    }

    Column {
        anchors.centerIn: parent
        width: parent.width * 2 / 3
        spacing: 5

        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            text: "Your Phone Number"
            font.bold: true
            wrapMode: Text.Wrap
            font.pixelSize: 12 * kgScaling
        }

        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            text: "Please, enter your phone number."
            wrapMode: Text.Wrap
            font.pixelSize: 12 * kgScaling
        }

        LineEdit {
            id: phoneEdit
            anchors.left: parent.left
            anchors.right: parent.right
            // Puts an S60 keypad straight into number mode instead of T9.
            inputMethodHints: Qt.ImhDialableCharactersOnly
            KeyNavigation.down: nextButton
        }

        Button {
            id: nextButton
            anchors.left: parent.left
            anchors.right: parent.right
            busy: root.authProgress
            KeyNavigation.up: phoneEdit
            onClicked: {
                if (phoneEdit.text.length == 0) {
                    snackBar.text = "Invalid phone number. Please try again.";
                    return;
                }

                snackBar.close();

                phoneNumber = phoneEdit.text;
                phoneNumber.replace(' ', "");
                phoneNumber.replace('-', "");

                phoneCodeHash = "";

                setAuthProgress(true);
                telegramClient.authSendCode(phoneNumber);
            }
        }
    }
}
