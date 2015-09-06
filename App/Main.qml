//=============================================================================|
// Copyright (C) 2015 Dan Watkins
// This file is licensed under the MIT License.
//=============================================================================|

import QtQuick 2.5
import QtQuick.Controls 1.4
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.1
import QtTestReviewGUI 1.0

ApplicationWindow {
    id: root
    title: qsTr("QtTestReviewGUI")
    width: 640
    height: 480
    visible: true

    menuBar: MenuBar {
        Menu {
            title: "File"

            MenuItem {
                text: "Open"
                onTriggered: {
                    fileDialog.open();
                }
            }
        }
    }

    TestResultsTreeView {
        id: testTableView
        anchors.fill: parent
        anchors.margins: 5

        model: TestResultsTreeViewModel {}
    }

    FileDialog {
        id: fileDialog
        modality: Qt.WindowModal
        title: "File to parse"
        selectMultiple: false
        nameFilters: [ "All files (*)" ]
        selectedNameFilter: "All files (*)"

        onAccepted: {  
            testTableView.model.parseFile(Qt.resolvedUrl(fileUrl));
            label_filePath.text = fileUrl
        }
    }

    statusBar: StatusBar {
        Row {
            spacing: 15

            Label {
                text: testTableView.model.statusText
            }

            Label {
                id: label_filePath
            }
        }
    }
}
