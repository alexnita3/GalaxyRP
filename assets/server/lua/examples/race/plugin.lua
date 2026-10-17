-- Race: a checkpoint race. /race puts the player at the start line, counts down, and times them
-- through the checkpoints in order. Racers run as holograms, so other players do not block them.
-- The best time per character is kept, and a new best overall is announced to everyone.
-- Shows teleport, set_effect, zones, Task, play_sound and storage.
--
-- To use it: copy this folder to lua/plugins/race and set START and CHECKPOINTS below to your
-- course (stand at each spot and type /viewpos in the console; a checkpoint is a box between two
-- opposite corners). Then run /scripts enable race in game (or lua_enable race from rcon). From
-- then on it starts with every map.

Plugin.register{ name = "Checkpoint race", version = "1.0", api = 3 }

local START = { x = 0, y = 0, z = 0 }
local START_YAW = 0
local CHECKPOINTS = {		-- in the order they must be run through
	{ mins = { x = 500, y = -100, z = -50 }, maxs = { x = 700, y = 100, z = 150 } },
	{ mins = { x = 1100, y = -100, z = -50 }, maxs = { x = 1300, y = 100, z = 150 } },
	{ mins = { x = 1700, y = -100, z = -50 }, maxs = { x = 1900, y = 100, z = 150 } },
}
local COUNTDOWN = 3						-- seconds
local TIME_LIMIT_MS = 5 * 60 * 1000		-- a race not finished by then is over
local SOUND = "sound/interface/secret_area"

local racers = {}	-- player -> { next = the checkpoint to reach, start = Server.time() at GO, nil before }

local function fmt(ms)
	return string.format("%d:%05.2f", ms // 60000, (ms % 60000) / 1000)
end

local function stop(player, why)
	if not racers[player] then
		return
	end
	racers[player] = nil
	if player.valid then
		if player.effect == "holo" then
			player:set_effect(nil)
		end
		if why then
			Msg.center(player, why)
		end
	end
end

local function finish(player, r)
	local ms = Server.time() - r.start
	stop(player, "^2Finished in " .. fmt(ms) .. "!")
	if player.character_id then
		local store = Storage.char(player)
		local best = store:get("best")
		if not best or ms < best then
			store:set("best", ms)
			Msg.print(player, "^3A new personal best: " .. fmt(ms))
		end
	end
	local record = Storage.get("record")
	if not record or ms < record.ms then
		Storage.set("record", { ms = ms, name = player.name })
		Msg.broadcast("^3" .. player.name .. "^7 set a new race record: ^3" .. fmt(ms))
	end
end

for i, cp in ipairs(CHECKPOINTS) do
	local zone, why = World.zone{ mins = cp.mins, maxs = cp.maxs }
	if not zone then
		print("race: checkpoint " .. i .. " could not be placed: " .. why)
		return
	end
	zone:on_enter(function(_, player)
		local r = racers[player]
		if not r or not r.start or r.next ~= i then
			return
		end
		if i == #CHECKPOINTS then
			finish(player, r)
			return
		end
		r.next = i + 1
		player:play_sound(SOUND)
		Msg.center(player, "Checkpoint " .. i .. "/" .. #CHECKPOINTS .. "  ^3" .. fmt(Server.time() - r.start))
	end)
end

local function countdown(player)
	for n = COUNTDOWN, 1, -1 do
		if not player.valid or not racers[player] then
			return
		end
		Msg.center(player, "^3" .. n)
		Task.wait(1000)
	end
	if player.valid and racers[player] then
		racers[player].start = Server.time()
		Msg.center(player, "^2GO!")
	end
end

Commands.register("race", function(player, args)
	if args[1] == "stop" then
		if racers[player] then
			stop(player, "Race abandoned.")
		else
			Msg.print(player, "You are not racing.")
		end
		return
	end
	if racers[player] then
		Msg.print(player, "You are already racing. /race stop gives up.")
		return
	end
	local ok, why = player:teleport(START, START_YAW)
	if not ok then
		Msg.print(player, "You cannot start a race now: " .. why .. ".")
		return
	end
	player:set_effect("holo")
	racers[player] = { next = 1 }
	Task.start(countdown, player)
	local record = Storage.get("record")
	if record then
		Msg.print(player, "The record is " .. fmt(record.ms) .. ", by " .. record.name .. ".")
	end
end, { help = "Starts a checkpoint race (/race stop gives up)", logged_in = true })

-- too slow, dead or knocked down: the race is over
Timer.every(1000, function()
	local now = Server.time()
	for player, r in pairs(racers) do
		if r.start and now - r.start > TIME_LIMIT_MS then
			stop(player, "^1Out of time.")
		end
	end
end)
Events.on("player_death", function(player)
	stop(player, "^1The race is over.")
end)
Events.on("player_downed", function(player)
	stop(player, "^1The race is over.")
end)
Events.on("client_disconnect", function(player)
	racers[player] = nil
end)
-- switched off or reloaded: the effects this plugin set go by themselves
