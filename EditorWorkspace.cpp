#include "EditorWorkspace.h"
#include "ArmySettings.h"
#include "LanternSettings.h"
#include "NamedUnitsSettings.h"
#include "UnitSettings.h"
#include "ArtifactSettings.h"
#include "ScenarioSettings.h"
#include "BuildingSettings.h"
#include "EventSettings.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <StringUtils.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "MapParser.h"
#include "ObjectManager.h"
#include "imgui.h"
#include "TextureManager.h"
#include <unordered_map>

ImU32 EditorWorkspace::GetTileColor(uint8_t tile_id) {
    switch (tile_id) {
    case 0: return IM_COL32(100, 200, 255, 255); case 1: return IM_COL32(0, 100, 255, 255);
    case 2: return IM_COL32(0, 0, 150, 255);     case 3: return IM_COL32(255, 100, 0, 255);
    case 4: return IM_COL32(150, 150, 150, 255); case 5: return IM_COL32(150, 255, 150, 255);
    case 6: return IM_COL32(50, 200, 50, 255);   case 7: return IM_COL32(180, 200, 50, 255);
    case 8: return IM_COL32(50, 100, 50, 255);   case 9: return IM_COL32(30, 60, 30, 255);
    case 10:return IM_COL32(255, 255, 100, 255); case 11:return IM_COL32(100, 80, 80, 255);
    case 12:return IM_COL32(100, 100, 100, 255); case 13:return IM_COL32(200, 180, 140, 255);
    case 14:return IM_COL32(255, 255, 255, 255); case 15:return IM_COL32(200, 240, 255, 255);
    default:return IM_COL32(255, 0, 255, 255);
    }
}

GameObject* EditorWorkspace::GetObjectById(uint32_t full_id, ObjType expected_type) {
    if (!obj_manager) return nullptr;

    // Быстрый хэш-кэш для мгновенного поиска за O(1)
    static const std::vector<GameObject>* last_obj_ptr = nullptr;
    static std::unordered_map<uint64_t, GameObject*> fast_cache;

    if (last_obj_ptr != &obj_manager->objects) {
        fast_cache.clear();
        for (auto& obj : obj_manager->objects) {
            if (!obj.isEmpty) {
                uint64_t key = ((uint64_t)obj.header.type << 32) | (obj.header.groupId * 256 + obj.header.objectId);
                fast_cache[key] = &obj;
            }
        }
        last_obj_ptr = &obj_manager->objects;
    }

    uint64_t search_key = ((uint64_t)expected_type << 32) | full_id;
    auto it = fast_cache.find(search_key);
    return (it != fast_cache.end()) ? it->second : nullptr;
}

bool EditorWorkspace::IsLandAt(int x, int y) {
    int map_w = loaded_map.is_loaded ? loaded_map.size_x : 50;
    int map_h = loaded_map.is_loaded ? loaded_map.size_y : 50;
    if (map_w <= 0) map_w = 50;
    if (map_h <= 0) map_h = 50;

    if (x < 0 || x >= map_w || y < 0 || y >= map_h) return true;

    for (const auto& b : map_buildings) {
        GameObject* obj = GetObjectById(b.id, ObjType::Functional);
        int w = obj ? obj->header.mapCellsW : 1;
        int h = obj ? obj->header.mapCellsH : 1;
        // Явное приведение к int для избежания underflow-багов беззнаковых типов
        int bx = (int)b.x;
        int by = (int)b.y;
        if (x >= b.x - w + 1 && x <= b.x && y >= b.y - h + 1 && y <= b.y) return true;
    }
    for (const auto& d : map_decors) {
        uint32_t gid = d.id / 256;
        uint32_t oid = d.id % 256;
        int obj_size = 1;
        if (gid <= 8) obj_size = std::max(1, (int)(oid / 10));
        // Явное приведение к int
        int dx = (int)d.x;
        int dy = (int)d.y;

        if (x >= d.x - obj_size + 1 && x <= d.x && y >= d.y - obj_size + 1 && y <= d.y) return true;
    }

    size_t idx = (size_t)y * map_w + x;
    if (idx < loaded_map.surface_tiles.size()) {
        uint8_t tile = loaded_map.surface_tiles[idx];
        return (tile > 2);
    }
    return true;
}

ImVec2 EditorWorkspace::GetBuildingVisualOffset(uint32_t group_id, uint32_t obj_id, int cells_w, int cells_h) {
    float shift_x = 0.0f;
    float shift_y = 0.0f;

    if (group_id == 13) {
        // Мосты (Каменные), размер 1x1.
        if      (obj_id == 0) { shift_x =  -2.0f; shift_y = -27.0f; }
        else if (obj_id == 1) { shift_x = -11.0f; shift_y = -22.0f; }
        else if (obj_id == 2) { shift_x =   0.0f; shift_y = -22.0f; }
        else if (obj_id == 3) { shift_x = -11.0f; shift_y = -22.0f; }
        else if (obj_id == 4) { shift_x = -11.0f; shift_y = -26.0f; }
        else if (obj_id == 5) { shift_x =  -1.0f; shift_y = -22.0f; }
        else if (obj_id == 6) { shift_x = -13.0f; shift_y = -23.0f; }
        else if (obj_id == 7) { shift_x = -18.0f; shift_y = -27.0f; }
        else if (obj_id == 8) { shift_x = -22.0f; shift_y = -22.0f; }
        else                  { shift_x = -16.0f; shift_y = -24.0f; }
    }
    else if (group_id == 14) {
        // Мосты (Деревянные), размер 1x1.
        if      (obj_id == 0) { shift_x = -11.0f; shift_y = -24.0f; }
        else if (obj_id == 1) { shift_x = -10.0f; shift_y = -25.0f; }
        else if (obj_id == 2) { shift_x = -11.0f; shift_y = -24.0f; }
        else if (obj_id == 3) { shift_x = -10.0f; shift_y = -24.0f; }
        else if (obj_id == 4) { shift_x =  -8.0f; shift_y = -22.0f; }
        else if (obj_id == 5) { shift_x =  -9.0f; shift_y = -27.0f; }
        else if (obj_id == 6) { shift_x =  -9.0f; shift_y = -23.0f; }
        else if (obj_id == 7) { shift_x =  -9.0f; shift_y = -32.0f; }
        else if (obj_id == 8) { shift_x = -35.0f; shift_y = -21.0f; }
        else                  { shift_x = -36.0f; shift_y = -34.0f; }
    }
    else {
        // Обычные строения: сдвигаем вверх, если строение широкое
        if (cells_w > cells_h) {
            shift_y = 11.0f;
        }
    }

    return ImVec2(shift_x, shift_y);
}

ImVec2 EditorWorkspace::GetTreeScatterOffset(int gridX, int gridY, uint32_t full_id) {
    // 1. Используем DOUBLE. Иначе теряется точность дроби при умножении на 1 млн,
    // что ломает Seed (игра считает это в 80-bit FPU).
    double val_sin = static_cast<double>((gridY * 800) + gridX);
    double sin_part = std::sin(val_sin) * 1000000.0;

    double val_cos = static_cast<double>((gridX * 600) + gridY);
    double cos_part = std::cos(val_cos) * 10000.0;

    // 2. Как в ASM (fistp qword), кастуем сначала в 64-bit int, чтобы сохранить знак
    double combined_hash = cos_part + sin_part + static_cast<double>(full_id);
    uint32_t seed = static_cast<uint32_t>(static_cast<int64_t>(combined_hash));

    // Оригинальный генератор игры
    auto GameRand = [&seed](int max_value) -> int {
        if (max_value == 0) return 0;
        seed = seed * 214013 + 2531011;
        return ((seed >> 16) & 0x7FFF) % max_value;
        };

    // 3. Шахматный порядок смещений
    int outOffsetX = 0;
    if ((gridY & 1) != 0) {
        outOffsetX = GameRand(16) + 4;
    }
    else {
        outOffsetX = 28 - GameRand(16);
    }
    int outOffsetY = GameRand(11);

    // Возвращаем "чистый" разброс. Минусы применим при расчете позиции в DrawMapLayers.
    return ImVec2(static_cast<float>(outOffsetX), static_cast<float>(outOffsetY));
}

EditorTexture EditorWorkspace::GetEditorTextureForArmy(const ArmyObject* army, uint8_t model, int x, int y, bool is_army) {
    if (!tex_manager) return EditorTexture();

    // 1. Проверяем параметр неактивности армии (activity != 0 означает "Армия не активна")
    if (is_army && army && army->activity != 0) {
        return tex_manager->tex_editor[6]; // E1 (Лагерь)
    }

    // 2. Активная армия на воде — выбор корабля
    if (is_army && loaded_map.is_loaded && !IsLandAt(x, y)) {
        int ship_type = army ? army->ship_type : 0;
        if (ship_type == 1) return tex_manager->tex_editor[17]; // SHIP1
        if (ship_type == 2) return tex_manager->tex_editor[18]; // SHIP2
        if (ship_type == 3) return tex_manager->tex_editor[19]; // SHIP3
        return tex_manager->tex_editor[9];                      // ship (по умолчанию 0)
    }

    // 3. Активная армия на суше — выбор по модели
    switch (model) {
    case 1:  return tex_manager->tex_editor[0];  // GUARD (Рыцарь)
    case 2:  return tex_manager->tex_editor[1];  // MAGE (Архимаг)
    case 3:  return tex_manager->tex_editor[2];  // archer (Следопыт)
    case 4:  return tex_manager->tex_editor[3];  // ENEMY (Феодал)
    case 5:  return tex_manager->tex_editor[4];  // ROGUE (Бандит)
    case 6:  return tex_manager->tex_editor[5];  // PEASANT (Крестьянин)
    case 7:  return tex_manager->tex_editor[6];  // E1
    case 8:  return tex_manager->tex_editor[7];  // E2
    case 9:  return tex_manager->tex_editor[8];  // E3
    case 10: return tex_manager->tex_editor[10]; // Undead
    case 11: return tex_manager->tex_editor[11]; // Ghost
    case 12: return tex_manager->tex_editor[12]; // zombi
    default: return tex_manager->tex_editor[0];
    }
}

EditorTexture EditorWorkspace::GetEditorTextureForLantern(LanternObject* l, uint8_t default_model) const {
    if (!tex_manager) return EditorTexture();

    uint8_t model = l ? l->model : default_model;

    if (model == 8) {
        return tex_manager->tex_editor[7]; // E2 (Чистый фонарик, активный со старта)
    }
    else if (model == 9) {
        int radius = l ? l->radius : 0;
        int ev_cnt = l ? l->event_count : 0;

        if (radius > 0 && ev_cnt == 0) return tex_manager->tex_editor[13]; // E4 (Чистый активируемый фонарик)
        if (radius == 0 && ev_cnt > 0) return tex_manager->tex_editor[8];  // E3 (Чистая мина)
        if (radius > 0 && ev_cnt > 0)  return tex_manager->tex_editor[14]; // E5 (Мина + фонарик)
        return tex_manager->tex_editor[15]; // E6 (Незаполненная точка / Пустышка)
    }

    return tex_manager->tex_editor[0];
}
void EditorWorkspace::UpdateArmyModels() {
    if (!res_manager) return;

    for (auto& army : map_armies) {
        int leader_id = army.units[0]; // Проверяем класс предводителя

        if (leader_id > 0 && static_cast<size_t>(leader_id) < res_manager->units.size()) {
            if (leader_id == 43 || leader_id == 44 || leader_id == 47 || leader_id == 96) {
                army.model = 12; // Зомби
            }
            else if (leader_id == 51 || leader_id == 52 || leader_id == 53 || leader_id == 92) {
                army.model = 11; // Призраки
            }
            else if (res_manager->units[leader_id].nature == "Undead") {
                army.model = 10; // Мертвые
            }
            else {
                if (army.behavior == 1) army.model = 5;      // Разбойники
                else if (army.behavior == 2) army.model = 6; // Крестьяне
                else army.model = 4;                          // Феодалы
            }
        }
        else {
            // Если предводителя нет (класс 0) — модель строго по стилю поведения
            if (army.behavior == 1) army.model = 5;
            else if (army.behavior == 2) army.model = 6;
            else army.model = 4;
        }
    }
}

void EditorWorkspace::LoadMap(const MapData& map) {
    loaded_map = map;
    cam_x = 0;
    cam_y = 0;
    map_decors = loaded_map.decors;
    map_buildings = loaded_map.buildings;
    map_armies = loaded_map.armies;
    map_lanterns = loaded_map.lanterns;

    // Разовый расчет моделей всех армий на карте при загрузке
    UpdateArmyModels();
}

// ---------------------------------------------------------
// ОСНОВНОЙ РЕНДЕР
// ---------------------------------------------------------
void EditorWorkspace::Render(const ImVec2& work_pos, const ImVec2& work_size) {
    // Держим фокус на Карте первые 2-3 кадра, пока ImGui окончательно связывает вкладки
    static int focus_frames = 3;
    if (focus_frames > 0) {
        ImGui::SetWindowFocus("Карта");
        focus_frames--;
    }

    // 1. Тулбар
    ImGui::Begin("Панель управления", nullptr, ImGuiWindowFlags_NoScrollbar);
    RenderToolbar();
    ImGui::End();

    // 2. Карта
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Карта", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // ВАЖНО: Если внутри окна Карты осталась старая строка 
    // "if (ImGui::IsWindowAppearing()) ImGui::SetWindowFocus(nullptr);" — УДАЛИТЕ ЕЁ.

    ImVec2 viewport_pos = ImGui::GetCursorScreenPos();
    ImVec2 viewport_size = ImGui::GetContentRegionAvail();

    InitGrid(viewport_pos, viewport_size);

    if (grid_end.x > grid_start.x && grid_end.y > grid_start.y) {
        HandleInput();
        DrawMapLayers(ImGui::GetWindowDrawList());
        DrawCursorAndRulers(ImGui::GetWindowDrawList());
        DrawTooltips();
    }
    ImGui::End();
    ImGui::PopStyleVar();

    // 3. Отдельное док-окно инструментов и палитры
    RenderRightPanel(work_size, 0.0f, 0.0f);

    // Модальные окна
    RenderModals();
    just_changed_tab = false;
}

