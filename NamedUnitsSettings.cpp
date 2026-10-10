#include "NamedUnitsSettings.h"
#include "ResourceManager.h"
#include "StringUtils.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <cstdio>
#include <algorithm>

static char filter_buf[256] = "";
static int selected_unit_idx = -1;
static char name_buf[256] = "";
static bool initialized = false;

// Динамические буферы без фиксированного ограничения в 32 элемента
static std::vector<uint16_t> temp_ids;
static std::vector<std::string> temp_texts; // В кодировке CP1251

// Честный перевод UTF-8 строки в нижний регистр для корректного поиска кириллицы
static std::string ToLowerUTF8(const std::string& s) {
    std::string res;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 'A' && c <= 'Z') {
            res += (char)(c + 32);
        }
        else if (c == 0xD0 && i + 1 < s.size()) {
            unsigned char c2 = (unsigned char)s[++i];
            if (c2 >= 0x90 && c2 <= 0x9F) { // А..П -> а..п
                res += (char)0xD0;
                res += (char)(c2 + 0x20);
            }
            else if (c2 >= 0xA0 && c2 <= 0xAF) { // Р..Я -> р..я
                res += (char)0xD1;
                res += (char)(c2 - 0x20);
            }
            else if (c2 == 0x81) { // Ё -> ё
                res += (char)0xD1;
                res += (char)0x91;
            }
            else {
                res += (char)c;
                res += (char)c2;
            }
        }
        else if (c == 0xD1 && i + 1 < s.size()) {
            res += (char)c;
            res += s[++i];
        }
        else {
            res += (char)c;
        }
    }
    return res;
}

