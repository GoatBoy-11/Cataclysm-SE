local lua_traits = {}

local trait_nyctophobia = MutationBranchId.new("NYCTOPHOBIA")
local trait_claustrophobia = MutationBranchId.new("CLAUSTROPHOBIA")
local trait_agoraphobia = MutationBranchId.new("AGORAPHOBIA")
local trait_clutter_intolerant = MutationBranchId.new("CLUTTER_INTOLERANT")
local trait_main_character = MutationBranchId.new("MAIN_CHARACTER")
local trait_small_talk_reflex = MutationBranchId.new("SMALL_TALK_REFLEX")
local trait_suburbanite = MutationBranchId.new("SUBURBANITE")
local trait_trail_blazer = MutationBranchId.new("TRAIL_BLAZER")
local trait_chattering_plush = MutationBranchId.new("CHATTERING_PLUSH")
local trait_anime_protagonist = MutationBranchId.new("ANIME_PROTAGONIST")
local trait_hemophobia = MutationBranchId.new("HEMOPHOBIA")
local trait_decidophobia = MutationBranchId.new("DECIDOPHOBIA")
local trait_atelphobia = MutationBranchId.new("ATELPHOBIA")
local trait_minimalist = MutationBranchId.new("MINIMALIST")
local trait_cowards_sprint = MutationBranchId.new("COWARDS_SPRINT")
local trait_outgunned = MutationBranchId.new("OUTGUNNED")
local trait_comfort_zone = MutationBranchId.new("COMFORT_ZONE")
local trait_lone_wolf = MutationBranchId.new("LONE_WOLF")
local seen_clutter = false
local seen_hemophobia = false
local seen_outgunned = false
local cowards_sprint_alert = false
local trait_storage = nil
local anime_flashback_armed = true
local anime_refuse_down_open = true

local ANIME_COOLDOWN_TURNS = TimeDuration.from_hours(24):to_turns()

local plush_item_ids = {
  ItypeId.new("teddy"),
  ItypeId.new("teddy_bear"),
  ItypeId.new("shark_plush"),
}

local effect_depressants = EffectTypeId.new("depressants")
local effect_downed = EffectTypeId.new("downed")
local effect_shakes = EffectTypeId.new("shakes")

local morale_indoor_misery = MoraleTypeDataId.new("morale_indoor_misery")
local morale_outdoor_misery = MoraleTypeDataId.new("morale_outdoor_misery")
local morale_clutter_intolerant = MoraleTypeDataId.new("morale_clutter_intolerant")
local morale_main_character = MoraleTypeDataId.new("morale_main_character")
local morale_anime_protagonist = MoraleTypeDataId.new("morale_anime_protagonist")
local morale_suburbanite_wild = MoraleTypeDataId.new("morale_suburbanite_wild")
local morale_suburbanite_indoors = MoraleTypeDataId.new("morale_suburbanite_indoors")
local morale_chattering_plush = MoraleTypeDataId.new("morale_chattering_plush")
local morale_hemophobia = MoraleTypeDataId.new("morale_hemophobia")
local morale_decidophobia = MoraleTypeDataId.new("morale_decidophobia")
local morale_atelphobia = MoraleTypeDataId.new("morale_atelphobia")
local morale_minimalist = MoraleTypeDataId.new("morale_minimalist")
local morale_outgunned = MoraleTypeDataId.new("morale_outgunned")
local morale_comfort_zone = MoraleTypeDataId.new("morale_comfort_zone")
local morale_lone_wolf = MoraleTypeDataId.new("morale_lone_wolf")
local morale_anime_nakama = MoraleTypeDataId.new("morale_anime_nakama")
local morale_anime_nakama_bored = MoraleTypeDataId.new("morale_anime_nakama_bored")
local morale_anime_flashback = MoraleTypeDataId.new("morale_anime_flashback")
local effect_anime_flashback = EffectTypeId.new("effect_anime_flashback")
local blood_field_ids = {
  FieldTypeId.new("fd_blood"):int_id(),
  FieldTypeId.new("fd_blood_veggy"):int_id(),
  FieldTypeId.new("fd_blood_insect"):int_id(),
  FieldTypeId.new("fd_blood_invertebrate"):int_id(),
  FieldTypeId.new("fd_gibs_flesh"):int_id(),
  FieldTypeId.new("fd_gibs_veggy"):int_id(),
  FieldTypeId.new("fd_gibs_insect"):int_id(),
  FieldTypeId.new("fd_gibs_invertebrate"):int_id(),
}
local moppable_field_ids = {
  FieldTypeId.new("fd_blood"):int_id(),
  FieldTypeId.new("fd_blood_veggy"):int_id(),
  FieldTypeId.new("fd_blood_insect"):int_id(),
  FieldTypeId.new("fd_blood_invertebrate"):int_id(),
  FieldTypeId.new("fd_gibs_flesh"):int_id(),
  FieldTypeId.new("fd_gibs_veggy"):int_id(),
  FieldTypeId.new("fd_gibs_insect"):int_id(),
  FieldTypeId.new("fd_gibs_invertebrate"):int_id(),
  FieldTypeId.new("fd_bile"):int_id(),
  FieldTypeId.new("fd_slime"):int_id(),
  FieldTypeId.new("fd_sludge"):int_id(),
}