void EditorWorkspace::RenderToolbar() {
    //ImGui::SetCursorPos(ImVec2(29.0f, 10.0f));
    // УДАЛИТЕ или закомментируйте жесткую привязку:
    //ImGui::Dummy(ImVec2(0.0f, 2.0f));
    float btn_sz = 22.0f;

    // 1. Кнопка настроек Артефактов
    GLuint art_tex = 0;
    if (res_manager && res_manager->artifacts.size() > 1) art_tex = res_manager->artifacts[1].tex;

    if (art_tex) {
        if (ImGui::ImageButton("##btn_art", (ImTextureID)(intptr_t)art_tex, ImVec2(btn_sz, btn_sz))) { open_artifact_settings = true; }
    }
    else {
        if (ImGui::Button("Art", ImVec2(btn_sz + 8, btn_sz + 8))) { open_artifact_settings = true; }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Настройка артефактов");

    ImGui::SameLine();

    // 2. Кнопка настроек Юнитов
    GLuint unit_tex = 0;
    if (res_manager && res_manager->units.size() > 1) unit_tex = res_manager->units[1].iconTex;

    if (unit_tex) {
        if (ImGui::ImageButton("##btn_unit", (ImTextureID)(intptr_t)unit_tex, ImVec2(btn_sz, btn_sz))) { open_unit_settings = true; }
    }
    else {
        if (ImGui::Button("Unit", ImVec2(btn_sz + 8, btn_sz + 8))) { open_unit_settings = true; }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Настройка юнитов");

    ImGui::SameLine();

    // 3. Показать сетку
    GLuint grid_tex = tex_manager ? tex_manager->tex_btn_grid[show_grid ? 1 : 0] : 0;
    if (grid_tex) {
        if (ImGui::ImageButton("##btn_grid", (ImTextureID)(intptr_t)grid_tex, ImVec2(btn_sz, btn_sz))) show_grid = !show_grid;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Показать сетку");
    }

    ImGui::SameLine();

    // 4. Показать зоны патрулирования
    GLuint patrol_tex = tex_manager ? tex_manager->tex_btn_patrol[show_patrol_zones ? 1 : 0] : 0;
    if (patrol_tex) {
        if (ImGui::ImageButton("##btn_patrol", (ImTextureID)(intptr_t)patrol_tex, ImVec2(btn_sz, btn_sz))) show_patrol_zones = !show_patrol_zones;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Показать зоны патрулирования");
    }

    ImGui::SameLine();

    // 5. Показать туман войны
    GLuint fog_tex = tex_manager ? tex_manager->tex_btn_fog[show_fog_of_war ? 1 : 0] : 0;
    if (fog_tex) {
        if (ImGui::ImageButton("##btn_fog", (ImTextureID)(intptr_t)fog_tex, ImVec2(btn_sz, btn_sz))) show_fog_of_war = !show_fog_of_war;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Показать туман войны");
    }
}

void EditorWorkspace::InitGrid(const ImVec2& work_pos, const ImVec2& work_size) {
    // Убираем фиксированный отступ right_margin (260.0f), так как панель теперь в отдельном окне
    float left_margin = 29.0f, top_margin = 25.0f, bottom_margin = 25.0f, right_margin = 25.0f;
    cell_w = 32.0f * editor_zoom;
    cell_h = 22.0f * editor_zoom;

    VIEW_COLS = (int)std::floor((work_size.x - left_margin - right_margin) / cell_w);
    VIEW_ROWS = (int)std::floor((work_size.y - top_margin - bottom_margin) / cell_h);

    grid_start = ImVec2(work_pos.x + left_margin, work_pos.y + top_margin);
    grid_end = ImVec2(grid_start.x + VIEW_COLS * cell_w, grid_start.y + VIEW_ROWS * cell_h);

    int map_w = loaded_map.is_loaded ? loaded_map.size_x : 50;
    int map_h = loaded_map.is_loaded ? loaded_map.size_y : 50;
    if (map_w <= 0) map_w = 50;
    if (map_h <= 0) map_h = 50;

    cam_x = std::clamp(cam_x, 0, std::max(0, map_w - VIEW_COLS));
    cam_y = std::clamp(cam_y, 0, std::max(0, map_h - VIEW_ROWS));

    ImVec2 mpos = ImGui::GetIO().MousePos;
    if (mpos.x != last_mouse_pos.x || mpos.y != last_mouse_pos.y) {
        last_mouse_pos = mpos; last_move_time = ImGui::GetTime();
    }
    show_cursor = (ImGui::GetTime() - last_move_time >= 0.005);

    ImGui::SetCursorScreenPos(grid_start);
    ImGui::InvisibleButton("##map_grid", ImVec2(grid_end.x - grid_start.x, grid_end.y - grid_start.y), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    // Включаем активное состояние строго при удержании Левой Кнопки для предотвращения багов при рисовании правой
    grid_active = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    grid_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    grid_clicked_right = ImGui::IsItemClicked(ImGuiMouseButton_Right);
    grid_hovered = ImGui::IsItemHovered();

    hover_tx = -1; hover_ty = -1;
    if (grid_hovered || grid_active || ImGui::IsItemActive()) {
        hover_tx = cam_x + (int)((mpos.x - grid_start.x) / cell_w);
        hover_ty = cam_y + (int)((mpos.y - grid_start.y) / cell_h);
    }
}

void EditorWorkspace::HandleInput() {
    if (!loaded_map.is_loaded || hover_tx == -1 || hover_ty == -1) return;
    int map_w = loaded_map.size_x; int map_h = loaded_map.size_y;

    if (current_move_mode != MoveMode::None) {
        if (grid_clicked) {
            if (current_move_mode == MoveMode::Building) {
                move_bld.x = hover_tx; move_bld.y = hover_ty;
                map_buildings.push_back(move_bld);
                loaded_map.building_texts.push_back(move_bld_text);
            }
            else if (current_move_mode == MoveMode::Army) {
                move_army.x = hover_tx; move_army.y = hover_ty;
                map_armies.push_back(move_army);
                loaded_map.army_texts.push_back(move_army_text);
            }
            else if (current_move_mode == MoveMode::Lantern) {
                move_lant.x = hover_tx; move_lant.y = hover_ty;
                map_lanterns.push_back(move_lant);
            }
            else if (current_move_mode == MoveMode::Hero) {
                if (move_hero_id == 1) { loaded_map.knight.pos_x = hover_tx; loaded_map.knight.pos_y = hover_ty; }
                else if (move_hero_id == 2) { loaded_map.archmage.pos_x = hover_tx; loaded_map.archmage.pos_y = hover_ty; }
                else if (move_hero_id == 3) { loaded_map.ranger.pos_x = hover_tx; loaded_map.ranger.pos_y = hover_ty; }
            }
            current_move_mode = MoveMode::None;
        }
        return;
    }

    if (grid_clicked_right) {
        if (loaded_map.knight.pos_x != 0 && loaded_map.knight.pos_x == hover_tx && loaded_map.knight.pos_y == hover_ty) { move_hero_id = 1; current_move_mode = MoveMode::Hero; loaded_map.knight.pos_x = 0; return; }
        if (loaded_map.archmage.pos_x != 0 && loaded_map.archmage.pos_x == hover_tx && loaded_map.archmage.pos_y == hover_ty) { move_hero_id = 2; current_move_mode = MoveMode::Hero; loaded_map.archmage.pos_x = 0; return; }
        if (loaded_map.ranger.pos_x != 0 && loaded_map.ranger.pos_x == hover_tx && loaded_map.ranger.pos_y == hover_ty) { move_hero_id = 3; current_move_mode = MoveMode::Hero; loaded_map.ranger.pos_x = 0; return; }

        for (size_t i = 0; i < map_armies.size(); ++i) {
            if (map_armies[i].x == hover_tx && map_armies[i].y == hover_ty) {
                move_army = map_armies[i];
                if (i < loaded_map.army_texts.size()) {
                    move_army_text = loaded_map.army_texts[i];
                    loaded_map.army_texts.erase(loaded_map.army_texts.begin() + i);
                }
                else {
                    move_army_text = ArmyTexts();
                }
                map_armies.erase(map_armies.begin() + i);
                current_move_mode = MoveMode::Army;
                return;
            }
        }
        for (size_t i = 0; i < map_lanterns.size(); ++i) {
            if (map_lanterns[i].x == hover_tx && map_lanterns[i].y == hover_ty) {
                move_lant = map_lanterns[i];
                map_lanterns.erase(map_lanterns.begin() + i);
                current_move_mode = MoveMode::Lantern;
                return;
            }
        }
        for (size_t i = 0; i < map_buildings.size(); ++i) {
            GameObject* obj = GetObjectById(map_buildings[i].id, ObjType::Functional);
            int w = obj ? obj->header.mapCellsW : 1, h = obj ? obj->header.mapCellsH : 1;
            if (hover_tx >= map_buildings[i].x - w + 1 && hover_tx <= map_buildings[i].x && hover_ty >= map_buildings[i].y - h + 1 && hover_ty <= map_buildings[i].y) {
                move_bld = map_buildings[i];
                if (i < loaded_map.building_texts.size()) {
                    move_bld_text = loaded_map.building_texts[i];
                    loaded_map.building_texts.erase(loaded_map.building_texts.begin() + i);
                }
                else {
                    move_bld_text = BuildingTexts();
                }
                map_buildings.erase(map_buildings.begin() + i);
                current_move_mode = MoveMode::Building;
                return;
            }
        }
    }

    if (current_tool == ToolMode::Info && grid_clicked) {
        if (loaded_map.knight.pos_x != 0 && loaded_map.knight.pos_x == hover_tx && loaded_map.knight.pos_y == hover_ty) { focus_hero_tab = 0; open_scenario_settings = true; return; }
        if (loaded_map.archmage.pos_x != 0 && loaded_map.archmage.pos_x == hover_tx && loaded_map.archmage.pos_y == hover_ty) { focus_hero_tab = 1; open_scenario_settings = true; return; }
        if (loaded_map.ranger.pos_x != 0 && loaded_map.ranger.pos_x == hover_tx && loaded_map.ranger.pos_y == hover_ty) { focus_hero_tab = 2; open_scenario_settings = true; return; }

        bool found = false;
        for (size_t i = 0; i < map_armies.size(); ++i) {
            if (map_armies[i].x == hover_tx && map_armies[i].y == hover_ty) { selected_army_idx = (int)i; open_army_settings = true; found = true; break; }
        }
        if (!found) {
            for (size_t i = 0; i < map_lanterns.size(); ++i) {
                if (map_lanterns[i].x == hover_tx && map_lanterns[i].y == hover_ty) { selected_lantern_idx = (int)i; open_lantern_settings = true; found = true; break; }
            }
        }
        if (!found) {
            for (size_t i = 0; i < map_buildings.size(); ++i) {
                GameObject* obj = GetObjectById(map_buildings[i].id, ObjType::Functional);
                int w = obj ? obj->header.mapCellsW : 1, h = obj ? obj->header.mapCellsH : 1;
                if (hover_tx >= map_buildings[i].x - w + 1 && hover_tx <= map_buildings[i].x && hover_ty >= map_buildings[i].y - h + 1 && hover_ty <= map_buildings[i].y) {
                    selected_building_idx = (int)i; open_building_settings = true; found = true; break;
                }
            }
        }
        return;
    }

    if (!(grid_active || grid_clicked)) return;

    // --- РЕАЛИЗАЦИЯ ИНСТРУМЕНТА ЗАЛИВКИ (FLOOD FILL) ---
    if (active_tab == 0 && current_tool == ToolMode::Fill && grid_clicked) {
        uint8_t target_type = loaded_map.surface_tiles[(size_t)hover_ty * map_w + hover_tx];
        uint8_t replacement_type = current_surface_type;

        bool has_other_types = false;
        for (int i = 0; i < 16; ++i) if (fill_replace_types[i]) has_other_types = true;

        // Если цвета различаются, либо мы дополнительно заменяем другие тайлы
        if (target_type != replacement_type || has_other_types) {
            std::vector<std::pair<int, int>> queue;
            std::vector<bool> visited(map_w * map_h, false);

            queue.push_back({ hover_tx, hover_ty });
            visited[(size_t)hover_ty * map_w + hover_tx] = true;

            size_t head = 0;
            while (head < queue.size()) {
                auto [cx, cy] = queue[head++];

                // Красим текущий тайл
                loaded_map.surface_tiles[(size_t)cy * map_w + cx] = replacement_type;

                // Проверяем 4-х соседей
                const int dx[] = { 0, 0, -1, 1 };
                const int dy[] = { -1, 1, 0, 0 };

                for (int i = 0; i < 4; ++i) {
                    int nx = cx + dx[i];
                    int ny = cy + dy[i];

                    if (nx >= 0 && nx < map_w && ny >= 0 && ny < map_h) {
                        // Ограничение по радиусу заливки
                        if (fill_limit_radius) {
                            int dist = std::max(std::abs(nx - hover_tx), std::abs(ny - hover_ty));
                            if (dist > fill_radius) continue;
                        }

                        size_t n_idx = (size_t)ny * map_w + nx;
                        if (!visited[n_idx]) {
                            uint8_t n_type = loaded_map.surface_tiles[n_idx];
                            // Заливаем, если тип совпадает с целевым, или если тип отмечен галочкой
                            if (n_type == target_type || fill_replace_types[n_type]) {
                                visited[n_idx] = true;
                                queue.push_back({ nx, ny });
                            }
                        }
                    }
                }
            }
        }
    }
    // --- ОБЫЧНАЯ КИСТЬ ---
    else if (active_tab == 0 && current_tool != ToolMode::Info && current_tool != ToolMode::Fill && grid_active) {
        int bw = current_brush_w; int bh = current_brush_h;
        int offset_x = (bw - 1) / 2; int offset_y = (bh - 1) / 2;
        for (int y = 0; y < bh; ++y) {
            for (int x = 0; x < bw; ++x) {
                int mx = hover_tx - offset_x + x; int my = hover_ty - offset_y + y;
                if (mx >= 0 && mx < map_w && my >= 0 && my < map_h) {
                    size_t idx = (size_t)my * map_w + mx;
                    if (idx < loaded_map.surface_tiles.size()) {
                        if (current_tool == ToolMode::Brush) loaded_map.surface_tiles[idx] = current_surface_type;
                        else if (current_tool == ToolMode::Eraser) loaded_map.surface_tiles[idx] = 0;
                    }
                }
            }
        }
    }
    else if ((active_tab == 1 || active_tab == 2) && current_tool != ToolMode::Info) {
        bool can_place = (active_tab == 1 && current_tool == ToolMode::Brush) ? grid_clicked : grid_active;
        if (can_place) {
            int bw = current_brush_w; int bh = current_brush_h;
            int offset_x = (bw - 1) / 2; int offset_y = (bh - 1) / 2;
            int start_x = hover_tx - offset_x; int start_y = hover_ty - offset_y;

            if (current_tool == ToolMode::Brush && current_object_idx >= 0 && current_object_idx < (int)obj_manager->objects.size()) {
                if (active_tab == 1 || (active_tab == 2 && bw == 1 && bh == 1)) {
                    int ax = start_x + bw - 1; int ay = start_y + bh - 1;
                    if (start_x >= 0 && start_y >= 0 && ax < map_w && ay < map_h) {
                        uint32_t full_id = obj_manager->objects[current_object_idx].header.groupId * 256 + obj_manager->objects[current_object_idx].header.objectId;
                        auto it = std::find_if(map_decors.begin(), map_decors.end(), [&](const DecorObject& d) { return d.x == ax && d.y == ay; });
                        if (it == map_decors.end()) map_decors.push_back({ (uint16_t)ax, (uint16_t)ay, full_id });
                    }
                }
                else if (active_tab == 2 && (bw > 1 || bh > 1)) {
                    uint32_t base_id = obj_manager->objects[current_object_idx].header.groupId * 256 + obj_manager->objects[current_object_idx].header.objectId;
                    for (int y = 0; y < bh; ++y) {
                        for (int x = 0; x < bw; ++x) {
                            int mx = start_x + x, my = start_y + y;
                            if (mx >= 0 && mx < map_w && my >= 0 && my < map_h) {
                                auto it = std::find_if(map_decors.begin(), map_decors.end(), [&](const DecorObject& d) { return d.x == mx && d.y == my; });
                                if (it == map_decors.end()) {
                                    uint32_t r_id = base_id + (rand() % 12);
                                    if (GetObjectById(r_id, ObjType::Decor)) map_decors.push_back({ (uint16_t)mx, (uint16_t)my, r_id });
                                }
                            }
                        }
                    }
                }
            }
            else if (current_tool == ToolMode::Eraser) {
                for (auto it = map_decors.begin(); it != map_decors.end(); ) {
                    uint32_t gid = it->id / 256, oid = it->id % 256;
                    int obj_size = 1; if (gid <= 8) obj_size = std::max(1, (int)(oid / 10));
                    if (hover_tx >= it->x - obj_size + 1 && hover_tx <= it->x && hover_ty >= it->y - obj_size + 1 && hover_ty <= it->y) {
                        it = map_decors.erase(it);
                    }
                    else ++it;
                }
            }
        }
    }
    else if (active_tab == 3) {
        if (current_tool == ToolMode::Brush && current_object_idx >= 0 && current_object_idx < (int)obj_manager->objects.size() && grid_clicked) {
            auto& obj = obj_manager->objects[current_object_idx];
            int start_x = hover_tx - obj.header.mapCellsW + 1, start_y = hover_ty - obj.header.mapCellsH + 1;
            if (start_x >= 0 && start_y >= 0 && hover_tx < map_w && hover_ty < map_h) {
                uint32_t full_id = obj.header.groupId * 256 + obj.header.objectId;
                auto it = std::find_if(map_buildings.begin(), map_buildings.end(), [&](const BuildingObject& b) { return b.x == hover_tx && b.y == hover_ty; });
                if (it == map_buildings.end()) {
                    BuildingObject b = {};
                    b.x = hover_tx; b.y = hover_ty; b.id = full_id;
                    b.group_id = 1;
                    b.raw_data.resize(loaded_map.is_version_5 ? 422 : 358, 0);
                    map_buildings.push_back(b);
                    loaded_map.building_texts.push_back(BuildingTexts());
                }
            }
        }
        else if (current_tool == ToolMode::Eraser && grid_active) {
            for (size_t i = 0; i < map_buildings.size(); ) {
                auto& b = map_buildings[i];
                GameObject* obj = GetObjectById(b.id, ObjType::Functional);
                int w = obj ? obj->header.mapCellsW : 1, h = obj ? obj->header.mapCellsH : 1;
                if (hover_tx >= b.x - w + 1 && hover_tx <= b.x && hover_ty >= b.y - h + 1 && hover_ty <= b.y) {
                    map_buildings.erase(map_buildings.begin() + i);
                    if (i < loaded_map.building_texts.size()) {
                        loaded_map.building_texts.erase(loaded_map.building_texts.begin() + i);
                    }
                }
                else ++i;
            }
        }
    }
    else if (active_tab == 4 && grid_clicked) {
        if (current_tool == ToolMode::Brush && current_object_idx >= 0 && current_object_idx <= 8) {
            int i = current_object_idx;
            if (i == 0) { if (loaded_map.knight.pos_x != 0) { loaded_map.knight.pos_x = hover_tx; loaded_map.knight.pos_y = hover_ty; } }
            else if (i == 1) { if (loaded_map.archmage.pos_x != 0) { loaded_map.archmage.pos_x = hover_tx; loaded_map.archmage.pos_y = hover_ty; } }
            else if (i == 2) { if (loaded_map.ranger.pos_x != 0) { loaded_map.ranger.pos_x = hover_tx; loaded_map.ranger.pos_y = hover_ty; } }
            else if (i >= 3 && i <= 6) {
                ArmyObject a = {};
                a.x = hover_tx; a.y = hover_ty;

                int max_id = 0;
                for (const auto& army : map_armies) { if (army.id > max_id) max_id = army.id; }
                a.id = max_id + 1;

                if (i == 3) a.model = 4;
                if (i == 4) a.model = 5;
                if (i == 5) a.model = 6;
                if (i == 6) { a.model = 4; a.activity = 1; }
                a.group_id = 1;
                a.raw_data.resize(loaded_map.is_version_5 ? 117 : 89, 0);

                map_armies.push_back(a);
                loaded_map.army_texts.push_back(ArmyTexts());
            }
            else if (i == 7 || i == 8) {
                LanternObject l = {};
                l.x = hover_tx; l.y = hover_ty;

                int max_l_id = 0;
                for (const auto& lant : map_lanterns) { if (lant.id > max_l_id) max_l_id = lant.id; }
                l.id = max_l_id + 1;

                l.model = (i == 7) ? 8 : 9;
                l.raw_data.resize(loaded_map.is_version_5 ? 101 : 99, 0);
                map_lanterns.push_back(l);
            }
        }
        else if (current_tool == ToolMode::Eraser) {
            for (size_t i = 0; i < map_armies.size(); ) {
                if (map_armies[i].x == hover_tx && map_armies[i].y == hover_ty) {
                    map_armies.erase(map_armies.begin() + i);
                    if (i < loaded_map.army_texts.size()) {
                        loaded_map.army_texts.erase(loaded_map.army_texts.begin() + i);
                    }
                }
                else { ++i; }
            }
            for (size_t i = 0; i < map_lanterns.size(); ) {
                if (map_lanterns[i].x == hover_tx && map_lanterns[i].y == hover_ty) {
                    map_lanterns.erase(map_lanterns.begin() + i);
                }
                else { ++i; }
            }
        }
    }
}

// ---------------------------------------------------------
// ОТРИСОВКА КАРТЫ И Z-СОРТИРОВКА
// ---------------------------------------------------------
void EditorWorkspace::DrawGradientQuad(ImDrawList* draw_list, ImTextureID tex_id,
    const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4,
    const ImVec2& uv1, const ImVec2& uv2, const ImVec2& uv3, const ImVec2& uv4,
    ImU32 col1, ImU32 col2, ImU32 col3, ImU32 col4)
{
    draw_list->PushTextureID(tex_id);
    draw_list->PrimReserve(6, 4); // Резервируем память под 2 треугольника (6 индексов) и 4 вершины

    ImDrawIdx idx = draw_list->_VtxCurrentIdx;
    draw_list->_IdxWritePtr[0] = idx; draw_list->_IdxWritePtr[1] = (ImDrawIdx)(idx + 1); draw_list->_IdxWritePtr[2] = (ImDrawIdx)(idx + 2);
    draw_list->_IdxWritePtr[3] = idx; draw_list->_IdxWritePtr[4] = (ImDrawIdx)(idx + 2); draw_list->_IdxWritePtr[5] = (ImDrawIdx)(idx + 3);

    // Точка 1: Верх-Лево
    draw_list->_VtxWritePtr[0].pos = p1; draw_list->_VtxWritePtr[0].uv = uv1; draw_list->_VtxWritePtr[0].col = col1;
    // Точка 2: Верх-Право
    draw_list->_VtxWritePtr[1].pos = p2; draw_list->_VtxWritePtr[1].uv = uv2; draw_list->_VtxWritePtr[1].col = col2;
    // Точка 3: Низ-Право
    draw_list->_VtxWritePtr[2].pos = p3; draw_list->_VtxWritePtr[2].uv = uv3; draw_list->_VtxWritePtr[2].col = col3;
    // Точка 4: Низ-Лево
    draw_list->_VtxWritePtr[3].pos = p4; draw_list->_VtxWritePtr[3].uv = uv4; draw_list->_VtxWritePtr[3].col = col4;

    draw_list->_VtxWritePtr += 4;
    draw_list->_IdxWritePtr += 6;
    draw_list->_VtxCurrentIdx += 4;

    draw_list->PopTextureID();
}

/*void EditorWorkspace::DrawGradientTriangle(ImDrawList* draw_list, ImTextureID tex_id,
    const ImVec2& p1, const ImVec2& p2, const ImVec2& p3,
    const ImVec2& uv1, const ImVec2& uv2, const ImVec2& uv3,
    ImU32 col1, ImU32 col2, ImU32 col3)
{
    draw_list->PushTextureID(tex_id);
    draw_list->PrimReserve(3, 3);

    ImDrawIdx idx = draw_list->_VtxCurrentIdx;
    draw_list->_IdxWritePtr[0] = idx;
    draw_list->_IdxWritePtr[1] = (ImDrawIdx)(idx + 1);
    draw_list->_IdxWritePtr[2] = (ImDrawIdx)(idx + 2);

    draw_list->_VtxWritePtr[0].pos = p1; draw_list->_VtxWritePtr[0].uv = uv1; draw_list->_VtxWritePtr[0].col = col1;
    draw_list->_VtxWritePtr[1].pos = p2; draw_list->_VtxWritePtr[1].uv = uv2; draw_list->_VtxWritePtr[1].col = col2;
    draw_list->_VtxWritePtr[2].pos = p3; draw_list->_VtxWritePtr[2].uv = uv3; draw_list->_VtxWritePtr[2].col = col3;

    draw_list->_VtxWritePtr += 3;
    draw_list->_IdxWritePtr += 3;
    draw_list->_VtxCurrentIdx += 3;

    draw_list->PopTextureID();
}*/

void EditorWorkspace::DrawMapLayers(ImDrawList* draw_list) {
    draw_list->PushClipRect(grid_start, grid_end, true);
    int map_w = loaded_map.is_loaded ? loaded_map.size_x : 50;
    int map_h = loaded_map.is_loaded ? loaded_map.size_y : 50;

    // === ПРОХОД 1: Отрисовка плотных базовых тайлов ===
    for (int y = 0; y < VIEW_ROWS; ++y) {
        for (int x = 0; x < VIEW_COLS; ++x) {
            int tx = cam_x + x, ty = cam_y + y;
            ImVec2 p_min(grid_start.x + x * cell_w, grid_start.y + y * cell_h);
            ImVec2 p_max(grid_start.x + (x + 1) * cell_w, grid_start.y + (y + 1) * cell_h);

            if (loaded_map.is_loaded && tx >= 0 && tx < map_w && ty >= 0 && ty < map_h) {
                uint8_t t_id = loaded_map.surface_tiles[(size_t)ty * map_w + tx];
                // Корректный расчет координат подтайла по диагональной изометрической формуле:
                // Индексы кусочка в атласе 8x11
                int col_idx = ((tx + 1) % 8 + 8) % 8;
                int row_idx = ((ty + 1) % 11 + 11) % 11;

                float base_u = (col_idx * 32.0f) / 256.0f;
                float base_v = (row_idx * 22.0f) / 242.0f;
                float u_step = 32.0f / 256.0f;
                float v_step = 22.0f / 242.0f;

                if (t_id < 16 && tex_manager && tex_manager->terrain_textures[t_id]) {
                    draw_list->AddImage((ImTextureID)(intptr_t)tex_manager->terrain_textures[t_id], p_min, p_max,
                        ImVec2(base_u, base_v), ImVec2(base_u + u_step, base_v + v_step));
                }
                else {
                    draw_list->AddRectFilled(p_min, p_max, GetTileColor(t_id));
                }
            }
            else {
                draw_list->AddRectFilled(p_min, p_max, IM_COL32(30, 30, 30, 255));
            }
        }
    }

    // === АВТЕНТИЧНЫЙ АВТОТАЙЛИНГ (БАЗОВЫЙ ТАЙЛ ВСЕГДА ВИДЕН + БЕСШОВНЫЙ СТЫК) ===
    // 128 (50%) — математический идеал, при котором шов между тайлами становится абсолютно невидимым.
    // Если захочется сделать растушевку чуть прозрачнее или плотнее — можно менять от 100 до 135.
    const uint8_t BLEND_ALPHA = 128;

    for (int y = 0; y < VIEW_ROWS; ++y) {
        for (int x = 0; x < VIEW_COLS; ++x) {
            int tx = cam_x + x, ty = cam_y + y;
            if (loaded_map.is_loaded && tx >= 0 && tx < map_w && ty >= 0 && ty < map_h) {
                uint8_t t_id = loaded_map.surface_tiles[(size_t)ty * map_w + tx];
                ImVec2 p_min(grid_start.x + x * cell_w, grid_start.y + y * cell_h);
                ImVec2 p_max(grid_start.x + (x + 1) * cell_w, grid_start.y + (y + 1) * cell_h);

                // Та же формула для бесшовного наложения соседних тайлов:
                // Индексы кусочка в атласе 8x11
                int col_idx = ((tx + 1) % 8 + 8) % 8;
                int row_idx = ((ty + 1) % 11 + 11) % 11;

                float base_u = (col_idx * 32.0f) / 256.0f;
                float base_v = (row_idx * 22.0f) / 242.0f;
                float u_step = 32.0f / 256.0f;
                float v_step = 22.0f / 242.0f;

                // 1. Опрашиваем всех 4 прямых соседей (любой отличающийся тайл дает наплыв)
                struct OverlayEdge { uint8_t id; int dir; }; // 0=Top, 1=Bottom, 2=Left, 3=Right
                std::vector<OverlayEdge> edges;

                auto check_edge = [&](int nx, int ny, int dir) {
                    if (nx >= 0 && nx < map_w && ny >= 0 && ny < map_h) {
                        uint8_t n_id = loaded_map.surface_tiles[(size_t)ny * map_w + nx];
                        if (n_id != t_id && n_id < 16) {
                            edges.push_back({ n_id, dir });
                        }
                    }
                    };

                check_edge(tx, ty - 1, 0); // Верх
                check_edge(tx, ty + 1, 1); // Низ
                check_edge(tx - 1, ty, 2); // Лево
                check_edge(tx + 1, ty, 3); // Право

                if (!edges.empty()) {
                    // Сортировка по ID: чтобы порядок слоев был детерминированным
                    std::sort(edges.begin(), edges.end(), [](const OverlayEdge& a, const OverlayEdge& b) {
                        return a.id < b.id;
                        });

                    // Наплыв идет ровно до середины ячейки (11px по Y, 16px по X)
                    float half_h = cell_h * 0.5f;
                    float half_w = cell_w * 0.5f;
                    float uv_half_v = v_step * 0.5f;
                    float uv_half_u = u_step * 0.5f;

                    for (const auto& edge : edges) {
                        if (!tex_manager || !tex_manager->terrain_textures[edge.id]) continue;
                        ImTextureID tex = (ImTextureID)(intptr_t)tex_manager->terrain_textures[edge.id];

                        if (edge.dir == 0) { // Верхний наплыв
                            ImVec2 p1 = p_min;
                            ImVec2 p2 = ImVec2(p_max.x, p_min.y);
                            ImVec2 p3 = ImVec2(p_max.x, p_min.y + half_h);
                            ImVec2 p4 = ImVec2(p_min.x, p_min.y + half_h);

                            ImVec2 uv1(base_u, base_v);
                            ImVec2 uv2(base_u + u_step, base_v);
                            ImVec2 uv3(base_u + u_step, base_v + uv_half_v);
                            ImVec2 uv4(base_u, base_v + uv_half_v);

                            DrawGradientQuad(draw_list, tex, p1, p2, p3, p4, uv1, uv2, uv3, uv4,
                                IM_COL32(255, 255, 255, BLEND_ALPHA), IM_COL32(255, 255, 255, BLEND_ALPHA),
                                IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
                        }
                        else if (edge.dir == 1) { // Нижний наплыв
                            ImVec2 p1 = ImVec2(p_min.x, p_max.y - half_h);
                            ImVec2 p2 = ImVec2(p_max.x, p_max.y - half_h);
                            ImVec2 p3 = p_max;
                            ImVec2 p4 = ImVec2(p_min.x, p_max.y);

                            ImVec2 uv1(base_u, base_v + v_step - uv_half_v);
                            ImVec2 uv2(base_u + u_step, base_v + v_step - uv_half_v);
                            ImVec2 uv3(base_u + u_step, base_v + v_step);
                            ImVec2 uv4(base_u, base_v + v_step);

                            DrawGradientQuad(draw_list, tex, p1, p2, p3, p4, uv1, uv2, uv3, uv4,
                                IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0),
                                IM_COL32(255, 255, 255, BLEND_ALPHA), IM_COL32(255, 255, 255, BLEND_ALPHA));
                        }
                        else if (edge.dir == 2) { // Левый наплыв
                            ImVec2 p1 = p_min;
                            ImVec2 p2 = ImVec2(p_min.x + half_w, p_min.y);
                            ImVec2 p3 = ImVec2(p_min.x + half_w, p_max.y);
                            ImVec2 p4 = ImVec2(p_min.x, p_max.y);

                            ImVec2 uv1(base_u, base_v);
                            ImVec2 uv2(base_u + uv_half_u, base_v);
                            ImVec2 uv3(base_u + uv_half_u, base_v + v_step);
                            ImVec2 uv4(base_u, base_v + v_step);

                            DrawGradientQuad(draw_list, tex, p1, p2, p3, p4, uv1, uv2, uv3, uv4,
                                IM_COL32(255, 255, 255, BLEND_ALPHA), IM_COL32(255, 255, 255, 0),
                                IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, BLEND_ALPHA));
                        }
                        else if (edge.dir == 3) { // Правый наплыв
                            ImVec2 p1 = ImVec2(p_max.x - half_w, p_min.y);
                            ImVec2 p2 = ImVec2(p_max.x, p_min.y);
                            ImVec2 p3 = p_max;
                            ImVec2 p4 = ImVec2(p_max.x - half_w, p_max.y);

                            ImVec2 uv1(base_u + u_step - uv_half_u, base_v);
                            ImVec2 uv2(base_u + u_step, base_v);
                            ImVec2 uv3(base_u + u_step, base_v + v_step);
                            ImVec2 uv4(base_u + u_step - uv_half_u, base_v + v_step);

                            DrawGradientQuad(draw_list, tex, p1, p2, p3, p4, uv1, uv2, uv3, uv4,
                                IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, BLEND_ALPHA),
                                IM_COL32(255, 255, 255, BLEND_ALPHA), IM_COL32(255, 255, 255, 0));
                        }
                    }
                }
            }
        }
    }

    // 2. Сборка Z-слоев объектов
    enum RType { HILL, BUILD, SQUAD_ARMY, SQUAD_LANTERN, MOUNT, TREE };
    struct RenderItem { int y, x, layer; RType type; uint32_t id; void* ptr; int sort_y; };
    std::vector<RenderItem> rq;

    // Определяем границы видимости камеры (+15 ячеек запаса для корректной отрисовки широких спрайтов)
    int c_min_x = cam_x - 15, c_max_x = cam_x + VIEW_COLS + 15;
    int c_min_y = cam_y - 15, c_max_y = cam_y + VIEW_ROWS + 15;

    // Функция, поднимающая Z-индекс (координату Y) для отрядов/событий, если они внутри строения
    auto GetArmySortY = [&](int ax, int ay) {
        int max_y = ay;
        for (const auto& b : map_buildings) {
            GameObject* obj = GetObjectById(b.id, ObjType::Functional);
            int w = obj ? obj->header.mapCellsW : 1, h = obj ? obj->header.mapCellsH : 1;
            if (ax >= b.x - w + 1 && ax <= b.x && ay >= b.y - h + 1 && ay <= b.y) {
                if (b.y > max_y) max_y = b.y;
            }
        }
        return max_y;
        };

    // ФИЛЬТРУЕМ объекты ПЕРЕД отправкой на тяжелую сортировку
    for (auto& d : map_decors) {
        if (d.x < c_min_x || d.x >= c_max_x || d.y < c_min_y || d.y >= c_max_y) continue;
        uint32_t gid = d.id / 256;
        if (gid <= 4) rq.push_back({ d.y, d.x, 1, HILL, d.id, nullptr, d.y });
        else if (gid <= 8) rq.push_back({ d.y, d.x, 4, MOUNT, d.id, nullptr, d.y });
        else rq.push_back({ d.y, d.x, 5, TREE, d.id, nullptr, d.y });
    }

    for (auto& b : map_buildings) {
        if (b.x < c_min_x || b.x >= c_max_x || b.y < c_min_y || b.y >= c_max_y) continue;
        rq.push_back({ b.y, b.x, 2, BUILD, b.id, nullptr, b.y });
    }

    for (auto& a : map_armies) {
        if (a.x < c_min_x || a.x >= c_max_x || a.y < c_min_y || a.y >= c_max_y) continue;
        rq.push_back({ a.y, a.x, 3, SQUAD_ARMY, a.model, &a, GetArmySortY(a.x, a.y) });
    }

    for (auto& l : map_lanterns) {
        if (l.x < c_min_x || l.x >= c_max_x || l.y < c_min_y || l.y >= c_max_y) continue;
        rq.push_back({ l.y, l.x, 3, SQUAD_LANTERN, l.model, &l, GetArmySortY(l.x, l.y) });
    }

    if (loaded_map.is_loaded) {
        if (loaded_map.knight.pos_x != 0 && loaded_map.knight.pos_x >= c_min_x && loaded_map.knight.pos_x < c_max_x && loaded_map.knight.pos_y >= c_min_y && loaded_map.knight.pos_y < c_max_y)
            rq.push_back({ loaded_map.knight.pos_y, loaded_map.knight.pos_x, 3, SQUAD_ARMY, 1, nullptr, GetArmySortY(loaded_map.knight.pos_x, loaded_map.knight.pos_y) });
        if (loaded_map.archmage.pos_x != 0 && loaded_map.archmage.pos_x >= c_min_x && loaded_map.archmage.pos_x < c_max_x && loaded_map.archmage.pos_y >= c_min_y && loaded_map.archmage.pos_y < c_max_y)
            rq.push_back({ loaded_map.archmage.pos_y, loaded_map.archmage.pos_x, 3, SQUAD_ARMY, 2, nullptr, GetArmySortY(loaded_map.archmage.pos_x, loaded_map.archmage.pos_y) });
        if (loaded_map.ranger.pos_x != 0 && loaded_map.ranger.pos_x >= c_min_x && loaded_map.ranger.pos_x < c_max_x && loaded_map.ranger.pos_y >= c_min_y && loaded_map.ranger.pos_y < c_max_y)
            rq.push_back({ loaded_map.ranger.pos_y, loaded_map.ranger.pos_x, 3, SQUAD_ARMY, 3, nullptr, GetArmySortY(loaded_map.ranger.pos_x, loaded_map.ranger.pos_y) });
    }

    std::sort(rq.begin(), rq.end(), [](const RenderItem& a, const RenderItem& b) {
        if (a.layer == 1 && b.layer != 1) return true;
        if (a.layer != 1 && b.layer == 1) return false;

        if (a.sort_y != b.sort_y) return a.sort_y < b.sort_y;
        if (a.layer != b.layer) return a.layer < b.layer;
        if (a.x != b.x) return a.x < b.x;
        return false;
        });

    // 3. Отрисовка Z-слоев
    for (auto& item : rq) {
        if (item.x >= cam_x - 15 && item.x < cam_x + VIEW_COLS + 15 && item.y >= cam_y - 15 && item.y < cam_y + VIEW_ROWS + 15) {
            if (item.type == SQUAD_ARMY || item.type == SQUAD_LANTERN) {
                EditorTexture tex;
                if (item.type == SQUAD_ARMY) {
                    tex = GetEditorTextureForArmy((const ArmyObject*)item.ptr, (uint8_t)item.id, item.x, item.y, true);
                }
                else {
                    tex = GetEditorTextureForLantern((LanternObject*)item.ptr, (uint8_t)item.id);
                }

                if (tex.id) {
                    float cx = grid_start.x + (item.x - cam_x) * cell_w + cell_w * 0.5f;
                    float cy = grid_start.y + (item.y - cam_y) * cell_h - 5.0f + cell_h * 0.5f; // По центру ячейки
                    ImVec2 p_min(cx - (tex.w * editor_zoom) * 0.5f, cy - (tex.h * editor_zoom) * 0.5f);
                    ImVec2 p_max(cx + (tex.w * editor_zoom) * 0.5f, cy + (tex.h * editor_zoom) * 0.5f);
                    draw_list->AddImage((ImTextureID)(intptr_t)tex.id, p_min, p_max);
                }
            }
            else {
                GameObject* obj = GetObjectById(item.id, item.type == BUILD ? ObjType::Functional : ObjType::Decor);
                if (obj && obj->texture) {
                    ImVec2 offset(0, 0);
                    if (item.type == BUILD) {
                        offset = GetBuildingVisualOffset(item.id / 256, item.id % 256, obj->header.mapCellsW, obj->header.mapCellsH);
                    }
                    else if (item.type == TREE) {
                        ImVec2 scatter = GetTreeScatterOffset(item.x, item.y, item.id);

                        // ТА САМАЯ МАГИЯ АССЕМБЛЕРА ИЗ ИГРЫ:
                        // Ствол дерева изначально центрируется по X (Width / 2).
                        // В ImGui мы вычитаем offset, поэтому для сдвига влево мы вычитаем Width/2.
                        offset.x = scatter.x - (obj->header.width / 2.0f);
                        offset.y = scatter.y;
                    }
                    else if (item.type == HILL || item.type == MOUNT) {
                        // Смещение на пол-ячейки (16px, 11px), компенсирующее наплыв спрайта холмов/гор/скал
                        offset = ImVec2(-16.0f, -11.0f);
                    }
                    ImVec2 p_max(grid_start.x + (item.x - cam_x + 1) * cell_w - offset.x * editor_zoom,
                        grid_start.y + (item.y - cam_y + 1) * cell_h - offset.y * editor_zoom);
                    ImVec2 p_min(p_max.x - obj->header.width * editor_zoom, p_max.y - obj->header.height * editor_zoom);

                    draw_list->AddImage((ImTextureID)(intptr_t)obj->texture, p_min, p_max);
                }
            }
        }
    }

    // 4. Отрисовка сетки проходимости поверх всех объектов
    if (show_grid) {
        enum PassState { TerrPass, TerrSlow, TerrImpass, WaterPass, WaterImpass, BuildingPass };
        std::vector<std::vector<PassState>> pass_map(VIEW_ROWS, std::vector<PassState>(VIEW_COLS, TerrPass));

        // Шаг 4.1: Базовая проходимость тайлов
        for (int y = 0; y < VIEW_ROWS; ++y) {
            for (int x = 0; x < VIEW_COLS; ++x) {
                int tx = cam_x + x, ty = cam_y + y;
                PassState st = TerrPass;
                if (loaded_map.is_loaded && tx >= 0 && tx < map_w && ty >= 0 && ty < map_h) {
                    uint8_t t_id = loaded_map.surface_tiles[(size_t)ty * map_w + tx];
                    if (t_id == 0 || t_id == 1) st = WaterPass;
                    else if (t_id == 2) st = WaterImpass;
                    else if (t_id == 3 || t_id == 9 || t_id == 15) st = TerrImpass;
                    else if (t_id == 8 || t_id == 14) st = TerrSlow;
                }
                else {
                    st = TerrImpass;
                }
                pass_map[y][x] = st;
            }
        }

        // Шаг 4.2: Декор (Холмы, Горы, Деревья)
        for (const auto& d : map_decors) {
            uint32_t gid = d.id / 256;
            uint32_t oid = d.id % 256;
            int obj_size = 1;
            if (gid <= 8) obj_size = std::max(1, (int)(oid / 10));

            int min_x = d.x - obj_size + 1; int max_x = d.x;
            int min_y = d.y - obj_size + 1; int max_y = d.y;

            if (max_x < cam_x || min_x >= cam_x + VIEW_COLS || max_y < cam_y || min_y >= cam_y + VIEW_ROWS) continue;

            PassState dec_st = TerrPass;
            if (gid >= 1 && gid <= 4) dec_st = TerrSlow; // Холмы - замедляны
            else if (gid >= 5 && gid <= 8) dec_st = TerrImpass; // Горы/Скалы - непроходимы
            else if (gid == 9 || gid == 10) dec_st = TerrSlow; // Деревья - замедляны
            else if (gid == 11) dec_st = TerrImpass; // Густой лес - непроходим

            for (int y = std::max(min_y, cam_y); y <= std::min(max_y, cam_y + VIEW_ROWS - 1); ++y) {
                for (int x = std::max(min_x, cam_x); x <= std::min(max_x, cam_x + VIEW_COLS - 1); ++x) {
                    pass_map[y - cam_y][x - cam_x] = dec_st;
                }
            }
        }

        // Шаг 4.3: Строения (Покрывают свои ячейки курсором)
        for (const auto& b : map_buildings) {
            GameObject* obj = GetObjectById(b.id, ObjType::Functional);
            int w = obj ? obj->header.mapCellsW : 1;
            int h = obj ? obj->header.mapCellsH : 1;

            int min_x = b.x - w + 1; int max_x = b.x;
            int min_y = b.y - h + 1; int max_y = b.y;

            if (max_x < cam_x || min_x >= cam_x + VIEW_COLS || max_y < cam_y || min_y >= cam_y + VIEW_ROWS) continue;

            for (int y = std::max(min_y, cam_y); y <= std::min(max_y, cam_y + VIEW_ROWS - 1); ++y) {
                for (int x = std::max(min_x, cam_x); x <= std::min(max_x, cam_x + VIEW_COLS - 1); ++x) {
                    pass_map[y - cam_y][x - cam_x] = BuildingPass;
                }
            }
        }

        // Шаг 4.4: Фактическая отрисовка сетки
        for (int y = 0; y < VIEW_ROWS; ++y) {
            for (int x = 0; x < VIEW_COLS; ++x) {
                ImVec2 p_min(grid_start.x + x * cell_w, grid_start.y + y * cell_h);
                ImVec2 p_max(grid_start.x + (x + 1) * cell_w, grid_start.y + (y + 1) * cell_h);

                PassState st = pass_map[y][x];
                if (st == BuildingPass) {
                    if (tex_manager && tex_manager->cursor_texture) {
                        draw_list->AddImage((ImTextureID)(intptr_t)tex_manager->cursor_texture, p_min, p_max, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 120));
                    }
                }
                else {
                    ImU32 outline_col = IM_COL32(255, 255, 255, 100);
                    ImU32 fill_col = IM_COL32(0, 0, 0, 0); // Легкая заливка для цвета
                    bool cross = false;

                    if (st == TerrPass) { outline_col = IM_COL32(0, 255, 0, 150); fill_col = IM_COL32(0, 255, 0, 20); }
                    else if (st == TerrSlow) { outline_col = IM_COL32(255, 255, 0, 150); fill_col = IM_COL32(255, 255, 0, 20); }
                    else if (st == TerrImpass) { outline_col = IM_COL32(255, 0, 0, 150); fill_col = IM_COL32(255, 0, 0, 20); cross = true; }
                    else if (st == WaterPass) { outline_col = IM_COL32(0, 150, 255, 150); fill_col = IM_COL32(0, 150, 255, 20); }
                    else if (st == WaterImpass) { outline_col = IM_COL32(0, 150, 255, 150); fill_col = IM_COL32(0, 150, 255, 20); cross = true; }

                    draw_list->AddRectFilled(p_min, p_max, fill_col);
                    draw_list->AddRect(p_min, p_max, outline_col, 0.0f, 0, 1.0f);

                    if (cross) {
                        draw_list->AddLine(p_min, p_max, outline_col, 1.5f);
                        draw_list->AddLine(ImVec2(p_max.x, p_min.y), ImVec2(p_min.x, p_max.y), outline_col, 1.5f);
                    }
                }
            }
        }
    }
    draw_list->PopClipRect();
}

