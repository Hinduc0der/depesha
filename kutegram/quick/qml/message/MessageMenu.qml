import QtQuick 1.0
import "../control"

// What can be done to the selected message. Opened with the left softkey while
// the message list has the focus -- the same key that opens the drawer
// everywhere else, because a keypad phone has nowhere else to put a menu.
// Item, а не Rectangle с прозрачностью: opacity наследуется всем поддеревом,
// поэтому полупрозрачным становился и белый фон меню, и его текст. На экране
// это выглядело как надписи, просвечивающие сквозь переписку.
Item {
    id: menuRoot

    property bool opened: false
    property int rowIndex: -1
    property string rowText: ""

    // Armed by the first press on Delete, cleared whenever the menu opens or
    // closes, so a stray press cannot take a message away.
    property bool confirmingDelete: false

    signal chosen(string action, int index, string text)

    anchors.fill: parent

    // Видимость без анимации и сразу: невидимый элемент не может взять фокус,
    // а с анимацией прозрачности он в момент открытия ещё невидим -- потому
    // меню и не слушалось клавиш вовсе.
    visible: opened

    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: 0.5
    }

    function open(index, text) {
        if (index < 0) {
            return;
        }

        rowIndex = index;
        rowText = text;
        confirmingDelete = false;
        opened = true;
        menuList.currentIndex = 0;
        menuList.forceActiveFocus();
    }

    function close() {
        opened = false;
        confirmingDelete = false;
    }

    MouseArea {
        anchors.fill: parent
        enabled: menuRoot.opened
        onClicked: {
            menuRoot.close();
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: menuList.count * 40 * kgScaling
        color: "#FFFFFF"

        MouseArea {
            anchors.fill: parent
        }

        ListView {
            id: menuList
            anchors.fill: parent
            // Не false: в QML это выключает не только пролистывание пальцем, но
            // и встроенную навигацию клавишами -- меню открывалось и не
            // двигалось.
            interactive: true
            keyNavigationWraps: false
            highlightFollowsCurrentItem: true
            highlightMoveDuration: 0

            highlight: Rectangle {
                width: menuList.width
                color: "#000000"
                opacity: menuList.activeFocus ? 0.14 : 0.0
            }

            model: ListModel {
                ListElement { action: "reply"; name: "Reply" }
                ListElement { action: "copy"; name: "Copy text" }
                ListElement { action: "delete"; name: "Delete" }
            }

            Keys.onPressed: {
                if (event.key == Qt.Key_Select || event.key == Qt.Key_Return
                        || event.key == Qt.Key_Enter) {
                    if (currentItem) {
                        currentItem.activate();
                    }
                    event.accepted = true;
                }
            }

            delegate: Item {
                id: menuRow
                width: ListView.view.width
                height: 40 * kgScaling

                function activate() {
                    menuRoot.chosen(action, menuRoot.rowIndex, menuRoot.rowText);
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        menuList.currentIndex = index;
                        menuRow.activate();
                    }
                }

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 12 * kgScaling
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    font.pixelSize: 12 * kgScaling
                    text: action == "delete" && menuRoot.confirmingDelete
                          ? "Delete - press again" : name
                    color: action == "delete" && menuRoot.confirmingDelete
                           ? "#CC0000" : "#000000"
                }
            }
        }
    }
}
