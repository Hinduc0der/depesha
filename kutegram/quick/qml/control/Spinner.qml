import QtQuick 1.0

Item {
    id: spinnerRoot

    width: 40 * kgScaling
    height: width

    property bool white: false

    Image {
        asynchronous: true
        anchors.centerIn: parent
        source: white ? "../../img/loading_white.png" : "../../img/loading.png"
        width: 20 * kgScaling
        height: width

        // No "smooth" here on purpose. This image is rotated continuously, and
        // a filtered rotation in software costs far more per frame than the
        // slight aliasing it removes -- on a 332 MHz phone it is the difference
        // between a usable UI and a slideshow.

        PropertyAnimation on rotation {
            // The animation used to run unconditionally, for every Spinner in
            // the scene, including hidden ones and one per message delegate.
            // Each frame dirtied an item and forced the whole view to repaint,
            // so the application was permanently busy animating spinners nobody
            // could see. In Qt Quick 1 "visible" already reflects the ancestors'
            // visibility, so this stops them properly.
            running: spinnerRoot.visible
            loops: Animation.Infinite
            from: 0
            to: 360
            easing.type: Easing.InOutQuad
            duration: 1400
        }
    }
}
