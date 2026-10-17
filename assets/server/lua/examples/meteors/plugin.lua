-- Meteors: a meteor shower world event. Admins with the Scripts power start one with
-- /meteors <seconds>; explosions, sounds and screen shakes then fall at random points of the area
-- set below, every one to three seconds. Visual only: nobody is hurt. Shows Task, World.effect,
-- World.sound and World.shake.
--
-- To use it: copy this folder to lua/plugins/meteors and set AREA_MINS and AREA_MAXS below to
-- two opposite corners of the area (stand at each and type /viewpos in the console). Then run
-- /scripts enable meteors in game (or lua_enable meteors from rcon). From then on it starts with
-- every map.

Plugin.register{ name = "Meteor shower", version = "1.0", api = 2 }

local AREA_MINS = { x = -1024, y = -1024, z = 0 }
local AREA_MAXS = { x = 1024, y = 1024, z = 256 }
local EFFECT = "env/med_explode2"
local SOUND = "sound/weapons/explosions/cargoexplode.wav"

local running = false

local function randomPoint()
	return {
		x = math.random(AREA_MINS.x, AREA_MAXS.x),
		y = math.random(AREA_MINS.y, AREA_MAXS.y),
		z = math.random(AREA_MINS.z, AREA_MAXS.z),
	}
end

local function shower(seconds)
	Msg.broadcast("^1Meteors^7 are falling! Take cover!")
	local stop = Server.time() + seconds * 1000
	while Server.time() < stop do
		local p = randomPoint()
		World.effect(EFFECT, p)
		World.sound(SOUND, p)
		World.shake(p, 600, 3, 1000)
		Task.wait(math.random(1000, 3000))
	end
	Msg.broadcast("^2The meteor shower is over.")
	running = false
end

Commands.register("meteors", function(player, args)
	local seconds = math.tointeger(tonumber(args[1] or ""))
	if not seconds or seconds < 5 or seconds > 600 then
		Msg.print(player, "Usage: /meteors <seconds>, 5 to 600.")
		return
	end
	if running then
		Msg.print(player, "A meteor shower is already falling.")
		return
	end
	running = true
	Task.start(shower, seconds)
	Msg.print(player, "Meteor shower started for " .. seconds .. " seconds.")
end, { help = "Starts a meteor shower (visual only) for <seconds>", admin = 29, logged_in = true })

-- the shower ends with the map; a reload starts with none falling
Events.on("unload", function() running = false end)
