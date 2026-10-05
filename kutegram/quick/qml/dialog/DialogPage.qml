import QtQuick 1.0
import Kutegram 1.0

Rectangle {
    id: pageRoot
    signal refresh()

    // --- keypad entry points, called from MainScreen -------------------------

    // The dialogs of the folder currently on screen. folderSlide's delegate is
    // the vertical ListView, so its currentItem *is* that list.
    function currentList() {
        return folderSlide.currentItem;
    }

    // Set while the pane wants the focus but the list it belongs to does not
    // exist yet. Starting with a saved session used to come up with dialogs on
    // screen and nothing that reacted to the joystick: the pane was focused
    // before the folders had loaded, and nothing ever tried again.
    property bool wantsFocus: false

    function focusList() {
        var list = currentList();
        if (!list) {
            wantsFocus = true;
            return;
        }

        wantsFocus = false;
        list.forceActiveFocus();
        if (list.currentIndex < 0)
            list.moveCurrent(1);
    }

    ListView {
        id: folderSlide

        anchors.fill: parent
        clip: true

        model: foldersModel
        // One folder either side, not five. Every cached folder is a whole
        // second ListView with its own delegates kept alive.
        cacheBuffer: width

        boundsBehavior: Flickable.StopAtBounds
        orientation: ListView.Horizontal
        snapMode: ListView.SnapOneItem
        highlightRangeMode: ListView.StrictlyEnforceRange
        highlightFollowsCurrentItem: true
        highlightMoveDuration: 200

        currentIndex: currentFolderIndex
        onCurrentItemChanged: {
            currentFolderIndex = currentIndex

            // The strip only ever moves because the user moved it, so the list
            // of the folder that arrived should end up holding the focus.
            // wantsFocus covers startup, where the folders load after the page
            // has already been asked to take the focus.
            if (currentItem && (pageRoot.wantsFocus || mainScreen.state != "CHAT"))
                pageRoot.focusList();
        }

        delegate: ListView {
            id: dialogsView
            width: folderSlide.width
            height: folderSlide.height
            cacheBuffer: pageRoot.height / 6

            clip: true

            boundsBehavior: Flickable.StopAtBounds

            // Nothing is selected until the user actually moves the joystick,
            // so an untouched list does not look like it has a pending action.
            currentIndex: -1
            keyNavigationWraps: false
            highlightFollowsCurrentItem: true
            highlightMoveDuration: 0

            highlight: Rectangle {
                width: dialogsView.width
                color: "#000000"
                opacity: dialogsView.activeFocus ? 0.14 : 0.06
            }

            // A row whose dialog is not in this folder is rendered by an empty
            // Repeater and is zero pixels tall, so plain ListView key handling
            // would park the highlight on an invisible row. Step over those.
            function moveCurrent(step) {
                if (count <= 0)
                    return;

                var from = currentIndex;
                if (from < 0)
                    from = step > 0 ? -1 : count;

                for (var i = from + step; i >= 0 && i < count; i += step) {
                    if (dialogsModel.inFolder(i, folderIndex)) {
                        currentIndex = i;
                        positionViewAtIndex(i, ListView.Contain);
                        return;
                    }
                }
            }

            function openCurrent() {
                // currentItem is the Repeater that wraps the row. Do not reach
                // for Repeater.itemAt() here: the row registers itself in
                // dialogItem instead, which works whatever the Qt Quick 1
                // minor version provides. Going through itemAt() failed
                // silently on the phone -- the key was handled, nothing opened.
                var row = currentItem;
                if (!row)
                    return;

                if (row.dialogItem)
                    row.dialogItem.openDialog();
            }

            // Up and down are deliberately absent: they are left to the
            // ListView's own key handling, the same path the message list uses,
            // which is the one that behaves itself on the phone. Left and right
            // are left alone too -- unaccepted, they reach the folder strip,
            // whose own key handling moves it by exactly one folder.
            //
            // The built-in walk knows nothing about rows hidden in this folder;
            // onCurrentIndexChanged below steps over those afterwards.
            Keys.onPressed: {
                switch (event.key) {
                case Qt.Key_Right:
                    if (folderSlide.count <= 1) {
                        // Nothing to move between, so right is free to mean what
                        // it means everywhere else on S60: go in.
                        openCurrent();
                        event.accepted = true;
                    }
                    break;
                case Qt.Key_Select:
                case Qt.Key_Return:
                case Qt.Key_Enter:
                    openCurrent();
                    event.accepted = true;
                    break;
                }
            }

            // Which way the last step went, so a run of hidden rows is crossed
            // in the direction the user was already moving.
            property int lastIndex: -1
            property bool adjusting: false

            onCurrentIndexChanged: {
                if (adjusting)
                    return;

                if (currentIndex >= 0 && !dialogsModel.inFolder(currentIndex, folderIndex)) {
                    var step = currentIndex >= lastIndex ? 1 : -1;
                    var i = currentIndex;

                    while (i >= 0 && i < count && !dialogsModel.inFolder(i, folderIndex))
                        i += step;

                    adjusting = true;
                    currentIndex = (i >= 0 && i < count) ? i : lastIndex;
                    adjusting = false;
                }

                lastIndex = currentIndex;

                if (currentIndex >= 0)
                    positionViewAtIndex(currentIndex, ListView.Contain);

                // The keypad never flicks the list, so onMovementEnded below is
                // not what loads the next page here either.
                if (currentIndex >= count - 3 && dialogsModel.canFetchMoreDownwards())
                    dialogsModel.fetchMoreDownwards();
            }

            onMovementEnded: {
                // Was canFetchMoreDownwards() -- the question, asked twice,
                // instead of the answer.
                if (atYEnd && dialogsModel.canFetchMoreDownwards()) {
                    dialogsModel.fetchMoreDownwards();
                }
            }

            model: dialogsModel

            delegate: Repeater {
                id: dialogRepeater

                // The row this Repeater produced, if any. Repeater parents its
                // items to its own parent rather than to itself, so there is no
                // way to walk down to them from here.
                property variant dialogItem: null

                model: dialogsModel.inFolder(index, folderIndex)
                height: count != 0 ? 40 * kgScaling : 0

                onCountChanged: {
                    if (count == 0)
                        dialogItem = null;
                }

                DialogItem {
                    id: dialogRow
                    y: dialogRepeater.y
                    width: dialogsView.width

                    // Must be the id, not "this": inside a QML signal handler
                    // "this" is not the item, so the assignment used to store a
                    // value that passed a truth test and then blew up with a
                    // TypeError on the first method call.
                    Component.onCompleted: dialogRepeater.dialogItem = dialogRow
                }
            }
        }
    }
}
