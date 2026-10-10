#define NOMINMAX
#include "MapParser.h"
#include <fstream>
#include <cstring>
#include <bzlib.h>
#include <algorithm>
#include <vector>
#include <filesystem>
#include <utility>
#include <string>
#include <cstdint>

class BinaryReader {
    const std::vector<uint8_t>& data;
public:
    BinaryReader(const std::vector<uint8_t>& d) : data(d) {}
    template<typename T> T ReadAt(size_t one_based_offset) {
        size_t zero_based = one_based_offset - 1; T value{};
        if (zero_based + sizeof(T) <= data.size()) std::memcpy(&value, &data[zero_based], sizeof(T));
        return value;
    }
    void ReadBytesAt(size_t one_based_offset, void* dest, size_t size) {
        size_t zero_based = one_based_offset - 1;
        if (zero_based + size <= data.size()) std::memcpy(dest, &data[zero_based], size);
    }
};

static std::string ReadNullString(const std::vector<uint8_t>& buf, size_t& offset) {
    std::string s;
    while (offset < buf.size() && buf[offset] != '\0') {
        s.push_back((char)buf[offset]);
        offset++;
    }
    if (offset < buf.size()) offset++;
    return s;
}

MapData ParseMapData(const std::filesystem::path& filepath) {
    MapData h = {};
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return h;

    std::streamsize file_size = file.tellg(); file.seekg(0, std::ios::beg);
    if (file_size < 12) return h;

    std::vector<uint8_t> buffer((size_t)file_size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), file_size)) return h;

    if (buffer.size() > 15 && buffer[12] == 'B' && buffer[13] == 'Z' && buffer[14] == 'h') {
        bz_stream strm = {};
        if (BZ2_bzDecompressInit(&strm, 0, 0) == BZ_OK) {
            strm.next_in = reinterpret_cast<char*>(buffer.data() + 12); strm.avail_in = buffer.size() - 12;
            std::vector<uint8_t> uncompressed; uncompressed.resize(1024 * 1024);
            strm.next_out = reinterpret_cast<char*>(uncompressed.data()); strm.avail_out = uncompressed.size();
            while (true) {
                int ret = BZ2_bzDecompress(&strm);
                if (ret == BZ_STREAM_END) { uncompressed.resize(strm.total_out_lo32); break; }
                if (ret != BZ_OK) { BZ2_bzDecompressEnd(&strm); return h; }
                if (strm.avail_out == 0) {
                    size_t old_size = uncompressed.size(); uncompressed.resize(old_size * 2);
                    strm.next_out = reinterpret_cast<char*>(uncompressed.data() + old_size); strm.avail_out = old_size;
                }
            }
            BZ2_bzDecompressEnd(&strm); buffer = std::move(uncompressed);
        }
    }

    if (buffer.size() < 303) return h;
    BinaryReader reader(buffer);

    reader.ReadBytesAt(1, h.magic, 12); h.magic[12] = '\0';
    bool is_version_5 = (std::string(h.magic).find("V.5") != std::string::npos);
    h.is_version_5 = is_version_5;

    h.size_x = reader.ReadAt<uint32_t>(13); h.size_y = reader.ReadAt<uint32_t>(17);
    h.random_seed = reader.ReadAt<uint32_t>(21); h.text_offset = reader.ReadAt<uint32_t>(25);
    h.surface_size = reader.ReadAt<uint32_t>(29); h.decor_size = reader.ReadAt<uint32_t>(33);
    h.building_size = reader.ReadAt<uint32_t>(37); h.army_size = reader.ReadAt<uint32_t>(41);
    h.lantern_size = reader.ReadAt<uint32_t>(45); h.event_size = reader.ReadAt<uint32_t>(49);
    h.start_date_min = reader.ReadAt<uint32_t>(57);

    // ==============================================
    // БЛОК ГЛОБАЛЬНЫХ ПАРАМЕТРОВ (Читаем как V.4)
    // ==============================================
    h.knight.exp = reader.ReadAt<uint16_t>(67);
    h.knight.gold = reader.ReadAt<uint16_t>(69);
    h.knight.mana = reader.ReadAt<uint16_t>(73);
    h.knight.start_building_id = reader.ReadAt<uint8_t>(77);
    for (int i = 0; i < 6; ++i) {
        h.knight.army[i].id = reader.ReadAt<uint8_t>(80 + i * 3);
        h.knight.army[i].level = reader.ReadAt<uint8_t>(80 + i * 3 + 1);
        h.knight.army[i].count = reader.ReadAt<uint8_t>(80 + i * 3 + 2);
    }
    h.knight.pos_x = reader.ReadAt<uint16_t>(98);
    h.knight.pos_y = reader.ReadAt<uint16_t>(100);
    for (int i = 0; i < 3; ++i) h.knight.artifacts[i] = reader.ReadAt<uint8_t>(102 + i);
    for (int i = 0; i < 5; ++i) h.knight.spells[i] = reader.ReadAt<uint8_t>(105 + i);

    h.archmage.exp = reader.ReadAt<uint16_t>(117);
    h.archmage.gold = reader.ReadAt<uint16_t>(119);
    h.archmage.mana = reader.ReadAt<uint16_t>(123);
    h.archmage.start_building_id = reader.ReadAt<uint8_t>(127);
    for (int i = 0; i < 6; ++i) {
        h.archmage.army[i].id = reader.ReadAt<uint8_t>(130 + i * 3);
        h.archmage.army[i].level = reader.ReadAt<uint8_t>(130 + i * 3 + 1);
        h.archmage.army[i].count = reader.ReadAt<uint8_t>(130 + i * 3 + 2);
    }
    h.archmage.pos_x = reader.ReadAt<uint16_t>(148);
    h.archmage.pos_y = reader.ReadAt<uint16_t>(150);
    for (int i = 0; i < 3; ++i) h.archmage.artifacts[i] = reader.ReadAt<uint8_t>(152 + i);
    for (int i = 0; i < 5; ++i) h.archmage.spells[i] = reader.ReadAt<uint8_t>(155 + i);

    h.ranger.exp = reader.ReadAt<uint16_t>(167);
    h.ranger.gold = reader.ReadAt<uint16_t>(169);
    h.ranger.mana = reader.ReadAt<uint16_t>(173);
    h.ranger.start_building_id = reader.ReadAt<uint8_t>(177);
    for (int i = 0; i < 6; ++i) {
        h.ranger.army[i].id = reader.ReadAt<uint8_t>(180 + i * 3);
        h.ranger.army[i].level = reader.ReadAt<uint8_t>(180 + i * 3 + 1);
        h.ranger.army[i].count = reader.ReadAt<uint8_t>(180 + i * 3 + 2);
    }
    h.ranger.pos_x = reader.ReadAt<uint16_t>(198);
    h.ranger.pos_y = reader.ReadAt<uint16_t>(200);
    for (int i = 0; i < 3; ++i) h.ranger.artifacts[i] = reader.ReadAt<uint8_t>(202 + i);
    for (int i = 0; i < 5; ++i) h.ranger.spells[i] = reader.ReadAt<uint8_t>(205 + i);

    h.victory_event_id = reader.ReadAt<uint16_t>(211);
    h.defeat_event_id = reader.ReadAt<uint16_t>(217);
    reader.ReadBytesAt(223, h.relations, 16);

    // Старые именные юниты
    h.named_units_count = reader.ReadAt<uint8_t>(239);
    uint8_t v4_ids[32] = { 0 };
    reader.ReadBytesAt(240, v4_ids, 32);
    h.named_units_ids.clear();
    size_t count = std::min((size_t)h.named_units_count, (size_t)32);
    for (size_t i = 0; i < count; ++i) h.named_units_ids.push_back((uint16_t)v4_ids[i]);

    h.scenario_type = reader.ReadAt<uint8_t>(272);
    h.keep_gold = reader.ReadAt<uint8_t>(273) > 0;
    h.gods_mercy = reader.ReadAt<uint8_t>(274) > 0;
    h.fame = reader.ReadAt<uint8_t>(275) > 0;
    h.exp_and_level = reader.ReadAt<uint8_t>(276) > 0;
    h.personal_artifacts = reader.ReadAt<uint8_t>(277) > 0;
    h.all_inventory = reader.ReadAt<uint8_t>(278) > 0;
    h.all_army = reader.ReadAt<uint8_t>(279) > 0;

    h.added_image_size = reader.ReadAt<uint32_t>(285);
    h.scenario_icon_id = reader.ReadAt<uint8_t>(289);
    h.playability = reader.ReadAt<uint16_t>(291);
    h.map_version = reader.ReadAt<uint16_t>(293);
    h.quests_count = reader.ReadAt<uint8_t>(295);

    size_t offset = 303; // Конец стандартного заголовка V.4

    // ==============================================
    // РАСШИРЕНИЯ ГЛОБАЛЬНЫХ ПАРАМЕТРОВ (V.5)
    // ==============================================
    if (is_version_5) {
        auto ext_read16 = [&](size_t off) { return reader.ReadAt<uint16_t>(off + 1); };

        h.knight.start_building_id = ext_read16(offset); offset += 2;
        for (int i = 0; i < 6; ++i) { h.knight.army[i].id = ext_read16(offset); offset += 2; }
        for (int i = 0; i < 3; ++i) { h.knight.artifacts[i] = ext_read16(offset); offset += 2; }
        for (int i = 0; i < 5; ++i) { h.knight.spells[i] = ext_read16(offset); offset += 2; }

        h.archmage.start_building_id = ext_read16(offset); offset += 2;
        for (int i = 0; i < 6; ++i) { h.archmage.army[i].id = ext_read16(offset); offset += 2; }
        for (int i = 0; i < 3; ++i) { h.archmage.artifacts[i] = ext_read16(offset); offset += 2; }
        for (int i = 0; i < 5; ++i) { h.archmage.spells[i] = ext_read16(offset); offset += 2; }

        h.ranger.start_building_id = ext_read16(offset); offset += 2;
        for (int i = 0; i < 6; ++i) { h.ranger.army[i].id = ext_read16(offset); offset += 2; }
        for (int i = 0; i < 3; ++i) { h.ranger.artifacts[i] = ext_read16(offset); offset += 2; }
        for (int i = 0; i < 5; ++i) { h.ranger.spells[i] = ext_read16(offset); offset += 2; }

        h.named_units_count = ext_read16(offset); offset += 2;
        h.named_units_ids.clear();
        for (int i = 0; i < h.named_units_count; ++i) {
            h.named_units_ids.push_back(ext_read16(offset));
            offset += 2;
        }
    }

    auto read8 = [&](size_t o) { return buffer[offset + o]; };
    auto read16 = [&](size_t o) { return (uint16_t)(buffer[offset + o] | (buffer[offset + o + 1] << 8)); };
    auto read16s = [&](size_t o) { return (int16_t)read16(o); };
    auto read32 = [&](size_t o) { return (uint32_t)(buffer[offset + o] | (buffer[offset + o + 1] << 8) | (buffer[offset + o + 2] << 16) | (buffer[offset + o + 3] << 24)); };

    // 1. Поверхность
    size_t end_offset = offset + h.surface_size;
    end_offset = std::min(end_offset, buffer.size());
    while (offset < end_offset && offset + 1 < buffer.size()) {
        uint8_t tile_id = buffer[offset]; uint8_t run_length_encoded = buffer[offset + 1];
        int run_length = run_length_encoded + 1;
        for (int i = 0; i < run_length; ++i) h.surface_tiles.push_back(tile_id);
        offset += 2;
    }
    offset = end_offset;
    size_t expected_tiles = (size_t)h.size_x * h.size_y;
    if (h.surface_tiles.size() < expected_tiles) h.surface_tiles.resize(expected_tiles, 0);

    // 2. Декор
    size_t decor_end = offset + h.decor_size;
    decor_end = std::min(decor_end, buffer.size());
    while (offset + 5 < decor_end) {
        h.decors.push_back({ read16(0), read16(2), (uint32_t)read16(4) });
        offset += 6;
    }
    offset = decor_end;

    // 3. Строения
    size_t building_end = offset + h.building_size;
    building_end = std::min(building_end, buffer.size());
    const size_t BLD_V4_SIZE = 358;
    const size_t BLD_EXT_SIZE = 2 + 2 + 24 + 12 + 12 + 12; // 64 байта
    size_t bld_struct_size = is_version_5 ? (BLD_V4_SIZE + BLD_EXT_SIZE) : BLD_V4_SIZE;

    while (offset + (BLD_V4_SIZE - 1) < building_end) {
        BuildingObject b;
        b.x = read16(0); b.y = read16(2); b.id = read16(4);
        b.image_id = read8(4); b.image_type = read8(5); b.b_type = read8(6);
        std::memcpy(b.events_array, &buffer[offset + 8], 128);
        for (int i = 0; i < 12; ++i) b.arts_array[i] = read8(136 + i);
        for (int i = 0; i < 6; ++i) {
            b.barracks_array[i * 3] = read8(264 + i * 3);
            b.barracks_array[i * 3 + 1] = read8(264 + i * 3 + 1);
            b.barracks_array[i * 3 + 2] = read8(264 + i * 3 + 2);
        }
        b.gold_income = read16(282); b.gold_max = read16(284);
        b.event_count = read8(288); b.size_x = read8(289); b.size_y = read8(290);
        b.owner = read8(292); b.tunnel = read8(293); b.has_barracks = read8(294) > 0;
        b.art_count = read8(295);
        for (int i = 0; i < 6; ++i) b.spells[i] = read8(308 + i);
        for (int i = 0; i < 6; ++i) {
            b.garrison_array[i * 3] = read8(314 + i * 3);
            b.garrison_array[i * 3 + 1] = read8(314 + i * 3 + 1);
            b.garrison_array[i * 3 + 2] = read8(314 + i * 3 + 2);
        }
        b.garrison_bonus = read8(332);
        b.art_min_price = read16(333); b.art_max_price = read16(335);
        b.group_id = read8(337);
        std::memcpy(b.relations, &buffer[offset + 338], 4);
        b.mana_income = read8(350); b.mana_max = read8(351);
        b.start_for_knight = read8(353) > 0; b.start_for_mage = read8(354) > 0;
        b.start_for_ranger = read8(355) > 0; b.all_troops = read8(356) > 0;
        b.garrison_only_ai = read8(357) > 0;

        if (is_version_5) {
            size_t e_off = BLD_V4_SIZE;
            b.owner = read16(e_off); e_off += 2;
            b.tunnel = read16(e_off); e_off += 2;
            for (int i = 0; i < 12; ++i) { b.arts_array[i] = read16(e_off); e_off += 2; }
            for (int i = 0; i < 6; ++i) { b.barracks_array[i * 3] = read16(e_off); e_off += 2; }
            for (int i = 0; i < 6; ++i) { b.spells[i] = read16(e_off); e_off += 2; }
            for (int i = 0; i < 6; ++i) { b.garrison_array[i * 3] = read16(e_off); e_off += 2; }
        }

        b.raw_data.assign(buffer.begin() + offset, buffer.begin() + offset + bld_struct_size);
        h.buildings.push_back(b);
        offset += bld_struct_size;
    }
    offset = building_end;

    // 4. Армии
    size_t army_end = offset + h.army_size;
    army_end = std::min(army_end, buffer.size());
    const size_t ARMY_V4_SIZE = 89;
    const size_t ARMY_EXT_SIZE = 2 + 2 + 2 + 12 + 6 + 2 + 2; // 28 байт
    size_t army_struct_size = is_version_5 ? (ARMY_V4_SIZE + ARMY_EXT_SIZE) : ARMY_V4_SIZE;

    while (offset + (ARMY_V4_SIZE - 1) < army_end) {
        ArmyObject a;
        a.x = read16(0); a.y = read16(2); a.id = read8(4); a.model = read8(5);
        a.tac_cost_1 = read16(6); a.speed_mod = read8(13); a.exp_as_player = read8(14) > 0;
        a.gold_reserve = read16(17); a.exp_recruit = read16(19); a.start_building_id = read8(25);
        a.units[0] = read8(26); a.units[1] = read8(27);
        for (int i = 0; i < 6; ++i) {
            a.units[2 + i * 3] = read8(28 + i * 3);
            a.units[2 + i * 3 + 1] = read8(28 + i * 3 + 1);
            a.units[2 + i * 3 + 2] = read8(28 + i * 3 + 2);
        }
        for (int i = 0; i < 3; ++i) a.arts[i] = read8(50 + i);
        a.named_hero_id = read8(58); a.behavior = read8(59); a.has_patrol = read8(60) > 0;
        a.patrol_data = read8(61); a.free_units = read8(62) > 0; a.activity = read8(63);
        a.group_id = read8(64); std::memcpy(a.relations, &buffer[offset + 65], 4);
        a.aggression = read8(69); a.respawn_time = read8(70); a.exp_mod = read8(71);
        a.ship_type = read8(72); a.tac_cost_2 = read16(74); a.ignore_ai = read8(76) > 0;
        a.pursue_player = read8(77) > 0; a.no_random_targets = read8(78) > 0;
        a.no_chat = read8(79) > 0; a.gold_income = read8(80); a.no_buildings = read8(81) > 0;
        a.garrison_str = read8(82); a.respawn_all = read8(83) > 0; a.active_spell = read8(84);
        a.behavior_model = read8(85);

        if (is_version_5) {
            size_t e_off = ARMY_V4_SIZE;
            a.id = read16(e_off); e_off += 2;
            a.start_building_id = read16(e_off); e_off += 2;
            a.units[0] = read16(e_off); e_off += 2; // ID героя
            for (int i = 0; i < 6; ++i) { a.units[2 + i * 3] = read16(e_off); e_off += 2; }
            for (int i = 0; i < 3; ++i) { a.arts[i] = read16(e_off); e_off += 2; }
            a.named_hero_id = read16(e_off); e_off += 2;
            a.active_spell = read16(e_off); e_off += 2;
        }

        a.raw_data.assign(buffer.begin() + offset, buffer.begin() + offset + army_struct_size);
        h.armies.push_back(a);
        offset += army_struct_size;
    }
    offset = army_end;

    // 5. Фонарики и мины
    size_t lantern_end = offset + h.lantern_size;
    lantern_end = std::min(lantern_end, buffer.size());
    const size_t LANT_V4_SIZE = 99;
    const size_t LANT_EXT_SIZE = 2; // 2 байта
    size_t lant_struct_size = is_version_5 ? (LANT_V4_SIZE + LANT_EXT_SIZE) : LANT_V4_SIZE;

    while (offset + (LANT_V4_SIZE - 1) < lantern_end) {
        LanternObject l;
        l.x = read16(0); l.y = read16(2); l.id = read8(4); l.model = read8(5);
        for (int i = 0; i < 5; ++i) l.events[i] = read16(8 + i * 2);
        for (int i = 0; i < 4; ++i) l.priorities[i] = read16(28 + i * 2);
        l.active_duration = read16(36); l.radius = read8(38); l.event_count = read8(39);
        l.vis_radius = read8(40);

        if (is_version_5) {
            l.id = read16(LANT_V4_SIZE);
        }

        l.raw_data.assign(buffer.begin() + offset, buffer.begin() + offset + lant_struct_size);
        h.lanterns.push_back(l);
        offset += lant_struct_size;
    }
    offset = lantern_end;

    // 6. События
    size_t event_end = offset + h.event_size;
    event_end = std::min(event_end, buffer.size());
    const size_t EVT_V4_SIZE = 171;
    const size_t EVT_EXT_SIZE = 110; // 55 IDs * 2 байт
    size_t evt_struct_size = is_version_5 ? (EVT_V4_SIZE + EVT_EXT_SIZE) : EVT_V4_SIZE;

    while (offset + (EVT_V4_SIZE - 1) < event_end) {
        EventObject e;
        e.color = read8(0); e.type = read8(1); e.date = read32(2);
        e.repeat_time = read16(6); e.duration = read16(8); e.archetype = read8(10);
        e.units_count = read16s(11); e.army_strength = read16s(13); e.inactive_army_id = read8(15);
        e.patrol_army_id = read8(16); e.patrol_change = (int8_t)read8(17); e.use_current_stats = read8(18) > 0;
        e.current_level = read16s(19); e.gold = read16s(21); e.mana = read16s(25);
        e.check_building_owner = read8(29) > 0;
        for (int i = 0; i < 3; ++i) e.buildings[i] = read8(30 + i);
        std::memcpy(e.building_groups, &buffer[offset + 33], 3);
        e.check_unit_in_army = read8(36) > 0;
        for (int i = 0; i < 3; ++i) e.units[i] = read8(37 + i);
        for (int i = 0; i < 3; ++i) e.named_units[i] = read8(40 + i);
        std::memcpy(e.unit_groups, &buffer[offset + 43], 3);
        e.check_artifacts = read8(46) > 0;
        for (int i = 0; i < 4; ++i) e.artifacts[i] = read8(47 + i);
        std::memcpy(e.artifact_groups, &buffer[offset + 50], 3);
        e.check_victory = read8(53) > 0;
        for (int i = 0; i < 2; ++i) e.vic_army[i] = read8(54 + i);
        e.check_event_yes = read8(56) > 0; e.ev_yes[0] = read16(57); e.ev_yes[1] = read16(59);
        e.check_event_no_happen = read8(61) > 0; e.ev_not[0] = read16(62); e.ev_not[1] = read16(64);
        e.check_army_defeated = read8(66) > 0;
        for (int i = 0; i < 2; ++i) e.def_army[i] = read8(67 + i);
        e.check_event_no = read8(69) > 0; e.ev_no[0] = read16(70); e.ev_no[1] = read16(72);
        e.meet_army_id = read8(74); e.active_army_id = read8(75); e.has_question = read8(76) > 0;
        e.relative_event_id = read16(77); e.relative_event_time = read16(79); e.spell_id = read8(81);
        e.image_id = read32(82); e.add_exp = read16s(86); e.add_gold = read16s(88); e.add_mana = read16s(90);
        for (int i = 0; i < 4; ++i) e.spells[i] = read8(92 + i);
        for (int i = 0; i < 4; ++i) e.add_units[i] = read8(96 + i);
        for (int i = 0; i < 4; ++i) e.add_named_units[i] = read8(100 + i);
        for (int i = 0; i < 4; ++i) e.rem_units[i] = read8(104 + i);
        for (int i = 0; i < 4; ++i) e.rem_named_units[i] = read8(108 + i);
        for (int i = 0; i < 4; ++i) e.add_arts[i] = read8(112 + i);
        for (int i = 0; i < 4; ++i) e.rem_arts[i] = read8(116 + i);
        for (int i = 0; i < 2; ++i) e.act_army[i] = read8(120 + i);
        e.deact_army = read8(122); e.quest_end_event = read16(123); e.event_delay = read16(125);
        for (int i = 0; i < 4; ++i) e.lanterns[i] = read16(127 + i * 2);
        e.dest_army_id = read8(135); e.class_change = read8(136); e.sub_event_id = read16(137);
        e.is_sub_event = read8(139) > 0; e.multi_event_inv = read8(140) > 0; e.src_army_id = read8(141);
        e.move_to_hero = read8(142) > 0; e.show_army_id = read8(143); e.hero_1_hp = read8(144) > 0;
        e.home_army_id = read8(145); e.combat_army_id = read8(146); e.no_meet_army = read8(147) > 0;
        e.repeat_after_yes = read8(148) > 0; e.custom_image_size = read32(149);

        if (is_version_5) {
            size_t e_off = EVT_V4_SIZE;
            e.inactive_army_id = read16(e_off); e_off += 2;
            e.patrol_army_id = read16(e_off); e_off += 2;
            for (int i = 0; i < 3; ++i) { e.buildings[i] = read16(e_off); e_off += 2; }
            for (int i = 0; i < 3; ++i) { e.units[i] = read16(e_off); e_off += 2; }
            for (int i = 0; i < 3; ++i) { e.named_units[i] = read16(e_off); e_off += 2; }
            for (int i = 0; i < 4; ++i) { e.artifacts[i] = read16(e_off); e_off += 2; }
            for (int i = 0; i < 2; ++i) { e.vic_army[i] = read16(e_off); e_off += 2; }
            for (int i = 0; i < 2; ++i) { e.def_army[i] = read16(e_off); e_off += 2; }
            e.meet_army_id = read16(e_off); e_off += 2;
            e.active_army_id = read16(e_off); e_off += 2;
            e.spell_id = read16(e_off); e_off += 2;
            for (int i = 0; i < 4; ++i) { e.spells[i] = read16(e_off); e_off += 2; }
            for (int i = 0; i < 4; ++i) { e.add_units[i] = read16(e_off); e_off += 2; }
            for (int i = 0; i < 4; ++i) { e.add_named_units[i] = read16(e_off); e_off += 2; }
            for (int i = 0; i < 4; ++i) { e.rem_units[i] = read16(e_off); e_off += 2; }
            for (int i = 0; i < 4; ++i) { e.rem_named_units[i] = read16(e_off); e_off += 2; }
            for (int i = 0; i < 4; ++i) { e.add_arts[i] = read16(e_off); e_off += 2; }
            for (int i = 0; i < 4; ++i) { e.rem_arts[i] = read16(e_off); e_off += 2; }
            for (int i = 0; i < 2; ++i) { e.act_army[i] = read16(e_off); e_off += 2; }
            e.deact_army = read16(e_off); e_off += 2;
            e.dest_army_id = read16(e_off); e_off += 2;
            e.class_change = read16(e_off); e_off += 2;
            e.src_army_id = read16(e_off); e_off += 2;
            e.show_army_id = read16(e_off); e_off += 2;
            e.home_army_id = read16(e_off); e_off += 2;
            e.combat_army_id = read16(e_off); e_off += 2;
        }

        e.raw_data.assign(buffer.begin() + offset, buffer.begin() + offset + evt_struct_size);
        h.events.push_back(e);
        offset += evt_struct_size;
    }

    // 7. Текстовые данные сценария
    if (h.text_offset > 0 && h.text_offset <= buffer.size()) {
        size_t t_off = h.text_offset;

        h.map_name = ReadNullString(buffer, t_off);
        h.map_desc = ReadNullString(buffer, t_off);
        h.campaign_name = ReadNullString(buffer, t_off);
        h.next_scenario = ReadNullString(buffer, t_off);

        for (size_t i = 0; i < h.buildings.size(); ++i) {
            BuildingTexts bt;
            bt.name = ReadNullString(buffer, t_off);
            bt.owner = ReadNullString(buffer, t_off);
            bt.desc = ReadNullString(buffer, t_off);
            h.building_texts.push_back(bt);
        }

        for (size_t i = 0; i < h.armies.size(); ++i) {
            ArmyTexts at;
            at.name = ReadNullString(buffer, t_off);
            at.leader = ReadNullString(buffer, t_off);
            at.desc = ReadNullString(buffer, t_off);
            h.army_texts.push_back(at);
        }

        for (size_t i = 0; i < h.events.size(); ++i) {
            EventTexts et;
            et.name = ReadNullString(buffer, t_off);
            et.question = ReadNullString(buffer, t_off);
            et.result = ReadNullString(buffer, t_off);
            h.event_texts.push_back(et);
        }

        for (size_t i = 0; i < h.named_units_count; ++i) {
            h.named_units_texts.push_back(ReadNullString(buffer, t_off));
        }

        if (h.named_units_ids.size() < h.named_units_texts.size()) {
            h.named_units_ids.resize(h.named_units_texts.size(), 1);
        }
        // Картинка к сценарию (LIT файл, приплюсованный в конец)
        if (h.added_image_size > 0 && t_off < buffer.size()) {
            size_t available_bytes = buffer.size() - t_off;
            size_t bytes_to_read = std::min(static_cast<size_t>(h.added_image_size), available_bytes);
            h.scenario_image_data.assign(buffer.begin() + t_off, buffer.begin() + t_off + bytes_to_read);
        }
    }

    h.is_loaded = true;
    return h;
}