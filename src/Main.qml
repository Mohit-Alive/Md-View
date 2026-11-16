import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtWebEngine

ApplicationWindow {
    id: win
    width: 1100
    height: 820
    minimumWidth: 520
    minimumHeight: 400
    visible: true
    title: backend.fileName ? qsTr("%1 — MD View").arg(backend.fileName) : qsTr("MD View")
    color: theme.background

    Material.theme: theme.mode === "light" ? Material.Light : Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    property bool findVisible: false
    property bool outlineVisible: false
    property int renderedRevision: -1
    // Letter shortcuts stay quiet while the find field has the keyboard.
    readonly property bool typing: findField.activeFocus
    readonly property color mutedText: Qt.tint(theme.foreground, Qt.alpha(theme.background, 0.4))
    readonly property color panel: Qt.tint(theme.background, Qt.alpha(theme.foreground, 0.07))
    readonly property color border: Qt.tint(theme.background, Qt.alpha(theme.foreground, 0.2))

    function js(call) {
        if (view.ready)
            view.runJavaScript(call)
    }

    function pushStrings() {
        win.js("mdview.setStrings(" + JSON.stringify({
            "outline": qsTr("Outline"),
            "empty": qsTr("This file is empty.")
        }) + ")")
    }

    function pushTheme() {
        win.js("mdview.setTheme(" + JSON.stringify({
            "colors": theme.colors,
            "mode": theme.mode,
            "radius": theme.radius
        }) + ")")
    }

    function pushDocument() {
        if (!view.ready || backend.revision === win.renderedRevision)
            return
        const keep = backend.reloaded && win.renderedRevision >= 0
        win.renderedRevision = backend.revision
        win.js("mdview.render(" + JSON.stringify(backend.text) + ", " + JSON.stringify(backend.baseUrl)
               + ", " + keep + ")")
    }

    function openFind() {
        win.findVisible = true
        findField.forceActiveFocus()
        findField.selectAll()
    }

    function closeFind() {
        win.findVisible = false
        findField.text = ""
        view.findText("")
        view.forceActiveFocus()
    }

    function find(backward) {
        if (findField.text.length === 0) {
            view.findText("")
            return
        }
        view.findText(findField.text, backward ? WebEngineView.FindBackward : 0)
    }

    // Links: Markdown files open here, everything else in the app the
    // desktop picks for it (the browser for the web, Files for folders).
    function followLink(url) {
        const s = url.toString()
        const local = s.startsWith("file:")
        if (local && backend.isMarkdownUrl(s.split("#")[0]))
            backend.follow(s.split("#")[0])
        else if (/^(https?|mailto|file):/i.test(s))
            Qt.openUrlExternally(url)
    }

    Connections {
        target: backend
        function onDocumentChanged() { win.pushDocument() }
    }

    Connections {
        target: theme
        function onChanged() { win.pushTheme() }
    }

    Connections {
        target: translator
        function onChanged() { win.pushStrings() }
    }

    // Shortcuts. Q quits, ? lists them all.
    Shortcut { sequences: ["?", "Shift+?"]; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: Qt.quit() }
    Shortcut { sequences: ["O", StandardKey.Open]; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: backend.chooseFile() }
    Shortcut { sequences: ["R", "F5"]; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: { win.renderedRevision = -1; backend.reload(); win.pushDocument() } }
    Shortcut { sequence: "E"; context: Qt.ApplicationShortcut; enabled: !win.typing && backend.filePath !== ""; onActivated: backend.openInEditor() }
    Shortcut { sequence: "T"; context: Qt.ApplicationShortcut; enabled: !win.typing && view.ready; onActivated: view.runJavaScript("mdview.toggleToc()", function(open) { win.outlineVisible = open }) }
    Shortcut { sequences: ["/", StandardKey.Find]; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.openFind() }
    Shortcut { sequence: "J"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.js("mdview.scrollBy(64)") }
    Shortcut { sequence: "K"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.js("mdview.scrollBy(-64)") }
    Shortcut { sequence: "D"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.js("mdview.pageBy(0.5)") }
    Shortcut { sequence: "U"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.js("mdview.pageBy(-0.5)") }
    Shortcut { sequence: "G"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.js("mdview.top()") }
    Shortcut { sequence: "Shift+G"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.js("mdview.bottom()") }
    Shortcut { sequences: ["Backspace", "Alt+Left"]; context: Qt.ApplicationShortcut; enabled: !win.typing && backend.canGoBack; onActivated: backend.goBack() }
    Shortcut { sequences: [StandardKey.ZoomIn, "Ctrl+="]; context: Qt.ApplicationShortcut; onActivated: view.zoomFactor = Math.min(3, view.zoomFactor + 0.1) }
    Shortcut { sequences: [StandardKey.ZoomOut]; context: Qt.ApplicationShortcut; onActivated: view.zoomFactor = Math.max(0.5, view.zoomFactor - 0.1) }
    Shortcut { sequence: "Ctrl+0"; context: Qt.ApplicationShortcut; onActivated: view.zoomFactor = 1 }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (win.helpVisible) win.helpVisible = false
            else if (win.findVisible) win.closeFind()
            else if (backend.error) backend.clearError()
        }
    }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Problems say what's wrong right in the window.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: bannerText.implicitHeight + 20
                visible: backend.error !== "" || backend.missing
                color: Qt.tint(theme.background, Qt.alpha(theme.colors.red || "#f85149", 0.18))

                Label {
                    id: bannerText
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.Wrap
                    color: theme.foreground
                    text: backend.error !== "" ? backend.error
                        : qsTr("%1 was moved or deleted. Showing the last version.").arg(backend.fileName)
                }
            }

            WebEngineView {
                id: view
                Layout.fillWidth: true
                Layout.fillHeight: true
                url: "qrc:/web/index.html"
                backgroundColor: theme.background
                visible: backend.filePath !== ""

                property bool ready: false

                settings.localContentCanAccessFileUrls: true
                settings.localContentCanAccessRemoteUrls: true
                settings.javascriptCanOpenWindows: false
                settings.javascriptCanAccessClipboard: false
                settings.pluginsEnabled: false
                settings.errorPageEnabled: false
                settings.focusOnNavigationEnabled: false

                onLoadingChanged: function(request) {
                    if (request.status === WebEngineView.LoadSucceededStatus && request.url.toString().startsWith("qrc:")) {
                        ready = true
                        win.renderedRevision = -1
                        win.pushStrings()
                        win.pushTheme()
                        win.pushDocument()
                        view.forceActiveFocus()
                    }
                }

                // The page only ever shows the bundled renderer; clicked links
                // and dropped files are handled here instead of navigating.
                onNavigationRequested: function(request) {
                    const url = request.url.toString()
                    if (url.startsWith("qrc:/web/index.html") && request.navigationType !== WebEngineNavigationRequest.LinkClickedNavigation) {
                        request.accept()
                        return
                    }
                    request.reject()
                    if (request.navigationType === WebEngineNavigationRequest.LinkClickedNavigation)
                        win.followLink(request.url)
                    else if (url.startsWith("file:") && backend.isMarkdownUrl(url))
                        backend.open(url)
                }

                onNewWindowRequested: function(request) {
                    win.followLink(request.requestedUrl)
                }

                onFindTextFinished: function(result) {
                    findCount.text = findField.text.length === 0 ? ""
                        : result.numberOfMatches === 0 ? qsTr("No matches")
                        : qsTr("%1 of %2").arg(result.activeMatch).arg(result.numberOfMatches)
                }

                onContextMenuRequested: function(request) {
                    request.accepted = true
                    contextMenu.linkUrl = request.linkUrl
                    contextMenu.hasSelection = request.selectedText.length > 0
                    contextMenu.popup()
                }

                onRenderProcessTerminated: function(status, code) {
                    // A crashed renderer comes back with the same document.
                    view.ready = false
                    view.reload()
                }
            }

            Rectangle {
                id: findBar
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                visible: win.findVisible
                color: win.panel

                Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: win.border }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12

                    TextField {
                        id: findField
                        Layout.preferredWidth: 320
                        placeholderText: qsTr("Find in document")
                        onTextChanged: win.find(false)
                        Keys.onReturnPressed: function(event) { win.find(event.modifiers & Qt.ShiftModifier) }
                        Keys.onEnterPressed: function(event) { win.find(event.modifiers & Qt.ShiftModifier) }
                        Keys.onEscapePressed: win.closeFind()
                    }

                    Label { id: findCount; color: win.mutedText }

                    Item { Layout.fillWidth: true }

                    Label {
                        color: win.mutedText
                        text: qsTr("Enter next · Shift+Enter previous · Esc close")
                        elide: Text.ElideRight
                        Layout.maximumWidth: findBar.width - 520
                    }
                }
            }
        }

        // With nothing open, say how to open something.
        Column {
            anchors.centerIn: parent
            spacing: 14
            visible: backend.filePath === ""

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("No document open")
                font.pixelSize: 26
                font.weight: Font.DemiBold
                color: theme.foreground
            }
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Press %1 to open a Markdown file, or %2 for every shortcut.").arg("O").arg("?")
                color: win.mutedText
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Open File")
                highlighted: true
                onClicked: backend.chooseFile()
            }
        }

        Menu {
            id: contextMenu
            property url linkUrl
            property bool hasSelection: false

            MenuItem {
                text: qsTr("Copy")
                enabled: contextMenu.hasSelection
                onTriggered: view.triggerWebAction(WebEngineView.Copy)
            }
            MenuItem {
                text: qsTr("Open Link")
                visible: contextMenu.linkUrl.toString() !== ""
                height: visible ? implicitHeight : 0
                onTriggered: win.followLink(contextMenu.linkUrl)
            }
            MenuItem {
                text: qsTr("Copy Link Address")
                visible: contextMenu.linkUrl.toString() !== ""
                height: visible ? implicitHeight : 0
                onTriggered: view.triggerWebAction(WebEngineView.CopyLinkToClipboard)
            }
            MenuSeparator {}
            MenuItem { text: qsTr("Find"); onTriggered: win.openFind() }
            MenuItem { text: win.outlineVisible ? qsTr("Hide Outline") : qsTr("Show Outline"); onTriggered: view.runJavaScript("mdview.toggleToc()", function(open) { win.outlineVisible = open }) }
            MenuItem { text: qsTr("Open in Editor"); enabled: backend.filePath !== ""; onTriggered: backend.openInEditor() }
        }

        // Every shortcut, shown with ?.
        Rectangle {
            anchors.fill: parent
            visible: win.helpVisible
            color: Qt.alpha(theme.background, 0.85)

            MouseArea { anchors.fill: parent; onClicked: win.helpVisible = false }

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - 48, helpList.implicitWidth + 64)
                height: Math.min(parent.height - 48, helpColumn.implicitHeight + 48)
                radius: theme.radius
                color: win.panel
                border.color: win.border

                Column {
                    id: helpColumn
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 16

                    Label {
                        text: qsTr("Keyboard Shortcuts")
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                        color: theme.foreground
                    }

                    Column {
                        id: helpList
                        spacing: 8

                        Repeater {
                            model: [
                                ["O", qsTr("Open a Markdown file")],
                                ["E", qsTr("Edit this file in your editor")],
                                ["R", qsTr("Reload")],
                                ["T", qsTr("Show or hide the outline")],
                                ["/  Ctrl+F", qsTr("Find in document")],
                                ["J  K", qsTr("Scroll down or up")],
                                ["D  U", qsTr("Half a page down or up")],
                                [qsTr("Space"), qsTr("Next page (Shift for previous)")],
                                ["G  Shift+G", qsTr("Top or bottom")],
                                [qsTr("Backspace"), qsTr("Back to the previous file")],
                                ["Ctrl+  Ctrl−  Ctrl+0", qsTr("Zoom in, out, or reset")],
                                ["?", qsTr("Show or hide these shortcuts")],
                                ["Q", qsTr("Quit")]
                            ]

                            delegate: Row {
                                required property var modelData
                                spacing: 24

                                Label {
                                    width: 190
                                    text: modelData[0]
                                    font.family: "monospace"
                                    color: theme.accent
                                }
                                Label {
                                    text: modelData[1]
                                    color: theme.foreground
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
