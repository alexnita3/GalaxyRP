Put Lua plugins here, one folder per plugin: lua/plugins/<name>/plugin.lua
The folder name is the plugin's name: lower-case letters, digits and _ only.

Every folder here with a plugin.lua runs, at every map start; a new one starts now with
/scripts enable <name>. /scripts disable <name> stops one until the next map: to keep it
off, move its folder out of here. lua/examples has ten to start from, and lua/lua_api.md
explains how to write one.
