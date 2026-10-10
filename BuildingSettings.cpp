#include "BuildingSettings.h"
#include "StringUtils.h"
#include <imgui.h>
#include <cstring>
#include <algorithm>
#include <gl/GL.h>
#include <string>

static bool bld_initialized = false;
static char buf_bld_name[256] = "";
static char buf_bld_owner[256] = "";
static char buf_bld_desc[1024] = "";
static BuildingObject temp_bld;
static int current_bld_idx = -1;

static int selected_assigned_event_idx = -1;

// Фильтры списка событий (Блок 3)
static char evt_search[256] = "";
static int evt_type_filter = 0; // 0=Все, 1=Глобал, 2=Локал, 3=Задания, 4=Сплетни
static int evt_color_filter = 0; // 0=Все, 1=Зел, 2=Син, 3=Жел, 4=Кра

// Фильтр для комбобокса армий
static char army_search[256] = "";

// Вспомогательные функции для мягкого переноса текста
static std::string AutoWrapString(const std::string& text, int max_chars = 75) {
    std::string result;
    int char_count = 0; size_t last_space_pos = std::string::npos; size_t line_start_pos = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\r') continue;
        if (c == '\n') { result += '\n'; char_count = 0; line_start_pos = result.size(); last_space_pos = std::string::npos; continue; }
        if ((c & 0xC0) != 0x80) char_count++;
        if (c == ' ') last_space_pos = result.size();
        result += (char)c;
        if (char_count >= max_chars && last_space_pos != std::string::npos && last_space_pos >= line_start_pos) {
            result[last_space_pos] = '\t'; result.insert(last_space_pos + 1, "\n"); line_start_pos = last_space_pos + 2; char_count = 0;
            for (size_t k = line_start_pos; k < result.size(); ++k) { if (((unsigned char)result[k] & 0xC0) != 0x80) char_count++; }
            last_space_pos = std::string::npos;
        }
    }
    return result;
}

static int AutoWrapCallback(ImGuiInputTextCallbackData* data) {
    const int MAX_LINE_CHARS = 75;
    int char_count = 0, line_start_byte = 0, last_space_byte = -1;
    for (int i = 0; i < data->BufTextLen; ++i) {
        unsigned char c = (unsigned char)data->Buf[i];
        if (c == '\n') { char_count = 0; line_start_byte = i + 1; last_space_byte = -1; continue; }
        if ((c & 0xC0) != 0x80) char_count++;
        if (c == ' ') last_space_byte = i;
        if (char_count >= MAX_LINE_CHARS && last_space_byte != -1 && last_space_byte >= line_start_byte) {
            data->DeleteChars(last_space_byte, 1); data->InsertChars(last_space_byte, "\t\n");
            i = last_space_byte + 1; line_start_byte = last_space_byte + 2; char_count = 0; last_space_byte = -1;
        }
    }
    return 0;
}

static std::string UnwrapString(const std::string& text) {
    std::string result;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\t') { result += ' '; if (i + 1 < text.size() && text[i + 1] == '\n') i++; }
        else result += text[i];
    }
    return result;
}

// Формирует текстовое описание состава армии
static std::string GetArmySummary(const ArmyObject& a) {
    bool has_leader = (a.units[0] != 0);
    int num_units = 0;
    for (int i = 0; i < 6; i++) num_units += a.units[2 + i * 3 + 2];

    std::string summary;
    if (has_leader && num_units > 0) summary = "Hero + " + std::to_string(num_units) + " units";
    else if (has_leader) summary = "One hero";
    else if (num_units > 0) summary = std::to_string(num_units) + " units";
    else summary = "Empty";

    summary += " (" + std::to_string(a.tac_cost_1) + "$)";
    return summary;
}

