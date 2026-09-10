#pragma once
#ifndef CATA_SRC_CLIMBING_H
#define CATA_SRC_CLIMBING_H

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "coordinates.h"
#include "point.h"
#include "translations.h"
#include "type_id.h"

class Character;
class JsonObject;

/**
 * A "Climbing Aid" is any trait, mutation, tool, furniture terrain, technique
 * or other affordance that facilitates movement up, down or across the environment.
 * Availability of climbing aids is subject to various conditions; exactly one is used at a time.
 */
class climbing_aid
{
    public:
        enum class category {
            special = 0,
            ter_furn,
            veh,
            item,
            character,
            trait,
            last
        };

        struct condition {
            category cat = category::special; // Called "type" in json
            std::string flag;
            int uses_item = 0;
            int range = 1;

            auto category_string() const noexcept -> std::string;

            auto deserialize( const JsonObject &jo ) -> void;
        };

        struct climb_cost {
            int pain = 0;
            int damage = 0;
            int kcal = 0;
            int thirst = 0;

            auto deserialize( const JsonObject &jo ) -> void;
        };

        static auto load_climbing_aid( const JsonObject &jo, const std::string &src ) -> void;
        static auto finalize_all() -> void;
        static auto check_consistency() -> void;
        static auto reset() -> void;
        auto load( const JsonObject &jo, std::string_view ) -> void;
        auto finalize() -> void;

        using lookup = std::vector<std::unordered_multimap<std::string, const climbing_aid *>>;
        using condition_list = std::vector<condition>;
        using aid_list = std::vector<const climbing_aid *>;

        static auto get_all() -> const std::vector<climbing_aid> &;
        bool was_loaded = false;

        static auto detect_conditions( Character &you,
                                       const tripoint_bub_ms &examp ) -> condition_list;

        static auto list( const condition_list &cond ) -> aid_list;
        static auto list_all( const condition_list &cond ) -> aid_list;

        static auto get_default() -> const climbing_aid &;
        static auto get_safest( const condition_list &cond, bool no_deploy = true ) -> const climbing_aid &;

        /**
         * Scans downwards from a tile to assess what lies between it and solid ground.
         */
        class fall_scan
        {
            public:
                explicit fall_scan( const tripoint_bub_ms &examp );

                tripoint_bub_ms examp;
                int height = 0;
                int height_until_creature = 0;
                int height_until_furniture = 0;
                int height_until_vehicle = 0;

                explicit operator bool() const {
                    return height != 0;
                }

                auto pos_top() const -> tripoint_bub_ms {
                    return examp;
                }
                auto pos_bottom() const -> tripoint_bub_ms {
                    tripoint_bub_ms ret = examp;
                    ret.z() -= height;
                    return ret;
                }
                auto pos_just_below() const -> tripoint_bub_ms {
                    tripoint_bub_ms ret = examp;
                    ret.z() -= 1;
                    return ret;
                }
                auto pos_furniture_or_floor() const -> tripoint_bub_ms {
                    tripoint_bub_ms ret = examp;
                    ret.z() -= std::min( height, height_until_furniture + 1 );
                    return ret;
                }

                auto furn_just_below() const -> bool {
                    return height > 0 && height_until_furniture == 0;
                }
                auto furn_below() const -> bool {
                    return height > 0 && height_until_furniture != height;
                }
                auto veh_just_below() const -> bool {
                    return height > 0 && height_until_vehicle == 0;
                }
                auto veh_below() const -> bool {
                    return height > 0 && height_until_vehicle != height;
                }
                auto crea_just_below() const -> bool {
                    return height > 0 && height_until_creature == 0;
                }
                auto crea_below() const -> bool {
                    return height > 0 && height_until_creature != height;
                }
        };

        climbing_aid_id id;
        std::vector<std::pair<climbing_aid_id, mod_id>> src;

        condition base_condition;

        int slip_chance_mod = 0;

        struct down_t {
            bool was_loaded = false;

            int max_height = 1;
            int easy_climb_back_up = 0;
            bool allow_remaining_height = true;

            translation menu_text;
            translation menu_cant;
            int menu_hotkey = 0;

            translation confirm_text;

            translation msg_before;
            translation msg_after;

            climb_cost cost;
            furn_str_id deploy_furn;

            auto enabled() const noexcept -> bool {
                return max_height >= 0;
            }

            auto deploys_furniture() const noexcept -> bool {
                return !deploy_furn.is_empty();
            }

            auto deserialize( const JsonObject &jo ) -> void;
        } down;

};

#endif // CATA_SRC_CLIMBING_H
