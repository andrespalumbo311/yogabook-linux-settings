import QtQuick
import Quickshell
import qs.Common
import qs.Widgets
import qs.Modules.Plugins

PluginComponent {
    id: root

    pillClickAction: () => {
        Quickshell.execDetached(["sh", "-c", "MANGO_INSTANCE_SIGNATURE=${MANGO_INSTANCE_SIGNATURE:-$(ls -t /run/user/$(id -u)/mango-*.sock 2>/dev/null | head -n1)} mmsg dispatch killclient"]);
    }

    horizontalBarPill: Component {
        DankIcon {
            name: "close"
            size: root.iconSize
            color: Theme.error
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    verticalBarPill: Component {
        DankIcon {
            name: "close"
            size: root.iconSize
            color: Theme.error
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }
}
