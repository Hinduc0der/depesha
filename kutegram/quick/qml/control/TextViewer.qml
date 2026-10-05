import QtQuick 1.0

// Просмотр текстового файла прямо в клиенте.
//
// Появился после того, как скачанный .md уронил телефон: для Symbian это
// неизвестный тип, открывать его нечем, а просьба к системе «открой это»
// заканчивалась зависанием. Текст проще показать самим -- тем более что читать
// присланный текстовый файл на телефоне и есть смысл происходящего.
Rectangle {
    id: viewerRoot

    property bool opened: false
    property string fileName: ""

    anchors.fill: parent
    color: "#FFFFFF"
    visible: opened

    function open(path) {
        fileName = fileBrowser.fileName(path);
        body.text = fileBrowser.readText(path);
        opened = true;
        flick.contentY = 0;
        focusDelay.start();
    }

    function close() {
        opened = false;
        // Текст может быть на сотни килобайт: держать его в памяти после
        // закрытия незачем.
        body.text = "";
    }

    // См. FilePicker: только что показанный элемент не может взять фокус сразу.
    Timer {
        id: focusDelay
        interval: 1
        onTriggered: flick.forceActiveFocus()
    }

    MouseArea {
        anchors.fill: parent
    }

    Rectangle {
        id: viewerHeader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 24 * kgScaling
        color: globalAccent

        Text {
            anchors.fill: parent
            anchors.leftMargin: 4 * kgScaling
            anchors.rightMargin: 4 * kgScaling
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideMiddle
            color: "#FFFFFF"
            font.pixelSize: 11 * kgScaling
            text: viewerRoot.fileName
        }
    }

    Flickable {
        id: flick
        anchors.top: viewerHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 4 * kgScaling
        clip: true

        contentWidth: width
        contentHeight: body.paintedHeight
        boundsBehavior: Flickable.StopAtBounds

        // Джойстик листает страницами: построчная прокрутка на этом экране
        // бесполезна, а другого способа двигаться тут нет.
        Keys.onPressed: {
            var step = flick.height - 20 * kgScaling;

            if (event.key == Qt.Key_Down) {
                flick.contentY = Math.min(flick.contentY + step,
                                          Math.max(0, flick.contentHeight - flick.height));
                event.accepted = true;
            } else if (event.key == Qt.Key_Up) {
                flick.contentY = Math.max(flick.contentY - step, 0);
                event.accepted = true;
            }
        }

        Text {
            id: body
            width: flick.width
            wrapMode: Text.Wrap
            font.pixelSize: 11 * kgScaling
            color: "#000000"
        }
    }
}
