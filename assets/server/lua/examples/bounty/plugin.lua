-- Bounty: a "kill N NPCs" quest. A player on a character types /bounty; three targets appear at the
-- points set below, and the hunt is over when all three are down. Kills the player makes are counted
-- per character, and they are told their tally. Shows World.npc, on_death, Storage.char and give.
--
-- To use it: copy this folder to lua/plugins/bounty and set SPAWN_POINTS below to spots on your map
-- (stand there and type /viewpos in the console). Then run /scripts enable bounty in game (or
-- lua_enable bounty from rcon). From then on it starts with every map.

Plugin.register{ name = "Bounty hunt", version = "1.0", api = 3 }

local TARGET_TYPE = "stormtrooper"		-- an NPC type (see /list npcs)
local SPAWN_POINTS = {
	{ x = 256, y = 0, z = 0 },
	{ x = -256, y = 0, z = 0 },
	{ x = 0, y = 256, z = 0 },
}
local REWARD_ITEM = "weapon_blaster"	-- given as a reward; nil for none

local hunts = {}	-- player -> { left = targets still up, targets = { npc = true } }

local function finish(player, hunt)
	hunts[player] = nil
	if not player.valid then
		return
	end
	local total = 0
	if player.character_id then
		total = Storage.char(player):get("kills") or 0
	end
	Msg.center(player, "^2Bounty complete!")
	Msg.print(player, "^2Bounty complete.^7 You have " .. total .. " bounty kills in all.")
	if REWARD_ITEM then
		local ok = player:give(REWARD_ITEM)
		if ok then
			Msg.print(player, "^3Your reward is waiting in your inventory.")
		end
	end
end

local function cancel(player)
	local hunt = hunts[player]
	if not hunt then
		return false
	end
	hunts[player] = nil
	for npc in pairs(hunt.targets) do
		if npc.valid then
			npc:free()
		end
	end
	return true
end

Commands.register("bounty", function(player, args)
	if args[1] == "cancel" then
		if cancel(player) then
			Msg.print(player, "Hunt called off.")
		else
			Msg.print(player, "You are not on a hunt.")
		end
		return
	end
	if not player.character_id then
		Msg.print(player, "You need to be playing a character to take a bounty.")
		return
	end
	if hunts[player] then
		Msg.print(player, "You are already on a hunt: " .. hunts[player].left .. " targets left. (/bounty cancel gives up)")
		return
	end

	local hunt = { left = 0, targets = {} }
	for _, pos in ipairs(SPAWN_POINTS) do
		local npc, why = World.npc(TARGET_TYPE, pos, { team = "enemy" })
		if npc then
			hunt.targets[npc] = true
			hunt.left = hunt.left + 1
			npc:on_death(function(dead, killer)
				if hunts[player] ~= hunt then
					return		-- this hunt is over
				end
				hunt.targets[dead] = nil
				hunt.left = hunt.left - 1
				if killer == player and player.character_id then
					local store = Storage.char(player)
					store:set("kills", (store:get("kills") or 0) + 1)
				end
				if hunt.left > 0 then
					Msg.center(player, "^3Target down.^7 " .. hunt.left .. " to go.")
				else
					finish(player, hunt)
				end
			end)
		else
			print("bounty: a target could not be placed: " .. why)
		end
	end
	if hunt.left == 0 then
		Msg.print(player, "No targets could be placed right now. Try again later.")
		return
	end
	hunts[player] = hunt
	Msg.center(player, "^1Bounty:^7 " .. hunt.left .. " targets. Hunt them down!")
end, { help = "Starts a bounty hunt (/bounty cancel gives up)", logged_in = true })

-- a player who leaves takes their hunt with them
Events.on("client_disconnect", function(player)
	cancel(player)
end)