// ---------------------------------------------------------
// ОВЕРЛЕИ И КУРСОРЫ (С ПРЕВЬЮ!)
// ---------------------------------------------------------
void EditorWorkspace::DrawCursorAndRulers(ImDrawList* draw_list) {
    draw_list->PushClipRect(grid_start, grid_end, true);

    int map_w = loaded_map.is_loaded ? loaded_map.size_x : 50;
    int map_h = loaded_map.is_loaded ? loaded_map.size_y : 50;
    if (map_w <= 0) map_w = 50;
    if (map_h <= 0) map_h = 50;

    if (show_cursor && grid_hovered && hover_tx != -1 && hover_ty != -1) {
        ImU32 prev_col = IM_COL32(255, 255, 255, 180); // Полупрозрачный для превью

        // 1. Отрисовка превью объекта ПЕРЕМЕЩЕНИЯ (MoveMode)
        if (current_move_mode != MoveMode::None) {
            ImVec2 mpos = ImGui::GetIO().MousePos;

            if (current_move_mode == MoveMode::Building) {
                GameObject* obj = GetObjectById(move_bld.id, ObjType::Functional);
                if (obj && obj->texture) {
                    ImVec2 offset = GetBuildingVisualOffset(move_bld.id / 256, move_bld.id % 256, obj->header.mapCellsW, obj->header.mapCellsH);
                    ImVec2 p_max(mpos.x + cell_w * 0.5f - offset.x * editor_zoom, mpos.y + cell_h * 0.5f - offset.y * editor_zoom);
                    ImVec2 p_min(p_max.x - obj->header.width * editor_zoom, p_max.y - obj->header.height * editor_zoom);
                    draw_list->AddImage((ImTextureID)(intptr_t)obj->texture, p_min, p_max, ImVec2(0, 0), ImVec2(1, 1), prev_col);
                }
            }
            else if (current_move_mode == MoveMode::Army || current_move_mode == MoveMode::Lantern || current_move_mode == MoveMode::Hero) {
                EditorTexture tex;
                if (current_move_mode == MoveMode::Army) {
                    tex = GetEditorTextureForArmy(&move_army, move_army.model, hover_tx, hover_ty, true);
                }
                else if (current_move_mode == MoveMode::Lantern) {
                    tex = GetEditorTextureForLantern(&move_lant, move_lant.model);
                }
                else {
                    tex = GetEditorTextureForArmy(nullptr, move_hero_id, hover_tx, hover_ty, true);
                }

                if (tex.id) {
                    float cx = mpos.x;
                    float cy = mpos.y;
                    ImVec2 p_min(cx - (tex.w * editor_zoom) * 0.5f, cy - (tex.h * editor_zoom) * 0.5f);
                    ImVec2 p_max(cx + (tex.w * editor_zoom) * 0.5f, cy + (tex.h * editor_zoom) * 0.5f);
                    draw_list->AddImage((ImTextureID)(intptr_t)tex.id, p_min, p_max, ImVec2(0, 0), ImVec2(1, 1), prev_col);
                }
            }
        }
        // 2. Отрисовка превью КИСТИ 
        else if (current_tool == ToolMode::Brush) {
            if ((active_tab == 1 || active_tab == 2 || active_tab == 3) && current_object_idx >= 0 && current_object_idx < (int)obj_manager->objects.size()) {
                auto& obj = obj_manager->objects[current_object_idx];
                if (obj.texture) {
                    int bw = current_brush_w; int bh = current_brush_h;
                    int offset_x = (bw - 1) / 2; int offset_y = (bh - 1) / 2;

                    if (active_tab == 2 && (bw > 1 || bh > 1)) {
                        int start_x = hover_tx - offset_x; int start_y = hover_ty - offset_y;
                        for (int y = 0; y < bh; ++y) {
                            for (int x = 0; x < bw; ++x) {
                                int draw_x = start_x + x;
                                int draw_y = start_y + y;
                                ImVec2 p_max(grid_start.x + (draw_x - cam_x + 1) * cell_w, grid_start.y + (draw_y - cam_y + 1) * cell_h);
                                ImVec2 p_min(p_max.x - obj.header.width * editor_zoom, p_max.y - obj.header.height * editor_zoom);
                                draw_list->AddImage((ImTextureID)(intptr_t)obj.texture, p_min, p_max, ImVec2(0, 0), ImVec2(1, 1), prev_col);
                            }
                        }
                    }
                    else {
                        int anchor_x = hover_tx - offset_x + bw - 1; int anchor_y = hover_ty - offset_y + bh - 1;
                        if (active_tab == 3) { anchor_x = hover_tx; anchor_y = hover_ty; }

                        ImVec2 offset(0, 0);
                        if (active_tab == 3) {
                            offset = GetBuildingVisualOffset(obj.header.groupId, obj.header.objectId, obj.header.mapCellsW, obj.header.mapCellsH);
                        }
                        else if (active_tab == 1 || (obj.header.groupId >= 1 && obj.header.groupId <= 8)) {
                            offset = ImVec2(-16.0f, -11.0f);
                        }
                        ImVec2 p_max(grid_start.x + (anchor_x - cam_x + 1) * cell_w - offset.x * editor_zoom,
                            grid_start.y + (anchor_y - cam_y + 1) * cell_h - offset.y * editor_zoom);
                        ImVec2 p_min(p_max.x - obj.header.width * editor_zoom, p_max.y - obj.header.height * editor_zoom);
                        draw_list->AddImage((ImTextureID)(intptr_t)obj.texture, p_min, p_max, ImVec2(0, 0), ImVec2(1, 1), prev_col);
                    }
                }
            }
            else if (active_tab == 4 && current_object_idx != -1) {
                EditorTexture tex;
                int i = current_object_idx;

                if (i <= 6) {
                    uint8_t m_id = 1;
                    if (i == 0) m_id = 1; else if (i == 1) m_id = 2; else if (i == 2) m_id = 3;
                    else if (i == 3) m_id = 4; else if (i == 4) m_id = 5; else if (i == 5) m_id = 6; else if (i == 6) m_id = 7;
                    tex = GetEditorTextureForArmy(nullptr, m_id, hover_tx, hover_ty, true);
                }
                else if (i == 7) {
                    tex = GetEditorTextureForLantern(nullptr, 8); // Фонарик
                }
                else if (i == 8) {
                    tex = GetEditorTextureForLantern(nullptr, 9); // Точка события
                }

                if (tex.id) {
                    float cx = grid_start.x + (hover_tx - cam_x) * cell_w + cell_w * 0.5f;
                    float cy = grid_start.y + (hover_ty - cam_y) * cell_h + cell_h * 0.5f;
                    ImVec2 p_min(cx - (tex.w * editor_zoom) * 0.5f, cy - (tex.h * editor_zoom) * 0.5f);
                    ImVec2 p_max(cx + (tex.w * editor_zoom) * 0.5f, cy + (tex.h * editor_zoom) * 0.5f);
                    draw_list->AddImage((ImTextureID)(intptr_t)tex.id, p_min, p_max, ImVec2(0, 0), ImVec2(1, 1), prev_col);
                }
            }
        }

        // 3. Отрисовка рамки самого курсора / выделения
        int bw = current_brush_w; int bh = current_brush_h;
        if (active_tab == 4) { bw = 1; bh = 1; }

        if (active_tab == 3) {
            if (current_tool == ToolMode::Info || current_tool == ToolMode::Eraser) {
                // Подсветка при наведении на существующее строение
                for (auto& b : map_buildings) {
                    GameObject* obj = GetObjectById(b.id, ObjType::Functional);
                    int w = obj ? obj->header.mapCellsW : 1, h = obj ? obj->header.mapCellsH : 1;
                    if (hover_tx >= b.x - w + 1 && hover_tx <= b.x && hover_ty >= b.y - h + 1 && hover_ty <= b.y) {
                        for (int cy = 0; cy < h; ++cy) {
                            for (int cx = 0; cx < w; ++cx) {
                                ImVec2 pm(grid_start.x + (b.x - w + 1 + cx - cam_x) * cell_w, grid_start.y + (b.y - h + 1 + cy - cam_y) * cell_h);
                                if (tex_manager && tex_manager->cursor_texture) draw_list->AddImage((ImTextureID)(intptr_t)tex_manager->cursor_texture, pm, ImVec2(pm.x + cell_w, pm.y + cell_h), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 180));
                                else draw_list->AddRectFilled(pm, ImVec2(pm.x + cell_w, pm.y + cell_h), IM_COL32(255, 255, 255, 100));
                            }
                        }
                        break;
                    }
                }
            }
            else if (current_tool == ToolMode::Brush && current_object_idx >= 0 && current_object_idx < (int)obj_manager->objects.size()) {
                // Подсветка сетки при размещении нового строения (кистью)
                auto& obj = obj_manager->objects[current_object_idx];
                int w = obj.header.mapCellsW, h = obj.header.mapCellsH;
                for (int cy = 0; cy < h; ++cy) {
                    for (int cx = 0; cx < w; ++cx) {
                        ImVec2 pm(grid_start.x + (hover_tx - w + 1 + cx - cam_x) * cell_w, grid_start.y + (hover_ty - h + 1 + cy - cam_y) * cell_h);
                        if (tex_manager && tex_manager->cursor_texture) draw_list->AddImage((ImTextureID)(intptr_t)tex_manager->cursor_texture, pm, ImVec2(pm.x + cell_w, pm.y + cell_h), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 180));
                        else draw_list->AddRectFilled(pm, ImVec2(pm.x + cell_w, pm.y + cell_h), IM_COL32(255, 255, 255, 100));
                    }
                }
            }
        }
        else if (current_tool == ToolMode::Fill && active_tab == 0) {
            // Для заливки подсвечиваем только ячейку под курсором
            ImVec2 pm(grid_start.x + (hover_tx - cam_x) * cell_w, grid_start.y + (hover_ty - cam_y) * cell_h);
            if (tex_manager && tex_manager->cursor_texture) draw_list->AddImage((ImTextureID)(intptr_t)tex_manager->cursor_texture, pm, ImVec2(pm.x + cell_w, pm.y + cell_h), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 180));
            else draw_list->AddRectFilled(pm, ImVec2(pm.x + cell_w, pm.y + cell_h), IM_COL32(255, 255, 255, 100));
        }
        else if (current_tool == ToolMode::Eraser || (current_tool == ToolMode::Brush && active_tab == 0)) {
            // Рисуем рамку "плиткой" для кисти ландшафта (вкладка 0) и для ластика
            int offset_x = (bw - 1) / 2; int offset_y = (bh - 1) / 2;
            for (int y = 0; y < bh; ++y) {
                for (int x = 0; x < bw; ++x) {
                    ImVec2 pm(grid_start.x + (hover_tx - offset_x + x - cam_x) * cell_w, grid_start.y + (hover_ty - offset_y + y - cam_y) * cell_h);
                    if (tex_manager && tex_manager->cursor_texture) {
                        draw_list->AddImage((ImTextureID)(intptr_t)tex_manager->cursor_texture, pm, ImVec2(pm.x + cell_w, pm.y + cell_h), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 180));
                    }
                    else {
                        draw_list->AddRectFilled(pm, ImVec2(pm.x + cell_w, pm.y + cell_h), IM_COL32(255, 255, 255, 100));
                    }
                }
            }
        }
    }
    draw_list->PopClipRect();

    int step_x = 1; if (cell_w < 30.0f) step_x = 2; if (cell_w < 15.0f) step_x = 5; if (cell_w < 8.0f) step_x = 10;
    int step_y = 1; if (cell_h < 20.0f) step_y = 2; if (cell_h < 10.0f) step_y = 5;

    draw_list->PushClipRect(ImVec2(grid_start.x, 0), ImVec2(grid_end.x, grid_start.y), true);
    for (int x = 0; x < VIEW_COLS; ++x) {
        int tx = cam_x + x; if (tx >= map_w) break;
        bool is_hovered = (tx == hover_tx);
        if (!is_hovered && (tx % step_x != 0 || (hover_tx != -1 && std::abs(tx - hover_tx) * cell_w < 24.0f))) continue;
        char buf[16]; snprintf(buf, sizeof(buf), "%d", tx); float txt_w = ImGui::CalcTextSize(buf).x;
        if (is_hovered) draw_list->AddRectFilled(ImVec2(grid_start.x + x * cell_w, grid_start.y - 22.0f), ImVec2(grid_start.x + (x + 1) * cell_w, grid_start.y - 2.0f), IM_COL32(0, 120, 255, 200));
        draw_list->AddText(ImVec2(grid_start.x + x * cell_w + (cell_w - txt_w) * 0.5f, grid_start.y - 20.0f), is_hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 255), buf);
    }
    draw_list->PopClipRect();

    draw_list->PushClipRect(ImVec2(0, grid_start.y), ImVec2(grid_start.x, grid_end.y), true);
    for (int y = 0; y < VIEW_ROWS; ++y) {
        int ty = cam_y + y; if (ty >= map_h) break;
        bool is_hovered = (ty == hover_ty);
        if (!is_hovered && (ty % step_y != 0 || (hover_ty != -1 && std::abs(ty - hover_ty) * cell_h < 16.0f))) continue;
        char buf[16]; snprintf(buf, sizeof(buf), "%d", ty); float txt_w = ImGui::CalcTextSize(buf).x;
        if (is_hovered) draw_list->AddRectFilled(ImVec2(grid_start.x - std::max(30.0f, txt_w + 10.0f), grid_start.y + y * cell_h), ImVec2(grid_start.x - 2.0f, grid_start.y + (y + 1) * cell_h), IM_COL32(0, 120, 255, 200));
        draw_list->AddText(ImVec2(grid_start.x - txt_w - 5.0f, grid_start.y + y * cell_h + (cell_h - ImGui::GetFontSize()) * 0.5f), is_hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 255), buf);
    }
    draw_list->PopClipRect();

    // -------------------------------------------------------------
    // ГОРИЗОНТАЛЬНЫЙ СКРОЛЛБАР С КНОПКАМИ ШАГА НА 1 КЛЕТКУ
    // -------------------------------------------------------------
    float avail_w = grid_end.x - grid_start.x;
    float avail_h = grid_end.y - grid_start.y;
    ImVec2 h_sb_min(grid_start.x, grid_end.y + 5.0f);
    ImVec2 h_sb_max(grid_end.x, grid_end.y + 25.0f);

    ImGui::PushButtonRepeat(true); // Позволяет зажимать кнопки для непрерывного шага

    // Кнопка Влево [<] (шаг на -1)
    ImGui::SetCursorScreenPos(ImVec2(grid_start.x, h_sb_min.y));
    if (ImGui::Button("<##h_left", ImVec2(20.0f, 20.0f))) {
        cam_x = std::max(0, cam_x - 1);
    }

    // Кнопка Вправо [>] (шаг на +1)
    ImGui::SetCursorScreenPos(ImVec2(grid_end.x - 20.0f, h_sb_min.y));
    if (ImGui::Button(">##h_right", ImVec2(20.0f, 20.0f))) {
        cam_x = std::min(std::max(0, map_w - VIEW_COLS), cam_x + 1);
    }

    float track_start_x = grid_start.x + 22.0f;
    float track_avail_w = avail_w - 44.0f;

    draw_list->AddRectFilled(ImVec2(track_start_x, h_sb_min.y), ImVec2(track_start_x + track_avail_w, h_sb_max.y), IM_COL32(40, 40, 40, 255));

    if (map_w > VIEW_COLS && track_avail_w > 20.0f) {
        float thumb_w = std::max(20.0f, std::min(1.0f, (float)VIEW_COLS / map_w) * track_avail_w);
        float bar_avail_w = track_avail_w - thumb_w;
        float thumb_x = track_start_x + ((float)cam_x / (map_w - VIEW_COLS)) * bar_avail_w;

        ImGui::SetCursorScreenPos(ImVec2(track_start_x, h_sb_min.y));
        ImGui::InvisibleButton("h_scroll", ImVec2(track_avail_w, 20.0f));

        if (ImGui::IsItemActive()) {
            cam_x = (int)(std::clamp((ImGui::GetIO().MousePos.x - track_start_x - thumb_w / 2.0f) / bar_avail_w, 0.0f, 1.0f) * (map_w - VIEW_COLS));
        }

        draw_list->AddRectFilled(ImVec2(thumb_x, h_sb_min.y + 2), ImVec2(thumb_x + thumb_w, h_sb_max.y - 2),
            ImGui::IsItemActive() || ImGui::IsItemHovered() ? IM_COL32(150, 150, 150, 255) : IM_COL32(100, 100, 100, 255));
    }

    // -------------------------------------------------------------
    // ВЕРТИКАЛЬНЫЙ СКРОЛЛБАР С КНОПКАМИ ШАГА НА 1 КЛЕТКУ
    // -------------------------------------------------------------
    ImVec2 v_sb_min(grid_end.x + 5.0f, grid_start.y);
    ImVec2 v_sb_max(grid_end.x + 25.0f, grid_end.y);

    // Кнопка Вверх [^] (шаг на -1)
    ImGui::SetCursorScreenPos(ImVec2(v_sb_min.x, grid_start.y));
    if (ImGui::Button("^##v_up", ImVec2(20.0f, 20.0f))) {
        cam_y = std::max(0, cam_y - 1);
    }

    // Кнопка Вниз [v] (шаг на +1)
    ImGui::SetCursorScreenPos(ImVec2(v_sb_min.x, grid_end.y - 20.0f));
    if (ImGui::Button("v##v_down", ImVec2(20.0f, 20.0f))) {
        cam_y = std::min(std::max(0, map_h - VIEW_ROWS), cam_y + 1);
    }

    ImGui::PopButtonRepeat(); // Выключаем автоповтор кнопок

    float track_start_y = grid_start.y + 22.0f;
    float track_avail_h = avail_h - 44.0f;

    draw_list->AddRectFilled(ImVec2(v_sb_min.x, track_start_y), ImVec2(v_sb_max.x, track_start_y + track_avail_h), IM_COL32(40, 40, 40, 255));

    if (map_h > VIEW_ROWS && track_avail_h > 20.0f) {
        float thumb_h = std::max(20.0f, std::min(1.0f, (float)VIEW_ROWS / map_h) * track_avail_h);
        float bar_avail_h = track_avail_h - thumb_h;
        float thumb_y = track_start_y + ((float)cam_y / (map_h - VIEW_ROWS)) * bar_avail_h;

        ImGui::SetCursorScreenPos(ImVec2(v_sb_min.x, track_start_y));
        ImGui::InvisibleButton("v_scroll", ImVec2(20.0f, track_avail_h));

        if (ImGui::IsItemActive()) {
            cam_y = (int)(std::clamp((ImGui::GetIO().MousePos.y - track_start_y - thumb_h / 2.0f) / bar_avail_h, 0.0f, 1.0f) * (map_h - VIEW_ROWS));
        }

        draw_list->AddRectFilled(ImVec2(v_sb_min.x + 2, thumb_y), ImVec2(v_sb_max.x - 2, thumb_y + thumb_h),
            ImGui::IsItemActive() || ImGui::IsItemHovered() ? IM_COL32(150, 150, 150, 255) : IM_COL32(100, 100, 100, 255));
    }
}

