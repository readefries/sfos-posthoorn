// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0

// Picks when snoozed mail comes back: a few usual moments, or any date and time.
Page {
    id: page

    property int count: 1

    signal chosen(var until)

    // The moments offered, worked out when the page opens.
    function at(daysAhead, hour) {
        var d = new Date()
        d.setDate(d.getDate() + daysAhead)
        d.setHours(hour, 0, 0, 0)
        return d
    }

    function choices() {
        var now = new Date()
        var list = []
        var later = new Date(now.getTime() + 3 * 3600 * 1000)
        list.push({ text: qsTr("Later today"), until: later })
        if (now.getHours() < 17)
            list.push({ text: qsTr("This evening"), until: at(0, 18) })
        list.push({ text: qsTr("Tomorrow morning"), until: at(1, 8) })
        // Saturday morning, unless it's the weekend already.
        var day = now.getDay()
        if (day >= 1 && day <= 4)
            list.push({ text: qsTr("This weekend"), until: at(6 - day, 9) })
        // Monday morning.
        list.push({ text: qsTr("Next week"), until: at(day === 0 ? 1 : 8 - day, 8) })
        return list
    }

    function pickDateAndTime() {
        var tomorrow = at(1, 8)
        var datePicker = pageStack.push("Sailfish.Silica.DatePickerDialog", { date: tomorrow })
        datePicker.accepted.connect(function() {
            var day = datePicker.date
            app.whenIdle(function() {
                var timePicker = pageStack.push("Sailfish.Silica.TimePickerDialog", { hour: 8, minute: 0 })
                timePicker.accepted.connect(function() {
                    var until = new Date(day.getFullYear(), day.getMonth(), day.getDate(),
                                         timePicker.hour, timePicker.minute)
                    page.chosen(until)
                })
            })
        })
    }

    SilicaListView {
        anchors.fill: parent
        model: page.choices()

        header: PageHeader {
            title: qsTr("Snooze until")
            description: qsTr("%n message(s)", "", page.count)
        }

        delegate: ListItem {
            contentHeight: Theme.itemSizeMedium
            onClicked: page.chosen(modelData.until)

            Column {
                anchors {
                    left: parent.left
                    right: parent.right
                    leftMargin: Theme.horizontalPageMargin
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                Label {
                    width: parent.width
                    text: modelData.text
                    color: highlighted ? Theme.highlightColor : Theme.primaryColor
                }
                Label {
                    width: parent.width
                    text: Format.formatDate(modelData.until, Formatter.DateMedium) + " "
                          + Format.formatDate(modelData.until, Formatter.TimeValue)
                    font.pixelSize: Theme.fontSizeSmall
                    color: highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                }
            }
        }

        footer: BackgroundItem {
            width: parent ? parent.width : 0
            height: Theme.itemSizeMedium
            onClicked: page.pickDateAndTime()

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Pick a date and time…")
                color: parent.highlighted ? Theme.highlightColor : Theme.primaryColor
            }
        }

        VerticalScrollDecorator { }
    }
}
