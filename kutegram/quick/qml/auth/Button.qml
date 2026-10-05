import QtQuick 1.0

// Works with both a touch screen and a phone keypad. The keypad path needs
// three things the MouseArea-only version never had: the item must be able to
// hold focus, the joystick centre must activate it, and the focus has to be
// visible on screen.
Item {
    id: buttonRoot
    signal clicked()

    // NOTE: no "property bool enabled" here -- Item already has one.
    //
    // "busy" is deliberately NOT wired to Item.enabled. Disabling an item that
    // currently holds the focus drops the focus on the floor, and nothing on a
    // keypad phone can pick it back up: after one failed request the whole
    // screen stopped reacting to the joystick. Busy therefore only dims the
    // button and swallows activations, leaving it focusable throughout.
    property bool busy: false

    // The label. Every caller so far wanted "Next", which is how it came to be
    // written into the Text below; a settings page wants its own word.
    property alias text: buttonText.text


    width: buttonText.width + 40 * kgScaling
    height: 40 * kgScaling

    // Symbian delivers the joystick centre as Key_Select. Return/Enter cover
    // the E90's real keyboard and the desktop builds; Space is a convention
    // for focused buttons.
    Keys.onPressed: {
        if (event.key == Qt.Key_Select || event.key == Qt.Key_Return
                || event.key == Qt.Key_Enter || event.key == Qt.Key_Space) {
            // Still accept the key when busy, otherwise it bubbles up to the
            // root and is read as "go back" while a request is in flight.
            event.accepted = true;

            if (!buttonRoot.busy)
                buttonRoot.clicked();
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 5
        color: globalAccent
        opacity: (buttonRoot.enabled && !buttonRoot.busy) ? 1.0 : 0.5
    }

    Text {
        id: buttonText
        anchors.centerIn: parent
        text: "Next"
        font.bold: true
        color: "#FFFFFF"
        font.pixelSize: 12 * kgScaling
    }

    MouseArea {
        id: innerArea
        anchors.fill: parent
        onClicked: {
            // Keep the focus ring in step with pointer use, so a device with
            // both a touch screen and a keypad does not lose its place.
            buttonRoot.forceActiveFocus();

            if (!buttonRoot.busy)
                buttonRoot.clicked();
        }
    }

    // Focus ring. The original bound this to a bare "activeFocus", which inside
    // a child Rectangle means *that rectangle's* focus and is therefore always
    // false -- the highlight could never appear.
    Rectangle {
        anchors.fill: parent
        radius: 5 * kgScaling
        color: "#000000"
        opacity: 0.1
        visible: buttonRoot.activeFocus
    }

    Rectangle {
        anchors.fill: parent
        radius: 5 * kgScaling
        color: "transparent"
        border.color: "#FFFFFF"
        border.width: 2 * kgScaling
        visible: buttonRoot.activeFocus
    }
}
