// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    CoverPlaceholder {
        text: "Posthoorn"
        icon.source: "/usr/share/icons/hicolor/86x86/apps/posthoorn.png"
    }

    CoverActionList {
        CoverAction {
            iconSource: "image://theme/icon-cover-sync"
            onTriggered: app.emailAgent.accountsSyncInbox()
        }
    }
}
