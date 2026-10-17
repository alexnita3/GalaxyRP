-- Terminal: an information console players use with the Use key. Shows World.spawn, on_use,
-- World.sound and per-character storage (a "first visit" note), and how to watch one of the map's
-- own entities (commented out at the bottom).
--
-- To use it: copy this folder to lua/plugins/terminal and set POSITION below to a spot on your
-- map (stand there and type /viewpos in the console: it prints where you are). Then run
-- /scripts enable terminal in game (or lua_enable terminal from rcon). From then on it starts with
-- every map.

Plugin.register{ name = "Info terminal", version = "1.0", api = 2 }

local POSITION = { x = 0, y = 0, z = 0 }	-- where the console stands
local YAW = 0								-- which way it faces, in degrees
local COOLDOWN_MS = 3000					-- between two uses by the same player

local PAGE = {
	"^3== Galaxy Information Terminal ==",
	"^7Welcome, traveller.",
	"^7Type ^3/list^7 to see what you can do here.",
}

local console, why = World.spawn("misc_model_breakable", {
	model = "models/map_objects/kejim/sec_panel.md3",
	origin = POSITION,
	angles = { x = 0, y = YAW, z = 0 },
	spawnflags = 65,	-- 1 SOLID + 64 USE_NOT_BREAK: it blocks players, and using it never breaks it
})
if not console then
	print("the terminal could not be placed: " .. why)
	return
end

local lastUse = {}	-- player -> Server.time() of their last use

console:on_use(function(ent, player)
	-- player is nil, or not a player at all, when a trigger or an NPC sets it off
	if not player or player.name == nil then
		return false
	end
	local now = Server.time()
	if lastUse[player] and now - lastUse[player] < COOLDOWN_MS then
		return false
	end
	lastUse[player] = now

	World.sound("sound/interface/secret_area", ent)
	Msg.center(player, table.concat(PAGE, "\n"))

	if player.character_id then
		local store = Storage.char(player)
		if not store:get("visited") then
			store:set("visited", true)
			Msg.chat(player, "^3Terminal^7: first visit logged. Welcome aboard!")
		end
	end
	return false	-- nothing else happens: the console stays as it is
end)

Events.on("client_disconnect", function(player)
	lastUse[player] = nil
end)

-- To react when one of the map's own entities is used (a door, a button...), give its targetname.
-- Returning false stops the use: here, the door stays shut for players without a character.
--
-- Events.on("use", function(ent, player)
-- 	if player and player.name and not player.character_id then
-- 		Msg.center(player, "^1Access denied.")
-- 		return false
-- 	end
-- end, { targetname = "vault_door" })