void EditorWorkspace::DrawTooltips() {
    if (!loaded_map.is_loaded || hover_tx == -1 || hover_ty == -1) return;
    if (current_move_mode != MoveMode::None) return; // Не показываем тултипы при перетаскивании

    ImVec2 mpos = ImGui::GetIO().MousePos;
    if (mpos.x < grid_start.x || mpos.x > grid_end.x || mpos.y < grid_start.y || mpos.y > grid_end.y) return;

    bool has_tooltip = false;

    // 1. Собираем все Армии в этой точке
    for (size_t i = 0; i < map_armies.size(); ++i) {
        auto& a = map_armies[i];
        if (a.x == hover_tx && a.y == hover_ty) {
            if (!has_tooltip) { ImGui::BeginTooltip(); has_tooltip = true; }
            else { ImGui::Separator(); }

            std::string army_name = (i < loaded_map.army_texts.size()) ? cp1251_to_utf8(loaded_map.army_texts[i].name) : "";
            if (army_name.empty()) army_name = "Без имени";

            ImGui::Text("#%d %s", a.id, army_name.c_str());
            ImGui::Text("%d / %d", a.tac_cost_1, a.tac_cost_2);
        }
    }

    // 2. Собираем Фонарики/Мины в этой точке
    for (size_t i = 0; i < map_lanterns.size(); ++i) {
        auto& l = map_lanterns[i];
        if (l.x == hover_tx && l.y == hover_ty) {
            if (!has_tooltip) { ImGui::BeginTooltip(); has_tooltip = true; }
            else { ImGui::Separator(); }

            ImGui::Text("Событие/Фонарик #%d", l.id);

            bool is_mine = l.event_count > 0;
            bool is_lantern = l.radius > 0; // Строго только если радиус больше нуля

            if (is_mine) {
                ImGui::Text("Игровых событий: %d", l.event_count);
                for (int e = 0; e < l.event_count; ++e) {
                    int eid = l.events[e];
                    std::string ev_name = "Неизвестно";
                    if (eid > 0 && eid - 1 < (int)loaded_map.event_texts.size()) {
                        ev_name = cp1251_to_utf8(loaded_map.event_texts[eid - 1].name);
                    }
                    if (ev_name.empty()) ev_name = "Без названия";
                    ImGui::Text(" - %s", ev_name.c_str());
                }
            }

            if (is_lantern) {
                int lantern_idx = 1;
                for (size_t j = 0; j < i; ++j) {
                    // Считаем для нумерации только те объекты, которые реально светят
                    if (map_lanterns[j].radius > 0) lantern_idx++;
                }
                ImGui::Text("Фонарик #%d, R%d", lantern_idx, l.radius);
            }
        }
    }

    // 3. Собираем Строения (проверяем попадание в любую клетку строения)
    for (size_t i = 0; i < map_buildings.size(); ++i) {
        auto& b = map_buildings[i];
        GameObject* obj = GetObjectById(b.id, ObjType::Functional);
        int w = obj ? obj->header.mapCellsW : 1;
        int h = obj ? obj->header.mapCellsH : 1;
        int bx = (int)b.x;
        int by = (int)b.y;

        if (hover_tx >= bx - w + 1 && hover_tx <= bx && hover_ty >= by - h + 1 && hover_ty <= by) {
            if (!has_tooltip) { ImGui::BeginTooltip(); has_tooltip = true; }
            else { ImGui::Separator(); }

            std::string b_name = (i < loaded_map.building_texts.size()) ? cp1251_to_utf8(loaded_map.building_texts[i].name) : "Без названия";
            if (b_name.empty()) b_name = "Без названия";

            uint32_t gid = b.id / 256;
            uint32_t oid = b.id % 256;

            ImGui::Text("#%d %s (%d - %d - %d)", (int)(i + 1), b_name.c_str(), b.id, gid, oid);

            if (b.owner == 0 || b.owner == 255) {
                std::string owner_text = "";
                if (i < loaded_map.building_texts.size()) {
                    owner_text = cp1251_to_utf8(loaded_map.building_texts[i].owner);
                }
                // Строка отобразится только если есть текстовое имя владельца у нейтрального здания
                if (!owner_text.empty()) {
                    ImGui::Text("Владелец: %s", owner_text.c_str());
                }
            }
            else {
                std::string owner_army_name = "";
                std::string owner_leader = "";
                for (size_t a = 0; a < map_armies.size(); ++a) {
                    if (map_armies[a].id == b.owner) {
                        if (a < loaded_map.army_texts.size()) {
                            owner_army_name = cp1251_to_utf8(loaded_map.army_texts[a].name);
                            owner_leader = cp1251_to_utf8(loaded_map.army_texts[a].leader);
                        }
                        break;
                    }
                }

                if (owner_leader.empty() && owner_army_name.empty()) {
                    ImGui::Text("Владелец: Армия #%d", b.owner);
                }
                else if (owner_leader.empty()) {
                    ImGui::Text("Владелец: Армия #%d %s", b.owner, owner_army_name.c_str());
                }
                else if (owner_army_name.empty()) {
                    ImGui::Text("Владелец: Армия #%d %s", b.owner, owner_leader.c_str());
                }
                else {
                    ImGui::Text("Владелец: Армия #%d %s / %s", b.owner, owner_leader.c_str(), owner_army_name.c_str());
                }
            }

            bool has_desc = (i < loaded_map.building_texts.size() && !loaded_map.building_texts[i].desc.empty());
            ImGui::Text("+%d Золота, +%d Маны%s", b.gold_income, b.mana_income, has_desc ? " (текст там)" : "");
        }
    }

    if (has_tooltip) {
        ImGui::EndTooltip();
    }
}

