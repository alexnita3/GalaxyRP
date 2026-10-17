-- Welcome message: greets every player once, when they first join the server.
--
-- To use it: copy this folder to lua/plugins/welcome, then run /scripts enable welcome in game
-- (or lua_enable welcome from rcon). To keep it on after a restart, add "welcome" to
-- rp_lua_plugins in galaxyrp_server.cfg.

Plugin.register{ name = "Welcome message", version = "1.0", api = 1 }

-- %s is the player's name
local GREETING = "^7Welcome to the server, %s^7!"
local HINT = "Type ^3/list^7 in the console to see what you can do here."

-- players who connected for the first time and have not been greeted yet. A player's connection
-- also begins again on every map change; those are not greeted twice.
local pending = {}

Events.on("client_connect", function(player, first_time)
	if first_time and not player.is_bot then
		pending[player] = true
	end
end)

Events.on("client_begin", function(player)
	if pending[player] then
		pending[player] = nil
		Msg.center(player, GREETING:format(player.name_raw))
		Msg.print(player, HINT)
	end
end)

Events.on("client_disconnect", function(player)
	pending[player] = nil
end)
