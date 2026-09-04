local missing = gapi.resolve_image_path("no_such_cse_image_zzz.png")
assert(missing == nil)

local found = gapi.resolve_image_path("demo_photo.png")
assert(found ~= nil)
assert(string.find(found, "demo_photo.png", 1, true) ~= nil)

assert(gapi.show_image("demo_photo.png") == true)
assert(gapi.show_image("no_such_cse_image_zzz.png") == false)
assert(gapi.show_image({ image = "demo_photo.png", mode = "native" }) == true)

test_data["ok"] = true
