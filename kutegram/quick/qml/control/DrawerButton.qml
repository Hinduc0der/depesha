import QtQuick 1.0

Item {
    id: drawerButtonRoot
    width: ListView.view.width
    height: 40 * kgScaling

    // A switch, as opposed to something that happens once.
    property bool isToggle: action == "markAsRead" || action == "notifications"
                            || action == "vibrate"
    // Через сами свойства, а не через appSettings.value(action): вызов метода
    // не создаёт зависимости, и привязка не пересчитывалась. Значение менялось,
    // а галочка на экране оставалась прежней.
    property bool toggleOn: action == "markAsRead" ? appSettings.markAsRead
                          : action == "notifications" ? appSettings.notifications
                          : action == "vibrate" ? appSettings.vibrate
                          : action == "mediaServer" ? appSettings.mediaServer : false

    // Shared by the pointer and the keypad so the two cannot drift apart.
    function activate() {
        if (isToggle) {
            appSettings.toggle(action);
            // Deliberately staying open: changing one switch usually means
            // changing another.
            return;
        }

        switch (action) {
        case "settings":
            drawerRoot.closeDrawer();
            mainScreen.openSettings();
            return;
        case "logout":
            // The drawer opens on the left softkey, so a stray press used to
            // wipe the session -- which on a 2FA account costs a minute of key
            // derivation to undo. Ask once.
            if (!drawerRoot.confirmingLogout) {
                drawerRoot.confirmingLogout = true;
                snackBar.text = "Press again to log out.";
                return;
            }

            drawerRoot.confirmingLogout = false;
            snackBar.close();
            telegramClient.resetSession();
            break;
        case "quit":
            platformUtils.quit();
            break;
        }

        drawerRoot.closeDrawer();
    }

    Item {
        id: actionIcon
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: height

        Image {
            anchors.centerIn: parent
            asynchronous: true
            source: drawerButtonRoot.isToggle
                    ? (drawerButtonRoot.toggleOn
                       ? "../../img/checkbox-marked-circle.png"
                       : "../../img/checkbox-blank-circle-outline.png")
                    : icon
            smooth: true
            width: 20 * kgScaling
            height: width
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            drawerButtonRoot.activate();
        }
    }

    Text {
        anchors.left: actionIcon.right
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        text: name
        font.pixelSize: 12 * kgScaling
    }
}
