import QtQuick 1.0

// Выбор файла для отправки.
//
// Свой, а не системный: по виджетному диалогу невозможно ходить джойстиком,
// в нём не переключиться на карту памяти, и он запускает вложенный цикл
// событий посреди обработчика QML. Здесь всё это просто не возникает.
Rectangle {
    id: pickerRoot

    property bool opened: false
    property string path: ""

    signal picked(string file)

    anchors.fill: parent
    color: "#FFFFFF"
    visible: opened

    function open() {
        opened = true;
        load("");

        // Фокус -- следующим тактом. Элемент только что стал видимым, а взять
        // фокус он может лишь когда видим уже окончательно: вызов сразу здесь
        // молча не срабатывает, и экран открывается неуправляемым.
        focusDelay.start();
    }

    Timer {
        id: focusDelay
        interval: 1
        onTriggered: entries.forceActiveFocus()
    }

    function close() {
        opened = false;
    }

    // Шаг вверх: из каталога — в родительский, из корня диска — к списку
    // дисков, а из списка дисков — закрыть выбор. Возвращает false, когда
    // выше идти уже некуда, чтобы клавишу "назад" обработал кто-то ещё.
    function up() {
        if (path.length == 0) {
            close();
            return false;
        }

        load(fileBrowser.parentOf(path));
        return true;
    }

    // Список отдаётся ListView как есть. ListModel тут не годится: его append()
    // принимает только простые типы QML, спотыкается на размере файла (64 бита)
    // и роняет функцию целиком -- проверено на десктопе, заполнение обрывалось
    // молча.
    property variant entriesList: []

    function load(newPath) {
        path = newPath;
        entriesList = fileBrowser.list(newPath);

        entries.currentIndex = entriesList.length > 0 ? 0 : -1;
        entries.positionViewAtBeginning();
    }

    function sizeText(bytes) {
        if (bytes > 1048576) {
            return Math.round(bytes / 1048576 * 10) / 10 + " MB";
        }

        if (bytes > 1024) {
            return Math.round(bytes / 1024) + " KB";
        }

        return bytes + " B";
    }

    MouseArea {
        anchors.fill: parent
    }

    Rectangle {
        id: pickerHeader
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
            elide: Text.ElideLeft
            color: "#FFFFFF"
            font.pixelSize: 11 * kgScaling
            text: pickerRoot.path.length == 0 ? "Select a drive" : pickerRoot.path
        }
    }

    ListView {
        id: entries
        anchors.top: pickerHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true

        boundsBehavior: Flickable.StopAtBounds
        keyNavigationWraps: false
        highlightFollowsCurrentItem: true
        highlightMoveDuration: 0
        focus: true

        highlight: Rectangle {
            width: entries.width
            color: "#000000"
            opacity: entries.activeFocus ? 0.14 : 0.06
        }

        model: pickerRoot.entriesList

        Keys.onPressed: {
            switch (event.key) {
            case Qt.Key_Select:
            case Qt.Key_Return:
            case Qt.Key_Enter:
            case Qt.Key_Right:
                if (currentItem) {
                    currentItem.activate();
                }
                event.accepted = true;
                break;
            case Qt.Key_Left:
                pickerRoot.up();
                event.accepted = true;
                break;
            }
        }

        delegate: Item {
            id: entryRow
            width: ListView.view.width
            height: 32 * kgScaling

            function activate() {
                if (modelData.isDir) {
                    pickerRoot.load(modelData.path);
                    return;
                }

                pickerRoot.picked(modelData.path);
                pickerRoot.close();
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    entries.currentIndex = index;
                    entryRow.activate();
                }
            }

            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 6 * kgScaling
                anchors.rightMargin: 6 * kgScaling
                anchors.top: parent.top
                anchors.topMargin: 3 * kgScaling
                elide: Text.ElideMiddle
                font.pixelSize: 12 * kgScaling
                font.bold: modelData.isDir
                text: modelData.isDir ? (modelData.name + "/") : modelData.name
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 6 * kgScaling
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 2 * kgScaling
                color: "#999999"
                font.pixelSize: 9 * kgScaling
                visible: !modelData.isDir
                text: pickerRoot.sizeText(modelData.size)
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: "#EEEEEE"
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: pickerRoot.entriesList.length == 0
        color: "#999999"
        font.pixelSize: 12 * kgScaling
        text: "Empty"
    }
}
