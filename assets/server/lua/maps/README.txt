Map scripts: Lua files that run on one map only.
Put them in lua/maps/<map name>/, for example lua/maps/mp/ffa5/doors.lua for mp/ffa5.

They run on that map, every time it starts. Each file runs on its own, like a plugin
called map:<file name>. lua/lua_api.md explains how to write them.
