# archon-timesync
**Archon keyboard LCD clock synchronization tool**

This tool synchronizes your archon keyboard to the system clock. It can be added to the startup apps to automatically sync your keyboard's clock, so that you don't have to open archon app periodically.

Valid argument options:
- `--ak47`: Sync clock of archon AK47 keyboard (NOT SUPPORTED AT THE MOMENT. SEE [THIS ISSUE](https://github.com/modular-arisu/archon-timesync/issues/1))
- `--ak74`: Sync clock of archon AK74 keyboard
- `--help`: Show help containing all valid argument options

\
**You must pass one keyboard when running this tool.**\
(e.g. `archon-timesync --ak74`, not `archon-timesync` nor `archon-timesync --ak74 --ak47`)