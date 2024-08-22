# macro-wl

`macro-wl` obtains exclusive control of a given HID device, matches any keys
pressed, and executes the corresponding configured commands. `macro-wl` is
intended to be run in Wayland environments.

## Dependencies

- libinput
- libevdev
- `stdbuf` (coreutils)

## Installation

```sh
make PREFIX="$HOME/.local" install
```

## Usage

```sh
macro-wl </dev/input/by-id/...>
```

### Configuration

Configuration is performed primarily via environment variables. These
can optionally be stored in a configuration file located, by default,
at `${XDG_CONFIG_HOME}/macro-wl/config.env` or a file pointed to by
`$MACRO_CONFIG`. The configuration file is simply a list of environment
variables.

Each key can be configured by setting the environment variable `MACRO_<keysym>`.
An example configuration can be found below.

Sample configuration:
```sh
MACRO_DEVICE="</dev/input/by-id/...>"
MACRO_KEY_ENTER="echo 'Hello, world!'"
```