void DrawBuildingSettingsModal(bool& open_flag, std::vector<BuildingObject>& buildings, MapData& map, int building_index, ResourceManager* resMgr, TextureManager* texMgr, ObjectManager* objMgr) {
    if (!open_flag || buildings.empty()) {
        bld_initialized = false;
        current_bld_idx = -1;
        return;
    }

    auto SaveCurrentBuilding = [&]() {
        if (current_bld_idx >= 0 && current_bld_idx < (int)buildings.size()) {
            buildings[current_bld_idx] = temp_bld;
            if (current_bld_idx >= (int)map.building_texts.size()) map.building_texts.resize(current_bld_idx + 1);
            map.building_texts[current_bld_idx].name = utf8_to_cp1251(buf_bld_name);
            map.building_texts[current_bld_idx].owner = utf8_to_cp1251(buf_bld_owner);
            map.building_texts[current_bld_idx].desc = utf8_to_cp1251(UnwrapString(buf_bld_desc));
        }
        };

    auto LoadBuilding = [&](int idx) {
        current_bld_idx = std::clamp(idx, 0, (int)buildings.size() - 1);
        temp_bld = buildings[current_bld_idx];

        if (current_bld_idx < (int)map.building_texts.size()) {
            strncpy_s(buf_bld_name, sizeof(buf_bld_name), cp1251_to_utf8(map.building_texts[current_bld_idx].name).c_str(), _TRUNCATE);
            strncpy_s(buf_bld_owner, sizeof(buf_bld_owner), cp1251_to_utf8(map.building_texts[current_bld_idx].owner).c_str(), _TRUNCATE);
            strncpy_s(buf_bld_desc, sizeof(buf_bld_desc), AutoWrapString(cp1251_to_utf8(map.building_texts[current_bld_idx].desc), 75).c_str(), _TRUNCATE);
        }
        else {
            buf_bld_name[0] = '\0'; buf_bld_owner[0] = '\0'; buf_bld_desc[0] = '\0';
        }
        selected_assigned_event_idx = -1;
        };

    // Первичная инициализация при открытии модального окна
    if (!bld_initialized) {
        ImGui::OpenPopup("Настройка строений"); // <--- ВАЖНО: именно этой строки не хватало!
        LoadBuilding(building_index);
        bld_initialized = true;
    }

    ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);

    // Применяем 18 цветов светлой темы (в точности как в ScenarioSettings / ArmySettings)
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.96f, 0.96f, 0.96f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));

    // Открываем модальное окно. Передача &open_flag автоматически рисует "крестик" в заголовке
    if (ImGui::BeginPopupModal("Настройка строений", &open_flag, ImGuiWindowFlags_NoSavedSettings)) {

        ImGui::Columns(2, "MainColumns", false);
        ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() - 350); // Левая колонка для Блоков 1 и 2

        // ======================================================================
        // ЛЕВАЯ КОЛОНКА
        // ======================================================================
        ImGui::BeginGroup();

        // ------------------ БЛОК 1: Основная информация ------------------
        ImGui::BeginChild("Block1", ImVec2(0, 240), true);

        // --- Подблок 1 (Верхний левый: Картинка 70x70 + 4 строки) ---
        ImGui::BeginGroup();
        GLuint bld_tex = 0;
        if (objMgr) {
            for (const auto& o : objMgr->objects) {
                if (o.header.type == ObjType::Functional && (o.header.groupId * 256 + o.header.objectId) == temp_bld.id) {
                    bld_tex = o.texture; break;
                }
            }
        }
        if (bld_tex) ImGui::Image((ImTextureID)(intptr_t)bld_tex, ImVec2(70, 70));
        else ImGui::Dummy(ImVec2(70, 70));
        ImGui::EndGroup();

        ImGui::SameLine();
        ImGui::BeginGroup();
        // 1 строка: Название
        ImGui::TextDisabled("Название");
        // 2 строка: Поле ввода
        ImGui::PushItemWidth(300);
        ImGui::InputText("##Name", buf_bld_name, sizeof(buf_bld_name));
        ImGui::PopItemWidth();

        // 3 строка: Армия-владелец
        ImGui::TextDisabled("Армия-владелец");

        // 4 строка: Кастомный комбобокс для армии
        ImGui::PushItemWidth(300);
        int owner_id = temp_bld.owner;
        std::string combo_preview = "Нейтральный";
        if (owner_id > 0 && owner_id != 255) {
            for (size_t i = 0; i < map.armies.size(); ++i) {
                if (map.armies[i].id == owner_id) {
                    combo_preview = "Армия #" + std::to_string(owner_id);
                    if (i < map.army_texts.size() && !map.army_texts[i].name.empty()) {
                        combo_preview += " - " + cp1251_to_utf8(map.army_texts[i].name);
                    }
                    break;
                }
            }
        }

        if (ImGui::BeginCombo("##OwnerArmy", combo_preview.c_str())) {
            ImGui::InputTextWithHint("##ArmySearch", "Поиск...", army_search, sizeof(army_search));
            ImGui::Separator();

            if (ImGui::Selectable("Нейтральный", owner_id == 0 || owner_id == 255)) temp_bld.owner = 0;

            for (size_t i = 0; i < map.armies.size(); ++i) {
                auto& a = map.armies[i];
                std::string a_name = "Армия #" + std::to_string(a.id);
                if (i < map.army_texts.size() && !map.army_texts[i].name.empty()) a_name += " - " + cp1251_to_utf8(map.army_texts[i].name);

                if (army_search[0] != '\0' && !utf8_contains_icase(a_name, army_search)) continue;

                ImGui::PushID(a.id);
                bool is_sel = (owner_id == a.id);
                if (ImGui::Selectable("##sel", is_sel, 0, ImVec2(0, 42))) temp_bld.owner = a.id;

                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                ImVec2 p_min = ImGui::GetItemRectMin();

                GLuint leader_tex = 0;
                int leader_class = a.units[0];
                if (leader_class > 0 && resMgr && leader_class < (int)resMgr->units.size()) {
                    leader_tex = resMgr->units[leader_class].iconTex;
                }

                if (leader_tex) draw_list->AddImage((ImTextureID)(intptr_t)leader_tex, ImVec2(p_min.x + 4, p_min.y + 4), ImVec2(p_min.x + 38, p_min.y + 38));
                else draw_list->AddRectFilled(ImVec2(p_min.x + 4, p_min.y + 4), ImVec2(p_min.x + 38, p_min.y + 38), IM_COL32(200, 200, 200, 255));

                draw_list->AddText(ImVec2(p_min.x + 44, p_min.y + 4), ImGui::GetColorU32(ImGuiCol_Text), a_name.c_str());
                draw_list->AddText(ImVec2(p_min.x + 44, p_min.y + 22), ImGui::GetColorU32(ImGuiCol_TextDisabled), GetArmySummary(a).c_str());

                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::EndGroup();

        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 5));

        // --- Подблок 2 (Средний) ---
        ImGui::BeginGroup();
        // 1 строка: 3 кнопки + надпись Нейтральный владелец
        if (ImGui::Button("<", ImVec2(25, 25))) { SaveCurrentBuilding(); LoadBuilding((current_bld_idx > 0) ? current_bld_idx - 1 : (int)buildings.size() - 1); } ImGui::SameLine();
        if (ImGui::Button("v", ImVec2(25, 25))) {} ImGui::SameLine();
        if (ImGui::Button(">", ImVec2(25, 25))) { SaveCurrentBuilding(); LoadBuilding((current_bld_idx < (int)buildings.size() - 1) ? current_bld_idx + 1 : 0); } ImGui::SameLine(115);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Нейтральный владелец");

        // 2 строка: поле ввода владельца + справа выпадающий список функционального типа
        ImGui::SetCursorPosX(115);
        ImGui::PushItemWidth(180);
        ImGui::InputText("##NeutOwner", buf_bld_owner, sizeof(buf_bld_owner));
        ImGui::PopItemWidth();

        ImGui::SameLine();
        const char* b_types[] = { "", "Город", "Деревня", "Замок", "Форт", "Таверна", "Рынок", "Церковь", "Кузница", "Верфь", "Алтарь", "Шахта", "Руина", "Каменный мост", "Дерев. мост" };
        int gid = temp_bld.id / 256;
        ImGui::PushItemWidth(150);
        if (ImGui::Combo("##FuncType", &gid, b_types, 15)) {
            temp_bld.id = (gid * 256) + (temp_bld.id % 256);
        }
        ImGui::PopItemWidth();

        // 3 строка: Стартовая принадлежность игроку и 3 галочки
        ImGui::Dummy(ImVec2(0, 2));
        ImGui::Text("Стартовая принадлежность игроку:"); ImGui::SameLine();
        ImGui::Checkbox("Рыцарь", &temp_bld.start_for_knight); ImGui::SameLine();
        ImGui::Checkbox("Архимаг", &temp_bld.start_for_mage); ImGui::SameLine();
        ImGui::Checkbox("Следопыт", &temp_bld.start_for_ranger);
        ImGui::EndGroup();

        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 5));

        // --- Подблок 3 (Краткое описание) ---
        ImGui::TextDisabled("Краткое описание:");
        ImGui::InputTextMultiline("##Desc", buf_bld_desc, sizeof(buf_bld_desc), ImVec2(-FLT_MIN, 50), ImGuiInputTextFlags_CallbackEdit, AutoWrapCallback);

        ImGui::EndChild(); // Конец Блока 1

        ImGui::Dummy(ImVec2(0, 10));

        // ------------------ БЛОК 2: Вкладки ------------------
        ImGui::BeginChild("Block2_Tabs", ImVec2(0, 0), true);

        // Логика видимости вкладок в зависимости от типа (Group ID)
        int func_id = temp_bld.id / 256;
        bool has_troops_barracks = (func_id == 1 || func_id == 2 || func_id == 3 || func_id == 4 || func_id == 7 || func_id == 8 || func_id == 10 || func_id == 12);
        bool has_troops_garrison = (func_id == 1 || func_id == 2 || func_id == 3 || func_id == 4 || func_id == 12);
        bool has_trade_shop = (func_id == 1 || func_id == 6 || func_id == 7 || func_id == 10);
        bool has_trade_lib = (func_id == 1 || func_id == 7 || func_id == 10);
        bool is_ruin = (func_id == 12);

        if (ImGui::BeginTabBar("BldTabs")) {

            // === ВКЛАДКА 1: События ===
            if (ImGui::BeginTabItem("События")) {
                ImGui::Text("Назначенные события для данного строения:");
                ImGui::BeginChild("AssignedEvents", ImVec2(0, -40), true);
                for (int i = 0; i < 64; ++i) {
                    if (temp_bld.events_array[i] == 0) continue;
                    int ev_id = temp_bld.events_array[i];
                    std::string ev_name = "Событие #" + std::to_string(ev_id);
                    if (ev_id > 0 && ev_id <= (int)map.events.size() && !map.event_texts[ev_id - 1].name.empty()) {
                        ev_name += " - " + cp1251_to_utf8(map.event_texts[ev_id - 1].name);
                    }
                    if (ImGui::Selectable(ev_name.c_str(), selected_assigned_event_idx == i)) selected_assigned_event_idx = i;
                }
                ImGui::EndChild();

                if (ImGui::Button("Удалить выбранное", ImVec2(150, 30)) && selected_assigned_event_idx >= 0) {
                    temp_bld.events_array[selected_assigned_event_idx] = 0;
                    int write_idx = 0;
                    for (int i = 0; i < 64; ++i) { if (temp_bld.events_array[i] != 0) temp_bld.events_array[write_idx++] = temp_bld.events_array[i]; }
                    for (int i = write_idx; i < 64; ++i) temp_bld.events_array[i] = 0;
                    selected_assigned_event_idx = -1;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(<-- Чтобы добавить событие, дважды кликните по нему в списке справа)");
                ImGui::EndTabItem();
            }

            // === ВКЛАДКА 2: Войска ===
            if ((has_troops_barracks || has_troops_garrison) && ImGui::BeginTabItem("Войска")) {
                if (ImGui::BeginTabBar("TroopsTabs")) {

                    if (has_troops_barracks && ImGui::BeginTabItem("Казармы и лазарет")) {
                        ImGui::Checkbox("Доступны войска всех типов", &temp_bld.all_troops);
                        ImGui::Columns(3, "BarracksCols", false);
                        ImGui::SetColumnWidth(0, 250); ImGui::SetColumnWidth(1, 100);
                        ImGui::Text("Доступные войска"); ImGui::NextColumn(); ImGui::Text("Есть"); ImGui::NextColumn(); ImGui::Text("Макс"); ImGui::NextColumn();

                        for (int i = 0; i < 6; ++i) {
                            int uid = temp_bld.barracks_array[i * 3];
                            int ustart = temp_bld.barracks_array[i * 3 + 1];
                            int umax = temp_bld.barracks_array[i * 3 + 2];

                            ImGui::PushID(i + 200);
                            ImGui::PushItemWidth(230);
                            if (DrawGridCombo("##UID", uid, resMgr, ResType::UNIT)) temp_bld.barracks_array[i * 3] = uid;
                            ImGui::PopItemWidth(); ImGui::NextColumn();

                            ImGui::PushItemWidth(80);
                            if (ImGui::InputInt("##Start", &ustart, 1)) temp_bld.barracks_array[i * 3 + 1] = std::clamp(ustart, 0, 255);
                            ImGui::PopItemWidth(); ImGui::NextColumn();

                            ImGui::PushItemWidth(80);
                            if (ImGui::InputInt("##Max", &umax, 1)) temp_bld.barracks_array[i * 3 + 2] = std::clamp(umax, 0, 255);
                            ImGui::PopItemWidth(); ImGui::NextColumn();
                            ImGui::PopID();
                        }
                        ImGui::Columns(1);
                        ImGui::EndTabItem();
                    }

                    if (has_troops_garrison && ImGui::BeginTabItem("Крепость и гарнизон")) {
                        ImGui::Checkbox("Гарнизон только для AI", &temp_bld.garrison_only_ai);
                        ImGui::SameLine(350);
                        int g_bonus = temp_bld.garrison_bonus;
                        ImGui::PushItemWidth(150);
                        if (ImGui::SliderInt("Доп. защита %", &g_bonus, 0, 100)) temp_bld.garrison_bonus = g_bonus;
                        ImGui::PopItemWidth();

                        ImGui::Columns(3, "GarrisonCols", false);
                        ImGui::SetColumnWidth(0, 250); ImGui::SetColumnWidth(1, 100);
                        ImGui::Text("Юнит гарнизона"); ImGui::NextColumn(); ImGui::Text("Уровень"); ImGui::NextColumn(); ImGui::Text("Число"); ImGui::NextColumn();

                        for (int i = 0; i < 6; ++i) {
                            int uid = temp_bld.garrison_array[i * 3];
                            int ulvl = temp_bld.garrison_array[i * 3 + 1];
                            int ucnt = temp_bld.garrison_array[i * 3 + 2];

                            ImGui::PushID(i + 300);
                            ImGui::PushItemWidth(230);
                            if (DrawGridCombo("##GUID", uid, resMgr, ResType::UNIT)) temp_bld.garrison_array[i * 3] = uid;
                            ImGui::PopItemWidth(); ImGui::NextColumn();

                            ImGui::PushItemWidth(80);
                            if (ImGui::InputInt("##Lvl", &ulvl, 1)) temp_bld.garrison_array[i * 3 + 1] = std::clamp(ulvl, 0, 255);
                            ImGui::PopItemWidth(); ImGui::NextColumn();

                            ImGui::PushItemWidth(80);
                            if (ImGui::InputInt("##Cnt", &ucnt, 1)) temp_bld.garrison_array[i * 3 + 2] = std::clamp(ucnt, 0, 255);
                            ImGui::PopItemWidth(); ImGui::NextColumn();
                            ImGui::PopID();
                        }
                        ImGui::Columns(1);
                        ImGui::EndTabItem();
                    }

                    if (is_ruin && ImGui::BeginTabItem("Клад")) {
                        ImGui::Text("Артефакты в кладе:");
                        ImGui::PushItemWidth(280);
                        for (int i = 0; i < 5; ++i) {
                            int aid = temp_bld.arts_array[i];
                            ImGui::PushID(i + 400);
                            if (DrawGridCombo("##Art", aid, resMgr, ResType::ARTIFACT)) temp_bld.arts_array[i] = aid;
                            ImGui::PopID();
                        }
                        ImGui::PopItemWidth();

                        ImGui::Dummy(ImVec2(0, 10));
                        int t_gold = temp_bld.gold_income;
                        ImGui::PushItemWidth(150);
                        if (ImGui::InputInt("Золото в кладе", &t_gold, 100)) temp_bld.gold_income = std::clamp(t_gold, 0, 65535);
                        ImGui::PopItemWidth();
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
                ImGui::EndTabItem();
            }

            // === ВКЛАДКА 3: Торговля ===
            if ((has_trade_shop || has_trade_lib) && ImGui::BeginTabItem("Торговля")) {
                if (ImGui::BeginTabBar("TradeTabs")) {

                    if (has_trade_shop && ImGui::BeginTabItem("Лавка редкостей")) {
                        ImGui::Text("Фиксированные артефакты в продаже:");
                        ImGui::PushItemWidth(280);
                        for (int i = 0; i < 6; ++i) {
                            int aid = temp_bld.arts_array[6 + i];
                            ImGui::PushID(i + 500);
                            if (DrawGridCombo("##Art", aid, resMgr, ResType::ARTIFACT)) temp_bld.arts_array[6 + i] = aid;
                            ImGui::PopID();
                        }
                        ImGui::PopItemWidth();

                        ImGui::Dummy(ImVec2(0, 10));
                        ImGui::Text("Генерация случайных артефактов:");
                        int r_cnt = temp_bld.art_count;
                        int r_min = temp_bld.art_min_price;
                        int r_max = temp_bld.art_max_price;

                        ImGui::PushItemWidth(100);
                        if (ImGui::InputInt("Число", &r_cnt)) temp_bld.art_count = std::clamp(r_cnt, 0, 255);
                        ImGui::SameLine();
                        if (ImGui::InputInt("Цена от", &r_min, 100)) temp_bld.art_min_price = std::clamp(r_min, 0, 65535);
                        ImGui::SameLine();
                        if (ImGui::InputInt("до", &r_max, 100)) temp_bld.art_max_price = std::clamp(r_max, 0, 65535);
                        ImGui::PopItemWidth();
                        ImGui::EndTabItem();
                    }

                    if (has_trade_lib && ImGui::BeginTabItem("Библиотека")) {
                        ImGui::Text("Заклинания для обучения:");
                        ImGui::PushItemWidth(280);
                        for (int i = 0; i < 6; ++i) {
                            int sid = temp_bld.spells[i];
                            ImGui::PushID(i + 600);
                            if (DrawGridCombo("##Spell", sid, resMgr, ResType::SPELL)) temp_bld.spells[i] = sid;
                            ImGui::PopID();
                        }
                        ImGui::PopItemWidth();
                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }
                ImGui::EndTabItem();
            }

            // === ВКЛАДКА 4: Союзы и доходы ===
            if (ImGui::BeginTabItem("Союзы и Доходы")) {
                ImGui::BeginChild("AllianceLeft", ImVec2(350, 0), false);
                ImGui::Text("Выбор группировки");
                int grp = temp_bld.group_id;

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.55f, 0.0f, 1.0f)); ImGui::RadioButton("Зеленые (игрок)", &grp, 1); ImGui::PopStyleColor();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.4f, 0.9f, 1.0f)); ImGui::RadioButton("Синие (союзник)", &grp, 2); ImGui::PopStyleColor();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.5f, 0.0f, 1.0f)); ImGui::RadioButton("Желтые (сосед)", &grp, 3); ImGui::PopStyleColor();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.15f, 0.15f, 1.0f)); ImGui::RadioButton("Красные (враг)", &grp, 4); ImGui::PopStyleColor();
                temp_bld.group_id = std::clamp(grp, 1, 4);

                ImGui::Dummy(ImVec2(0, 10));
                ImGui::Text("Дружелюбие и мир");
                for (int i = 0; i < 4; i++) {
                    int rel = (int8_t)temp_bld.relations[i];
                    ImGui::PushID(i + 700);
                    ImGui::PushItemWidth(150);
                    if (ImGui::SliderInt("##Rel", &rel, -3, 3)) temp_bld.relations[i] = (uint8_t)rel;
                    ImGui::PopItemWidth();
                    ImGui::PopID();
                }
                ImGui::EndChild();

                ImGui::SameLine();

                ImGui::BeginChild("EconomyRight", ImVec2(0, 0), false);
                if (!is_ruin) {
                    ImGui::Text("Золотой запас");
                    int g_inc = temp_bld.gold_income, g_max = temp_bld.gold_max;
                    ImGui::PushItemWidth(100);
                    if (ImGui::InputInt("Ежедневный доход##G", &g_inc, 10)) temp_bld.gold_income = std::clamp(g_inc, 0, 65535);
                    ImGui::SameLine();
                    if (ImGui::InputInt("Максимум##G", &g_max, 100)) temp_bld.gold_max = std::clamp(g_max, 0, 65535);

                    ImGui::Dummy(ImVec2(0, 10));
                    ImGui::Text("Запас маны");
                    int m_inc = temp_bld.mana_income, m_max = temp_bld.mana_max;
                    if (ImGui::InputInt("Ежедневный доход##M", &m_inc, 5)) temp_bld.mana_income = std::clamp(m_inc, 0, 255);
                    ImGui::SameLine();
                    if (ImGui::InputInt("Максимум##M", &m_max, 10)) temp_bld.mana_max = std::clamp(m_max, 0, 255);
                    ImGui::PopItemWidth();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::EndChild(); // Конец Блока 2

        ImGui::EndGroup(); // Конец левой колонки

        // ======================================================================
        // ПРАВАЯ КОЛОНКА (БЛОК 3: Доступные события)
        // ======================================================================
        ImGui::NextColumn();
        ImGui::BeginGroup();

        ImGui::Text("Доступные события");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##SearchEvt", "Поиск по названию...", evt_search, sizeof(evt_search));

        ImGui::TextDisabled("Тип:");
        ImGui::RadioButton("Все##t", &evt_type_filter, 0); ImGui::SameLine();
        ImGui::RadioButton("Глобал", &evt_type_filter, 1); ImGui::SameLine();
        ImGui::RadioButton("Локал", &evt_type_filter, 2);
        ImGui::RadioButton("Задания", &evt_type_filter, 3); ImGui::SameLine();
        ImGui::RadioButton("Сплетни", &evt_type_filter, 4);

        ImGui::TextDisabled("Цвет:");
        const char* colors[] = { "Все цвета", "Зеленый", "Синий", "Желтый", "Красный" };
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::Combo("##ColorFlt", &evt_color_filter, colors, 5);

        ImGui::Dummy(ImVec2(0, 5));

        ImVec4 ev_bg_colors[] = {
            ImVec4(0.18f, 0.55f, 0.22f, 0.3f), // Зеленый
            ImVec4(0.20f, 0.45f, 0.78f, 0.3f), // Синий
            ImVec4(0.70f, 0.55f, 0.15f, 0.3f), // Желтый
            ImVec4(0.78f, 0.25f, 0.25f, 0.3f)  // Красный
        };

        ImGui::BeginChild("AvailEventsList", ImVec2(0, -50), true);

        if (ImGui::BeginTable("EvtTbl", 1, ImGuiTableFlags_RowBg)) {
            ImGuiListClipper clipper;
            clipper.Begin((int)map.events.size());
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                    auto& ev = map.events[row];
                    int ev_id = row + 1;

                    // Фильтрация
                    if (evt_color_filter > 0 && ev.color != evt_color_filter - 1) continue;
                    if (evt_type_filter == 1 && ev.type != 0) continue; // Global
                    if (evt_type_filter == 2 && ev.type != 1) continue; // Local
                    if (evt_type_filter == 3 && ev.type != 2) continue; // Quest
                    if (evt_type_filter == 4 && ev.type != 3) continue; // Rumor

                    std::string e_name = (row < map.event_texts.size()) ? cp1251_to_utf8(map.event_texts[row].name) : "";
                    if (e_name.empty()) e_name = "Событие #" + std::to_string(ev_id);

                    if (evt_search[0] != '\0' && !utf8_contains_icase(e_name, evt_search)) continue;

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    int c_idx = std::clamp((int)ev.color, 0, 3);
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::ColorConvertFloat4ToU32(ev_bg_colors[c_idx]));

                    std::string display_name = std::to_string(ev_id) + ". ";
                    if (ev.type == 2) display_name += "[З] ";
                    else if (ev.type == 3) display_name += "[С] ";
                    display_name += e_name;

                    if (ImGui::Selectable(display_name.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                        if (ImGui::IsMouseDoubleClicked(0)) {
                            for (int k = 0; k < 64; ++k) {
                                if (temp_bld.events_array[k] == 0) {
                                    temp_bld.events_array[k] = ev_id;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();

        // --- Кнопки в правом нижнем углу ---
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 40);
        if (ImGui::Button("Сохранить", ImVec2(100, 25))) {
            SaveCurrentBuilding();
            ImGui::CloseCurrentPopup();
            open_flag = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Выход", ImVec2(100, 25))) {
            ImGui::CloseCurrentPopup();
            open_flag = false;
        }

        ImGui::EndGroup(); // Конец правой колонки

        ImGui::Columns(1);
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(18);

    // Сброс состояния при закрытии (в т.ч. через крестик)
    if (!open_flag) {
        bld_initialized = false;
        current_bld_idx = -1;
    }
}