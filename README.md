# CavaMonitor

This plugin provides real-time audio spectrum analysis for use in Quickshell or other QML applications.

## Dependencies

### Runtime Dependencies
- Qt6 (Core, Qml)
- PipeWire
- FFTW3
- Cava library

### Build Dependencies
- CMake >= 3.20
- C++20 compatible compiler
- Qt6 development files
- PipeWire development files
- FFTW3 development files
- Cava library with headers

## Installation

### Build and Install Cava Library

Since the standard `cava` package only includes the binary in most distros, you need to build the library from source:

```bash
# Clone cava
git clone https://github.com/karlstav/cava.git
cd cava

# Build just the core library
gcc -shared -fPIC -o libcava.so cavacore.c -lm -lfftw3 -I.

# Install
sudo cp libcava.so /usr/local/lib/
sudo mkdir -p /usr/local/include/cava
sudo cp cavacore.h /usr/local/include/cava/
sudo ldconfig
```

### 3. Build CavaMonitor Plugin

```bash
git clone https://github.com/yourusername/CavaMonitor.git
cd CavaMonitor

mkdir build
cd build

# Configure
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/.local/lib/qt6/qml

# Build
make

# Install
make install
```

### 4. Set Environment Variables
Add to your `~/.bashrc`, `~/.zshrc`, or `~/.config/environment.d/qt.conf`:

```bash
export LD_LIBRARY_PATH="$HOME/.local/lib/qt6/qml/CavaMonitor:$HOME/.local/lib/qt6/qml:/usr/local/lib:$LD_LIBRARY_PATH"
export QML2_IMPORT_PATH="$HOME/.local/lib/qt6/qml:$QML2_IMPORT_PATH"
```


## Usage
### Basic Example

```qml
import QtQuick
import CavaMonitor 1.0

Item {
    CavaMonitor {
        id: cava
        bars: 64        // Number of frequency bars
        active: true    // Start/stop audio monitoring
        
        onValuesChanged: {
            // values is a QVector<double> with frequency data (0.0 to 1.0+)
            console.log("First bar:", values[0])
        }
    }
    
    // Visualizer bars
    Row {
        spacing: 2
        Repeater {
            model: cava.bars
            Rectangle {
                required property int index
                width: 10
                height: Math.max(5, cava.values[index] * 200)
                color: Qt.hsla(index / cava.bars, 0.8, 0.6, 1.0)
                
                Behavior on height {
                    NumberAnimation { duration: 50; easing.type: Easing.OutCubic }
                }
            }
        }
    }
}
```

### Quickshell Integration(My config):

```qml
import Quickshell
import Quickshell.Wayland
import QtQuick
import QtQuick.Layouts
import CavaMonitor 1.0

Scope {
    id: root
    Colors { id: colors }
    
    property int barCount: 40
    property int maxBarWidth: 300
    property int barHeight: 15
    property int barGap: 10
    
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
```

### Properties

- **`bars: int`** - Number of frequency bars (default: 64)
  - Minimum: 1
  - Recommended: 16-128
  
- **`active: bool`** - Enable/disable audio monitoring (default: false)

- **`values: QVector<double>`** (read-only) - Array of frequency bar values
  - Length equals `bars` property
  - Range: typically 0.0 to 1.0, but can exceed 1.0 for loud audio
  - Updates at ~60fps when audio is playing

### Signals

- **`barsChanged()`** - Emitted when the number of bars changes
- **`activeChanged()`** - Emitted when active state changes
- **`valuesChanged()`** - Emitted when frequency values update


## Architecture

1. **PipeWire Worker Thread**: Captures audio from PipeWire in a separate thread
2. **Double Buffer**: Lock-free audio buffer swapping for thread safety
3. **CAVA Processor**: Runs FFT and applies monstercat smoothing filter
4. **Qt Integration**: Emits signals to QML on value changes

### Audio Processing

- **Sample Rate**: 44100 Hz
- **Chunk Size**: 512 samples
- **FFT**: Performed by CAVA library
- **Smoothing**: Monstercat filter (left-right pass with 1.5 decay)
- **Frequency Range**: 50 Hz - 10000 Hz (configurable in source)

## Troubleshooting

### Plugin not found
```bash
# Verify installation
ls ~/.local/lib/qt6/qml/CavaMonitor/

# Check environment variables
echo $QML2_IMPORT_PATH
echo $LD_LIBRARY_PATH
```

### Library loading errors
```bash
# Ensure libcava.so is installed
ls /usr/local/lib/libcava.so

# Update library cache
sudo ldconfig
```

### No audio detected
- Ensure PipeWire is running: `pw-top`
- Check audio is playing in another application
- Set `active: true` in your QML

## Contributing

Contributions welcome! Please open an issue or pull request.

## License

MIT
 
## Credits

- Built for [Quickshell](https://github.com/outfoxxed/quickshell)
- Uses [CAVA](https://github.com/karlstav/cava) for FFT processing
- Audio capture via [PipeWire](https://pipewire.org/)
