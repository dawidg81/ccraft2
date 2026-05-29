# 0.17.0

* Finished implementing world db

# 0.16.0

* Fixed server freeze on teleporting to player from other world.

# 0.15.3

* Fixed checking creation of db/ directory.

# 0.15.2

* Fixed database creation to be in db/ directory.

# 0.15.1

* Added welcome message.

# 0.15.0

* Added commands conherent to PlayerDB.

# 0.14.0

* Database files will now show in db/ directory.

# 0.13.1-prePlayerDB

* Fix in receiving player's IP.

# 0.13.0-prePlayerDB

* Added PlayerDB.

# 0.12.3-config

* Player now directly receives server name and MOTD from configuration file.

# 0.12.3-inconfig

* Added configuration file support.

# 0.12.3-preconfig

* Defaulted server name and server motd before config implementation.

# 0.12.2-preport

* Player now can't teleport to themselves and kick themselves. Attempting to make cross-level teleportation.

# 0.12.1

* Fixed new world generation

# 0.12.0

* Now more advanced world file format.

# 0.11.2

* Added text wrapping if doesn't fit in 64 character row.

# 0.11.1

* Fixed compilation errors. Missing `<algorithm>` header.

# 0.11.0

* Updated commands construction. Now each command contains system name, usage description, short and long description.

# 0.10.4

* Fixed world backup system. Backups are now saved only if block changes in map
are detected. Even less disk flooding.

# 0.10.3

* Fixed server crash on joining from c0.30 clients.

# 0.10.2

* Automatic level backups are now being done every 5 minutes instead of each
minute so it doesn't flood up the disk quickly.

# 0.10.1

* Fixed server socket: no more `Accept failed` errors.

# 0.10.0

* Added backup system for worlds. Added `backtp` and `revert` commands.

# 0.9.0

* `/kick` command now accepts reason as argument.

# 0.8.0

* Added multi-world system. World files are saved in `maps/` directory. New
commands coming with the new system.

# 0.7.1

* Added variables that in the future will be used for configuration file.

# 0.7.0

* Added new command `/save` that lets admins save the level they are currently on.

# 0.6.0

* Added new command `/tp` that lets you teleport to others or to yourself.

Earlier versions than 0.6.0 were not documented.
