import QtQuick 1.0
import "../control"

Rectangle {
    // Hint the account owner set for their password, if any. Filled in from
    // account.password before the page is shown.
    property string passwordHint: ""

    // The check has three separate places it can stall, and on screen they used
    // to look identical. Each one now names itself.
    //   -1 idle   0 waiting for account.getPassword   1 worker created
    //    2 worker actually running (checkProgress is meaningful)
    property int checkStage: -1
    property int checkProgress: 0

    function stageText() {
        if (checkStage == 0)
            return "Requesting password parameters...";
        if (checkStage == 1)
            return "Starting the check...";
        return "Checking the password: " + checkProgress + "%";
    }

    function focusFirst() {
        passwordEdit.forceActiveFocus();
    }

    function clearPassword() {
        passwordEdit.text = "";
    }

    Column {
        anchors.centerIn: parent
        width: parent.width * 2 / 3
        spacing: 5

        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            text: "Two-Step Verification"
            font.bold: true
            wrapMode: Text.Wrap
            font.pixelSize: 12 * kgScaling
        }

        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            text: passwordHint.length != 0
                  ? "Please, enter your password.\nHint: " + passwordHint
                  : "Please, enter your password."
            wrapMode: Text.Wrap
            font.pixelSize: 12 * kgScaling
        }

        LineEdit {
            id: passwordEdit
            anchors.left: parent.left
            anchors.right: parent.right
            echoMode: TextInput.Password
            // No predictive text on a password field.
            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            KeyNavigation.down: nextButton
        }

        Button {
            id: nextButton
            anchors.left: parent.left
            anchors.right: parent.right
            busy: root.authProgress
            KeyNavigation.up: passwordEdit
            onClicked: {
                if (passwordEdit.text.length == 0) {
                    snackBar.text = "Please, enter your password.";
                    return;
                }

                snackBar.close();

                checkStage = 0;
                checkProgress = 0;
                setAuthProgress(true);
                telegramClient.authCheckPassword(passwordEdit.text);
            }
        }

        // Deriving the key is 100000 rounds of HMAC-SHA512; on an E90 that is
        // tens of seconds of silence, which looks exactly like a hang unless
        // the screen says otherwise.
        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            visible: root.authProgress && checkStage >= 0
            text: stageText() + "\nThis takes a while on this device, please wait."
            wrapMode: Text.Wrap
            color: "#777777"
            font.pixelSize: 10 * kgScaling
        }
    }
}
