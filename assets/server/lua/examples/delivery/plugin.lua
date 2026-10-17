-- Delivery: a job. A player on a character types /job at the depot; they leave a deposit and are
-- sent to one of the drop-off points with a package. Delivering it in time pays the fee, gives the
-- deposit back and earns some XP. Running out of time, dying, logging out or changing character
-- loses the job and the deposit. Each character's deliveries are counted.
-- Shows take_credits, add_credits, add_xp, xp_next, zones, timers and storage.
--
-- To use it: copy this folder to lua/plugins/delivery and set DEPOT and DROP_OFFS below to boxes on
-- your map (stand at two opposite corners of each and type /viewpos in the console). Then run
-- /scripts enable delivery in game (or lua_enable delivery from rcon). To keep it on after a restart,
-- add "delivery" to rp_lua_plugins in galaxyrp_server.cfg.
--
-- The amounts are cut to rp_lua_maxcredits and rp_lua_maxxp (5000 and 10 by default), and every
-- payment is written to the server log.

Plugin.register{ name = "Delivery job", version = "1.0", api = 4 }

local DEPOT = { mins = { x = -128, y = -128, z = -32 }, maxs = { x = 128, y = 128, z = 128 } }
local DROP_OFFS = {
	{ name = "the cantina", mins = { x = 1000, y = -100, z = -32 }, maxs = { x = 1200, y = 100, z = 128 } },
	{ name = "the hangar", mins = { x = -1200, y = -100, z = -32 }, maxs = { x = -1000, y = 100, z = 128 } },
	{ name = "the market", mins = { x = -100, y = 1000, z = -32 }, maxs = { x = 100, y = 1200, z = 128 } },
}
local DEPOSIT = 100
local FEE = 250
local XP = 1
local TIME_LIMIT_MS = 3 * 60 * 1000

local jobs = {}		-- player -> { to = drop-off index, char = character_id, ends = Server.time() }
local atDepot = {}	-- player -> true while in the depot

local function lose(player, why)
	jobs[player] = nil
	if player.valid then
		Msg.center(player, "^1Delivery failed:^7 " .. why .. "\nThe deposit is lost.")
	end
end

local depot, why = World.zone{ mins = DEPOT.mins, maxs = DEPOT.maxs }
if not depot then
	print("delivery: the depot could not be placed: " .. why)
	return
end
depot:on_enter(function(_, player)
	atDepot[player] = true
	if not jobs[player] then
		Msg.center(player, "^3Depot^7: type ^3/job^7 to take a delivery (deposit " .. DEPOSIT .. " credits).")
	end
end)
depot:on_leave(function(_, player)
	atDepot[player] = nil
end)

for i, d in ipairs(DROP_OFFS) do
	local zone, zwhy = World.zone{ mins = d.mins, maxs = d.maxs }
	if not zone then
		print("delivery: drop-off " .. d.name .. " could not be placed: " .. zwhy)
		return
	end
	zone:on_enter(function(_, player)
		local job = jobs[player]
		if not job or job.to ~= i or player.character_id ~= job.char then
			return
		end
		jobs[player] = nil
		local paid = player:add_credits(FEE + DEPOSIT, "delivery to " .. d.name)
		local levels = player:add_xp(XP, "delivery to " .. d.name)
		local store = Storage.char(player)
		local count = (store:get("deliveries") or 0) + 1
		store:set("deliveries", count)
		Msg.center(player, "^2Delivered!^7 +" .. (paid or 0) .. " credits")
		local line = "^2Delivery " .. count .. " done.^7 You were paid " .. FEE .. " credits and got your deposit back."
		if levels and levels > 0 then
			line = line .. " ^3New level!"
		elseif player.xp_next then
			line = line .. " XP: " .. player.xp .. "/" .. player.xp_next .. "."
		end
		Msg.print(player, line)
	end)
end

Commands.register("job", function(player)
	if not player.character_id then
		Msg.print(player, "You need to be playing a character to take a job.")
		return
	end
	local job = jobs[player]
	if job then
		local left = math.max(0, job.ends - Server.time()) // 1000
		Msg.print(player, "Take the package to " .. DROP_OFFS[job.to].name .. ": " .. left .. " seconds left.")
		return
	end
	if not atDepot[player] then
		Msg.print(player, "Jobs are given out at the depot.")
		return
	end
	local ok, why2 = player:take_credits(DEPOSIT, "delivery deposit")
	if not ok then
		Msg.print(player, "You cannot leave the deposit: " .. why2 .. ".")
		return
	end
	local to = math.random(#DROP_OFFS)
	jobs[player] = { to = to, char = player.character_id, ends = Server.time() + TIME_LIMIT_MS }
	local count = Storage.char(player):get("deliveries") or 0
	Msg.center(player, "Take this package to ^3" .. DROP_OFFS[to].name .. "^7.\nYou have " .. TIME_LIMIT_MS // 60000 .. " minutes.")
	Msg.print(player, "Deposit of " .. DEPOSIT .. " credits taken. Deliveries so far: " .. count .. ".")
end, { help = "Takes a delivery job at the depot (deposit " .. DEPOSIT .. " credits)", logged_in = true })

-- out of time, or no longer the character that took the job (logged out or switched)
Timer.every(1000, function()
	local now = Server.time()
	for player, job in pairs(jobs) do
		if not player.valid then
			jobs[player] = nil
		elseif player.character_id ~= job.char then
			jobs[player] = nil
		elseif now > job.ends then
			lose(player, "out of time")
		end
	end
end)
Events.on("player_death", function(player)
	if jobs[player] then
		lose(player, "you died")
	end
end)
Events.on("client_disconnect", function(player)
	jobs[player] = nil
	atDepot[player] = nil
end)
