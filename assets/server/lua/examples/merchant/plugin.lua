-- Merchant: an NPC players talk to with the Use key. A dialogue offers a ride to another spot of the
-- map, a weapon for credits and a lesson (a skill level, paid with the player's own skillpoints).
-- Each answer leads to the next dialogue. Shows Dialog.open, World.npc, on_use, teleport, give,
-- take_credits and skill_up.
--
-- To use it: copy this folder to lua/plugins/merchant and set POSITION and DOCKS below to spots on
-- your map (stand there and type /viewpos in the console). Then run /scripts enable merchant in game
-- (or lua_enable merchant from rcon); from then on it starts with every map.
--
-- To use a map's own Use trigger instead of an NPC: point the trigger (trigger_multiple with
-- spawnflags 4) at a target_lua with event "merchant", and uncomment the map_event handler below.

Plugin.register{ name = "Merchant", version = "1.0", api = 5 }

local POSITION = { x = 0, y = 0, z = 0 }
local YAW = 0
local DOCKS = { x = 512, y = 0, z = 0 }		-- where the ride goes
local ITEM = "weapon_blaster"
local PRICE = 500
local SKILL = 1								-- as /skillup numbers skills (1 is Jump)

local SPEAKER	-- the NPC, placed at the bottom
local talk		-- defined below: the first dialogue

local function farewell(player, line)
	Dialog.open(player, { title = "Merchant", text = line, options = { "Goodbye" } }, function() end)
end

local function buy(player)
	if not player.character_id then
		farewell(player, "I only trade with people who have a name. Come back with a character.")
		return
	end
	if (player.credits or 0) < PRICE then
		farewell(player, "That is " .. PRICE .. " credits, and you have " .. (player.credits or 0) .. ". Come back richer.")
		return
	end
	local ok, why = player:give(ITEM)
	if not ok then
		farewell(player, "I cannot sell you that now: " .. why .. ".")
		return
	end
	player:take_credits(PRICE, "merchant: " .. ITEM)
	farewell(player, "A fine blaster. Pleasure doing business.")
end

local function learn(player)
	local level, max, name = player:skill(SKILL)
	if not level then
		farewell(player, "Lessons are for people with a character.")
		return
	end
	Dialog.open(player, {
		title = "Merchant",
		text = "A lesson in " .. name .. " costs you one skillpoint. You are at level " .. level .. " of " .. max .. ".",
		options = { "Teach me", "Not now" },
		speaker = SPEAKER,
	}, function(p, choice)
		if choice ~= 1 then return end
		local ok, why = p:skill_up(SKILL)
		if ok then
			farewell(p, "Well done. Practise it.")
		else
			farewell(p, "I cannot teach you that now: " .. why .. ".")
		end
	end)
end

talk = function(player)
	Dialog.open(player, {
		title = "Merchant",
		text = "Hello wanderer, what do you want to do?",
		options = { "Take me to the docks", "Buy a blaster (" .. PRICE .. " credits)", "Teach me something", "Nothing, goodbye" },
		speaker = SPEAKER,
	}, function(p, choice)
		if choice == 1 then
			local ok, why = p:teleport(DOCKS)
			if not ok then
				farewell(p, "The docks will have to wait: " .. why .. ".")
			end
		elseif choice == 2 then
			buy(p)
		elseif choice == 3 then
			learn(p)
		end
	end)
end

SPEAKER = World.npc("protocol", POSITION, { yaw = YAW, team = "neutral" })
if not SPEAKER then
	print("merchant: the NPC could not be placed")
	return
end
SPEAKER:freeze()		-- it stands where it was put
SPEAKER:on_use(function(npc, player)
	if player and player.name then
		talk(player)
	end
	return false		-- the NPC's own use (following the player...) does not happen
end)

-- Events.on("map_event", function(name, player)
-- 	if player and player.name then talk(player) end
-- end, { name = "merchant" })
