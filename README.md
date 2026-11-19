# batify2

<a href="https://aur.archlinux.org/packages/batify2/"><img src="https://raw.githubusercontent.com/themix-project/oomox/master/packaging/download_aur.png" height="54"></a>

### Description

Simple battery notification. Runs as a daemon, reads battery information from sysfs and sends notifications (status change, low/critical level) via libnotify.

### Dependencies

- GLib 2
- libnotify
- CMake 3.10+

### Build

```
mkdir build && cd build
cmake ..
cmake --build .
```

Install:

```
cmake --install .    # or: sudo make install
```

### Usage

```
batify [OPTION…]
```

### Options

#### Help Options:

- `-h`, `--help` - Show help options

#### Application Options:

- `-d`, `--debug` - Enable debug information
- `-i`, `--interval` - Update interval in seconds (default: 5)
- `-t`, `--timeout` - Notification timeout in seconds (default: -1, notification-server default; 0 - never expires)
- `-l`, `--low-level` - Low battery level in percent (default: 20)
- `-c`, `--critical-level` - Critical battery level in percent (default: 10)
- `-f`, `--full-capacity` - Full capacity for battery in percent (default: 98)

### Examples

`batify`

`batify -i 10`

`batify -l 25 -c 15 -f 98`

`batify -d`

### License

MIT — see [LICENSE](LICENSE).
