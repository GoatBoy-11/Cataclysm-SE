#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "iuse.h"
#include "point.h"
#include "translations.h"

class JsonObject;
class player;
class item;
struct iteminfo;

enum class image_display_mode {
    native,
    fullscreen,
    scale
};

struct show_image_options {
    std::string image;
    translation caption;
    image_display_mode mode = image_display_mode::native;
    double scale = 1.0;
};

struct image_dest_rect {
    point pos = point_zero;
    point size = point_zero;
    auto operator==( const image_dest_rect & ) const -> bool = default; // *NOPAD*
};

struct image_dest_rect_options {
    point image_size = point_zero;
    point screen_size = point_zero;
    image_display_mode mode = image_display_mode::native;
    double scale = 1.0;
};

/// Parse `"native"`, `"fullscreen"`, or `"scale"`.
auto image_display_mode_from_string( const std::string &str ) -> std::optional<image_display_mode>;

/// Roots searched for images: active mods' `images/` and `gfx/images/`, then `gfx/images/`.
auto get_image_search_roots() -> std::vector<std::string>;

/// Resolve `image` against explicit roots. Rejects `..` and paths that escape a root.
auto resolve_image_path_in_roots( const std::string &image,
                                  const std::vector<std::string> &roots ) -> std::optional<std::string>;

/// Resolve `image` against `get_image_search_roots()`.
auto resolve_image_path( const std::string &image ) -> std::optional<std::string>;

/// Compute a centered destination rectangle. Native uses 1:1 pixels; fullscreen letterboxes;
/// scale multiplies native size by `scale`.
auto get_image_dest_rect( const image_dest_rect_options &opts ) -> std::optional<image_dest_rect>;

/// Show a modal PNG overlay. Returns false if the file cannot be resolved.
/// In `test_mode`, a resolved path returns true without opening the UI.
auto show_image( const show_image_options &opts ) -> bool;

class show_image_actor : public iuse_actor
{
    public:
        std::string image;
        translation caption;
        image_display_mode mode = image_display_mode::native;
        double scale = 1.0;

        show_image_actor( const std::string &type = "show_image" ) : iuse_actor( type ) {}

        ~show_image_actor() override = default;
        void load( const JsonObject &jo ) override;
        int use( player &p, item &it, bool t, const tripoint_bub_ms &pos ) const override;
        std::unique_ptr<iuse_actor> clone() const override;
        void info( const item &, std::vector<iteminfo> &dump ) const override;
};
