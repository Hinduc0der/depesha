import QtQuick 1.0

// A FocusScope so that a KeyNavigation chain can address the LineEdit itself
// while the text cursor lands on the inner TextInput. Up/Down are not consumed
// by a single-line TextInput, so they bubble back up here and drive the chain;
// Left/Right stay with the cursor, as they should.
FocusScope {
    id: lineEditRoot

    width: 160 * kgScaling
    height: 40 * kgScaling

    property alias text: innerInput.text
    property alias inputMethodHints: innerInput.inputMethodHints
    property alias echoMode: innerInput.echoMode

    state: innerInput.activeFocus ? "FOCUSED" : "NOT_FOCUSED"

    states: [
        State {
            name: "NOT_FOCUSED"
            PropertyChanges {
                target: focusedRect
                width: 0
            }
        },
        State {
            name: "FOCUSED"
            PropertyChanges {
                target: focusedRect
                width: parent.width - 10 * kgScaling
            }
        }
    ]

    transitions: [
        Transition {
            NumberAnimation {
                properties: "width"
                easing.type: Easing.InOutQuad
                duration: 200
            }
        }
    ]

    MouseArea {
        anchors.fill: parent
        onClicked: {
            lineEditRoot.forceActiveFocus();
        }
    }

    TextInput {
        id: innerInput
        // Makes the FocusScope hand its focus down to the actual editor, so
        // forceActiveFocus() on the LineEdit puts the caret here.
        focus: true
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 5 * kgScaling
        anchors.rightMargin: 5 * kgScaling
        font.pixelSize: 12 * kgScaling
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: parent.width - 10 * kgScaling
        height: 2 * kgScaling
        color: "#999999"
    }

    Rectangle {
        id: focusedRect
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: parent.width - 10 * kgScaling
        height: 2 * kgScaling
        color: globalAccent
    }
}