local clutter_radius = 8
local clutter_threshold = 12
local clutter_step = 5
local max_penalty = 30
local max_bonus = 20

local main_character_lines = {
  locale.gettext("For a moment you feel like this is your story."),
  locale.gettext("The scene pauses around you.  Probably nothing."),
  locale.gettext("You strike a pose nobody asked for."),
  locale.gettext("This would look better with a soundtrack."),
  locale.gettext("You narrate your next step under your breath."),
}

local chattering_plush_lines = {
  locale.gettext("Something in your pack mutters nonsense."),
  locale.gettext("You hear a faint electronic chirp from your belongings."),
  locale.gettext("A muffled voice insists you should drink more water."),
  locale.gettext("Your gear makes a noise that is almost language."),
}

local small_talk_lines = {
  locale.gettext("You blurt out a greeting to nobody in particular."),
  locale.gettext("\"Lovely weather,\" you say, to the room."),
  locale.gettext("You nod at a stranger who may or may not be there."),
  locale.gettext("\"Howdy,\" you announce, unprompted."),
}

local anime_confidence_lines = {
  locale.gettext("I can still win!"),
  locale.gettext("My friends are counting on me!"),
  locale.gettext("This is where the comeback starts!"),
  locale.gettext("You're not taking me down here!"),
  locale.gettext("I refuse to lose on a trash heap like this!"),
}

local anime_technique_names = {
  "Firefist Attack",
  "Burning Blade",
  "Devil Sphere",
  "Meteor Rush",
  "Final Chapter Strike",
  "Heroic Uppercut",
  "Soul Edge Flash",
}

local anime_dialogue_yells = {
  locale.gettext("GOOD TO SEE YOU!"),
  locale.gettext("I HAVE A LOT TO SAY!"),
  locale.gettext("LISTEN UP!"),
  locale.gettext("WE NEED TO TALK!"),
  locale.gettext("HEY! OVER HERE!"),
}

local anime_flashback_lines = {
  locale.gettext("You remember a promise you made before everything fell apart."),
  locale.gettext("A half-forgotten voice tells you to get back up."),
  locale.gettext("For one heartbeat the world slows, and you remember why you keep going."),
}

local in_darkness_alert = false

---@return number
local function nyctophobia_threshold() return gapi.light_ambient_lit() - 3.0 end

---@param duration TimeDuration
---@return boolean
local function one_turn_in(duration)
  local turns = duration:to_turns()
  if turns <= 0 then return false end
  return gapi.rng(1, turns) == 1
end

