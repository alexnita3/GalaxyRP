-- Timed announcements: shows the next message from a list in everyone's chat every few minutes.
--
-- To use it: copy this folder to lua/plugins/announcements, edit MESSAGES and MINUTES below, then
-- run /scripts enable announcements in game (or lua_enable announcements from rcon). From then on
-- it starts with every map.

Plugin.register{ name = "Timed announcements", version = "1.0", api = 1 }

local MINUTES = 10

local MESSAGES = {
	"^3[Server]^7 Remember to stay in character in IC chat. Use ^3/ooc^7 for out-of-character talk.",
	"^3[Server]^7 New here? Type ^3/list^7 in the console to see the commands.",
	"^3[Server]^7 Found a bug? Tell an admin.",
}

-- which message comes next; kept in storage so the rotation carries on after a map change
local next_index = Storage.get("next") or 1

Timer.every(MINUTES * 60 * 1000, function()
	if #MESSAGES == 0 then
		return
	end
	if next_index > #MESSAGES then
		next_index = 1
	end
	Msg.broadcast(MESSAGES[next_index])
	next_index = next_index + 1
	Storage.set("next", next_index)
end)
