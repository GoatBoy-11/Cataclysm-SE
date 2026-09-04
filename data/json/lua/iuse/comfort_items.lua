-- Comfort items: the body pillows and the magical girl doll.
--
-- Imported from the civilian_variety mod. The morale id is resolved here at
-- module scope rather than in preload.lua, because preload runs before any JSON
-- is read and a morale type referenced then does not exist yet; main.lua, which
-- requires this file, runs after.

local storage = game.mod_storage[game.current_mod]

local comfort_items = {}

local morale_feeling_good = MoraleTypeDataId.new("morale_feeling_good")

-- One turn is one second, so this is a day.
local PILLOW_COOLDOWN_TURNS = 24 * 60 * 60

-- Vanilla's DOLLCHAT says exactly one line per press; this keeps that cadence.
-- An earlier version said two or three at once, which read as the doll emptying
-- its whole repertoire rather than answering you.
local DOLL_LINES = {
  "\"In the name of the tides, I will punish you!\"",
  "\"Mariner Moon, transform!\"",
  "\"I am not just a schoolgirl.  I am the guardian of the seventh sea!\"",
  "\"Everyone is counting on us.  Especially me.  I am counting on us a great deal.\"",
  "\"You cannot hide from the moonlight!\"",
  "\"That is my friend you are talking about!\"",
  "\"Even when it is dark, the tide still comes in.\"",
  "\"I did the homework.  I did MOST of the homework.\"",
  "\"Together, we are the storm!\"",
  "\"Do not cry.  Or do, and then let us go and win anyway.\"",
}

-- The cooldown lives in mod storage rather than on an effect, so it saves with
-- the world and stays invisible. It is stored per player, not per pillow, so
-- owning two does not get you two hugs a day.
function comfort_items.pillow_hug(params)
  local who = params.user
  if not who then
    return 0
  end

  local now = gapi.current_turn():to_turn()
  local last = storage.pillow_last_turn
  if last and now - last < PILLOW_COOLDOWN_TURNS then
    gapi.add_msg(MsgType.info, "You have already taken what comfort there is in it today.")
    return 0
  end

  storage.pillow_last_turn = now
  who:add_morale(morale_feeling_good, 10, 18, TimeDuration.from_hours(4),
    TimeDuration.from_hours(2), false)
  gapi.add_msg(MsgType.good, "You embrace the body pillow.  You feel slightly less alone.")
  -- Return 0: a GENERIC item has no charges, and returning 1 would ask the game
  -- to consume one, which on a chargeless item destroys it.
  return 0
end

function comfort_items.doll_chat(params)
  local who = params.user
  if not who then
    return 0
  end
  gapi.add_msg(MsgType.neutral, "The doll says, tinnily: " .. DOLL_LINES[gapi.rng(1, #DOLL_LINES)])
  -- One battery charge per press, matching vanilla talking_doll.
  return 1
end

return comfort_items