---@generic T
---@param list T[]
---@return T|nil
local function random_entry(list)
  if #list == 0 then return nil end
  local idx = gapi.rng(1, #list)
  return list[idx]
end

---@param map Map
---@param pt TripointBubMs
---@return boolean
local function is_passable(map, pt)
  local ter = map:get_ter_at(pt):obj()
  if ter:has_flag("IMPASSABLE") or ter:get_movecost() <= 0 then return false end
  local furn = map:get_furn_at(pt):obj()
  if furn:has_flag("IMPASSABLE") then return false end
  return true
end

---@param who Character
local function is_wielding_mop(who)
  for _, it in pairs(who:all_items(false)) do
    if who:is_wielding(it) then
      local itype = it:get_type():obj()
      if itype and itype:can_use("MOP") then return true end
    end
  end
  return false
end

---@param here Map
---@param center TripointBubMs
local function auto_mop_surrounding(here, center)
  local mopped_tiles = 0
  for _, pt in ipairs(here:points_in_radius(center, 1)) do
    local mopped_tile = false
    for _, field_id in ipairs(moppable_field_ids) do
      if here:has_field_at(pt, field_id) then
        here:remove_field_at(pt, field_id)
        mopped_tile = true
      end
    end
    if mopped_tile then mopped_tiles = mopped_tiles + 1 end
  end
  return mopped_tiles
end

---@param who Character
---@param morale_id MoraleTypeDataId
---@param penalty integer
local function apply_penalty(who, morale_id, penalty)
  local magnitude = math.min(math.max(penalty, 0), max_penalty)
  if magnitude <= 0 then
    who:rem_morale(morale_id)
    return
  end

  who:add_morale(
    morale_id,
    -magnitude,
    -magnitude,
    TimeDuration.from_minutes(20),
    TimeDuration.from_minutes(20),
    true,
    nil
  )
end

---@param who Character
---@param morale_id MoraleTypeDataId
---@param bonus integer
local function apply_bonus(who, morale_id, bonus)
  local magnitude = math.min(math.max(bonus, 0), max_bonus)
  if magnitude <= 0 then
    who:rem_morale(morale_id)
    return
  end

  who:add_morale(
    morale_id,
    magnitude,
    magnitude,
    TimeDuration.from_minutes(20),
    TimeDuration.from_minutes(20),
    true,
    nil
  )
end

---@param who Character
---@return boolean
local function carries_plush(who)
  for _, plush_id in ipairs(plush_item_ids) do
    if who:has_item_with_id(plush_id, false) then return true end
  end
  return false
end

---@param here Map
---@param center TripointBubMs
---@param radius integer
---@param field_ids FieldId[]
---@return integer
local function count_fields_near(here, center, radius, field_ids)
  local total = 0
  for _, pt in ipairs(here:points_in_radius(center, radius, 0)) do
    for _, field_id in ipairs(field_ids) do
      if here:has_field_at(pt, field_id) then
        total = total + 1
        break
      end
    end
  end
  return total
end

---@param who Character
---@param range integer
---@return integer
local function count_hostiles_in_range(who, range)
  local hostiles = who:get_hostile_creatures(range)
  local count = 0
  if hostiles then
    for _ in pairs(hostiles) do count = count + 1 end
  end
  return count
end

---@param key string
---@return integer
local function turns_since_stored(key)
  if not trait_storage then return math.huge end
  local last = trait_storage[key]
  if not last then return math.huge end
  return gapi.current_turn():to_turn() - last
end

---@param key string
local function store_turn(key)
  if trait_storage then trait_storage[key] = gapi.current_turn():to_turn() end
end

---@param key string
---@return boolean
local function anime_cooldown_ready(key)
  return turns_since_stored(key) >= ANIME_COOLDOWN_TURNS
end

---@param ratio number
---@return integer
local function anime_technique_roll_threshold(ratio)
  if ratio > 0.75 then return 0 end
  local chance = 0.05 + (1.0 - ratio) * 0.10
  return math.floor(chance * 100)
end

---@param here Map
---@param center TripointBubMs
---@param radius integer
---@return boolean
local function has_follower_nearby(here, center, radius)
  for _, pt in ipairs(here:points_in_radius(center, radius, 0)) do
    local npc = gapi.get_npc_at(pt)
    if npc and npc:is_following() then return true end
  end
  return false
end

---@param who Character
---@return number
local function inventory_fill_ratio(who)
  local capacity = who:volume_capacity():to_milliliter()
  if capacity <= 0 then return 0 end
  return who:volume_carried():to_milliliter() / capacity
end

---@param here Map
---@param center TripointBubMs
---@return boolean
local function is_among_trees(here, center)
  if here:has_flag_ter("TREE", center) then return true end
  for _, pt in ipairs(here:points_in_radius(center, 2, 0)) do
    if here:has_flag_ter("TREE", pt) then return true end
  end
  return false
end

---@param here Map
---@param center TripointBubMs
---@return boolean
local function has_npc_nearby(here, center, radius)
  for _, pt in ipairs(here:points_in_radius(center, radius, 0)) do
    if gapi.get_npc_at(pt) then return true end
  end
  return false
end

---@param who Character
---@param amount integer
---@param minimum integer
local function drain_focus(who, amount, minimum)
  if not who.focus_pool then return end
  local floor = minimum or 0
  local target = who.focus_pool - amount
  if target < floor then target = floor end
  who.focus_pool = target
end

---@param here Map
---@param pt TripointBubMs
---@return boolean
local function is_loot_on_floor(here, pt)
  local furn = here:get_furn_at(pt):obj()
  if furn and (furn:has_flag("CONTAINER") or furn:has_flag("SEALED") or furn:has_flag("PLACE_ITEM")) then
    return false
  end
  return true
end

---@param here Map
---@param center TripointBubMs
---@return integer
local function count_loose_items(here, center)
  local you = gapi.get_avatar()
  if not you then return 0 end
  local total = 0

  for _, pt in ipairs(here:points_in_radius(center, clutter_radius, 0)) do
    if you:sees(pt) then
      if is_loot_on_floor(here, pt) then
        local items = here:get_items_at(pt)
        total = total + #items
      end
    end
  end
  return total
end

---@param count integer
---@return integer
local function clutter_penalty(count)
  if count <= clutter_threshold then return 0 end
  local extra = count - clutter_threshold
  local steps = math.ceil(extra / clutter_step)
  return math.min(steps * 2, max_penalty)
end

local function tick_nyctophobia()
  ---@type Avatar
  local you = gapi.get_avatar()
  if not you:has_trait(trait_nyctophobia) then return end
  if you:get_effect_int(effect_depressants) > 3 then return end

  local here = gapi.get_map()
  local pos = you:get_pos_ms()
  local threshold = nyctophobia_threshold()
  local dark_places = {}

  for _, pt in ipairs(here:points_in_radius(pos, 5)) do
    if you:sees(pt) and here:ambient_light_at(pt) < threshold and is_passable(here, pt) then
      table.insert(dark_places, pt)
    end
  end

  local in_darkness = here:ambient_light_at(pos) < threshold
  local chance = in_darkness and 50 or 200

  if #dark_places > 0 and gapi.rng(1, chance) == 1 and one_turn_in(TimeDuration.from_hours(1)) then
    local target = random_entry(dark_places)
    if target then gapi.spawn_hallucination(target) end
  end

  if not in_darkness then
    if in_darkness_alert and you:is_avatar() then
      gapi.add_msg(MsgType.good, locale.gettext("You feel relief as you step back into the light."))
    end
    in_darkness_alert = false
    return
  end

  if you:is_avatar() and not in_darkness_alert then
    gapi.add_msg(MsgType.bad, locale.gettext("You feel a twinge of panic as darkness engulfs you."))
    in_darkness_alert = true
  end

  if gapi.rng(1, 2) == 1 and one_turn_in(TimeDuration.from_hours(1)) then you:sound_hallu() end

  if gapi.rng(1, 200) == 1 and one_turn_in(TimeDuration.from_hours(1)) and not you:is_on_ground() then
    if you:is_avatar() then
      gapi.add_msg(
        MsgType.bad,
        locale.gettext(
          "Your fear of the dark is so intense that your trembling legs fail you, and you fall to the ground."
        )
      )
    end
    you:add_effect(effect_downed, TimeDuration.from_minutes(gapi.rng(1, 2)))
  end

  if gapi.rng(1, 200) == 1 and one_turn_in(TimeDuration.from_hours(1)) and not you:has_effect(effect_shakes) then
    if you:is_avatar() then
      gapi.add_msg(
        MsgType.bad,
        locale.gettext("Your fear of the dark is so intense that your hands start shaking uncontrollably.")
      )
    end
    you:add_effect(effect_shakes, TimeDuration.from_minutes(gapi.rng(1, 2)))
  end

  if gapi.rng(1, 200) == 1 and one_turn_in(TimeDuration.from_hours(1)) then
    if you:is_avatar() then
      gapi.add_msg(
        MsgType.bad,
        locale.gettext(
          "Your fear of the dark is so intense that you start breathing rapidly, and you feel like your heart is ready to jump out of the chest."
        )
      )
    end
  end
end

local function tick_morale_traits()
  local you = gapi.get_avatar()
  if not you then return end

  if you:get_effect_int(effect_depressants) > 3 then
    you:rem_morale(morale_indoor_misery)
    you:rem_morale(morale_outdoor_misery)
    return
  end

  local here = gapi.get_map()
  local pos = you:get_pos_ms()
  local is_outside = here:is_outside(pos)

  if you:has_trait(trait_claustrophobia) then
    if not is_outside then
      apply_penalty(you, morale_indoor_misery, 15)
      if gapi.rng(1, 5) == 1 then drain_focus(you, 1, 20) end
    else
      you:rem_morale(morale_indoor_misery)
    end
  end

  if you:has_trait(trait_agoraphobia) then
    if is_outside then
      apply_penalty(you, morale_outdoor_misery, 15)
      if gapi.rng(1, 5) == 1 then drain_focus(you, 1, 20) end
    else
      you:rem_morale(morale_outdoor_misery)
    end
  end
end

local function tick_clutter_intolerant()
  local you = gapi.get_avatar()
  if not you then return end

  if you:get_effect_int(effect_depressants) > 3 then
    you:rem_morale(morale_clutter_intolerant)
    return
  end

  local here = gapi.get_map()
  local pos = you:get_pos_ms()

  if you:has_trait(trait_clutter_intolerant) then
    local loose_items = count_loose_items(here, pos)
    local penalty = clutter_penalty(loose_items)
    apply_penalty(you, morale_clutter_intolerant, penalty)
    if penalty > 0 and not seen_clutter then
      gapi.add_msg(MsgType.bad, locale.gettext("It's so cluttered here..."))
      seen_clutter = true
    end
    if penalty == 0 then seen_clutter = false end
    if penalty > 0 then
      local chance_scale = math.max(1, math.floor(penalty / 5))
      if gapi.rng(1, 30) <= chance_scale then drain_focus(you, 1, 20) end
    end
  else
    you:rem_morale(morale_clutter_intolerant)
  end
end

-- Forward declaration.  tick_cse_traits_fast calls this before the definition
-- further down is reached, and a `local function` is only in scope from its own
-- line onwards - without this the call resolved to a nil global and the
-- every-second hook errored on every tick for anyone carrying the trait.
local tick_anime_protagonist

local function tick_cse_traits_fast()
  local you = gapi.get_avatar()
  if not you then return end
  if you:get_effect_int(effect_depressants) > 3 then return end

  if you:has_trait(trait_main_character) and gapi.rng(1, 1200) == 1 then
    local line = random_entry(main_character_lines)
    if line and you:is_avatar() then
      gapi.add_msg(MsgType.neutral, line)
    end
    if gapi.rng(1, 2) == 1 then
      apply_bonus(you, morale_main_character, gapi.rng(3, 8))
    else
      apply_penalty(you, morale_main_character, gapi.rng(2, 6))
    end
  end

  if you:has_trait(trait_anime_protagonist) then
    tick_anime_protagonist(you)
  else
    you:rem_morale(morale_anime_protagonist)
    you:rem_morale(morale_anime_nakama)
    you:rem_morale(morale_anime_nakama_bored)
    you:rem_morale(morale_anime_flashback)
  end
end

---@param you Avatar
-- Assigns to the local forward-declared above tick_cse_traits_fast; declaring a
-- fresh `local function` here would shadow it and leave the earlier call nil.
function tick_anime_protagonist(you)
  local max_hp = you:get_hp_max()
  if max_hp <= 0 then return end

  local ratio = you:get_hp() / max_hp
  local here = gapi.get_map()
  local pos = you:get_pos_ms()

  if ratio < 0.75 then
    apply_bonus(you, morale_anime_protagonist, math.floor((0.75 - ratio) * 24))
  elseif ratio >= 0.95 then
    apply_penalty(you, morale_anime_protagonist, math.floor((ratio - 0.9) * 40))
  else
    you:rem_morale(morale_anime_protagonist)
  end

  if ratio >= 0.3 then
    anime_flashback_armed = true
  elseif ratio < 0.3 and anime_flashback_armed and anime_cooldown_ready("anime_flashback_turn") then
    anime_flashback_armed = false
    store_turn("anime_flashback_turn")
    local line = random_entry(anime_flashback_lines)
    if line and you:is_avatar() then
      gapi.add_msg(MsgType.good, line)
      gapi.add_msg(MsgType.good, locale.gettext("The memory hits like a power-up."))
    end
    apply_bonus(you, morale_anime_flashback, 15)
    you:add_effect(effect_anime_flashback, TimeDuration.from_minutes(10))
    you:mod_stamina(math.min(you:get_stamina_max() - you:get_stamina(), 2000))
  end

  if not you:has_effect(effect_downed) then
    anime_refuse_down_open = true
  elseif you:has_effect(effect_downed) and anime_refuse_down_open and ratio < 0.25
      and anime_cooldown_ready("anime_refuse_down_turn") and gapi.rng(1, 4) == 1 then
    anime_refuse_down_open = false
    store_turn("anime_refuse_down_turn")
    you:remove_effect(effect_downed)
    if you:is_avatar() then
      gapi.add_msg(MsgType.good, locale.gettext("You refuse to stay down!"))
    end
  end

  if ratio < 0.5 and count_hostiles_in_range(you, 12) > 0 and gapi.rng(1, 400) == 1 then
    local line = random_entry(anime_confidence_lines)
    if line and you:is_avatar() then gapi.add_msg(MsgType.good, line) end
  end

  if has_follower_nearby(here, pos, 6) then
    if ratio < 0.5 then
      apply_bonus(you, morale_anime_nakama, 12)
      you:rem_morale(morale_anime_nakama_bored)
    elseif ratio > 0.8 then
      apply_penalty(you, morale_anime_nakama_bored, 8)
      you:rem_morale(morale_anime_nakama)
    else
      you:rem_morale(morale_anime_nakama)
      you:rem_morale(morale_anime_nakama_bored)
    end
  else
    you:rem_morale(morale_anime_nakama)
    you:rem_morale(morale_anime_nakama_bored)
  end
end

local function tick_cse_traits_slow()
  local you = gapi.get_avatar()
  if not you then return end

  if you:get_effect_int(effect_depressants) > 3 then
    you:rem_morale(morale_suburbanite_wild)
    you:rem_morale(morale_suburbanite_indoors)
    you:rem_morale(morale_chattering_plush)
    you:rem_morale(morale_hemophobia)
    you:rem_morale(morale_decidophobia)
    you:rem_morale(morale_minimalist)
    you:rem_morale(morale_outgunned)
    you:rem_morale(morale_comfort_zone)
    you:rem_morale(morale_lone_wolf)
    you:rem_morale(morale_anime_nakama)
    you:rem_morale(morale_anime_nakama_bored)
    you:rem_morale(morale_anime_flashback)
    return
  end

  local here = gapi.get_map()
  local pos = you:get_pos_ms()

  if you:has_trait(trait_suburbanite) then
    if is_among_trees(here, pos) then
      apply_penalty(you, morale_suburbanite_wild, 12)
    else
      you:rem_morale(morale_suburbanite_wild)
    end
    if here:has_flag_at("INDOORS", pos) then
      apply_bonus(you, morale_suburbanite_indoors, 8)
    else
      you:rem_morale(morale_suburbanite_indoors)
    end
  else
    you:rem_morale(morale_suburbanite_wild)
    you:rem_morale(morale_suburbanite_indoors)
  end

  if you:has_trait(trait_chattering_plush) then
    if carries_plush(you) then
      apply_bonus(you, morale_chattering_plush, 6)
    else
      you:rem_morale(morale_chattering_plush)
    end
    if gapi.rng(1, 25) == 1 then
      local line = random_entry(chattering_plush_lines)
      if line and you:is_avatar() then gapi.add_msg(MsgType.neutral, line) end
    end
  else
    you:rem_morale(morale_chattering_plush)
  end

  if you:has_trait(trait_hemophobia) then
    local blood_tiles = count_fields_near(here, pos, 5, blood_field_ids)
    local penalty = math.min(blood_tiles * 3, max_penalty)
    apply_penalty(you, morale_hemophobia, penalty)
    if penalty > 0 then
      if not seen_hemophobia and you:is_avatar() then
        gapi.add_msg(MsgType.bad, locale.gettext("The smell of blood turns your stomach."))
        seen_hemophobia = true
      end
      if gapi.rng(1, 20) == 1 then drain_focus(you, 1, 20) end
    else
      seen_hemophobia = false
    end
  else
    you:rem_morale(morale_hemophobia)
    seen_hemophobia = false
  end

  if you:has_trait(trait_decidophobia) then
    local active_missions = you:get_active_missions()
    local mission_count = 0
    if active_missions then
      for _ in pairs(active_missions) do mission_count = mission_count + 1 end
    end
    if mission_count > 3 then
      apply_penalty(you, morale_decidophobia, math.min((mission_count - 3) * 4, max_penalty))
      if gapi.rng(1, 15) == 1 then drain_focus(you, 1, 20) end
    else
      you:rem_morale(morale_decidophobia)
    end
  else
    you:rem_morale(morale_decidophobia)
  end

  if you:has_trait(trait_minimalist) then
    local fill_ratio = inventory_fill_ratio(you)
    if fill_ratio <= 0.5 then
      apply_bonus(you, morale_minimalist, math.floor((0.5 - fill_ratio) * 20))
    elseif fill_ratio >= 0.9 then
      apply_penalty(you, morale_minimalist, math.floor((fill_ratio - 0.9) * 80))
    else
      you:rem_morale(morale_minimalist)
    end
  else
    you:rem_morale(morale_minimalist)
  end

  if you:has_trait(trait_small_talk_reflex) and has_npc_nearby(here, pos, 4) then
    if gapi.rng(1, 30) == 1 then
      local line = random_entry(small_talk_lines)
      if line and you:is_avatar() then gapi.add_msg(MsgType.neutral, line) end
      gapi.play_variant_sound("shout", "default", 8, true)
    end
  end

  if you:has_trait(trait_outgunned) then
    local hostile_count = count_hostiles_in_range(you, 10)
    if hostile_count >= 3 then
      apply_penalty(you, morale_outgunned, math.min((hostile_count - 2) * 4, max_penalty))
      if not seen_outgunned and you:is_avatar() then
        gapi.add_msg(MsgType.bad, locale.gettext("There are too many of them..."))
        seen_outgunned = true
      end
      if gapi.rng(1, 12) == 1 then drain_focus(you, 1, 20) end
    else
      you:rem_morale(morale_outgunned)
      seen_outgunned = false
    end
  else
    you:rem_morale(morale_outgunned)
    seen_outgunned = false
  end

  if you:has_trait(trait_comfort_zone) then
    local max_hp = you:get_hp_max()
    if max_hp > 0 and here:has_flag_at("INDOORS", pos) and you:get_hp() / max_hp >= 0.75 then
      apply_bonus(you, morale_comfort_zone, 10)
    else
      you:rem_morale(morale_comfort_zone)
    end
  else
    you:rem_morale(morale_comfort_zone)
  end

  if you:has_trait(trait_lone_wolf) then
    if not has_npc_nearby(here, pos, 10) and count_hostiles_in_range(you, 8) == 0 then
      apply_bonus(you, morale_lone_wolf, 8)
    else
      you:rem_morale(morale_lone_wolf)
    end
  else
    you:rem_morale(morale_lone_wolf)
  end
end

---@param params OnCreatureMeleeAttackedParams
local function on_creature_melee_attacked(params)
  ---@type Character
  local char = params.char
  if not char or not char:is_avatar() then return end
  if not char:has_trait(trait_anime_protagonist) then return end
  if not params.success then return end
  if char:get_effect_int(effect_depressants) > 3 then return end

  local max_hp = char:get_hp_max()
  if max_hp <= 0 then return end

  local ratio = char:get_hp() / max_hp
  local threshold = anime_technique_roll_threshold(ratio)
  if threshold <= 0 or gapi.rng(1, 100) > threshold then return end

  local name = random_entry(anime_technique_names)
  if name then char:shout(name .. "!", false) end
end

---@param params OnDialogueStartParams
local function on_dialogue_start(params)
  local you = gapi.get_avatar()
  if not you or not you:has_trait(trait_anime_protagonist) then return end
  if you:get_effect_int(effect_depressants) > 3 then return end

  local line = random_entry(anime_dialogue_yells)
  if line then you:shout(line, false) end
end

---@param params OnCraftFailureParams
local function on_craft_failure(params)
  local crafter = params.crafter
  if not crafter or not crafter:has_trait(trait_atelphobia) then return end
  if crafter:get_effect_int(effect_depressants) > 3 then return end

  apply_penalty(crafter, morale_atelphobia, 8)
  drain_focus(crafter, 2, 20)
  if crafter:is_avatar() then
    gapi.add_msg(
      MsgType.bad,
      locale.gettext("Your hands won't cooperate.  The ruined work feels like a verdict on you.")
    )
  end
end

---@param params OnCharacterTryMoveParams
local function apply_cowards_sprint_move_bonus(params)
  ---@type Character
  local ch = params.char
  if not ch or not ch:has_trait(trait_cowards_sprint) then return end

  local max_hp = ch:get_hp_max()
  if max_hp <= 0 then return end

  local ratio = ch:get_hp() / max_hp
  if ratio >= 0.3 then
    cowards_sprint_alert = false
    return
  end

  if ch:is_avatar() and not cowards_sprint_alert then
    gapi.add_msg(MsgType.good, locale.gettext("Fear puts springs in your step."))
    cowards_sprint_alert = true
  end

  ch:mod_moves(math.floor((0.3 - ratio) * 120))
end

---@param params OnCharacterTryMoveParams
local function apply_trail_blazer_move_cost(params)
  ---@type Character
  local ch = params.char
  if not ch or not ch:has_trait(trait_trail_blazer) then return end

  local dest = params.to
  if not dest then return end

  local here = gapi.get_map()
  if here:has_flag_at("ROAD", dest) then
    ch:mod_moves(-30)
  elseif here:is_outside(dest) and not here:has_flag_at("INDOORS", dest) then
    ch:mod_moves(20)
  end
end

---@param params OnCharacterTryMoveParams
local function on_character_try_move(params)
  ---@type Character
  local ch = params.char
  if not ch then return true end

  local here = gapi.get_map()
  local dest = params.to

  if not ch:has_trait(trait_nyctophobia) then return true end
  if ch:get_effect_int(effect_depressants) > 3 then return true end
  if params.movement_mode == CharacterMoveMode.run then return true end

  if not dest then return true end

  local threshold = nyctophobia_threshold()
  if here:ambient_light_at(dest) >= threshold then return true end

  if ch:is_avatar() then
    gapi.add_msg(
      MsgType.bad,
      locale.gettext(
        "It's so dark and scary in there!  You can't force yourself to walk into this tile.  Switch to running movement mode to move there."
      )
    )
  end
  return false
end

---@param params OnCharacterTryMoveParams
local function on_character_try_move_with_auto_mop(params)
  local allowed = on_character_try_move(params)
  if not allowed then return false end

  apply_trail_blazer_move_cost(params)
  apply_cowards_sprint_move_bonus(params)

  ---@type Character
  local ch = params.char
  if not ch then return true end
  if params.movement_mode ~= CharacterMoveMode.walk then return true end

  local dest = params.to
  if not dest then return true end

  local here = gapi.get_map()
  if not is_wielding_mop(ch) then return true end

  ch:mod_moves(-150 * auto_mop_surrounding(here, dest))
  return true
end

---@param mod table
function lua_traits.register(mod)
  trait_storage = mod.storage
  mod.on_character_try_move = on_character_try_move_with_auto_mop
  mod.on_nyctophobia_tick = tick_nyctophobia
  mod.on_morale_traits_tick = tick_morale_traits
  mod.on_clutter_intolerant_tick = tick_clutter_intolerant
  mod.on_cse_traits_fast_tick = tick_cse_traits_fast
  mod.on_cse_traits_slow_tick = tick_cse_traits_slow
  mod.on_craft_failure = on_craft_failure
  mod.on_creature_melee_attacked = on_creature_melee_attacked
  mod.on_dialogue_start = on_dialogue_start
end

return lua_traits
