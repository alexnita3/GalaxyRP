-- Play time: counts how long each character has been played on this server, and adds /playtime
-- so a player can see their character's total. Shows per-character storage (Storage.char).
--
-- To use it: copy this folder to lua/plugins/playtime, then run /scripts enable playtime in game
-- (or lua_enable playtime from rcon). To keep it on after a restart, add "playtime" to
-- rp_lua_plugins in galaxyrp_server.cfg.
--
-- Time is counted for logged-in players playing a character. It is saved every minute, when the
-- player leaves and at map end, so at most a minute is lost when a player switches character.

Plugin.register{ name = "Play time", version = "1.0", api = 1 }

local SAVE_EVERY_MS = 60 * 1000

-- player -> { char = character id, since = Server.realtime() when counting started }
local sessions = {}

-- adds the time since the last save to the character the player is playing, if it is still the
-- one counting started on, and starts counting again from now
local function save(player)
	local s = sessions[player]
	local now = Server.realtime()
	local char = player.character_id
	if s and char and s.char == char then
		local store = Storage.char(player)
		store:set("seconds", (store:get("seconds") or 0) + (now - s.since))
	end
	if char then
		sessions[player] = { char = char, since = now }
	else
		sessions[player] = nil
	end
end

local function format_seconds(total)
	local hours = total // 3600
	local minutes = (total % 3600) // 60
	return string.format("%d hour%s %d minute%s", hours, hours == 1 and "" or "s", minutes, minutes == 1 and "" or "s")
end

Events.on("init", function()
	-- after a /scripts reload the players are already here: start counting them now
	for _, player in ipairs(Server.players()) do
		save(player)
	end
end)

Events.on("client_begin", save)

Events.on("client_disconnect", function(player)
	save(player)
	sessions[player] = nil
end)

Events.on("shutdown", function()
	for player in pairs(sessions) do
		if player.valid then
			save(player)
		end
	end
end)

Timer.every(SAVE_EVERY_MS, function()
	for player in pairs(sessions) do
		if player.valid then
			save(player)
		else
			sessions[player] = nil
		end
	end
	-- characters the player switched to since the last save start counting too
	for _, player in ipairs(Server.players()) do
		if not sessions[player] then
			save(player)
		end
	end
end)

Commands.register("playtime", function(player)
	if not player.character_id then
		Msg.print(player, "Log in and pick a character to see its play time.")
		return
	end
	save(player)
	local total = Storage.char(player):get("seconds") or 0
	Msg.print(player, "^3" .. player.name_raw .. "^7 has played for " .. format_seconds(total) .. ".")
end, { help = "Shows how long your character has played on this server", logged_in = true })
