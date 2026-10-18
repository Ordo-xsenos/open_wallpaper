pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: root

    required property ManagerController controller
    property bool ru: root.controller.language === "ru"
    property int page: 0
    property var selected: ({
    })

    function t(russian, english) {
        return ru ? russian : english;
    }

    function changeSetting(key, value) {
        let next = Object.assign({
        }, root.controller.settings);
        next[key] = value;
        root.controller.updateSettings(next);
    }

    function sceneTitle(id) {
        for (let i = 0; i < root.controller.scenes.length; ++i) if (root.controller.scenes[i].id === id) {
            return root.controller.scenes[i].title;
        }
        return t("Встроенный градиент", "Built-in gradient");
    }

    width: 1180
    height: 780
    minimumWidth: 900
    minimumHeight: 620
    visible: true
    title: root.controller.status.mode === "headless-test" ? "AetherWall · HEADLESS TEST" : "AetherWall"
    color: "#15181e"
    font.family: Qt.platform.os === "windows" ? "Segoe UI" : "sans-serif"
    font.pixelSize: 14
    palette.window: "#15181e"
    palette.windowText: "#f0f2f6"
    palette.base: "#1e232b"
    palette.alternateBase: "#252c35"
    palette.text: "#f0f2f6"
    palette.button: "#2b323d"
    palette.buttonText: "#f0f2f6"
    palette.highlight: "#85b7ff"
    palette.highlightedText: "#101b2b"
    palette.placeholderText: "#b8c1ce"

    Connections {
        function onChanged() {
            if (!root.selected.id)
                return ;

            for (let i = 0; i < root.controller.scenes.length; ++i) if (root.controller.scenes[i].id === root.selected.id) {
                root.selected = root.controller.scenes[i];
                return ;
            }
        }

        target: root.controller
    }

    FileDialog {
        id: packagePicker

        title: root.t("Импорт сцены", "Import scene")
        nameFilters: ["AetherWall (*.awscene)"]
        onAccepted: root.controller.importScene(packagePicker.selectedFile)
    }

    FolderDialog {
        id: folderPicker

        title: root.t("Выберите папку с manifest.json", "Choose a folder containing manifest.json")
        onAccepted: root.controller.importScene(folderPicker.selectedFolder)
    }

    Dialog {
        id: deleteDialog

        title: root.t("Удалить сцену из библиотеки?", "Delete scene from the library?")
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: {
            root.controller.removeScene(root.selected.id);
            root.selected = ({
            });
        }

        Label {
                            textFormat: Text.PlainText
            width: 360
            wrapMode: Text.Wrap
            text: root.t("Будет удалена управляемая копия. Исходные файлы останутся на месте. Активную сцену сначала замените другой.", "The managed copy will be deleted. Original files remain. Apply another scene before deleting an active scene.")
        }

    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 208
            Layout.fillHeight: true
            color: "#1b2027"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8

                RowLayout {
                    Layout.topMargin: 12
                    Layout.bottomMargin: 28
                    spacing: 10

                    Image {
                        source: "qrc:/qt/qml/AetherWall/Manager/assets/aetherwall.png"
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                    }

                    Label {
                            textFormat: Text.PlainText
                        text: "AetherWall"
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                    }

                }

                Repeater {
                    model: [root.t("Библиотека", "Library"), root.t("Каталог", "Discover"), root.t("Мониторы", "Monitors"), root.t("Производительность", "Performance"), root.t("Настройки", "Settings")]

                    delegate: Button {
                        required property int index
                        required property string modelData

                        Layout.fillWidth: true
                        text: modelData
                        checked: root.page === index
                        checkable: true
                        onClicked: root.page = index
                        Accessible.name: modelData
                    }

                }

                Item {
                    Layout.fillHeight: true
                }

                Label {
                            textFormat: Text.PlainText
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: root.t("Обои продолжают работать после закрытия этого окна.", "Wallpapers keep running after you close this window.")
                    color: "#b8c1ce"
                    font.pixelSize: 12
                }

                Button {
                    Layout.fillWidth: true
                    text: root.t("Пауза для всех", "Pause all")
                    enabled: root.controller.connected && !root.controller.busy
                    onClicked: root.controller.pause(true, "*")
                }

                Button {
                    Layout.fillWidth: true
                    text: root.t("Продолжить для всех", "Resume all")
                    enabled: root.controller.connected && !root.controller.busy
                    onClicked: root.controller.pause(false, "*")
                }

            }

        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 24
            Layout.minimumHeight: 0
            Layout.minimumWidth: 0
            spacing: 20

            RowLayout {
                Layout.fillWidth: true

                Label {
                            textFormat: Text.PlainText
                    text: [root.t("Библиотека", "Library"), root.t("Каталог", "Discover"), root.t("Мониторы", "Monitors"), root.t("Производительность", "Performance"), root.t("Настройки", "Settings")][root.page]
                    font.pixelSize: 28
                    font.weight: Font.DemiBold
                }

                Item {
                    Layout.fillWidth: true
                }

                BusyIndicator {
                    running: root.controller.busy
                    visible: running
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    Accessible.name: root.t("Выполняется команда", "Command in progress")
                }

                Label {
                            textFormat: Text.PlainText
                    text: root.controller.connected ? (root.controller.status.mode === "headless-test" ? root.t("Тестовый транспорт · без GPU", "Test transport · no GPU") : root.t("Движок подключён", "Engine connected")) : root.t("Нет подключения к движку", "Engine disconnected")
                    color: "#b8c1ce"
                }

                Button {
                    text: root.t("Запустить движок", "Start engine")
                    visible: !root.controller.connected
                    onClicked: root.controller.startCore()
                }

            }

            Frame {
                Layout.fillWidth: true
                visible: root.controller.error.length > 0

                RowLayout {
                    anchors.fill: parent

                    Label {
                            textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: root.controller.error
                        wrapMode: Text.Wrap
                        Accessible.role: Accessible.AlertMessage
                    }

                    Button {
                        text: root.t("Закрыть", "Dismiss")
                        onClicked: root.controller.dismissError()
                    }

                }

            }

            StackLayout {
                Layout.minimumHeight: 0
                Layout.minimumWidth: 0
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.page

                ColumnLayout {
                    Layout.minimumHeight: 0
                    spacing: 16

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        TextField {
                            id: search

                            Layout.fillWidth: true
                            placeholderText: root.t("Поиск по названию или автору", "Search title or author")
                            Accessible.name: placeholderText
                        }

                        CheckBox {
                            id: favorites

                            text: root.t("Избранное", "Favorites")
                        }

                        ComboBox {
                            id: sort

                            model: [root.t("По названию", "By title"), root.t("Избранное сначала", "Favorites first")]
                            Accessible.name: root.t("Сортировка", "Sort order")
                        }

                        Button {
                            text: root.t("Импорт", "Import")
                            enabled: root.controller.connected && !root.controller.busy
                            onClicked: importMenu.open()

                            Menu {
                                id: importMenu

                                MenuItem {
                                    text: root.t("Пакет .awscene", ".awscene package")
                                    onTriggered: packagePicker.open()
                                }

                                MenuItem {
                                    text: root.t("Папка сцены", "Scene folder")
                                    onTriggered: folderPicker.open()
                                }

                            }

                        }

                    }

                    RowLayout {
                        objectName: "libraryBody"
                        Layout.minimumHeight: 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 24

                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            GridView {
                                id: sceneGrid

                                objectName: "sceneGrid"
                                anchors.fill: parent
                                clip: true
                                cellWidth: width / Math.max(1, Math.floor(width / 240))
                                cellHeight: cellWidth * 0.5625 + 82
                                model: root.controller.filterScenes(root.controller.scenes, search.text, favorites.checked, sort.currentIndex)

                                ScrollBar.vertical: ScrollBar {
                                }

                                delegate: Button {
                                    id: sceneButton

                                    required property var modelData

                                    width: sceneGrid.cellWidth - 16
                                    height: sceneGrid.cellHeight - 16
                                    padding: 0
                                    Accessible.name: modelData.title + ", " + modelData.author
                                    onClicked: root.selected = modelData

                                    background: Rectangle {
                                        color: sceneButton.hovered ? "#2b323d" : "#1e232b"
                                        radius: 12
                                        border.width: sceneButton.activeFocus || root.selected.id === sceneButton.modelData.id ? 2 : 0
                                        border.color: "#85b7ff"
                                    }

                                    contentItem: ColumnLayout {
                                        spacing: 6

                                        Image {
                                            source: sceneButton.modelData.previewUrl
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: sceneButton.width * 0.5625
                                            fillMode: Image.PreserveAspectCrop
                                            asynchronous: true
                                            sourceSize.width: 480
                                            sourceSize.height: 270
                                        }

                                        Label {
                            textFormat: Text.PlainText
                                            Layout.fillWidth: true
                                            Layout.leftMargin: 12
                                            Layout.rightMargin: 12
                                            text: sceneButton.modelData.title
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }

                                        Label {
                            textFormat: Text.PlainText
                                            Layout.fillWidth: true
                                            Layout.leftMargin: 12
                                            Layout.rightMargin: 12
                                            Layout.bottomMargin: 10
                                            text: sceneButton.modelData.author
                                            color: "#b8c1ce"
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                        }

                                    }

                                }

                            }

                            ColumnLayout {
                                anchors.centerIn: parent
                                width: Math.min(parent.width - 32, 360)
                                spacing: 12
                                visible: sceneGrid.count === 0

                                Label {
                            textFormat: Text.PlainText
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    horizontalAlignment: Text.AlignHCenter
                                    font.pixelSize: 20
                                    text: root.controller.connected ? root.t("Сцены не найдены", "No scenes found") : root.t("Подключите движок", "Connect the engine")
                                }

                                Label {
                            textFormat: Text.PlainText
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    horizontalAlignment: Text.AlignHCenter
                                    color: "#b8c1ce"
                                    text: root.controller.connected ? root.t("Измените поиск или импортируйте локальную сцену.", "Change your search or import a local scene.") : root.t("Запустите Core, чтобы открыть библиотеку и управлять обоями.", "Start Core to browse the library and control wallpapers.")
                                }

                            }

                        }

                        ScrollView {
                            id: detailScroll
                            objectName: "sceneDetails"
                            Layout.preferredWidth: 264
                            Layout.minimumWidth: 240
                            Layout.maximumWidth: 280
                            Layout.minimumHeight: 0
                            Layout.fillHeight: true
                            contentWidth: availableWidth
                            clip: true
                            ColumnLayout {
                                width: detailScroll.availableWidth
                                spacing: 12

                            Image {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 150
                                source: root.selected.previewUrl || ""
                                fillMode: Image.PreserveAspectCrop
                                sourceSize.width: 528
                                sourceSize.height: 300
                                visible: !!root.selected.id
                            }

                            Label {
                            textFormat: Text.PlainText
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                font.pixelSize: 21
                                font.weight: Font.DemiBold
                                text: root.selected.title || root.t("Выберите сцену", "Choose a scene")
                            }

                            Label {
                            textFormat: Text.PlainText
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                color: "#b8c1ce"
                                text: root.selected.id ? root.selected.author + "\n" + root.selected.license : root.t("Превью статичны: анимацию запускает только движок.", "Previews are static. Only the engine runs animation.")
                            }

                            Label {
                            textFormat: Text.PlainText
                                text: root.t("Применить на", "Apply to")
                                visible: !!root.selected.id
                            }

                            ComboBox {
                                id: targetMonitor

                                Layout.fillWidth: true
                                model: [{
                                    "id": "*",
                                    "alias": root.t("Все мониторы", "All monitors")
                                }].concat(root.controller.monitors)
                                textRole: "alias"
                                valueRole: "id"
                                Accessible.name: root.t("Целевой монитор", "Target monitor")
                                enabled: root.controller.connected && !root.controller.busy
                            }

                            Button {
                                Layout.fillWidth: true
                                highlighted: true
                                text: root.t("Применить сцену", "Apply scene")
                                enabled: root.controller.connected && !root.controller.busy && !!root.selected.id && root.controller.monitors.length > 0
                                onClicked: root.controller.apply(targetMonitor.currentValue || "*", root.selected.id)
                            }

                            Button {
                                Layout.fillWidth: true
                                text: root.selected.favorite ? root.t("Убрать из избранного", "Remove favorite") : root.t("В избранное", "Add to favorites")
                                enabled: root.controller.connected && !root.controller.busy && !!root.selected.id
                                onClicked: root.controller.favorite(root.selected.id, !root.selected.favorite)
                            }

                            Button {
                                Layout.fillWidth: true
                                text: root.t("Открыть папку", "Open folder")
                                enabled: !!root.selected.folderUrl
                                onClicked: root.controller.openFolder(root.selected.folderUrl)
                            }


                            Button {
                                Layout.fillWidth: true
                                text: root.t("Удалить из библиотеки…", "Delete from library…")
                                enabled: root.controller.connected && !root.controller.busy && !!root.selected.id
                                onClicked: deleteDialog.open()
                            }

                            }
                        }

                    }

                }

                Discover {
                    controller: root.controller
                    ru: root.ru
                    onShowSettings: root.page=4
                }
                ScrollView {
                    id: monitorScroll

                    contentWidth: availableWidth
                    clip: true

                    ColumnLayout {
                        width: monitorScroll.availableWidth
                        spacing: 16

                        Label {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            color: "#b8c1ce"
                            text: root.t("У каждого монитора своя сцена. Выберите её в библиотеке и укажите нужный экран.", "Each monitor has its own scene. Select a scene in Library and choose its target display.")
                        }

                        Repeater {
                            model: root.controller.monitors

                            delegate: Frame {
                                id: monitorCard

                                required property var modelData

                                Layout.fillWidth: true

                                ColumnLayout {
                                    anchors.fill: parent

                                    Label {
                            textFormat: Text.PlainText
                                        text: monitorCard.modelData.alias + (monitorCard.modelData.primary ? root.t(" · Основной", " · Primary") : "")
                                        font.pixelSize: 20
                                        font.weight: Font.DemiBold
                                    }

                                    Label {
                            textFormat: Text.PlainText
                                        text: monitorCard.modelData.width + " × " + monitorCard.modelData.height + " · " + Number(monitorCard.modelData.refreshHz).toFixed(2) + " Hz · DPI " + monitorCard.modelData.dpi
                                        color: "#b8c1ce"
                                    }

                                    Label {
                            textFormat: Text.PlainText
                                        text: root.t("Координаты: ", "Position: ") + monitorCard.modelData.left + ", " + monitorCard.modelData.top
                                        color: "#b8c1ce"
                                    }

                                    Label {
                            textFormat: Text.PlainText
                                        Layout.fillWidth: true
                                        text: root.sceneTitle(monitorCard.modelData.sceneId)
                                        wrapMode: Text.Wrap
                                    }

                                    RowLayout {
                                        Label {
                            textFormat: Text.PlainText
                                            text: monitorCard.modelData.pauseReasons ? root.t("На паузе", "Paused") : root.t("Работает", "Active")
                                        }

                                        Button {
                                            text: monitorCard.modelData.manualPause ? root.t("Продолжить", "Resume") : root.t("Пауза", "Pause")
                                            enabled: root.controller.connected && !root.controller.busy
                                            onClicked: root.controller.pause(!monitorCard.modelData.manualPause, monitorCard.modelData.id)
                                        }

                                    }

                                }

                            }

                        }

                        Button {
                            text: root.t("Повторно обнаружить мониторы", "Reload displays")
                            enabled: root.controller.connected && !root.controller.busy
                            onClicked: root.controller.reloadDisplays()
                        }

                    }

                }

                ScrollView {
                    id: performanceScroll

                    contentWidth: availableWidth
                    clip: true

                    ColumnLayout {
                        width: performanceScroll.availableWidth
                        spacing: 20

                        Label {
                            textFormat: Text.PlainText
                            text: root.t("Частота кадров", "Frame rate")
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                        }

                        ComboBox {
                            Layout.preferredWidth: 320
                            Layout.maximumWidth: parent.width
                            model: ["Eco · 15 FPS", "Balanced · 30 FPS", "Smooth · 60 FPS"]
                            currentIndex: root.controller.settings.fps === 15 ? 0 : (root.controller.settings.fps === 60 ? 2 : 1)
                            enabled: root.controller.connected && !root.controller.busy
                            Accessible.name: root.t("Пресет кадров", "Frame-rate preset")
                            onActivated: root.changeSetting("fps", [15, 30, 60][currentIndex])
                        }

                        Label {
                            textFormat: Text.PlainText
                            text: root.t("Графический адаптер", "Graphics adapter")
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                        }

                        ComboBox {
                            Layout.preferredWidth: 320
                            Layout.maximumWidth: parent.width
                            model: [root.t("Автоматически", "Auto"), root.t("Энергосбережение", "Power saving"), root.t("Высокая производительность", "High performance")]
                            currentIndex: ["auto", "power-saving", "high-performance"].indexOf(root.controller.settings.gpu)
                            enabled: root.controller.connected && !root.controller.busy
                            Accessible.name: root.t("Выбор GPU", "GPU preference")
                            onActivated: root.changeSetting("gpu", ["auto", "power-saving", "high-performance"][currentIndex])
                        }

                        CheckBox {
                            text: root.t("Пауза при полноэкранном приложении", "Pause for fullscreen applications")
                            checked: !!root.controller.settings.pauseFullscreen
                            enabled: root.controller.connected && !root.controller.busy
                            onClicked: root.changeSetting("pauseFullscreen", checked)
                        }

                        CheckBox {
                            text: root.t("Пауза при развёрнутом окне", "Pause for maximized windows")
                            checked: !!root.controller.settings.pauseMaximized
                            enabled: root.controller.connected && !root.controller.busy
                            onClicked: root.changeSetting("pauseMaximized", checked)
                        }

                        CheckBox {
                            text: root.t("Пауза при питании от батареи", "Pause on battery")
                            checked: !!root.controller.settings.pauseBattery
                            enabled: root.controller.connected && !root.controller.busy
                            onClicked: root.changeSetting("pauseBattery", checked)
                        }

                        Label {
                            textFormat: Text.PlainText
                            text: root.t("Измерения движка", "Engine measurements")
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                        }

                        Label {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            color: "#b8c1ce"
                            text: root.t("Обновление раз в 5 секунд. Частота — успешные Present, время — отправка команд. Это не процент загрузки GPU.", "Updated every 5 seconds. Rate counts successful Presents; duration measures command submission. These are not GPU utilization percentages.")
                        }

                        Repeater {
                            model: root.controller.metrics

                            delegate: Label {
                            textFormat: Text.PlainText
                                required property var modelData

                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: modelData.alias + " · " + Number(modelData.presentRate).toFixed(1) + " Present/s · avg " + Number(modelData.submissionAvgMs).toFixed(2) + " ms · P95 " + Number(modelData.submissionP95Ms).toFixed(2) + " ms\nGPU draw: " + (modelData.gpuSamples > 0 ? Number(modelData.gpuDrawAvgMs).toFixed(2) + " ms" : root.t("нет измерения", "not measured"))
                            }

                        }

                        Label {
                            textFormat: Text.PlainText
                            visible: root.controller.metrics.length === 0
                            color: "#b8c1ce"
                            text: root.t("Измерений пока нет.", "No measurements yet.")
                        }

                    }

                }

                ScrollView {
                    id: settingsScroll

                    contentWidth: availableWidth
                    clip: true

                    ColumnLayout {
                        width: settingsScroll.availableWidth
                        spacing: 20

                        Label {
                            textFormat: Text.PlainText
                            text: root.t("Язык интерфейса", "Interface language")
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                        }

                        ComboBox {
                            Layout.preferredWidth: 320
                            Layout.maximumWidth: parent.width
                            model: ["Русский", "English"]
                            currentIndex: root.ru ? 0 : 1
                            enabled: root.controller.connected && !root.controller.busy
                            Accessible.name: root.t("Язык", "Language")
                            onActivated: root.changeSetting("language", currentIndex === 0 ? "ru" : "en")
                        }

                        CheckBox {
                            text: root.t("Запускать движок при входе в Windows", "Start engine when signing in to Windows")
                            checked: !!root.controller.settings.autostart
                            enabled: root.controller.connected && !root.controller.busy
                            onClicked: root.changeSetting("autostart", checked)
                        }

                        Label {
                            textFormat: Text.PlainText; text: root.t("Каталог", "Catalog"); font.pixelSize: 20; font.weight: Font.DemiBold }
                        TextField { id: catalogAddress; Layout.fillWidth: true; text: root.controller.settings.catalogBaseUrl || ""; placeholderText: "https://catalog.example"; Accessible.name: root.t("Адрес каталога", "Catalog address") }
                        Label {
                            textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#b8c1ce"; text: root.t("Укажите адрес своего сервера. Без сервера доступны встроенные примеры на странице каталога.", "Enter your server address. Bundled examples are available in Discover without a server.") }
                        CheckBox { id: advancedCatalog; text: root.t("Дополнительные параметры каталога", "Advanced catalog settings") }
                        ColumnLayout {
                            Layout.fillWidth: true; visible: advancedCatalog.checked
                            Label {
                            textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap; text: root.t("Разрешённые HTTPS-источники файлов, через запятую", "Allowed HTTPS asset origins, separated by commas") }
                            TextField { id: catalogOrigins; Layout.fillWidth: true; text: (root.controller.settings.catalogAssetOrigins || []).join(", "); placeholderText: "https://cdn.example"; Accessible.name: root.t("Источники файлов", "Asset origins") }
                            CheckBox { id: localCatalog; text: root.t("Разрешить HTTP для локального сервера", "Allow HTTP for a loopback server"); checked: !!root.controller.settings.allowLocalCatalog }
                        }
                        Button {
                            text: root.t("Сохранить адрес каталога", "Save catalog address"); enabled: root.controller.connected && !root.controller.busy
                            onClicked: {
                                let next=Object.assign({},root.controller.settings)
                                next.catalogBaseUrl=catalogAddress.text.trim()
                                next.catalogAssetOrigins=catalogOrigins.text.split(",").map(x=>x.trim()).filter(x=>x.length>0)
                                next.allowLocalCatalog=localCatalog.checked
                                root.controller.updateSettings(next)
                            }
                        }
                        Label {
                            textFormat: Text.PlainText
                            text: root.t("Данные и диагностика", "Data and diagnostics")
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                        }

                        Label {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.WrapAnywhere
                            color: "#b8c1ce"
                            text: root.controller.status.dataDirectory || root.t("Путь станет доступен после подключения к Core.", "Connect to Core to view the data path.")
                        }

                        Label {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            visible: !!root.controller.status.settingsWarning
                            text: root.controller.status.settingsWarning || ""
                        }

                        Button {
                            text: root.t("Открыть журнал", "Open logs")
                            enabled: !!root.controller.status.logDirectory
                            onClicked: root.controller.openLogs()
                        }

                        Button {
                            text: root.t("Сбросить настройки", "Reset settings")
                            enabled: root.controller.connected && !root.controller.busy
                            onClicked: root.controller.resetSettings()
                        }

                        Label {
                            textFormat: Text.PlainText
                            text: "AetherWall · " + Qt.application.version
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                        }

                        Label {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            color: "#b8c1ce"
                            text: root.t("Локальные сцены, каталог и управление движком. Закрытие Manager не останавливает обои; для завершения используйте кнопку ниже или меню в трее.", "Local scenes, catalog and engine control. Closing Manager keeps wallpapers running; use the button below or the tray menu to stop everything.")
                        }

                        Button {
                            text: root.t("Завершить полностью", "Exit completely")
                            enabled: root.controller.connected && !root.controller.busy
                            onClicked: root.controller.exitCompletely()
                        }

                    }

                }

            }

        }

    }

}
