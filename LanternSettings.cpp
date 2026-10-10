#include "LanternSettings.h"
#include "StringUtils.h"
#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

static bool initialized = false;
static LanternObject temp_lant;
static char evt_search[256] = "";
static int evt_type_filter = 0; // 0=Все, 1=Задания, 2=Сплетни, 3=Глобальное, 4=Локальное
static int evt_color_filter = 0; // 0=Все, 1=Зел, 2=Син, 3=Желт, 4=Крас
static int current_lantern_idx = -1;

void DrawLanternSettingsModal(bool& open_flag, std::vector<LanternObject>& lanterns, MapData& map, int lantern_index) {
    if (!open_flag) {
        initialized = false;
        return;
    }

    // Подготавливаем данные ПЕРЕД вызовом OpenPopup, чтобы знать модель объекта
    if (!initialized || current_lantern_idx != lantern_index) {
        if (lantern_index >= 0 && lantern_index < (int)lanterns.size()) {
            temp_lant = lanterns[lantern_index];
        }
        else {
            temp_lant = LanternObject(); // Защитный фолбэк
        }

        // Динамический заголовок
        const char* popup_title = (temp_lant.model == 8) ? "Настройка фонарика" : "Настройка мин";
        ImGui::OpenPopup(popup_title);

        current_lantern_idx = lantern_index;
        evt_search[0] = '\0';
        evt_type_filter = 0;
        evt_color_filter = 0;
        initialized = true;
    }

    // Тот же самый заголовок для BeginPopupModal
    const char* popup_title = (temp_lant.model == 8) ? "Настройка фонарика" : "Настройка мин";

    // Устанавливаем РАЗНЫЕ размеры для разных окон (ImGuiCond_Appearing срабатывает при каждом новом открытии)
    if (temp_lant.model == 8) {
        ImGui::SetNextWindowSize(ImVec2(320, 160), ImGuiCond_Appearing);
    }
    else {
        ImGui::SetNextWindowSize(ImVec2(800, 520), ImGuiCond_Appearing);
    }

    if (ImGui::BeginPopupModal(popup_title, NULL, ImGuiWindowFlags_NoSavedSettings)) {

        if (temp_lant.model == 8) {
            // ==========================================
            // МОДЕЛЬ 8: ЧИСТЫЙ ФОНАРИК
            // ==========================================
            ImGui::Text("Фонарик #%d", temp_lant.id);
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 10));

            int rad = temp_lant.radius;
            ImGui::PushItemWidth(150);
            if (ImGui::InputInt("Радиус видимости", &rad, 1, 5)) temp_lant.radius = std::clamp(rad, 0, 255);
            ImGui::PopItemWidth();

            ImGui::Dummy(ImVec2(0, 20));
            if (ImGui::Button("Сохранить", ImVec2(120, 30))) {
                lanterns[current_lantern_idx] = temp_lant;
                ImGui::CloseCurrentPopup();
                open_flag = false;
                initialized = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Выход", ImVec2(120, 30))) {
                ImGui::CloseCurrentPopup();
                open_flag = false;
                initialized = false;
            }
        }
        else {
            // ==========================================
            // МОДЕЛЬ 9: МИНА / МИНА+ФОНАРИК
            // ==========================================
            ImGui::Columns(2, "MineColumns", false);
            ImGui::SetColumnWidth(0, 370.0f);

            // --- ЛЕВЫЙ БЛОК (НАСТРОЙКА МИНЫ) ---
            ImGui::Text("Возможные события (%d/5):", temp_lant.event_count);
            ImGui::BeginChild("AssignedEvents", ImVec2(0, 115), true);
            for (int i = 0; i < 5; ++i) {
                if (i < temp_lant.event_count) {
                    int e_id = temp_lant.events[i];
                    std::string e_name = (e_id > 0 && e_id <= (int)map.events.size()) ? cp1251_to_utf8(map.event_texts[e_id - 1].name) : "Неизвестно";
                    if (e_name.empty()) e_name = "Без названия";

                    char buf[128]; snprintf(buf, sizeof(buf), "%d. %s##ev_%d", e_id, e_name.c_str(), i);

                    // Удаление по клику
                    if (ImGui::Selectable(buf)) {
                        for (int j = i; j < 4; ++j) { temp_lant.events[j] = temp_lant.events[j + 1]; }
                        temp_lant.events[4] = 0;
                        temp_lant.event_count--;
                        i--; // Смещаем индекс назад для корректной отрисовки
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Нажмите, чтобы убрать из мины");
                }
                else {
                    ImGui::TextDisabled("%d. <Пусто>", i + 1);
                }
            }
            ImGui::EndChild();

            ImGui::Dummy(ImVec2(0, 5));
            int rad = temp_lant.radius;
            ImGui::PushItemWidth(100);
            if (ImGui::InputInt("Радиус видимости в начале игры", &rad, 1, 5)) temp_lant.radius = std::clamp(rad, 0, 255);
            ImGui::PopItemWidth();

            bool active = temp_lant.vis_radius > 0;
            if (ImGui::Checkbox("Активен в начале игры", &active)) temp_lant.vis_radius = active ? 1 : 0;

            ImGui::Dummy(ImVec2(0, 5));
            ImVec4 cols[4] = {
                ImVec4(0.0f, 0.7f, 0.0f, 1.0f), // Зеленый
                ImVec4(0.0f, 0.5f, 1.0f, 1.0f), // Синий
                ImVec4(0.8f, 0.8f, 0.0f, 1.0f), // Желтый
                ImVec4(0.9f, 0.2f, 0.2f, 1.0f)  // Красный
            };
            const char* p_names[4] = { "Зеленых (игрок)", "Синих (союзник)", "Желтых (сосед)", "Красных (враг)" };
            for (int i = 0; i < 4; ++i) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(cols[i], "Приоритет для %s", p_names[i]);
                ImGui::SameLine(250);
                ImGui::PushItemWidth(100);
                int p = temp_lant.priorities[i];
                ImGui::PushID(i + 100);
                if (ImGui::InputInt("##p", &p, 1, 10)) temp_lant.priorities[i] = std::clamp(p, 0, 65535);
                ImGui::PopID();
                ImGui::PopItemWidth();
            }

            ImGui::Dummy(ImVec2(0, 5));
            int dur = temp_lant.active_duration;
            ImGui::PushItemWidth(100);
            if (ImGui::InputInt("Срок активного состояния", &dur, 1, 10)) temp_lant.active_duration = std::clamp(dur, 0, 65535);
            ImGui::PopItemWidth();

            ImGui::Dummy(ImVec2(0, 10));
            if (ImGui::Button("Сохранить", ImVec2(100, 30))) {
                lanterns[current_lantern_idx] = temp_lant;
                ImGui::CloseCurrentPopup();
                open_flag = false;
                initialized = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Выход", ImVec2(100, 30))) {
                ImGui::CloseCurrentPopup();
                open_flag = false;
                initialized = false;
            }

            // --- ПРАВЫЙ БЛОК (СПИСОК СОБЫТИЙ И ФИЛЬТРЫ) ---
            ImGui::NextColumn();
            ImGui::Text("Фильтры событий:");

            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##SearchEvt", "Поиск по названию...", evt_search, sizeof(evt_search));

            ImGui::RadioButton("Все##tf", &evt_type_filter, 0); ImGui::SameLine();
            ImGui::RadioButton("Задания", &evt_type_filter, 1); ImGui::SameLine();
            ImGui::RadioButton("Сплетни", &evt_type_filter, 2);
            ImGui::RadioButton("Глобальное", &evt_type_filter, 3); ImGui::SameLine();
            ImGui::RadioButton("Локальное", &evt_type_filter, 4);

            const char* c_filters[] = { "Все цвета", "Зеленый (Игрок)", "Синий (Союзник)", "Желтый (Сосед)", "Красный (Враг)" };
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::Combo("##ColorFilter", &evt_color_filter, c_filters, 5);

            ImGui::Dummy(ImVec2(0, 5));
            ImGui::Text("Доступные события (Двойной клик для добавления):");
            ImGui::BeginChild("AllEventsList", ImVec2(0, 0), true);

            for (size_t i = 0; i < map.events.size(); ++i) {
                auto& ev = map.events[i];
                std::string e_name = (i < map.event_texts.size()) ? cp1251_to_utf8(map.event_texts[i].name) : "Без названия";
                if (e_name.empty()) e_name = "Без названия";

                if (evt_search[0] != '\0' && !utf8_contains_icase(e_name, evt_search)) continue;
                if (evt_color_filter > 0 && ev.color != evt_color_filter - 1) continue;
                if (evt_type_filter == 1 && ev.type != 2) continue;
                if (evt_type_filter == 2 && ev.type != 3) continue;
                if (evt_type_filter == 3 && ev.type != 1) continue;
                if (evt_type_filter == 4 && ev.type != 0) continue;

                char buf[256]; snprintf(buf, sizeof(buf), "%d. %s", (int)(i + 1), e_name.c_str());

                ImGui::Selectable(buf, false, ImGuiSelectableFlags_AllowDoubleClick);
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    if (temp_lant.event_count < 5) {
                        bool already_exists = false;
                        for (int k = 0; k < temp_lant.event_count; ++k) {
                            if (temp_lant.events[k] == i + 1) already_exists = true;
                        }
                        if (!already_exists) {
                            temp_lant.events[temp_lant.event_count] = (uint16_t)(i + 1);
                            temp_lant.event_count++;
                        }
                    }
                }
            }
            ImGui::EndChild();
            ImGui::Columns(1);
        }
        ImGui::EndPopup();
    }
}