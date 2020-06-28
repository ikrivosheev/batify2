# batify2

<a href="https://aur.archlinux.org/packages/batify2/"><img src="https://raw.githubusercontent.com/themix-project/oomox/master/packaging/download_aur.png" height="54"></a>


### Description
Simple battery notification. 

### Usage

```
batify [OPTION…] [BATTERY ID]
```

### Options

#### Help Options:
* `-h`, `--help` - Show help options

#### Application Options:
* `-d`, `--debug` - Enable/disable debug information
* `-i`, `--interval` - Update interval in seconds
* `-t`, `--timeout` - Notification timeout
* `-l`, `--low-level` - Low battery level in percent
* `-c`, `--critical-level` - Critical battery level in percent
* `-f`, `--full-capacity` - Full capacity for battery

### Examples

`batify 1`

`batify -i 10 1`

`batify -l 25 -c 15 -f 98 1`
