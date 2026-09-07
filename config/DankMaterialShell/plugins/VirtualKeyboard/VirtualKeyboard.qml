import QtQuick
import Quickshell
import qs.Common
import qs.Widgets
import qs.Modules.Plugins

PluginComponent {
    id: root

    pillClickAction: () => {
        Quickshell.execDetached(["/home/andres/.local/bin/toggle-keyboard"]);
    }

    horizontalBarPill: Component {
        DankIcon {
            name: "keyboard"
            size: root.iconSize
            color: Theme.primary
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    verticalBarPill: Component {
        DankIcon {
            name: "keyboard"
            size: root.iconSize
            color: Theme.primary
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }
}
