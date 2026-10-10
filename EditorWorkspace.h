#pragma once
#include "imgui.h"
#include "MapParser.h"
#include "TextureManager.h"
#include "ObjectManager.h"
#include "ResourceManager.h"
#include <vector>
#include <cstdint>

enum class ToolMode { Brush, Eraser, Info, Fill };

enum class MoveMode { None, Building, Army, Lantern, Hero };

class EditorWorkspace {
public:
    MapData loaded_map = {};
    std::vector<DecorObject> map_decors;
    std::vector<BuildingObject> map_buildings;
    std::vector<ArmyObject> map_armies;
    std::vector<LanternObject> map_lanterns;

    int cam_x = 0, cam_y = 0;
    float editor_zoom = 1.0f;
    bool show_grid = false; // Отключена по умолчанию
    bool show_patrol_zones = false; // Для кнопки "Показать зоны патрулирования"
    bool show_fog_of_war = false;   // Для кнопки "Показать туман войны"

    TextureManager* tex_manager = nullptr;
    ObjectManager* obj_manager = nullptr;
    ResourceManager* res_manager = nullptr; // Новый менеджер

    ToolMode current_tool = ToolMode::Info; // По умолчанию активна инфа

    // Новые переменные для кисти и заливки
    int current_brush_w = 1;
    int current_brush_h = 1;
    bool fill_limit_radius = false;
    int fill_radius = 10;
    bool fill_replace_types[16] = { false };

    //int current_brush_size = 1;
    int active_tab = 0;
    int hill_filter_idx = 0;
    int building_filter_idx = 0;
    int current_surface_type = 0;
    int current_object_idx = -1;

    double last_move_time = 0.0;
    ImVec2 last_mouse_pos = ImVec2(-1, -1);
    int focus_hero_tab = -1; // Новая переменная для фокуса вкладки героя
    bool open_settings_modal = false;
    bool open_building_settings = false;
    bool open_army_settings = false;
    bool open_lantern_settings = false;
    bool open_scenario_settings = false;
    bool just_changed_tab = false;
    bool open_named_units_settings = false;
    bool open_event_settings = false; // <--- ДОБАВЛЕНО
    int selected_event_idx = -1;
    // Добавьте в public секцию
    bool open_unit_settings = false;
    bool open_artifact_settings = false;
    void RenderScenarioMenu(); // Функция для отрисовки пункта меню "Сценарий"
    int selected_building_idx = -1;
    int selected_army_idx = -1;
    int selected_lantern_idx = -1;

    MoveMode current_move_mode = MoveMode::None;
    BuildingObject move_bld = {}; // <-- Зануляем
    ArmyObject move_army = {};    // <-- Зануляем
    LanternObject move_lant = {}; // <-- Зануляем
    int move_hero_id = 0;
    BuildingTexts move_bld_text; // <--- ИСПРАВЛЕНИЕ 2 (Кэш текста здания для перемещения)
    ArmyTexts move_army_text;    // <--- ИСПРАВЛЕНИЕ 2 (Кэш текста армии для перемещения

    ImVec2 grid_start, grid_end;
    float cell_w = 0.0f;
    float cell_h = 0.0f;
    int VIEW_COLS = 0;
    int VIEW_ROWS = 0;
    int hover_tx = -1, hover_ty = -1;
    bool grid_active = false, grid_clicked = false, grid_hovered = false, grid_clicked_right = false;
    bool show_cursor = false;
    void LoadMap(const MapData& map);
    void UpdateArmyModels();
    void Render(const ImVec2& work_pos, const ImVec2& work_size);

private:
    void InitGrid(const ImVec2& work_pos, const ImVec2& work_size);
    void HandleInput();
    void DrawMapLayers(ImDrawList* draw_list);
    void DrawCursorAndRulers(ImDrawList* draw_list);
    void RenderToolbar(); // <--- ДОБАВЛЕНО
    void RenderRightPanel(const ImVec2& work_size, float top_margin, float bottom_margin);
    void RenderModals();
    void DrawTooltips();
    void DrawObjectGrid(int tab_idx, int start_idx, int end_idx, float panel_w, float spacing);
    void DrawSquadsGrid(float panel_w, float spacing);
    // Вспомогательная функция для отрисовки кастомного прямоугольника с градиентом
    void DrawGradientQuad(ImDrawList* draw_list, ImTextureID tex_id,
        const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4,
        const ImVec2& uv1, const ImVec2& uv2, const ImVec2& uv3, const ImVec2& uv4,
        ImU32 col1, ImU32 col2, ImU32 col3, ImU32 col4);
    ImU32 GetTileColor(uint8_t tile_id);
    GameObject* GetObjectById(uint32_t full_id, ObjType expected_type);

    bool IsLandAt(int x, int y);
    ImVec2 GetTreeScatterOffset(int gridX, int gridY, uint32_t full_id);
    ImVec2 GetBuildingVisualOffset(uint32_t group_id, uint32_t obj_id, int cells_w, int cells_h);
    EditorTexture GetEditorTextureForArmy(const ArmyObject* army, uint8_t model, int x, int y, bool is_army);
    EditorTexture GetEditorTextureForLantern(LanternObject* l, uint8_t default_model) const;
};