#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <filesystem>

struct ArmyUnit {
    uint16_t id = 0;
    uint8_t level = 0;
    uint8_t count = 0;
};

struct HeroData {
    uint16_t exp = 0;
    uint16_t gold = 0;
    uint16_t mana = 0;
    uint16_t start_building_id = 0;
    ArmyUnit army[6] = {};
    uint16_t pos_x = 0;
    uint16_t pos_y = 0;
    uint16_t artifacts[3] = {};
    uint16_t spells[5] = {};
};

struct DecorObject {
    uint16_t x = 0;
    uint16_t y = 0;
    uint32_t id = 0;
};

struct BuildingObject {
    uint16_t x = 0;
    uint16_t y = 0;
    uint32_t id = 0;

    uint8_t image_id = 0;
    uint8_t image_type = 0;
    uint8_t b_type = 0;
    uint16_t events_array[64] = {};
    uint16_t arts_array[12] = {};
    uint16_t barracks_array[18] = {}; //структура должна быть аналогична ArmyUnit по количеству байт(2 под id, по 1 байту для стартового количесва и макс количества)
    uint16_t gold_income = 0;
    uint16_t gold_max = 0;
    uint8_t event_count = 0;
    uint8_t size_x = 0;
    uint8_t size_y = 0;
    uint16_t owner = 0;
    uint16_t tunnel = 0;
    bool has_barracks = false;
    uint8_t art_count = 0;
    uint16_t spells[6] = {};
    uint16_t garrison_array[18] = {}; //структура должна быть аналогична ArmyUnit по количеству байт(2 под id, по 1 байту для уровня и количества)
    uint8_t garrison_bonus = 0;
    uint16_t art_min_price = 0;
    uint16_t art_max_price = 0;
    uint8_t group_id = 0;
    uint8_t relations[4] = {};
    uint8_t mana_income = 0;
    uint8_t mana_max = 0;
    bool start_for_knight = false;
    bool start_for_mage = false;
    bool start_for_ranger = false;
    bool all_troops = false;
    bool garrison_only_ai = false;

    std::vector<uint8_t> raw_data; // Полный дамп 358 байт
};

struct ArmyObject {
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t id = 0;
    uint8_t model = 0;

    uint16_t tac_cost_1 = 0;
    uint8_t speed_mod = 0;
    bool exp_as_player = false;
    uint16_t gold_reserve = 0;
    uint16_t exp_recruit = 0;
    uint16_t start_building_id = 0;
    uint16_t units[20] = {}; // 2 байт id предводителя, его уровень 1 байт и 6 описаний юнитов(2 байта id, по 1 байту на их уровень и количество)
    uint16_t arts[3] = {};
    uint16_t named_hero_id = 0;
    uint8_t behavior = 0;
    bool has_patrol = false;
    uint8_t patrol_data = 0;
    bool free_units = false;
    uint8_t activity = 0;
    uint8_t group_id = 0;
    uint8_t relations[4] = {};
    uint8_t aggression = 0;
    uint8_t respawn_time = 0;
    uint8_t exp_mod = 0;
    uint8_t ship_type = 0;
    uint16_t tac_cost_2 = 0;
    bool ignore_ai = false;
    bool pursue_player = false;
    bool no_random_targets = false;
    bool no_chat = false;
    uint8_t gold_income = 0;
    bool no_buildings = false;
    uint8_t garrison_str = 0;
    bool respawn_all = false;
    uint16_t active_spell = 0;
    uint8_t behavior_model = 0;

    std::vector<uint8_t> raw_data; // Полный дамп 89 байт
};

struct LanternObject {
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t id = 0;
    uint8_t model = 0;

    uint16_t events[5] = {};
    uint16_t priorities[4] = {};
    uint16_t active_duration = 0;
    uint8_t radius = 0;
    uint8_t event_count = 0;
    uint8_t vis_radius = 0;

    std::vector<uint8_t> raw_data; // Полный дамп 99 байт
};

