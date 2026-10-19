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
    property int thumbnailSize: 220
    property bool diagnosticsCopied: false
    onClosing: settingsWindow.close()
    onPageChanged: {
        if (page === 2)
            monitorsDialog.open();
        if (page === 3 || page === 4) {
            settingsWindow.section = page === 3 ? 0 : 1;
            settingsWindow.show();
        }
    }
    function copyReport() {
        controller.copyDiagnostics();
        diagnosticsCopied = true;
        copiedTimer.restart();
    }
    Timer {
        id: copiedTimer
        interval: 2500
        onTriggered: root.diagnosticsCopied = false
    }
    property var selected: ({})

    function t(russian, english) {
        return ru ? russian : english;
    }

    function engineStatusText() {
        if (!controller.connected) {
            if (controller.launchState === "starting")
                return t("Запускаем фон…", "Starting wallpaper…");
            if (controller.launchState === "checking")
                return t("Проверяем подключение…", "Checking connection…");
            return t("Не удалось запустить фон", "Wallpaper could not start");
        }
        if (controller.status.renderState === "failed")
            return t("Обои не запущены · библиотека доступна", "Wallpaper unavailable · library connected") + (controller.status.mode === "headless-test" ? " · TEST" : "");
        if (controller.status.mode === "headless-test")
            return t("Тестовый транспорт · без GPU", "Test transport · no GPU");
        if (controller.status.renderState === "recovering")
            return t("Восстановление обоев…", "Recovering wallpaper…");
        if (controller.status.renderState === "ready")
            return t("Фон работает", "Wallpaper running");
        return t("Подготовка обоев…", "Preparing wallpaper…");
    }

    function changeSetting(key, value) {
        let next = Object.assign({}, root.controller.settings);
        next[key] = value;
        root.controller.updateSettings(next);
    }

    function sceneTitle(id) {
        for (let i = 0; i < root.controller.scenes.length; ++i)
            if (root.controller.scenes[i].id === id) {
                return root.controller.scenes[i].title;
            }
        return t("Встроенный градиент", "Built-in gradient");
    }

    width: Math.min(1440, Screen.desktopAvailableWidth || 1440)
    height: Math.min(900, Screen.desktopAvailableHeight || 900)
    minimumWidth: Math.min(1000, Screen.desktopAvailableWidth || 1000)
    minimumHeight: Math.min(650, Screen.desktopAvailableHeight || 650)
    visible: true
    title: root.controller.status.mode === "headless-test" ? "AetherWall · HEADLESS TEST" : "AetherWall"
    color: "#1b2430"
    font.family: Qt.platform.os === "windows" ? "Segoe UI" : "sans-serif"
    font.pixelSize: 13
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
            if (!root.selected.id) {
                if (root.controller.scenes.length > 0)
                    root.selected = root.controller.scenes[0];
                return;
            }

            for (let i = 0; i < root.controller.scenes.length; ++i)
                if (root.controller.scenes[i].id === root.selected.id) {
                    root.selected = root.controller.scenes[i];
                    return;
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
        objectName: "deleteDialog"

        title: root.t("Удалить сцену из библиотеки?", "Delete scene from the library?")
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.NoButton
        background: Rectangle {
            color: "#222c39"
            radius: 3
            border.color: "#495361"
        }
        header: AwLabel {
            text: deleteDialog.title
            font.pixelSize: 18
            padding: 14
        }
        footer: Item {
            implicitHeight: 54
            RowLayout {
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8
                AwButton {
                    text: root.t("Отмена", "Cancel")
                    onClicked: deleteDialog.reject()
                }
                AwButton {
                    text: root.t("Удалить", "Delete")
                    highlighted: true
                    onClicked: deleteDialog.accept()
                }
            }
        }
        onAccepted: {
            root.controller.removeScene(root.selected.id);
            root.selected = ({});
        }

        AwLabel {
            textFormat: Text.PlainText
            width: 360
            wrapMode: Text.Wrap
            text: root.t("Будет удалена управляемая копия. Исходные файлы останутся на месте. Активную сцену сначала замените другой.", "The managed copy will be deleted. Original files remain. Apply another scene before deleting an active scene.")
        }
    }

    Dialog {
        id: monitorsDialog
        title: root.t("Мониторы", "Displays")
        anchors.centerIn: parent
        width: Math.min(root.width - 64, 760)
        height: Math.min(root.height - 80, 600)
        modal: true
        standardButtons: Dialog.NoButton
        background: Rectangle {
            color: "#222c39"
            radius: 3
            border.color: "#495361"
        }
        header: AwLabel {
            text: monitorsDialog.title
            font.pixelSize: 18
            padding: 14
        }
        footer: Item {
            implicitHeight: 54
            AwButton {
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                text: root.t("Закрыть", "Close")
                onClicked: monitorsDialog.close()
            }
        }
        contentItem: ScrollView {
            id: monitorScroll

            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: monitorScroll.availableWidth
                spacing: 16

                AwLabel {
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

                            AwLabel {
                                textFormat: Text.PlainText
                                text: monitorCard.modelData.alias + (monitorCard.modelData.primary ? root.t(" · Основной", " · Primary") : "")
                                font.pixelSize: 20
                                font.weight: Font.DemiBold
                            }

                            AwLabel {
                                textFormat: Text.PlainText
                                text: monitorCard.modelData.width + " × " + monitorCard.modelData.height + " · " + Number(monitorCard.modelData.refreshHz).toFixed(2) + " Hz · DPI " + monitorCard.modelData.dpi
                                color: "#b8c1ce"
                            }

                            AwLabel {
                                textFormat: Text.PlainText
                                text: root.t("Координаты: ", "Position: ") + monitorCard.modelData.left + ", " + monitorCard.modelData.top
                                color: "#b8c1ce"
                            }

                            AwLabel {
                                textFormat: Text.PlainText
                                Layout.fillWidth: true
                                text: root.sceneTitle(monitorCard.modelData.sceneId)
                                wrapMode: Text.Wrap
                            }

                            RowLayout {
                                AwLabel {
                                    textFormat: Text.PlainText
                                    text: !root.controller.connected ? root.t("Нет подключения", "Disconnected") : root.controller.status.renderState === "failed" ? root.t("Обои недоступны", "Wallpaper unavailable") : root.controller.status.mode === "headless-test" ? root.t("Тестовый монитор · без GPU", "Test display · no GPU") : root.controller.status.renderState === "recovering" ? root.t("Восстановление…", "Recovering…") : root.controller.status.renderState !== "ready" ? root.t("Подготовка…", "Preparing…") : monitorCard.modelData.pauseReasons ? root.t("На паузе", "Paused") : root.t("Работает", "Active")
                                }

                                AwButton {
                                    text: monitorCard.modelData.manualPause ? root.t("Продолжить", "Resume") : root.t("Пауза", "Pause")
                                    enabled: root.controller.connected && !root.controller.busy && root.controller.status.renderState === "ready"
                                    onClicked: root.controller.pause(!monitorCard.modelData.manualPause, monitorCard.modelData.id)
                                }
                            }
                        }
                    }
                }

                AwButton {
                    text: root.t("Повторно обнаружить мониторы", "Reload displays")
                    enabled: root.controller.connected && !root.controller.busy
                    onClicked: root.controller.reloadDisplays()
                }
            }
        }
    }
    ApplicationWindow {
        id: settingsWindow
        objectName: "settingsWindow"
        property int section: 1
        transientParent: root
        title: root.t("Настройки AetherWall", "AetherWall settings")
        width: 720
        height: 720
        minimumWidth: 580
        minimumHeight: 480
        color: "#1b2430"
        font.family: root.font.family
        font.pixelSize: 13
        palette: root.palette
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 18
            RowLayout {
                AwButton {
                    text: root.t("Производительность", "Performance")
                    checked: settingsWindow.section === 0
                    onClicked: settingsWindow.section = 0
                }
                AwButton {
                    text: root.t("Общие и диагностика", "General and diagnostics")
                    checked: settingsWindow.section === 1
                    onClicked: settingsWindow.section = 1
                }
                Item {
                    Layout.fillWidth: true
                }
            }
            StackLayout {
                currentIndex: settingsWindow.section
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 0
                ScrollView {
                    id: performanceScroll

                    contentWidth: availableWidth
                    clip: true

                    ColumnLayout {
                        width: performanceScroll.availableWidth
                        spacing: 20

                        AwLabel {
                            textFormat: Text.PlainText
                            text: root.t("Частота кадров", "Frame rate")
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                        }

                        AwComboBox {
                            Layout.preferredWidth: 320
                            Layout.maximumWidth: parent.width
                            model: ["Eco · 15 FPS", "Balanced · 30 FPS", "Smooth · 60 FPS"]
                            currentIndex: root.controller.settings.fps === 15 ? 0 : (root.controller.settings.fps === 60 ? 2 : 1)
                            enabled: root.controller.connected && !root.controller.busy
                            Accessible.name: root.t("Пресет кадров", "Frame-rate preset")
                            onActivated: root.changeSetting("fps", [15, 30, 60][currentIndex])
                        }

                        AwLabel {
                            textFormat: Text.PlainText
                            text: root.t("Графический адаптер", "Graphics adapter")
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                        }

                        AwComboBox {
                            Layout.preferredWidth: 320
                            Layout.maximumWidth: parent.width
                            model: [root.t("Автоматически", "Auto"), root.t("Энергосбережение", "Power saving"), root.t("Высокая производительность", "High performance")]
                            currentIndex: ["auto", "power-saving", "high-performance"].indexOf(root.controller.settings.gpu)
                            enabled: root.controller.connected && !root.controller.busy
                            Accessible.name: root.t("Выбор GPU", "GPU preference")
                            onActivated: root.changeSetting("gpu", ["auto", "power-saving", "high-performance"][currentIndex])
                        }

                        AwCheckBox {
                            text: root.t("Пауза при полноэкранном приложении", "Pause for fullscreen applications")
                            checked: !!root.controller.settings.pauseFullscreen
                            enabled: root.controller.connected && !root.controller.busy
                            onClicked: root.changeSetting("pauseFullscreen", checked)
                        }

                        AwCheckBox {
                            text: root.t("Пауза при развёрнутом окне", "Pause for maximized windows")
                            checked: !!root.controller.settings.pauseMaximized
                            enabled: root.controller.connected && !root.controller.busy
                            onClicked: root.changeSetting("pauseMaximized", checked)
                        }

                        AwCheckBox {
                            text: root.t("Пауза при питании от батареи", "Pause on battery")
                            checked: !!root.controller.settings.pauseBattery
                            enabled: root.controller.connected && !root.controller.busy
                            onClicked: root.changeSetting("pauseBattery", checked)
                        }

                        AwLabel {
                            textFormat: Text.PlainText
                            text: root.t("Измерения движка", "Engine measurements")
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                        }

                        AwLabel {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            color: "#b8c1ce"
                            text: root.t("Обновление раз в 5 секунд. Частота — успешные Present, время — отправка команд. Это не процент загрузки GPU.", "Updated every 5 seconds. Rate counts successful Presents; duration measures command submission. These are not GPU utilization percentages.")
                        }

                        Repeater {
                            model: root.controller.metrics

                            delegate: AwLabel {
                                textFormat: Text.PlainText
                                required property var modelData

                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: modelData.alias + " · " + Number(modelData.presentRate).toFixed(1) + " Present/s · avg " + Number(modelData.submissionAvgMs).toFixed(2) + " ms · P95 " + Number(modelData.submissionP95Ms).toFixed(2) + " ms\nGPU draw: " + (modelData.gpuSamples > 0 ? Number(modelData.gpuDrawAvgMs).toFixed(2) + " ms" : root.t("нет измерения", "not measured"))
                            }
                        }

                        AwLabel {
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

                        AwLabel {
                            textFormat: Text.PlainText
                            text: root.t("Язык интерфейса", "Interface language")
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                        }

                        AwComboBox {
                            Layout.preferredWidth: 320
                            Layout.maximumWidth: parent.width
                            model: ["Русский", "English"]
                            currentIndex: root.ru ? 0 : 1
                            enabled: root.controller.connected && !root.controller.busy
                            Accessible.name: root.t("Язык", "Language")
                            onActivated: root.changeSetting("language", currentIndex === 0 ? "ru" : "en")
                        }

                        AwCheckBox {
                            text: root.t("Запускать движок при входе в Windows", "Start engine when signing in to Windows")
                            checked: !!root.controller.settings.autostart
                            enabled: root.controller.connected && !root.controller.busy
                            onClicked: root.changeSetting("autostart", checked)
                        }

                        AwLabel {
                            textFormat: Text.PlainText
                            text: root.t("Каталог", "Catalog")
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                        }
                        AwTextField {
                            id: catalogAddress
                            Layout.fillWidth: true
                            text: root.controller.settings.catalogBaseUrl || ""
                            placeholderText: "https://catalog.example"
                            Accessible.name: root.t("Адрес каталога", "Catalog address")
                        }
                        AwLabel {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            color: "#b8c1ce"
                            text: root.t("Укажите адрес своего сервера. Без сервера доступны встроенные примеры на странице каталога.", "Enter your server address. Bundled examples are available in Discover without a server.")
                        }
                        AwCheckBox {
                            id: advancedCatalog
                            text: root.t("Дополнительные параметры каталога", "Advanced catalog settings")
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: advancedCatalog.checked
                            AwLabel {
                                textFormat: Text.PlainText
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: root.t("Разрешённые HTTPS-источники файлов, через запятую", "Allowed HTTPS asset origins, separated by commas")
                            }
                            AwTextField {
                                id: catalogOrigins
                                Layout.fillWidth: true
                                text: (root.controller.settings.catalogAssetOrigins || []).join(", ")
                                placeholderText: "https://cdn.example"
                                Accessible.name: root.t("Источники файлов", "Asset origins")
                            }
                            AwCheckBox {
                                id: localCatalog
                                text: root.t("Разрешить HTTP для локального сервера", "Allow HTTP for a loopback server")
                                checked: !!root.controller.settings.allowLocalCatalog
                            }
                        }
                        AwButton {
                            text: root.t("Сохранить адрес каталога", "Save catalog address")
                            enabled: root.controller.connected && !root.controller.busy
                            onClicked: {
                                let next = Object.assign({}, root.controller.settings);
                                next.catalogBaseUrl = catalogAddress.text.trim();
                                next.catalogAssetOrigins = catalogOrigins.text.split(",").map(x => x.trim()).filter(x => x.length > 0);
                                next.allowLocalCatalog = localCatalog.checked;
                                root.controller.updateSettings(next);
                            }
                        }
                        AwLabel {
                            textFormat: Text.PlainText
                            text: root.t("Данные и диагностика", "Data and diagnostics")
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                        }

                        AwLabel {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.WrapAnywhere
                            color: "#b8c1ce"
                            text: root.controller.status.dataDirectory || root.t("Путь станет доступен после подключения к Core.", "Connect to Core to view the data path.")
                        }

                        AwLabel {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            visible: !!root.controller.status.settingsWarning
                            text: root.controller.status.settingsWarning || ""
                        }

                        AwButton {
                            text: root.t("Открыть журнал", "Open logs")
                            enabled: true
                            onClicked: root.controller.openLogs()
                        }

                        AwButton {
                            text: root.diagnosticsCopied ? root.t("Скопировано", "Copied") : root.t("Скопировать диагностику", "Copy diagnostics")
                            onClicked: root.copyReport()
                        }
                        AwButton {
                            text: root.t("Сбросить настройки", "Reset settings")
                            enabled: root.controller.connected && !root.controller.busy
                            onClicked: root.controller.resetSettings()
                        }

                        AwLabel {
                            textFormat: Text.PlainText
                            text: "AetherWall · " + Qt.application.version
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                        }

                        AwLabel {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            color: "#b8c1ce"
                            text: root.t("Локальные сцены, каталог и управление движком. Закрытие Manager не останавливает обои; для завершения используйте кнопку ниже или меню в трее.", "Local scenes, catalog and engine control. Closing Manager keeps wallpapers running; use the button below or the tray menu to stop everything.")
                        }

                        AwButton {
                            text: root.t("Завершить полностью", "Exit completely")
                            enabled: root.controller.connected && !root.controller.busy
                            onClicked: root.controller.exitCompletely()
                        }
                    }
                }
            }
        }
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 62
            color: "#273342"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                spacing: 10
                Image {
                    source: "qrc:/qt/qml/AetherWall/Manager/assets/aetherwall.png"
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                }
                AwLabel {
                    text: "AetherWall"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    Layout.rightMargin: 24
                }
                AwButton {
                    text: root.t("Установленные", "Installed")
                    checked: root.page !== 1
                    onClicked: root.page = 0
                    implicitHeight: 38
                }
                AwButton {
                    text: root.t("Каталог", "Discover")
                    checked: root.page === 1
                    onClicked: root.page = 1
                    implicitHeight: 38
                }
                Item {
                    Layout.fillWidth: true
                }
                AwButton {
                    text: root.t("Мониторы", "Displays")
                    onClicked: monitorsDialog.open()
                }
                AwButton {
                    text: root.t("Настройки", "Settings")
                    onClicked: settingsWindow.show()
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: connectionRow.implicitHeight + 18
            color: "#1d2733"
            RowLayout {
                id: connectionRow
                anchors.fill: parent
                anchors.margins: 9
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                spacing: 10
                Rectangle {
                    implicitWidth: 8
                    implicitHeight: 8
                    radius: 4
                    color: root.controller.connected && root.controller.status.renderState !== "failed" ? "#9cceab" : "#e9b676"
                }
                AwLabel {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: root.engineStatusText()
                }
                AwButton {
                    text: root.t("Пауза", "Pause")
                    enabled: root.controller.connected && !root.controller.busy
                    onClicked: root.controller.pause(true)
                }
                AwButton {
                    text: root.t("Продолжить", "Resume")
                    enabled: root.controller.connected && !root.controller.busy
                    onClicked: root.controller.pause(false)
                }
                AwButton {
                    visible: !root.controller.connected || root.controller.status.renderState === "failed"
                    text: root.t("Повторить запуск", "Retry startup")
                    enabled: root.controller.launchState !== "starting" && root.controller.launchState !== "checking"
                    onClicked: root.controller.connected ? root.controller.reloadDisplays() : root.controller.startCore()
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            visible: !!root.controller.error || !!root.controller.launchError || !!root.controller.status.renderError
            implicitHeight: errorRow.implicitHeight + 24
            color: "#3b3028"
            RowLayout {
                id: errorRow
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12
                AwLabel {
                    Layout.fillWidth: true
                    wrapMode: Text.WrapAnywhere
                    text: root.controller.error || root.controller.launchError || root.controller.status.renderError || ""
                    Accessible.role: Accessible.AlertMessage
                }
                AwButton {
                    text: root.t("Журнал", "Logs")
                    onClicked: root.controller.openLogs()
                }
                AwButton {
                    text: root.diagnosticsCopied ? root.t("Скопировано", "Copied") : root.t("Диагностика", "Copy diagnostics")
                    onClicked: root.copyReport()
                }
                AwButton {
                    visible: !!root.controller.error
                    text: root.t("Закрыть", "Dismiss")
                    onClicked: root.controller.dismissError()
                }
            }
        }
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            Layout.minimumHeight: 0
            Layout.margins: 14
            currentIndex: root.page === 1 ? 1 : 0
            ColumnLayout {
                Layout.minimumHeight: 0
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    AwTextField {
                        id: search

                        Layout.fillWidth: true
                        placeholderText: root.t("Поиск по названию или автору", "Search title or author")
                        Accessible.name: placeholderText
                    }

                    AwCheckBox {
                        id: favorites

                        text: root.t("Избранное", "Favorites")
                    }

                    AwComboBox {
                        id: sort

                        model: [root.t("По названию", "By title"), root.t("Избранное сначала", "Favorites first")]
                        Accessible.name: root.t("Сортировка", "Sort order")
                    }

                    AwComboBox {
                        id: previewSize
                        Layout.preferredWidth: 145
                        model: [root.t("Крупные", "Large"), root.t("Средние", "Medium"), root.t("Мелкие", "Small")]
                        currentIndex: 1
                        onModelChanged: Qt.callLater(function () {
                            currentIndex = [280, 220, 170].indexOf(root.thumbnailSize);
                        })
                        Accessible.name: root.t("Размер превью", "Preview size")
                        onActivated: root.thumbnailSize = [280, 220, 170][currentIndex]
                    }
                    AwButton {
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

                SplitView {
                    objectName: "libraryBody"
                    Layout.minimumHeight: 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    orientation: Qt.Horizontal
                    handle: Rectangle {
                        implicitWidth: 8
                        color: SplitHandle.hovered ? "#528eb9" : "#202936"
                    }

                    Item {
                        SplitView.fillWidth: true
                        SplitView.minimumWidth: 380

                        GridView {
                            id: sceneGrid

                            objectName: "sceneGrid"
                            anchors.fill: parent
                            clip: true
                            cellWidth: width / Math.max(1, Math.floor(width / root.thumbnailSize))
                            cellHeight: cellWidth * 0.5625 + 64
                            model: root.controller.filterScenes(root.controller.scenes, search.text, favorites.checked, sort.currentIndex)

                            ScrollBar.vertical: ScrollBar {}

                            delegate: AwButton {
                                id: sceneButton

                                required property var modelData

                                width: sceneGrid.cellWidth - 10
                                height: sceneGrid.cellHeight - 10
                                padding: 0
                                leftPadding: 0
                                rightPadding: 0
                                topPadding: 0
                                bottomPadding: 0
                                Accessible.name: modelData.title + ", " + modelData.author
                                Accessible.selectable: true
                                Accessible.selected: root.selected.id === modelData.id
                                onClicked: root.selected = modelData

                                background: Rectangle {
                                    color: sceneButton.hovered ? "#2b323d" : "#1e232b"
                                    radius: 3
                                    border.width: sceneButton.activeFocus || root.selected.id === sceneButton.modelData.id ? 2 : 0
                                    border.color: sceneButton.activeFocus ? "#d8edff" : "#85b7ff"
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

                                    AwLabel {
                                        textFormat: Text.PlainText
                                        Layout.fillWidth: true
                                        Layout.leftMargin: 12
                                        Layout.rightMargin: 12
                                        text: sceneButton.modelData.title
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }

                                    AwLabel {
                                        textFormat: Text.PlainText
                                        Layout.fillWidth: true
                                        Layout.leftMargin: 12
                                        Layout.rightMargin: 12
                                        Layout.bottomMargin: 10
                                        text: sceneButton.modelData.author + (root.controller.monitors.some(m => m.sceneId === sceneButton.modelData.id) ? root.t(" · Активные обои", " · Active wallpaper") : "")
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

                            AwLabel {
                                textFormat: Text.PlainText
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                horizontalAlignment: Text.AlignHCenter
                                font.pixelSize: 20
                                text: root.controller.connected ? root.t("Сцены не найдены", "No scenes found") : root.t("Подключите движок", "Connect the engine")
                            }

                            AwLabel {
                                textFormat: Text.PlainText
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                horizontalAlignment: Text.AlignHCenter
                                color: "#b8c1ce"
                                text: root.controller.connected ? root.t("Измените поиск или импортируйте локальную сцену.", "Change your search or import a local scene.") : root.t("Запустите Core, чтобы открыть библиотеку и управлять обоями.", "Start Core to browse the library and control wallpapers.")
                            }
                        }
                    }

                    Rectangle {
                        SplitView.preferredWidth: 340
                        SplitView.minimumWidth: 280
                        SplitView.maximumWidth: 480
                        color: "#222c39"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 12
                            ScrollView {
                                id: detailScroll
                                objectName: "sceneDetails"
                                Layout.minimumHeight: 0
                                Layout.fillHeight: true
                                Layout.fillWidth: true
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

                                    AwLabel {
                                        textFormat: Text.PlainText
                                        Layout.fillWidth: true
                                        wrapMode: Text.Wrap
                                        font.pixelSize: 21
                                        font.weight: Font.DemiBold
                                        text: root.selected.title || root.t("Выберите сцену", "Choose a scene")
                                    }

                                    AwLabel {
                                        textFormat: Text.PlainText
                                        Layout.fillWidth: true
                                        wrapMode: Text.Wrap
                                        color: "#b8c1ce"
                                        text: root.selected.id ? root.selected.author + "\n" + root.selected.license : root.t("Превью статичны: анимацию запускает только движок.", "Previews are static. Only the engine runs animation.")
                                    }

                                    AwLabel {
                                        textFormat: Text.PlainText
                                        text: root.t("Применить на", "Apply to")
                                        visible: !!root.selected.id
                                    }

                                    AwComboBox {
                                        id: targetMonitor

                                        Layout.fillWidth: true
                                        model: [
                                            {
                                                "id": "*",
                                                "alias": root.t("Все мониторы", "All monitors")
                                            }
                                        ].concat(root.controller.monitors)
                                        textRole: "alias"
                                        valueRole: "id"
                                        Accessible.name: root.t("Целевой монитор", "Target monitor")
                                        enabled: root.controller.connected && !root.controller.busy
                                    }

                                    AwButton {
                                        Layout.fillWidth: true
                                        text: root.selected.favorite ? root.t("Убрать из избранного", "Remove favorite") : root.t("В избранное", "Add to favorites")
                                        enabled: root.controller.connected && !root.controller.busy && !!root.selected.id
                                        onClicked: root.controller.favorite(root.selected.id, !root.selected.favorite)
                                    }

                                    AwButton {
                                        Layout.fillWidth: true
                                        text: root.t("Открыть папку", "Open folder")
                                        enabled: !!root.selected.folderUrl
                                        onClicked: root.controller.openFolder(root.selected.folderUrl)
                                    }

                                    AwButton {
                                        Layout.fillWidth: true
                                        text: root.t("Удалить из библиотеки…", "Delete from library…")
                                        enabled: root.controller.connected && !root.controller.busy && !!root.selected.id
                                        onClicked: deleteDialog.open()
                                    }
                                }
                            }
                            AwButton {
                                Layout.fillWidth: true
                                highlighted: true
                                text: root.t("Применить сцену", "Apply scene")
                                enabled: root.controller.connected && !root.controller.busy && root.controller.status.renderState !== "failed" && !!root.selected.id && root.controller.monitors.length > 0
                                onClicked: root.controller.apply(targetMonitor.currentValue || "*", root.selected.id)
                            }
                        }
                    }
                }
            }
            Discover {
                controller: root.controller
                ru: root.ru
                onShowSettings: {
                    settingsWindow.section = 1;
                    settingsWindow.show();
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 34
            color: "#1d2733"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                AwLabel {
                    text: root.controller.scenes.length + root.t(" обоев в библиотеке", " wallpapers installed")
                    color: "#b8c1ce"
                    font.pixelSize: 12
                }
                Item {
                    Layout.fillWidth: true
                }
                AwLabel {
                    text: root.t("Закрытие окна оставляет фон работать", "Closing this window keeps wallpaper running")
                    color: "#b8c1ce"
                    font.pixelSize: 12
                }
            }
        }
    }
}
