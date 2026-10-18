pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: discover
    objectName: "discoverPage"
    required property ManagerController controller
    required property bool ru
    property var selected: ({})
    function failureText() {
        if (!ru) return controller.catalog.error
        const messages = {
            integrity: "Размер или SHA-256 пакета не совпадает с каталогом. Ничего не установлено; повторите загрузку или сообщите автору.",
            package: "Пакет не прошёл проверку сцены. Ничего не установлено; запросите исправленный пакет у автора.",
            storage: "Не удалось записать файл. Проверьте свободное место и доступ к папке кэша, затем повторите.",
            configuration: "Адрес отклонён. Проверьте HTTPS-адрес каталога и разрешённые адреса файлов в настройках.",
            response: "Ответ каталога не прошёл проверку. Повторите запрос или проверьте сервер.",
            timeout: "Время ожидания истекло. Проверьте подключение и повторите загрузку.",
            install: "Пакет проверен, но установка не завершена. Проверьте подключение к движку и повторите установку.",
            download: "Загрузка не завершена. Проверьте подключение и повторите загрузку.",
            catalog: "Каталог недоступен. Проверьте адрес и подключение, затем повторите запрос."
        }
        return messages[controller.catalog.errorCode] || ""
    }
    signal showSettings()
    function t(russian, english) { return ru ? russian : english }
    function installed(id) {
        for(let i=0;i<controller.scenes.length;++i) if(controller.scenes[i].id===id) return true
        return false
    }
    function browse(page) { controller.catalog.browse(search.text,category.currentValue || "",["new","popular","trending"][sort.currentIndex],page) }
    function phaseLabel() {
        const phases={downloading:t("Скачивание", "Downloading"),validating:t("Проверка пакета", "Validating package"),installing:t("Установка в библиотеку", "Installing into library"),
            ready:t("Пакет проверен. Можно установить в библиотеку.", "Package verified. Ready to install into library."),installed:t("Установлено. Сцену можно применить в библиотеке.", "Installed. Apply the scene from Library."),
            cancelling:t("Отмена проверки…", "Cancelling validation…"),cancelled:t("Загрузка отменена", "Download cancelled"),error:t("Загрузка не завершена", "Download not completed")}
        return phases[controller.catalog.phase] || ""
    }
    Connections {
        target: discover.controller.catalog
        function onChanged() {
            if(!discover.selected.id) return
            for(let i=0;i<discover.controller.catalog.items.length;++i)
                if(discover.controller.catalog.items[i].id===discover.selected.id) { discover.selected=discover.controller.catalog.items[i]; return }
        }
    }
    Timer { id: debounce; interval: 350; onTriggered: discover.browse(1) }
    ColumnLayout {
        anchors.fill: parent; spacing: 16
        RowLayout {
            Layout.fillWidth: true; spacing: 12
            TextField { id: search; Layout.fillWidth: true; placeholderText: discover.t("Найти сцену", "Find a scene"); Accessible.name: placeholderText; onTextEdited: debounce.restart() }
            ComboBox { id: category; Layout.preferredWidth: 138; model: [{id:""}].concat(discover.controller.catalog.categories); textRole: "id"; valueRole: "id"; displayText: currentIndex===0 ? discover.t("Все категории", "All categories") : currentText; Accessible.name: discover.t("Категория", "Category"); onActivated: discover.browse(1) }
            ComboBox { id: sort; Layout.preferredWidth: 136; model: [discover.t("Новые", "New"),discover.t("Популярные", "Popular"),discover.t("В тренде", "Trending")]; Accessible.name: discover.t("Порядок выдачи", "Sort order"); onActivated: discover.browse(1) }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { Layout.fillWidth: true; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#b8c1ce"; text: discover.controller.catalog.demo ? discover.t("Встроенные оригинальные примеры · без внешнего сервера", "Bundled original examples · no external server") : discover.t("Настройте адрес своего каталога. Установленные обои работают без сети.", "Configure your catalog address. Installed wallpapers work offline.") }
            Button { text: discover.t("Примеры", "Examples"); onClicked: discover.controller.catalog.useDemo() }
            Button { visible: discover.controller.catalog.demo; text: discover.t("Сервер", "Server"); onClicked: discover.controller.catalog.useConfigured() }
            Button { text: discover.t("Адрес каталога", "Catalog address"); onClicked: discover.showSettings() }
            BusyIndicator { Layout.preferredWidth: 26; Layout.preferredHeight: 26; running: discover.controller.catalog.loading; visible: running; Accessible.name: discover.t("Загрузка каталога", "Loading catalog") }
        }
        Label { Layout.fillWidth: true; textFormat: Text.PlainText; wrapMode: Text.Wrap; visible: discover.controller.catalog.error.length>0; text: discover.failureText(); Accessible.role: Accessible.AlertMessage }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 0; spacing: 20
            Item {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumWidth: 0
                GridView {
                    id: grid
                    anchors.fill: parent; clip: true
                    cellWidth: width/Math.max(1,Math.floor(width/235)); cellHeight: cellWidth*0.5625+102
                    model: discover.controller.catalog.items
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Button {
                        id: card
                        required property var modelData
                        width: grid.cellWidth-16; height: grid.cellHeight-16; padding: 0
                        checkable: true
                        checked: discover.selected.id===modelData.id
                        Accessible.name: modelData.title+", "+modelData.author+", "+modelData.license+(discover.installed(modelData.id) ? discover.t(", установлено", ", installed") : "")
                        onClicked: discover.selected=modelData
                        background: Rectangle { radius: 12; color: card.hovered ? "#2b323d" : "#1e232b"; border.width: card.activeFocus || discover.selected.id===card.modelData.id ? 2 : 0; border.color: "#85b7ff" }
                        contentItem: ColumnLayout {
                            spacing: 5
                            Item {
                                Layout.fillWidth: true; Layout.preferredHeight: card.width*0.5625
                                Image { id: thumbnail; anchors.fill: parent; source: card.modelData.previewLocal; fillMode: Image.PreserveAspectCrop; sourceSize.width: 480; sourceSize.height: 270; asynchronous: true }
                                Label { anchors.centerIn: parent; textFormat: Text.PlainText; visible: thumbnail.status!==Image.Ready; color: "#b8c1ce"; text: discover.t("Превью недоступно", "Preview unavailable") }
                            }
                            Label { Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12; textFormat: Text.PlainText; text: card.modelData.title; elide: Text.ElideRight; font.weight: Font.DemiBold }
                            Label { Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12; textFormat: Text.PlainText; text: card.modelData.author; elide: Text.ElideRight; font.pixelSize: 12; color: "#b8c1ce" }
                            Label { Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12; Layout.bottomMargin: 10; textFormat: Text.PlainText; text: card.modelData.license+(discover.installed(card.modelData.id) ? discover.t(" · Установлено", " · Installed") : ""); font.pixelSize: 12; color: "#b8c1ce" }
                        }
                    }
                }
                ColumnLayout {
                    anchors.centerIn: parent; width: Math.min(parent.width-24,360); spacing: 12
                    visible: grid.count===0 && !discover.controller.catalog.loading
                    Label { Layout.fillWidth: true; textFormat: Text.PlainText; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; font.pixelSize: 20; text: discover.t("Сцен пока нет", "No scenes to show") }
                    Label { Layout.fillWidth: true; textFormat: Text.PlainText; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; color: "#b8c1ce"; text: discover.t("Откройте встроенные примеры, настройте адрес или измените поиск.", "Open the bundled examples, configure an address, or change the search.") }
                    Button { Layout.alignment: Qt.AlignHCenter; text: discover.t("Повторить запрос", "Retry request"); onClicked: discover.browse(1) }
                }
            }
            ColumnLayout {
                Layout.preferredWidth: 264; Layout.minimumWidth: 230; Layout.maximumWidth: 280; Layout.fillHeight: true; Layout.minimumHeight: 0
                spacing: 12
                ScrollView {
                id: detail
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 0
                contentWidth: availableWidth; clip: true
                ScrollBar.vertical.policy: ScrollBar.AlwaysOn
                ColumnLayout {
                    width: detail.availableWidth; spacing: 12
                    Image { Layout.fillWidth: true; Layout.preferredHeight: 150; visible: !!discover.selected.id; source: discover.selected.previewLocal || ""; fillMode: Image.PreserveAspectCrop; sourceSize.width: 528; sourceSize.height: 300 }
                    Label { Layout.fillWidth: true; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: 21; font.weight: Font.DemiBold; text: discover.selected.title || discover.t("Выберите сцену", "Choose a scene") }
                    Label { Layout.fillWidth: true; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#b8c1ce"; text: discover.selected.id ? discover.selected.author+"\n"+discover.selected.license : discover.t("Загрузка не заменяет активные обои. После установки примените сцену в библиотеке.", "Downloading keeps your current wallpaper. Apply the scene from Library after installation.") }
                    Label { Layout.fillWidth: true; textFormat: Text.PlainText; wrapMode: Text.Wrap; text: discover.selected.description || "" }
                    Label { Layout.fillWidth: true; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#b8c1ce"; text: discover.selected.id ? discover.t("Пакет: ", "Package: ")+(Number(discover.selected.packageSize)/1024).toFixed(1)+" KiB · v"+discover.selected.version : "" }
                    Button { Layout.fillWidth: true; visible: !!discover.selected.id && !discover.controller.catalog.demo; text: discover.t("Лицензия и источник", "License and source"); onClicked: discover.controller.catalog.openLicense(discover.selected.id) }
                    Label { Layout.fillWidth: true; visible: !!discover.selected.id && discover.controller.catalog.demo; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#b8c1ce"; text: discover.t("Оригинальные работы AetherWall. Исходники и условия CC0-1.0 включены в репозиторий.", "Original AetherWall works. Source and CC0-1.0 terms are included in the repository.") }
                }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 8
                    Button {
                        Layout.fillWidth: true; highlighted: true
                        text: discover.installed(discover.selected.id) ? discover.t("Установлено", "Installed") : discover.t("Скачать сцену", "Download scene")
                        enabled: !!discover.selected.id && !discover.installed(discover.selected.id) && ["downloading","validating","installing","cancelling"].indexOf(discover.controller.catalog.phase)<0
                        onClicked: discover.controller.catalog.download(discover.selected.id)
                    }
                    Label { Layout.fillWidth: true; textFormat: Text.PlainText; wrapMode: Text.Wrap; text: discover.phaseLabel() }
                    ProgressBar { Layout.fillWidth: true; visible: ["downloading","validating","installing","cancelling"].indexOf(discover.controller.catalog.phase)>=0; indeterminate: discover.controller.catalog.phase!=="downloading"; from: 0; to: Math.max(1,discover.controller.catalog.expected); value: discover.controller.catalog.received; Accessible.name: discover.t("Прогресс загрузки", "Download progress") }
                    Label { visible: discover.controller.catalog.phase==="downloading"; textFormat: Text.PlainText; text: (discover.controller.catalog.received/1048576).toFixed(2)+" / "+(discover.controller.catalog.expected/1048576).toFixed(2)+" MiB" }
                    Button { Layout.fillWidth: true; visible: ["downloading","validating"].indexOf(discover.controller.catalog.phase)>=0; text: discover.t("Отменить", "Cancel"); onClicked: discover.controller.catalog.cancel() }
                    Button { Layout.fillWidth: true; visible: ["error","cancelled"].indexOf(discover.controller.catalog.phase)>=0; text: discover.t("Повторить загрузку", "Retry download"); onClicked: discover.controller.catalog.retry() }
                    Button { Layout.fillWidth: true; visible: discover.controller.catalog.phase==="ready"; enabled: discover.controller.connected && !discover.controller.busy; text: discover.t("Установить в библиотеку", "Install into library"); onClicked: discover.controller.installCatalogPackage() }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Button { text: discover.t("Назад", "Previous"); enabled: discover.controller.catalog.page>1 && !discover.controller.catalog.loading; onClicked: discover.browse(discover.controller.catalog.page-1) }
            Label { Layout.fillWidth: true; textFormat: Text.PlainText; horizontalAlignment: Text.AlignHCenter; color: "#b8c1ce"; text: discover.t("Страница ", "Page ")+discover.controller.catalog.page+" / "+discover.controller.catalog.pages+discover.t(" · найдено: ", " · results: ")+discover.controller.catalog.total }
            Button { text: discover.t("Далее", "Next"); enabled: discover.controller.catalog.page<discover.controller.catalog.pages && !discover.controller.catalog.loading; onClicked: discover.browse(discover.controller.catalog.page+1) }
        }
    }
}
