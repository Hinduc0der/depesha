import QtQuick 1.0
import "../auth"

// Where the media server is configured.
//
// The phone has no decoder for Opus, VP9 or Lottie and never will, so voice
// messages, video and animated stickers can only be played if something else
// converts them first. That something is a small service the user runs; see
// vps-voice-transcode.py. With no server the client still shows text and
// pictures -- it simply says it cannot play what needs decoding.
Rectangle {
    id: settingsRoot

    property bool opened: false

    // Read once when the page opens: walking the cache directories is not
    // something to do on every repaint.
    property string cacheSizeText: ""
    property bool cacheCleared: false

    anchors.fill: parent
    color: "#FFFFFF"
    visible: opened

    function open() {
        urlEdit.text = appSettings.mediaServerUrl;
        tokenEdit.text = appSettings.mediaServerToken;
        cacheSizeText = cacheManager.sizeText;
        cacheCleared = false;
        opened = true;
        focusDelay.start();
    }

    // Saving on the way out rather than on every keystroke: the file lives in
    // phone memory and this is a slow phone.
    function close() {
        appSettings.mediaServerUrl = urlEdit.text;
        appSettings.mediaServerToken = tokenEdit.text;
        opened = false;
    }

    // См. FilePicker: только что показанный элемент не может взять фокус
    // немедленно.
    Timer {
        id: focusDelay
        interval: 1
        onTriggered: enableRow.forceActiveFocus()
    }

    MouseArea {
        anchors.fill: parent
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: 8 * kgScaling
        contentHeight: column.height
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Column {
            id: column
            width: parent.width
            spacing: 6 * kgScaling

            Text {
                text: "Media server"
                font.bold: true
                font.pixelSize: 14 * kgScaling
            }

            // На кнопочном телефоне неоткуда узнать, чем закрывается экран,
            // если этого не написать.
            Text {
                width: column.width
                wrapMode: Text.Wrap
                color: "#999999"
                font.pixelSize: 10 * kgScaling
                text: "Back key saves and closes this screen."
            }

            Text {
                width: column.width
                wrapMode: Text.Wrap
                color: "#666666"
                font.pixelSize: 10 * kgScaling
                text: "This phone cannot decode voice messages, video or "
                      + "animated stickers on its own. A small service converts "
                      + "them into something it can play. Without one, text and "
                      + "pictures still work."
            }

            Item {
                id: enableRow
                width: column.width
                height: 30 * kgScaling

                Rectangle {
                    anchors.fill: parent
                    color: "#000000"
                    opacity: enableRow.activeFocus ? 0.14 : 0
                }

                Image {
                    id: enableIcon
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 20 * kgScaling
                    height: width
                    smooth: true
                    asynchronous: true
                    source: appSettings.mediaServer
                            ? "../../img/checkbox-marked-circle.png"
                            : "../../img/checkbox-blank-circle-outline.png"
                }

                Text {
                    anchors.left: enableIcon.right
                    anchors.leftMargin: 6 * kgScaling
                    anchors.verticalCenter: parent.verticalCenter
                    font.pixelSize: 12 * kgScaling
                    text: appSettings.mediaServer ? "Enabled" : "Disabled"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        enableRow.forceActiveFocus();
                        appSettings.toggle("mediaServer");
                    }
                }

                Keys.onPressed: {
                    if (event.key == Qt.Key_Select || event.key == Qt.Key_Return
                            || event.key == Qt.Key_Enter) {
                        appSettings.toggle("mediaServer");
                        event.accepted = true;
                    }
                }

                KeyNavigation.down: urlEdit
            }

            Text {
                text: "Address"
                color: "#666666"
                font.pixelSize: 10 * kgScaling
            }

            LineEdit {
                id: urlEdit
                width: column.width
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                KeyNavigation.up: enableRow
                KeyNavigation.down: tokenEdit
            }

            // Что лежит в настройках сейчас. Пока строка не совпала с полем --
            // значит ещё не сохранено, и это видно, а не подразумевается.
            Text {
                width: column.width
                wrapMode: Text.Wrap
                font.pixelSize: 10 * kgScaling
                color: urlEdit.text == appSettings.mediaServerUrl ? "#2E7D32" : "#999999"
                text: appSettings.mediaServerUrl.length == 0
                      ? "Nothing saved yet"
                      : (urlEdit.text == appSettings.mediaServerUrl
                         ? "Saved: " + appSettings.mediaServerUrl
                         : "Not saved yet, was: " + appSettings.mediaServerUrl)
            }

            Text {
                text: "Token, if the server asks for one"
                color: "#666666"
                font.pixelSize: 10 * kgScaling
            }

            LineEdit {
                id: tokenEdit
                width: column.width
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                KeyNavigation.up: urlEdit
                KeyNavigation.down: checkButton
            }

            Text {
                width: column.width
                font.pixelSize: 10 * kgScaling
                color: "#999999"
                text: appSettings.mediaServerToken.length == 0
                      ? "No token saved" : "Token saved"
            }

            Button {
                id: checkButton
                width: column.width
                text: "Save and check connection"
                KeyNavigation.up: tokenEdit
                KeyNavigation.down: clearCacheButton

                onClicked: {
                    // Saved first: the check has to use what is on screen, not
                    // what was there when the page opened.
                    appSettings.mediaServerUrl = urlEdit.text;
                    appSettings.mediaServerToken = tokenEdit.text;
                    appSettings.checkMediaServer();
                }
            }

            Text {
                width: column.width
                wrapMode: Text.Wrap
                font.pixelSize: 10 * kgScaling
                color: "#33556F"
                visible: appSettings.mediaServerStatus.length != 0
                text: appSettings.mediaServerStatus
            }

            Rectangle {
                width: column.width
                height: 1
                color: "#DDDDDD"
            }

            Text {
                text: "Cache"
                font.bold: true
                font.pixelSize: 14 * kgScaling
            }

            Text {
                width: column.width
                wrapMode: Text.Wrap
                color: "#666666"
                font.pixelSize: 10 * kgScaling
                text: "Avatars, pictures, stickers and converted media, "
                      + cacheSizeText + " in all. Deleting them frees phone "
                      + "memory; anything still needed is fetched again. "
                      + "The oldest are dropped by themselves at every start."
            }

            Button {
                id: clearCacheButton
                width: column.width
                text: cacheCleared ? "Cache cleared" : "Clear cache now"
                KeyNavigation.up: checkButton

                onClicked: {
                    cacheManager.clear();
                    cacheSizeText = cacheManager.sizeText;
                    cacheCleared = true;
                }
            }
        }
    }
}