struct EventObject {
    uint8_t color = 0;
    uint8_t type = 0;
    uint32_t date = 0;
    uint16_t repeat_time = 0;
    uint16_t duration = 0;
    uint8_t archetype = 0;
    int16_t units_count = 0;
    int16_t army_strength = 0;
    uint16_t inactive_army_id = 0;
    uint16_t patrol_army_id = 0;
    int8_t patrol_change = 0;
    bool use_current_stats = false;
    int16_t current_level = 0;
    int16_t gold = 0;
    int16_t mana = 0;
    bool check_building_owner = false;
    uint16_t buildings[3] = {};
    uint8_t building_groups[3] = {};
    bool check_unit_in_army = false;
    uint16_t units[3] = {};
    uint16_t named_units[3] = {};
    uint8_t unit_groups[3] = {};
    bool check_artifacts = false;
    uint16_t artifacts[4] = {};
    uint8_t artifact_groups[3] = {};
    bool check_victory = false;
    uint16_t vic_army[2] = {};
    bool check_event_yes = false;
    uint16_t ev_yes[2] = {};
    bool check_event_no_happen = false;
    uint16_t ev_not[2] = {};
    bool check_army_defeated = false;
    uint16_t def_army[2] = {};
    bool check_event_no = false;
    uint16_t ev_no[2] = {};
    uint16_t meet_army_id = 0;
    uint16_t active_army_id = 0;
    bool has_question = false;
    uint16_t relative_event_id = 0;
    uint16_t relative_event_time = 0;
    uint16_t spell_id = 0;
    uint32_t image_id = 0;
    int16_t add_exp = 0;
    int16_t add_gold = 0;
    int16_t add_mana = 0;
    uint16_t spells[4] = {};
    uint16_t add_units[4] = {};
    uint16_t add_named_units[4] = {};
    uint16_t rem_units[4] = {};
    uint16_t rem_named_units[4] = {};
    uint16_t add_arts[4] = {};
    uint16_t rem_arts[4] = {};
    uint16_t act_army[2] = {};
    uint16_t deact_army = 0;
    uint16_t quest_end_event = 0;
    uint16_t event_delay = 0;
    uint16_t lanterns[4] = {};
    uint16_t dest_army_id = 0;
    uint16_t class_change = 0;
    uint16_t sub_event_id = 0;
    bool is_sub_event = false;
    bool multi_event_inv = false;
    uint16_t src_army_id = 0;
    bool move_to_hero = false;
    uint16_t show_army_id = 0;
    bool hero_1_hp = false;
    uint16_t home_army_id = 0;
    uint16_t combat_army_id = 0;
    bool no_meet_army = false;
    bool repeat_after_yes = false;
    uint32_t custom_image_size = 0;

    std::vector<uint8_t> raw_data; // Полный дамп 171 байт
};

struct BuildingTexts {
    std::string name;
    std::string owner;
    std::string desc;
};

struct ArmyTexts {
    std::string name;
    std::string leader;
    std::string desc;
};

struct EventTexts {
    std::string name;
    std::string question;
    std::string result;
};

struct MapData {
    char magic[13] = {};
    uint32_t size_x = 0;
    uint32_t size_y = 0;
    uint32_t random_seed = 0;
    uint32_t text_offset = 0;

    uint32_t surface_size = 0;
    uint32_t decor_size = 0;
    uint32_t building_size = 0;
    uint32_t army_size = 0;
    uint32_t lantern_size = 0;
    uint32_t event_size = 0;
    uint32_t start_date_min = 0;

    HeroData knight = {};
    HeroData archmage = {};
    HeroData ranger = {};

    uint16_t victory_event_id = 0;
    uint16_t defeat_event_id = 0;
    int8_t relations[16] = {};

    // Динамическая поддержка до 65535 именных юнитов
    uint16_t named_units_count = 0;
    std::vector<uint16_t> named_units_ids;
    uint8_t scenario_type = 0;

    bool keep_gold = false;
    bool gods_mercy = false;
    bool fame = false;
    bool exp_and_level = false;
    bool personal_artifacts = false;
    bool all_inventory = false;
    bool all_army = false;

    uint32_t added_image_size = 0;
    uint8_t scenario_icon_id = 0;
    uint16_t playability = 0;
    uint16_t map_version = 0;
    uint8_t quests_count = 0;

    std::vector<uint8_t> surface_tiles;
    std::vector<DecorObject> decors;
    std::vector<BuildingObject> buildings;
    std::vector<ArmyObject> armies;
    std::vector<LanternObject> lanterns;
    std::vector<EventObject> events;

    std::string map_name;
    std::string map_desc;
    std::string campaign_name;
    std::string next_scenario;

    std::vector<BuildingTexts> building_texts;
    std::vector<ArmyTexts> army_texts;
    std::vector<EventTexts> event_texts;
    std::vector<std::string> named_units_texts;
    std::vector<uint8_t> scenario_image_data;

    bool is_loaded = false;
    bool is_version_5 = false;
};

MapData ParseMapData(const std::filesystem::path& filepath);