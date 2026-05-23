import Quickshell
import Quickshell.Wayland
import QtQuick
import QtQuick.Layouts
import CavaMonitor 1.0

Scope {
    id: root
    Colors { id: colors }
    
    property int barCount: 30
    property int maxBarWidth: 400
    property int barHeight: 15
    property int barGap: 6
    
    // CavaMonitor plugin replaces the Process-based approach
    CavaMonitor {
        id: cava
        bars: root.barCount
        active: true
    }
    
    Variants {
        model: Quickshell.screens
        
        PanelWindow {
            id: window
            required property var modelData
            screen: modelData
            color: "transparent"
            
            WlrLayershell.layer: WlrLayer.Bottom
            WlrLayershell.exclusionMode: ExclusionMode.Ignore
            
            anchors {
                right: true
                top: true
                bottom: true
            }
            
            implicitWidth: root.maxBarWidth
            
            Rectangle {
                anchors.fill: parent
                color: "transparent"
                
                Column {
                    anchors {
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                    }
                    spacing: root.barGap
                    
                    Repeater {
                        model: root.barCount
                        
                        Rectangle {
                            id: barItem
                            required property int index
                            
                            readonly property real magnitude: cava.values[index] || 0
                            
                            height: root.barHeight
                            width: 6 + (magnitude * root.maxBarWidth)
                            radius: root.barHeight / 2
                            
                            anchors.right: parent.right
                            
                            color: colors.color4
                            
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: "#ffffff" }
                                GradientStop { position: 0.3; color: colors.color4 }
                                GradientStop { position: 1.0; color: colors.color5 }
                            }
                            
                            border.color: Qt.rgba(1, 1, 1, 0.2)
                            border.width: 1
                            opacity: 0.4 + magnitude * 0.6
                            
                            Behavior on width {
                                NumberAnimation {
                                    duration: 50
                                    easing.type: Easing.OutCubic
                                }
                            }
                            
                            Behavior on opacity {
                                NumberAnimation {
                                    duration: 50
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