// ---------------------------------------------------------
// ПАНЕЛЬ ИНСТРУМЕНТОВ
// ---------------------------------------------------------
void EditorWorkspace::DrawObjectGrid(int tab_idx, int start_idx, int end_idx, float panel_w, float spacing) {
    if (!obj_manager || obj_manager->objects.empty()) { ImGui::TextDisabled("Файл Objects.ugs не загружен"); return; }
    float icon_size = 48.0f;
    // Учитываем реальный размер кнопки с отступами рамки
    float item_w = icon_size + ImGui::GetStyle().FramePadding.x * 2.0f;
    float avail_w = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, (int)((avail_w + spacing) / (item_w + spacing)));
    int drawn = 0;
    ObjType target_type = (tab_idx == 3) ? ObjType::Functional : ObjType::Decor;

    for (int i = start_idx; i <= end_idx && i < (int)obj_manager->objects.size(); ++i) {
        auto& obj = obj_manager->objects[i];
        if (obj.isEmpty || obj.texture == 0 || obj.header.type != target_type) continue;

        if (tab_idx == 1) {
            if (hill_filter_idx > 0 && obj.header.groupId != hill_filter_idx) continue;

            // Для холмов берем максимальное значение из X и Y как ориентир для размера кисти
            int brush_s = std::max(current_brush_w, current_brush_h);
            if (current_tool == ToolMode::Brush && brush_s >= 1 && brush_s <= 6) {
                uint32_t min_id = (uint32_t)(brush_s * 10);
                uint32_t max_id = min_id + 9;
                if (obj.header.objectId < min_id || obj.header.objectId > max_id) continue;
            }
        }
        else if (tab_idx == 2) {
            // Если хотя бы одна из сторон больше 1, показываем составные леса
            if ((current_brush_w > 1 || current_brush_h > 1) && current_tool != ToolMode::Info) {
                uint32_t compound_ids[] = { 2304, 2316, 2328, 2340, 2352, 2364, 2376, 2388, 2668, 2816, 2828, 2840, 2852, 2900, 2924 };
                uint32_t full_id = obj.header.groupId * 256 + obj.header.objectId;
                bool found = false; for (uint32_t cid : compound_ids) { if (full_id == cid) { found = true; break; } }
                if (!found) continue;
            }
        }
        else if (tab_idx == 3) {
            if (building_filter_idx > 0 && obj.header.groupId != building_filter_idx) continue;
        }

        if (drawn % cols != 0) ImGui::SameLine();
        ImGui::PushID(i);
        bool is_sel = (current_object_idx == i);
        if (is_sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
        if (ImGui::ImageButton("##obj", (ImTextureID)(intptr_t)obj.texture, ImVec2(icon_size, icon_size))) {
            current_object_idx = i;
            if (current_tool == ToolMode::Info) {
                current_tool = ToolMode::Brush;
                current_brush_w = 1;
                current_brush_h = 1;
            }
            else current_tool = ToolMode::Brush;
        }
        if (is_sel) ImGui::PopStyleColor();
        ImGui::PopID(); drawn++;
    }
    if (drawn == 0) ImGui::TextDisabled("Нет подходящих объектов");
}
void EditorWorkspace::DrawSquadsGrid(float panel_w, float spacing) {
    const char* names[9] = { "ГГ Рыцарь", "ГГ Архимаг", "ГГ Следопыт", "Феодал", "Бандит", "Крестьянин", "Неакт. Армия", "Фонарик", "Точка события" };
    float icon_size = 48.0f; int cols = std::max(1, (int)(panel_w / (icon_size + spacing)));

    for (int i = 0; i < 9; ++i) {
        if (i % cols != 0) ImGui::SameLine();
        bool disabled = false;
        if (i == 0 && loaded_map.knight.pos_x == 0) disabled = true;
        if (i == 1 && loaded_map.archmage.pos_x == 0) disabled = true;
        if (i == 2 && loaded_map.ranger.pos_x == 0) disabled = true;

        ImGui::PushID(i + 1000);
        if (disabled) ImGui::BeginDisabled();
        bool is_sel = (current_object_idx == i && active_tab == 4);
        if (is_sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
        if (ImGui::ImageButton("##sq", (ImTextureID)(intptr_t)tex_manager->tex_editor[i].id, ImVec2(icon_size, icon_size))) {
            current_object_idx = i;
            if (current_tool == ToolMode::Info) {
                current_tool = ToolMode::Brush;
                current_brush_w = 1;
                current_brush_h = 1;
            }
            else current_tool = ToolMode::Brush;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", names[i]);
        if (is_sel) ImGui::PopStyleColor();
        if (disabled) ImGui::EndDisabled();
        ImGui::PopID();
    }
}

void EditorWorkspace::RenderRightPanel(const ImVec2& work_size, float top_margin, float bottom_margin) {
    // Было: ImGui::SetCursorPos(...); ImGui::BeginChild("ToolPanel", ...);
    // Стало:
    ImGui::Begin("Инструменты");

    bool map_loaded = loaded_map.is_loaded;
    if (!map_loaded) ImGui::BeginDisabled();

    float panel_w = ImGui::GetContentRegionAvail().x;
    float mm_size_px = 220.0f;

    // 1. Избегаем дробного (искажающего) масштабирования!
    // Определяем размеры карты
    int mw = map_loaded ? loaded_map.size_x : 50;
    int mh = map_loaded ? loaded_map.size_y : 50;
    if (mw <= 0) mw = 50;
    if (mh <= 0) mh = 50;

    // Максимально доступная ширина панели
    float max_avail_w = std::max(220.0f, panel_w - 16.0f);

    // Вычисляем строго ЦЕЛОЧИСЛЕННЫЙ масштаб (1x, 2x, 3x, 4x) для идеальных пикселей
    int mm_scale = std::max(1, (int)std::min(max_avail_w / mw, max_avail_w / mh));

    float mm_draw_w = mw * mm_scale;
    float mm_draw_h = mh * mm_scale;

    const char* map_title = "Глобальная карта";
    float center_offset = std::max(0.0f, (panel_w - mm_draw_w) * 0.5f);

    ImGui::SetCursorPosX(center_offset + (mm_draw_w - ImGui::CalcTextSize(map_title).x) * 0.5f);
    ImGui::Text("%s", map_title);

    ImGui::SetCursorPosX(center_offset);
    ImVec2 mm_pos = ImGui::GetCursorScreenPos();

    // 2. Округляем координаты окна для Pixel-Perfect отрисовки в экранной сетке
    mm_pos.x = std::floor(mm_pos.x);
    mm_pos.y = std::floor(mm_pos.y);

    ImGui::InvisibleButton("minimap_btn", ImVec2(mm_draw_w, mm_draw_h));

    if (ImGui::IsItemActive() && map_loaded) {
        cam_x = std::clamp((int)((ImGui::GetIO().MousePos.x - mm_pos.x) / mm_scale) - (VIEW_COLS / 2), 0, std::max(0, mw - VIEW_COLS));
        cam_y = std::clamp((int)((ImGui::GetIO().MousePos.y - mm_pos.y) / mm_scale) - (VIEW_ROWS / 2), 0, std::max(0, mh - VIEW_ROWS));
    }

    ImDrawList* fg_draw = ImGui::GetWindowDrawList();
    fg_draw->AddRectFilled(mm_pos, ImVec2(mm_pos.x + mm_draw_w, mm_pos.y + mm_draw_h), IM_COL32(20, 20, 20, 255));

    if (map_loaded) {
        ImVec2 ms = mm_pos;
        ImVec2 me = ImVec2(ms.x + mm_draw_w, ms.y + mm_draw_h);

        static GLuint minimap_tex = 0;
        static uint32_t mm_last_w = 0, mm_last_h = 0;
        static std::vector<uint32_t> mm_pixels;

        // Фактический размер текстуры с учетом масштабирования
        int final_w = mw * mm_scale;
        int final_h = mh * mm_scale;

        if (minimap_tex == 0 || mm_last_w != final_w || mm_last_h != final_h) {
            if (minimap_tex != 0) glDeleteTextures(1, &minimap_tex);
            glGenTextures(1, &minimap_tex);
            glBindTexture(GL_TEXTURE_2D, minimap_tex);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, final_w, final_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            // Фильтрация все еще отключена на всякий случай
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);

            mm_last_w = final_w;
            mm_last_h = final_h;
            mm_pixels.resize(final_w * final_h);
        }

        // Лямбда-функция для заливки квадрата 4х4 (или другого размера в зависимости от mm_scale)
        auto DrawScaledPixel = [&](int map_x, int map_y, uint32_t col) {
            int start_x = map_x * mm_scale;
            int start_y = map_y * mm_scale;
            for (int dy = 0; dy < mm_scale; ++dy) {
                int py = start_y + dy;
                for (int dx = 0; dx < mm_scale; ++dx) {
                    mm_pixels[py * final_w + start_x + dx] = col;
                }
            }
            };

        // 3. Запись тайлов поверхности
        for (uint32_t y = 0; y < mh; ++y) {
            for (uint32_t x = 0; x < mw; ++x) {
                size_t idx = (size_t)y * mw + x;
                uint8_t t_id = (idx < loaded_map.surface_tiles.size()) ? loaded_map.surface_tiles[idx] : 0;
                DrawScaledPixel(x, y, GetTileColor(t_id));
            }
        }

        // 4. Запись декораций
        for (const auto& d : map_decors) {
            GameObject* obj = GetObjectById(d.id, ObjType::Decor);
            if (obj) {
                uint32_t c = obj->header.special;
                uint8_t r = c & 0xFF;
                uint8_t g = (c >> 8) & 0xFF;
                uint8_t b = (c >> 16) & 0xFF;
                uint8_t lum = (c >> 24) & 0xFF;

                if (lum > 0) {
                    float max_c = (float)std::max({ r, g, b });
                    if (max_c > 0.0f) {
                        float l_scale = (float)lum / max_c;
                        r = (uint8_t)std::clamp((int)(r * l_scale), 0, 255);
                        g = (uint8_t)std::clamp((int)(g * l_scale), 0, 255);
                        b = (uint8_t)std::clamp((int)(b * l_scale), 0, 255);
                    }
                }
                uint32_t color = IM_COL32(r, g, b, 255);

                uint32_t gid = d.id / 256;
                uint32_t oid = d.id % 256;
                int obj_size = 1;
                if (gid <= 8) obj_size = std::max(1, (int)(oid / 10));

                int min_x = std::clamp((int)d.x - obj_size + 1, 0, (int)mw);
                int min_y = std::clamp((int)d.y - obj_size + 1, 0, (int)mh);
                int max_x = std::clamp((int)d.x + 1, 0, (int)mw);
                int max_y = std::clamp((int)d.y + 1, 0, (int)mh);

                for (int py = min_y; py < max_y; ++py) {
                    for (int px = min_x; px < max_x; ++px) {
                        DrawScaledPixel(px, py, color);
                    }
                }
            }
        }

        // 5. Запись строений
        for (const auto& b : map_buildings) {
            GameObject* obj = GetObjectById(b.id, ObjType::Functional);
            int w = obj ? obj->header.mapCellsW : 1;
            int h = obj ? obj->header.mapCellsH : 1;

            uint32_t gid = b.id / 256;
            uint32_t color = IM_COL32(220, 220, 220, 255);
            if (gid == 3 || gid == 4) color = IM_COL32(255, 50, 50, 255);
            else if (gid == 2) color = IM_COL32(255, 255, 50, 255);

            int min_x = std::clamp((int)b.x - w + 1, 0, (int)mw);
            int min_y = std::clamp((int)b.y - h + 1, 0, (int)mh);
            int max_x = std::clamp((int)b.x + 1, 0, (int)mw);
            int max_y = std::clamp((int)b.y + 1, 0, (int)mh);

            for (int py = min_y; py < max_y; ++py) {
                for (int px = min_x; px < max_x; ++px) {
                    DrawScaledPixel(px, py, color);
                }
            }
        }

        // Передача готовой УЖЕ РАСТЯНУТОЙ текстуры в видеокарту
        glBindTexture(GL_TEXTURE_2D, minimap_tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, final_w, final_h, GL_RGBA, GL_UNSIGNED_BYTE, mm_pixels.data());

        // Рисуем ВСЮ миникарту ОДНИМ вызовом 1:1 по пикселям экрана
        fg_draw->AddImage((ImTextureID)(intptr_t)minimap_tex, ms, me);

        if (ImGui::IsItemHovered()) {
            int tx = std::clamp((int)((ImGui::GetIO().MousePos.x - ms.x) / mm_scale) - (VIEW_COLS / 2), 0, std::max(0, mw - VIEW_COLS));
            int ty = std::clamp((int)((ImGui::GetIO().MousePos.y - ms.y) / mm_scale) - (VIEW_ROWS / 2), 0, std::max(0, mh - VIEW_ROWS));
            fg_draw->AddRect(ImVec2(ms.x + tx * mm_scale, ms.y + ty * mm_scale), ImVec2(ms.x + (tx + std::min(VIEW_COLS, mw)) * mm_scale, ms.y + (ty + std::min(VIEW_ROWS, mh)) * mm_scale), IM_COL32(0, 0, 0, 255), 0.0f, 0, 2.0f);
        }
        fg_draw->AddRect(ImVec2(ms.x + cam_x * mm_scale, ms.y + cam_y * mm_scale), ImVec2(ms.x + (cam_x + std::min(VIEW_COLS, mw)) * mm_scale, ms.y + (cam_y + std::min(VIEW_ROWS, mh)) * mm_scale), IM_COL32(255, 255, 255, 255), 0.0f, 0, 1.5f);
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    const char* brush_title = "Размеры кисти рисования"; ImGui::SetCursorPosX((panel_w - ImGui::CalcTextSize(brush_title).x) * 0.5f); ImGui::Text("%s", brush_title); ImGui::Dummy(ImVec2(0.0f, 2.0f));

    float spacing = ImGui::GetStyle().ItemSpacing.x; float btn_w = (mm_size_px - spacing * 3) / 4.0f;

    if (active_tab == 3) {
        ImGui::SetCursorPosX(center_offset); ImGui::SetNextItemWidth(btn_w * 4 + spacing * 3);
        const char* b_filters[15] = { "1. Все", "2. Город", "3. Деревня", "4. Замок", "5. Форт", "6. Таверна", "7. Рынок", "8. Церковь", "9. Кузница", "10. Верфь", "11. Алтарь", "12. Шахта", "13. Руина", "14. Каменный мост", "15. Деревянный мост" };
        ImGui::Combo("##build_filter", &building_filter_idx, b_filters, 15);
        ImGui::SetCursorPosX(center_offset + (btn_w + spacing) * 2);
    }
    else {
        if (active_tab == 1) {
            ImGui::SetCursorPosX(center_offset); ImGui::SetNextItemWidth(btn_w * 4 + spacing * 3);
            const char* h_filters[9] = { "0. Все", "1. Травяной холм", "2. Степной холм", "3. Выжженный холм", "4. Песчаный холм", "5. Травяная гора", "6. Выжженная гора", "7. Песчаная гора", "8. Скалы" };
            ImGui::Combo("##hill_filter", &hill_filter_idx, h_filters, 9);
        }
        const char* size_labels[] = { "1x1", "2x2", "3x3", "4x4", "5x5", "6x6" };
        for (int i = 0; i < 6; ++i) {
            if (i == 0 || i == 4) ImGui::SetCursorPosX(center_offset); else ImGui::SameLine();
            bool disabled = (active_tab == 4 && i > 0);
            if (disabled) ImGui::BeginDisabled();
            bool is_sel = (current_tool == ToolMode::Brush && current_brush_w == i + 1 && current_brush_h == i + 1);
            if (is_sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
            if (ImGui::Button(size_labels[i], ImVec2(btn_w, 26.0f))) { current_tool = ToolMode::Brush; current_brush_w = i + 1; current_brush_h = i + 1; }
            if (is_sel) ImGui::PopStyleColor();
            if (disabled) ImGui::EndDisabled();
        }
        ImGui::SameLine();
    }

    bool del_disabled = (active_tab == 0);
    if (del_disabled) { ImGui::BeginDisabled(); if (current_tool == ToolMode::Eraser) current_tool = ToolMode::Info; }
    bool del_sel = (current_tool == ToolMode::Eraser);
    if (del_sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
    if (ImGui::Button("Del", ImVec2(btn_w, 26.0f))) current_tool = ToolMode::Eraser;
    if (del_sel) ImGui::PopStyleColor();
    if (del_disabled) ImGui::EndDisabled();

    ImGui::SameLine();
    bool info_sel = (current_tool == ToolMode::Info);
    if (info_sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
    if (ImGui::Button("Info", ImVec2(btn_w, 26.0f))) current_tool = ToolMode::Info;
    if (info_sel) ImGui::PopStyleColor();

    // =============== НОВЫЙ БЛОК: ПРОИЗВОЛЬНАЯ КИСТЬ И ЗАЛИВКА ===============
    ImGui::Dummy(ImVec2(0.0f, 5.0f));
    ImGui::SetCursorPosX(center_offset);
    ImGui::TextDisabled("Произвольная кисть:");
    ImGui::SetCursorPosX(center_offset);

    bool custom_brush_disabled = (active_tab == 4);
    if (custom_brush_disabled) ImGui::BeginDisabled();

    ImGui::PushItemWidth(btn_w - 15.0f);
    ImGui::AlignTextToFramePadding(); ImGui::Text("W"); ImGui::SameLine();
    // 0 шаг отключает кнопки [-] и [+] в InputInt
    ImGui::InputInt("##bw", &current_brush_w, 0);
    if (current_brush_w < 1) current_brush_w = 1;

    ImGui::SameLine();
    ImGui::Text("H"); ImGui::SameLine();
    ImGui::InputInt("##bh", &current_brush_h, 0);
    if (current_brush_h < 1) current_brush_h = 1;
    ImGui::PopItemWidth();

    if (custom_brush_disabled) ImGui::EndDisabled();

    ImGui::SameLine();
    bool fill_sel = (current_tool == ToolMode::Fill);
    if (fill_sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
    if (ImGui::Button("Заливка", ImVec2(btn_w * 2 + spacing, 26.0f))) {
        current_tool = ToolMode::Fill;
        if (active_tab != 0) {
            active_tab = 0;
            just_changed_tab = true; // Триггер автоматического открытия вкладки "Тайлы"
        }
    }
    if (fill_sel) ImGui::PopStyleColor();

    // Меню с настройками заливки при клике ПКМ по кнопке "Заливка"
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("FillSettingsPopup");
    }

    if (ImGui::BeginPopup("FillSettingsPopup")) {
        ImGui::Text("Настройки заливки");
        ImGui::Separator();
        ImGui::Checkbox("Ограничить радиус заливки", &fill_limit_radius);
        if (fill_limit_radius) {
            ImGui::SetNextItemWidth(100);
            ImGui::InputInt("Радиус##fill_rad", &fill_radius, 1, 5);
            if (fill_radius < 1) fill_radius = 1;
        }
        ImGui::Separator();
        ImGui::Text("Дополнительно заменять тайлы:");
        for (int i = 0; i < 16; ++i) {
            char buf[32]; snprintf(buf, sizeof(buf), "Тайл %d", i);
            if (i % 2 != 0) ImGui::SameLine(120);
            ImGui::Checkbox(buf, &fill_replace_types[i]);
        }
        ImGui::EndPopup();
    }
    // =========================================================================

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    auto HandleTab = [&](const char* title, int tab_idx) {
        if (just_changed_tab) ImGui::SetNextItemOpen(active_tab == tab_idx);

        bool clicked = ImGui::CollapsingHeader(title);
        if (ImGui::IsItemClicked()) {
            if (active_tab == tab_idx) active_tab = -1;
            else {
                active_tab = tab_idx;
                current_tool = ToolMode::Info; // При смене вкладки переходим в режим Info по умолчанию
                if (tab_idx == 1 || tab_idx == 2) { current_brush_w = 3; current_brush_h = 3; }
                else if (tab_idx == 4) { current_brush_w = 1; current_brush_h = 1; }
            }
            just_changed_tab = true;
        }
        return active_tab == tab_idx;
        };

    // Лямбда-помощник для ограничения высоты раскрытого меню: 
    // резервируем место под оставшиеся заголовки (tabs_below), 
    // чтобы они гарантированно оставались внизу экрана без скролла основного окна.
    auto BeginTabChild = [](const char* id, int tabs_below) {
        float reserve_h = tabs_below * ImGui::GetFrameHeightWithSpacing();
        float h = std::max(20.0f, ImGui::GetContentRegionAvail().y - reserve_h - 2.0f);
        // Добавляем флаг скроллбара, чтобы ширина рассчитывалась корректно с первого кадра
        ImGui::BeginChild(id, ImVec2(0, h), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        };

    if (HandleTab("1. Типы поверхности", 0)) {
        BeginTabChild("##child_tab0", 4);
        float avail_w = ImGui::GetContentRegionAvail().x;
        float icon_w = 48.0f, icon_h = 40.0f;
        float item_w = icon_w + ImGui::GetStyle().FramePadding.x * 2.0f;
        int cols = std::max(1, (int)((avail_w + spacing) / (item_w + spacing)));

        for (int i = 0; i < 16; ++i) {
            if (i % cols != 0) ImGui::SameLine();
            ImGui::PushID(i);

            // Фикс ошибки PopStyleColor: Сохраняем состояние is_sel до отрисовки кнопки
            bool is_sel = (current_surface_type == i);
            if (is_sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));

            if (tex_manager && tex_manager->terrain_textures[i] != 0) {
                if (ImGui::ImageButton("##tex", (ImTextureID)(intptr_t)tex_manager->terrain_textures[i], ImVec2(icon_w, icon_h), ImVec2(0, 0), ImVec2(32.0f / 256.0f, 22.0f / 242.0f))) {
                    current_surface_type = i;
                    if (current_tool == ToolMode::Info || current_tool == ToolMode::Fill) { current_tool = ToolMode::Brush; current_brush_w = 1; current_brush_h = 1; }
                    else current_tool = ToolMode::Brush;
                }
            }
            else {
                if (ImGui::ColorButton("##col", ImGui::ColorConvertU32ToFloat4(GetTileColor(i)), 0, ImVec2(icon_w, icon_h))) {
                    current_surface_type = i;
                    if (current_tool == ToolMode::Info || current_tool == ToolMode::Fill) { current_tool = ToolMode::Brush; current_brush_w = 1; current_brush_h = 1; }
                    else current_tool = ToolMode::Brush;
                }
            }

            // Используем заранее закешированную переменную is_sel, чтобы избежать вылета при смене значения
            if (is_sel) ImGui::PopStyleColor();
            ImGui::PopID();
        }
        ImGui::EndChild();
    }

    if (HandleTab("2. Холмы, горы, скалы", 1)) {
        BeginTabChild("##child_tab1", 3);
        DrawObjectGrid(1, 0, 479, ImGui::GetContentRegionAvail().x, spacing);
        ImGui::EndChild();
    }

    if (HandleTab("3. Леса, деревья, кусты", 2)) {
        BeginTabChild("##child_tab2", 2);
        DrawObjectGrid(2, 480, 1079, ImGui::GetContentRegionAvail().x, spacing);
        ImGui::EndChild();
    }

    if (HandleTab("4. Города, здания, постройки", 3)) {
        BeginTabChild("##child_tab3", 1);
        DrawObjectGrid(3, 1080, 1219, ImGui::GetContentRegionAvail().x, spacing);
        ImGui::EndChild();
    }

    if (HandleTab("5. Отряды и события", 4)) {
        BeginTabChild("##child_tab4", 0);
        DrawSquadsGrid(ImGui::GetContentRegionAvail().x, spacing);
        ImGui::EndChild();
    }

    if (!map_loaded) ImGui::EndDisabled();

    // Было: ImGui::EndChild();
    // Стало:
    ImGui::End();
}

void EditorWorkspace::RenderScenarioMenu() {
    if (ImGui::MenuItem("Параметры сценария")) { open_scenario_settings = true; }
    if (ImGui::MenuItem("Система событий")) { open_event_settings = true; } // <--- ИЗМЕНЕНО
    if (ImGui::MenuItem("Настройка именных юнитов")) { open_named_units_settings = true; }
    ImGui::Separator();

    // ==================================
    // СПИСОК АРМИЙ
    // ==================================
    if (ImGui::BeginMenu("Список армий")) {
        const char* army_types[] = { "Геройские армии", "Бандитские толпы", "Крестьянские банды", "Отряды нежити" };

        for (int type = 0; type < 4; ++type) {
            bool has_any = false;
            for (auto& a : map_armies) { if (a.behavior_model == type) { has_any = true; break; } }

            if (has_any) {
                if (ImGui::BeginMenu(army_types[type])) {
                    for (size_t i = 0; i < map_armies.size(); ++i) {
                        if (map_armies[i].behavior_model == type) {
                            std::string name = "Армия #" + std::to_string(map_armies[i].id);
                            if (i < loaded_map.army_texts.size() && !loaded_map.army_texts[i].name.empty()) {
                                name += " - " + cp1251_to_utf8(loaded_map.army_texts[i].name);
                            }
                            if (ImGui::MenuItem(name.c_str())) {
                                cam_x = std::clamp((int)map_armies[i].x - VIEW_COLS / 2, 0, std::max(0, (int)loaded_map.size_x - VIEW_COLS));
                                cam_y = std::clamp((int)map_armies[i].y - VIEW_ROWS / 2, 0, std::max(0, (int)loaded_map.size_y - VIEW_ROWS));
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
            }
        }
        ImGui::EndMenu();
    }

    // ==================================
    // СПИСОК СТРОЕНИЙ
    // ==================================
    if (ImGui::BeginMenu("Список строений")) {
        const char* b_group_names[] = {
            "", "Город", "Деревня", "Замок", "Форт", "Таверна", "Рынок",
            "Церковь", "Кузница", "Верфь", "Алтарь", "Шахта", "Руина",
            "Каменный мост", "Деревянный мост"
        };

        for (int g = 1; g <= 14; ++g) {
            bool has_any = false;
            for (auto& b : map_buildings) { if (b.id / 256 == g) { has_any = true; break; } }

            if (has_any) {
                if (ImGui::BeginMenu(b_group_names[g])) {
                    for (size_t i = 0; i < map_buildings.size(); ++i) {
                        if (map_buildings[i].id / 256 == g) {
                            std::string name = "Строение #" + std::to_string(map_buildings[i].id);
                            if (i < loaded_map.building_texts.size() && !loaded_map.building_texts[i].name.empty()) {
                                name += " - " + cp1251_to_utf8(loaded_map.building_texts[i].name);
                            }
                            if (ImGui::MenuItem(name.c_str())) {
                                cam_x = std::clamp((int)map_buildings[i].x - VIEW_COLS / 2, 0, std::max(0, (int)loaded_map.size_x - VIEW_COLS));
                                cam_y = std::clamp((int)map_buildings[i].y - VIEW_ROWS / 2, 0, std::max(0, (int)loaded_map.size_y - VIEW_ROWS));
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
            }
        }
        ImGui::EndMenu();
    }
}

void EditorWorkspace::RenderModals() {
    if (open_settings_modal) ImGui::OpenPopup("Параметры редактора");
    if (ImGui::BeginPopupModal("Параметры редактора", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Настройки отображения");
        ImGui::SliderFloat("Масштаб камеры", &editor_zoom, 1.0f, 3.0f, "%.1fx");
        //ImGui::Checkbox("Отображать сетку проходимости тайлов", &show_grid);
        ImGui::Separator();
        if (ImGui::Button("Закрыть", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); open_settings_modal = false; }
        ImGui::EndPopup();
    }

    if (open_building_settings) {
        DrawBuildingSettingsModal(open_building_settings, map_buildings, loaded_map, selected_building_idx, res_manager, tex_manager, obj_manager);
    }

    if (open_army_settings) {
        // Передаем все ресурсы внутрь
        DrawArmySettingsModal(open_army_settings, map_armies, loaded_map, selected_army_idx, res_manager, tex_manager, obj_manager);
    }
    if (open_event_settings) { // <--- ДОБАВЛЕНО
        DrawEventSettingsModal(open_event_settings, loaded_map, selected_event_idx, res_manager, tex_manager, obj_manager);
    }

    if (open_lantern_settings) {
        DrawLanternSettingsModal(open_lantern_settings, map_lanterns, loaded_map, selected_lantern_idx);
    }
    if (open_named_units_settings) {
        DrawNamedUnitsSettingsModal(open_named_units_settings, loaded_map, res_manager);
    }
    if (open_scenario_settings) {
        DrawScenarioSettingsModal(open_scenario_settings, loaded_map, res_manager, tex_manager, obj_manager, focus_hero_tab);
        if (!open_scenario_settings) focus_hero_tab = -1;
    }
    if (open_unit_settings) {
        DrawUnitSettingsModal(open_unit_settings, res_manager);
    }

    if (open_artifact_settings) {
        DrawArtifactSettingsModal(open_artifact_settings, res_manager);
    }
}