void DrawNamedUnitsSettingsModal(bool& open_flag, MapData& map, ResourceManager* resMgr) {
    if (!open_flag) {
        initialized = false;
        return;
    }

    if (!initialized) {
        ImGui::OpenPopup("Настройка именных юнитов");
        selected_unit_idx = -1;
        filter_buf[0] = '\0';
        name_buf[0] = '\0';

        // Загружаем данные карты в динамические векторы
        temp_ids = map.named_units_ids;
        temp_texts = map.named_units_texts;

        // Синхронизируем размеры ID и текстов
        if (temp_ids.size() < temp_texts.size()) {
            temp_ids.resize(temp_texts.size(), 1);
        }
        else if (temp_texts.size() < temp_ids.size()) {
            temp_texts.resize(temp_ids.size(), "");
        }

        initialized = true;
    }

    ImGui::SetNextWindowSize(ImVec2(680, 650), ImGuiCond_FirstUseEver);

    if (ImGui::BeginPopupModal("Настройка именных юнитов", &open_flag, ImGuiWindowFlags_NoSavedSettings)) {

        // =========================================================
        // БЛОК 1: Настройки выбранного юнита (Сверху)
        // =========================================================
        ImGui::BeginChild("UnitSettings", ImVec2(0, 120), true);
        if (selected_unit_idx >= 0 && selected_unit_idx < (int)temp_ids.size()) {
            ImGui::Text("Настройки юнита #%d", selected_unit_idx + 1);
            ImGui::Separator();

            ImGui::Text("Имя юнита:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(300);
            if (ImGui::InputText("##UnitName", name_buf, sizeof(name_buf))) {
                temp_texts[selected_unit_idx] = utf8_to_cp1251(name_buf);
            }

            ImGui::Dummy(ImVec2(0, 5));
            ImGui::Text("Базовый класс:");
            ImGui::SameLine();
            int b_class = temp_ids[selected_unit_idx];
            ImGui::PushItemWidth(250);

            if (DrawGridCombo("##BaseClass", b_class, resMgr, ResType::UNIT)) {
                temp_ids[selected_unit_idx] = (uint16_t)b_class;
            }
            ImGui::PopItemWidth();
        }
        else {
            ImGui::TextDisabled("Выберите именного юнита из списка ниже или добавьте нового");
        }
        ImGui::EndChild();

        // =========================================================
        // БЛОК 2: Кнопки управления (Save, Add, Del) + Количество
        // =========================================================
        ImGui::BeginGroup();

        bool can_save = (selected_unit_idx >= 0 && selected_unit_idx < (int)temp_ids.size());
        if (!can_save) ImGui::BeginDisabled();
        if (ImGui::Button("Save", ImVec2(70, 25))) {
            temp_texts[selected_unit_idx] = utf8_to_cp1251(name_buf);
        }
        if (!can_save) ImGui::EndDisabled();

        ImGui::SameLine();

        // Поддержка добавления вплоть до 65535 юнитов
        bool can_add = (temp_ids.size() < 65535);
        if (!can_add) ImGui::BeginDisabled();
        if (ImGui::Button("Add", ImVec2(70, 25))) {
            temp_ids.push_back(1); // Базовый класс по умолчанию
            temp_texts.push_back(utf8_to_cp1251("Новый юнит"));

            selected_unit_idx = (int)temp_ids.size() - 1;
            snprintf(name_buf, sizeof(name_buf), "%s", "Новый юнит");
        }
        if (!can_add) ImGui::EndDisabled();

        ImGui::SameLine();

        bool can_del = (selected_unit_idx >= 0 && selected_unit_idx < (int)temp_ids.size());
        if (!can_del) ImGui::BeginDisabled();
        if (ImGui::Button("Del", ImVec2(70, 25))) {
            temp_ids.erase(temp_ids.begin() + selected_unit_idx);
            temp_texts.erase(temp_texts.begin() + selected_unit_idx);

            if (selected_unit_idx >= (int)temp_ids.size()) {
                selected_unit_idx = (int)temp_ids.size() - 1;
            }
            if (selected_unit_idx >= 0) {
                std::string u_name = cp1251_to_utf8(temp_texts[selected_unit_idx]);
                snprintf(name_buf, sizeof(name_buf), "%s", u_name.c_str());
            }
            else {
                name_buf[0] = '\0';
            }
        }
        if (!can_del) ImGui::EndDisabled();

        // Отображение реального количества динамических юнитов
        ImGui::SameLine(0, 20);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
        ImGui::Text("Текущее количество именных юнитов: %zu / 65535", temp_ids.size());

        ImGui::EndGroup();

        ImGui::Dummy(ImVec2(0, 10));

        // =========================================================
        // БЛОК 3: Список юнитов, Фильтр и ImGuiListClipper
        // =========================================================
        ImGui::SetNextItemWidth(250);
        ImGui::InputText("Фильтр (Имя/Класс)", filter_buf, sizeof(filter_buf));

        std::string filter_utf8 = ToLowerUTF8(filter_buf);

        // Индексация для фильтрации
        std::vector<int> filtered_indices;
        filtered_indices.reserve(temp_ids.size());

        for (size_t i = 0; i < temp_ids.size(); ++i) {
            int b_class = temp_ids[i];
            std::string class_str_utf8 = "";
            if (b_class >= 0 && b_class < (int)resMgr->units.size()) {
                std::string raw = resMgr->units[b_class].name;
                class_str_utf8 = ToLowerUTF8(raw) + " " + ToLowerUTF8(cp1251_to_utf8(raw));
            }

            std::string utf8_name = cp1251_to_utf8(temp_texts[i]);
            if (utf8_name.empty()) {
                utf8_name = "Юнит #" + std::to_string(i + 1);
            }

            if (!filter_utf8.empty()) {
                std::string name_low = ToLowerUTF8(utf8_name);
                if (name_low.find(filter_utf8) == std::string::npos &&
                    class_str_utf8.find(filter_utf8) == std::string::npos) {
                    continue;
                }
            }
            filtered_indices.push_back((int)i);
        }

        ImGui::BeginChild("UnitList", ImVec2(0, -50), true);

        // ImGuiListClipper обеспечивает отрисовку со скоростью 60+ FPS даже для 65535 юнитов
        ImGuiListClipper clipper;
        clipper.Begin((int)filtered_indices.size());

        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                int i = filtered_indices[row];

                std::string utf8_name = cp1251_to_utf8(temp_texts[i]);
                if (utf8_name.empty()) {
                    utf8_name = "Юнит #" + std::to_string(i + 1);
                }

                ImGui::PushID(i);

                bool is_selected = (selected_unit_idx == i);
                float width = ImGui::GetContentRegionAvail().x;
                ImVec2 start_pos = ImGui::GetCursorScreenPos();

                ImGui::BeginGroup();

                // Имя юнита слева
                ImGui::AlignTextToFramePadding();
                ImGui::Text("%s", utf8_name.c_str());

                // Выбор класса через сетку справа
                ImGui::SameLine(width * 0.40f);
                ImGui::PushItemWidth(width * 0.58f);
                int list_class = temp_ids[i];
                if (DrawGridCombo("##list_class", list_class, resMgr, ResType::UNIT)) {
                    temp_ids[i] = (uint16_t)list_class;
                }
                ImGui::PopItemWidth();

                ImGui::EndGroup();

                ImVec2 min_bb = ImGui::GetItemRectMin();
                ImVec2 max_bb = ImGui::GetItemRectMax();
                min_bb.x = start_pos.x;
                max_bb.x = start_pos.x + width;
                min_bb.y -= 2.0f;
                max_bb.y += 2.0f;

                // Клик по строке для выбора
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    selected_unit_idx = i;
                    snprintf(name_buf, sizeof(name_buf), "%s", utf8_name.c_str());
                }

                if (is_selected) {
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        min_bb,
                        max_bb,
                        IM_COL32(50, 100, 200, 110),
                        4.0f
                    );
                }

                ImGui::Dummy(ImVec2(0, 4.0f));
                ImGui::PopID();
            }
        }
        ImGui::EndChild();

        // =========================================================
        // БЛОК 4: ПОДВАЛ (Сохранить и Выход)
        // =========================================================
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 40);
        ImGui::Separator();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 220);

        if (ImGui::Button("Сохранить", ImVec2(100, 25))) {
            if (selected_unit_idx >= 0 && selected_unit_idx < (int)temp_ids.size()) {
                temp_texts[selected_unit_idx] = utf8_to_cp1251(name_buf);
            }

            map.named_units_count = (uint16_t)temp_ids.size();
            map.named_units_ids = temp_ids;
            map.named_units_texts = temp_texts;

            ImGui::CloseCurrentPopup();
            open_flag = false;
            initialized = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Выход", ImVec2(100, 25))) {
            ImGui::CloseCurrentPopup();
            open_flag = false;
            initialized = false;
        }

        ImGui::EndPopup();
    }

    if (!open_flag) {
        initialized = false;
    }
}