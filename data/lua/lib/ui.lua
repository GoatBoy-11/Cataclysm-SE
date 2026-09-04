local ui = {}

---@param str string
---@param color Color?
ui.query_any_key = function(str, color)
  local popup = QueryPopup.new()
  popup:message(str)
  if color then popup:message_color(color) end
  popup:allow_any_key(true)
  popup:query()
end

---@param str string
---@return boolean
ui.query_yn = function(str)
  local popup = QueryPopup.new()
  popup:message(str)
  return popup:query_yn() == "YES"
end

---@param str string
---@param color Color?
ui.popup = function(str, color) ui.query_any_key(str, color) end

---@class ShowImageOptions
---@field image string
---@field caption string?
---@field mode "native"|"fullscreen"|"scale"?
---@field scale number?

---@param image string|ShowImageOptions
---@return boolean
ui.show_image = function(image) return gapi.show_image(image) end

---@param image string
---@return string?
ui.resolve_image_path = function(image) return gapi.resolve_image_path(image) end

return ui
