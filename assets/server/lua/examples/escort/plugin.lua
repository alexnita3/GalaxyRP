-- Escort: a guard takes a player to a spot through an ambush. A player on a character types /escort at the
-- start point: a guard appears and covers them (it follows and shoots anyone not on their side), and a few
-- enemies wait on the way. At the end, a dialogue asks whether the guard should hold the spot: it runs to its
-- post and guards it, or it follows the player back. Either way the player earns some XP. Shows the NPC orders
-- (cover, follow, go_to, guard, attack, stop), how each one ends, a zone and a dialogue.
--
-- To use it: copy this folder to lua/plugins/escort and set START, GOAL, POST and AMBUSH below to spots on
-- your map (stand there and type /viewpos in the console). On a map without NPC waypoints, NPCs walk in
-- straight lines: keep POST in plain sight of GOAL. Then run /scripts enable escort in game (or
-- lua_enable escort from rcon); to keep it on after a restart, add "escort" to rp_lua_plugins. The XP is
-- subject to rp_lua_maxxp.

Plugin.register{ name = "Escort", version = "1.0", api = 6 }

local START = { x = 0, y = 0, z = 0 }			-- where /escort works and the guard appears
local GOAL = { x = 1024, y = 0, z = 0 }			-- the spot to reach (a zone 128 units around it)
local POST = { x = 1100, y = 96, z = 0 }		-- where the guard stands when it holds the spot
local AMBUSH = { { x = 512, y = 160, z = 0 }, { x = 512, y = -160, z = 0 } }
local GUARD_TYPE = "rebel"						-- NPC types: see /list npcs
local FOE_TYPE = "stormtrooper"
local XP = 2

local jobs = {}		-- player -> { guard =, foes = { npc, ... }, arrived = true once at GOAL }

-- the job is over: the guard and what is left of the ambush go
local function finish(player, text)
	local job = jobs[player]
	if not job then
		return
	end
	jobs[player] = nil
	if job.guard.valid then
		job.guard:free()
	end
	for _, foe in ipairs(job.foes) do
		if foe.valid then
			foe:free()
		end
	end
	if text and player.valid then
		Msg.print(player, text)
	end
end

-- how a guard's order ended: only losing the player, or the guard, ends the job (a new order replacing the
-- last one, or the guard reaching its post, does not)
local function on_guard_order(player)
	return function(guard, done, why)
		if why == "leader_left" then
			finish(player)
		elseif why == "died" then
			finish(player, "^1Your guard has fallen.^7 The escort is over.")
		end
	end
end

local function hold_the_spot(player, job)
	job.guard:go_to(POST, { run = true, radius = 32 }, function(guard, done, why)
		if done and player.valid then
			guard:guard(player, on_guard_order(player))	-- at its post: stands there for the player
			Msg.print(player, "^3Your guard holds the spot.^7 Type /escort again to end the job.")
		elseif why == "stuck" or why == "timeout" then
			guard:follow(player, on_guard_order(player))	-- it could not get there: it comes along instead
			if player.valid then
				Msg.print(player, "Your guard could not reach its post and follows you instead.")
			end
		end
	end)
end

local function arrived(zone, player)
	local job = jobs[player]
	if not job or job.arrived or not job.guard.valid then
		return
	end
	job.arrived = true
	local levels = player:add_xp(XP, "escort")
	Dialog.open(player, {
		title = "Guard",
		text = "We made it. Shall I hold this spot for you?",
		options = { "Yes, guard it", "No, come with me" },
		speaker = job.guard,
		range = 512,
	}, function(p, choice)
		if not job.guard.valid then
			return
		end
		if choice == 1 then
			hold_the_spot(p, job)
		else
			job.guard:follow(p, on_guard_order(p))	-- any other answer, or none: it stays with the player
		end
	end)
	if levels and levels > 0 then
		Msg.center(player, "^2Escort complete!^7 New level!")
	end
end

local goal = World.zone{
	mins = { x = GOAL.x - 128, y = GOAL.y - 128, z = GOAL.z - 64 },
	maxs = { x = GOAL.x + 128, y = GOAL.y + 128, z = GOAL.z + 128 },
}
if goal then
	goal:on_enter(arrived)
end

Commands.register("escort", function(player)
	if jobs[player] then
		finish(player, "The escort job is over.")
		return
	end
	if not player.character_id then
		Msg.print(player, "You need a character for this job.")
		return
	end
	if World.distance(player.origin, START) > 256 then
		Msg.print(player, "The guards wait at the start point.")
		return
	end
	local guard, why = World.npc(GUARD_TYPE, { x = START.x + 48, y = START.y, z = START.z }, { team = "player" })
	if not guard then
		Msg.print(player, "No guard is free: " .. why)
		return
	end
	guard:on_use(function() return false end)		-- the Use key does not make it someone else's follower
	local job = { guard = guard, foes = {} }
	jobs[player] = job
	for _, pos in ipairs(AMBUSH) do
		local foe = World.npc(FOE_TYPE, pos, { team = "enemy" })
		if foe then
			job.foes[#job.foes + 1] = foe
		end
	end
	-- the guard can only be given orders once it has finished spawning: next frame
	Timer.after(100, function()
		if jobs[player] ~= job or not guard.valid or not player.valid then
			return
		end
		local ok, err = guard:cover(player, on_guard_order(player))
		if not ok then
			finish(player, "Your guard cannot come: " .. err)
			return
		end
		for _, foe in ipairs(job.foes) do
			if foe.valid then
				foe:attack(player)	-- the ambush goes for the player, not the guard
			end
		end
		Msg.print(player, "^3Your guard covers you.^7 Reach the marked spot. Type /escort again to call it off.")
	end)
end, { help = "Start or end an escort job", logged_in = true })

Events.on("player_death", function(player)
	finish(player, "^1You fell.^7 The escort is over.")
end)

Events.on("client_disconnect", function(player)
	finish(player)
end)
