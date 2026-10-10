#include "EventSettings.h"
#include "StringUtils.h"
#include <imgui.h>
#include <cstring>
#include <algorithm>
#include <string>

static bool initialized = false;
static char buf_name[256] = "";
static char buf_question[2048] = "";
static char buf_result[2048] = "";
static EventObject temp_event;

// Фильтры списка справа
static int filter_type = 0; // 0=Все, 1=Глобальное, 2=Локальное, 3=Квест, 4=Слух

void DrawEventSettingsModal(bool& open_flag, MapData& map, int& selected_idx, ResourceManager* resMgr, TextureManager* texMgr, ObjectManager* objMgr) {
    if (!open_flag) {
        initialized = false;
        return;
    }

    auto SaveCurrentEvent = [&]() {
        if (selected_idx >= 0 && selected_idx < (int)map.events.size()) {
            map.events[selected_idx] = temp_event;
            if (selected_idx >= (int)map.event_texts.size()) {
                map.event_texts.resize(selected_idx + 1);
            }
            map.event_texts[selected_idx].name = utf8_to_cp1251(buf_name);
            map.event_texts[selected_idx].question = utf8_to_cp1251(buf_question);
            map.event_texts[selected_idx].result = utf8_to_cp1251(buf_result);
        }
        };

    auto LoadEvent = [&](int idx) {
        if (idx >= 0 && idx < (int)map.events.size()) {
            selected_idx = idx;
            temp_event = map.events[idx];

            if (idx < (int)map.event_texts.size()) {
                strncpy_s(buf_name, sizeof(buf_name), cp1251_to_utf8(map.event_texts[idx].name).c_str(), _TRUNCATE);
                strncpy_s(buf_question, sizeof(buf_question), cp1251_to_utf8(map.event_texts[idx].question).c_str(), _TRUNCATE);
                strncpy_s(buf_result, sizeof(buf_result), cp1251_to_utf8(map.event_texts[idx].result).c_str(), _TRUNCATE);
            }
            else {
                buf_name[0] = '\0'; buf_question[0] = '\0'; buf_result[0] = '\0';
            }
        }
        };

    if (!initialized) {
        ImGui::OpenPopup("Система событий");
        if (map.events.empty()) {
            // Если событий нет, создаем пустышку
            EventObject new_ev;
            map.events.push_back(new_ev);
            map.event_texts.push_back(EventTexts());
        }
        if (selected_idx < 0) selected_idx = 0;
        LoadEvent(selected_idx);
        initialized = true;
    }

    ImGui::SetNextWindowSize(ImVec2(850, 520), ImGuiCond_FirstUseEver);

    // Применяем светлую тему как в других диалогах
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.96f, 0.96f, 0.96f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));

    if (ImGui::BeginPopupModal("Система событий", NULL, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Columns(2, "EventColumns", false);
        ImGui::SetColumnWidth(0, 590); // Левая панель деталей

        // ==========================================
        // ЛЕВАЯ ПАНЕЛЬ (ДЕТАЛИ)
        // ==========================================
        ImGui::BeginGroup();

        ImGui::Text("Событие #%d", selected_idx + 1);
        ImGui::SameLine(100);
        ImGui::PushItemWidth(230);
        ImGui::InputText("##Name", buf_name, sizeof(buf_name));
        ImGui::PopItemWidth();

        ImGui::SameLine(340);
        if (ImGui::Button("New", ImVec2(50, 22))) {
            SaveCurrentEvent();
            EventObject new_ev;
            map.events.push_back(new_ev);
            map.event_texts.push_back(EventTexts());
            LoadEvent((int)map.events.size() - 1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Save", ImVec2(50, 22))) {
            SaveCurrentEvent();
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy", ImVec2(50, 22))) {
            SaveCurrentEvent();
            map.events.push_back(temp_event);
            EventTexts t = map.event_texts[selected_idx];
            t.name += " (Копия)";
            map.event_texts.push_back(t);
            LoadEvent((int)map.events.size() - 1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Del", ImVec2(50, 22))) {
            if (map.events.size() > 1 && selected_idx >= 0) {
                map.events.erase(map.events.begin() + selected_idx);
                if (selected_idx < (int)map.event_texts.size()) {
                    map.event_texts.erase(map.event_texts.begin() + selected_idx);
                }
                LoadEvent(std::max(0, selected_idx - 1));
            }
        }

        ImGui::Dummy(ImVec2(0, 5));

        if (ImGui::BeginTabBar("EventTabs")) {

            // --- Вкладка 1: Событие и игрок ---
            if (ImGui::BeginTabItem("Событие и игрок")) {
                ImGui::Dummy(ImVec2(0, 5));
                ImGui::Text("Тип"); ImGui::SameLine(120); ImGui::Text("Время срабатывания");

                ImGui::BeginGroup();
                int ev_type = temp_event.archetype;
                ImGui::RadioButton("Глобальное", &ev_type, 0);
                ImGui::RadioButton("Локальное", &ev_type, 1);
                ImGui::RadioButton("Квест", &ev_type, 2);
                ImGui::RadioButton("Слух", &ev_type, 3);
                temp_event.archetype = (uint8_t)ev_type;
                ImGui::EndGroup();

                ImGui::SameLine(120);
                ImGui::BeginGroup();
                int day = (int)temp_event.date;
                ImGui::Text("День:"); ImGui::SameLine();
                ImGui::PushItemWidth(80);
                ImGui::InputInt("##Day", &day, 1);
                temp_event.date = (uint32_t)std::max(0, day);
                ImGui::PopItemWidth();

                ImGui::SameLine();
                bool repeat = temp_event.repeat_time > 0;
                if (ImGui::Checkbox("Повтор", &repeat)) {
                    if (!repeat) temp_event.repeat_time = 0;
                    else temp_event.repeat_time = 1;
                }
                if (repeat) {
                    ImGui::SameLine();
                    int rpt = temp_event.repeat_time;
                    ImGui::PushItemWidth(60);
                    if (ImGui::InputInt("##Rpt", &rpt, 1)) temp_event.repeat_time = (uint16_t)std::max(1, rpt);
                    ImGui::PopItemWidth();
                }
                ImGui::EndGroup();

                ImGui::Separator();

                ImGui::Text("Требования к герою для события:");
                int req_lvl = temp_event.current_level;
                int req_gld = temp_event.gold;
                int req_man = temp_event.mana;

                ImGui::PushItemWidth(100);
                ImGui::InputInt("Уровень", &req_lvl, 1); ImGui::SameLine();
                ImGui::InputInt("Золото", &req_gld, 100); ImGui::SameLine();
                ImGui::InputInt("Мана", &req_man, 10);
                ImGui::PopItemWidth();
                temp_event.current_level = (int16_t)req_lvl;
                temp_event.gold = (int16_t)req_gld;
                temp_event.mana = (int16_t)req_man;

                ImGui::Separator();
                bool qst = temp_event.has_question;
                ImGui::Checkbox("Подтверждающий вопрос", &qst);
                temp_event.has_question = qst ? 1 : 0;

                ImGui::Text("Текст / Задание:");
                ImGui::InputTextMultiline("##QuestMemo", buf_question, sizeof(buf_question), ImVec2(560, 150));

                ImGui::EndTabItem();
            }

            // --- Вкладка 2: Событие и герои ---
            if (ImGui::BeginTabItem("Событие и герои")) {
                ImGui::TextDisabled("Здесь можно настроить условия на армии и строения");
                ImGui::Dummy(ImVec2(0, 5));

                ImGui::Checkbox("Принадлежность строений игроку", &temp_event.check_building_owner);
                if (temp_event.check_building_owner) {
                    ImGui::PushItemWidth(200);
                    int bld = temp_event.buildings[0];
                    if (ImGui::InputInt("ID Строения 1", &bld)) temp_event.buildings[0] = (uint16_t)std::max(0, bld);
                    ImGui::PopItemWidth();
                }

                ImGui::Separator();
                ImGui::Checkbox("Имеющиеся артефакты", &temp_event.check_artifacts);
                if (temp_event.check_artifacts) {
                    ImGui::PushItemWidth(200);
                    for (int i = 0; i < 4; i++) {
                        int art_id = temp_event.artifacts[i];
                        ImGui::PushID(i + 100);
                        if (DrawGridCombo("##ArtReq", art_id, resMgr, ResType::ARTIFACT)) temp_event.artifacts[i] = art_id;
                        ImGui::PopID();
                    }
                    ImGui::PopItemWidth();
                }

                ImGui::Separator();
                ImGui::Text("Специфические условия на армию (ID):");
                ImGui::PushItemWidth(100);
                int act_arm = temp_event.active_army_id;
                int deact_arm = temp_event.inactive_army_id;
                ImGui::InputInt("Армия активна", &act_arm); ImGui::SameLine();
                ImGui::InputInt("Армия не активна", &deact_arm);
                temp_event.active_army_id = (uint16_t)std::max(0, act_arm);
                temp_event.inactive_army_id = (uint16_t)std::max(0, deact_arm);
                ImGui::PopItemWidth();

                ImGui::EndTabItem();
            }

            // --- Вкладка 3: Результат 1 ---
            if (ImGui::BeginTabItem("Результат 1")) {
                ImGui::Text("Текстовое сообщение");
                ImGui::InputTextMultiline("##ResultMemo", buf_result, sizeof(buf_result), ImVec2(560, 100));

                ImGui::Separator();
                ImGui::Text("Изменение ресурсов");
                int a_exp = temp_event.add_exp;
                int a_gld = temp_event.add_gold;
                int a_man = temp_event.add_mana;

                ImGui::PushItemWidth(100);
                ImGui::InputInt("Опыт", &a_exp, 10); ImGui::SameLine();
                ImGui::InputInt("Золото", &a_gld, 100); ImGui::SameLine();
                ImGui::InputInt("Мана", &a_man, 10);
                ImGui::PopItemWidth();

                temp_event.add_exp = (int16_t)a_exp;
                temp_event.add_gold = (int16_t)a_gld;
                temp_event.add_mana = (int16_t)a_man;

                ImGui::Separator();
                ImGui::Text("Добавить юнитов:");
                ImGui::PushItemWidth(180);
                for (int i = 0; i < 4; i++) {
                    int uid = temp_event.add_units[i];
                    ImGui::PushID(i + 200);
                    if (DrawGridCombo("##AddU", uid, resMgr, ResType::UNIT, false, 4)) temp_event.add_units[i] = uid;
                    if (i % 2 == 0) ImGui::SameLine();
                    ImGui::PopID();
                }
                ImGui::PopItemWidth();

                ImGui::Dummy(ImVec2(0, 10));
                ImGui::Text("Добавить артефакты:");
                ImGui::PushItemWidth(180);
                for (int i = 0; i < 4; i++) {
                    int artid = temp_event.add_arts[i];
                    ImGui::PushID(i + 300);
                    if (DrawGridCombo("##AddA", artid, resMgr, ResType::ARTIFACT)) temp_event.add_arts[i] = artid;
                    if (i % 2 == 0) ImGui::SameLine();
                    ImGui::PopID();
                }
                ImGui::PopItemWidth();

                ImGui::EndTabItem();
            }

            // --- Вкладка 4: Результат 2 ---
            if (ImGui::BeginTabItem("Результат 2")) {
                ImGui::Text("Управление армиями по ID");
                int act_a1 = temp_event.act_army[0];
                int act_a2 = temp_event.act_army[1];
                int deact_a = temp_event.deact_army;

                ImGui::PushItemWidth(120);
                ImGui::InputInt("Активировать А1", &act_a1); ImGui::SameLine();
                ImGui::InputInt("Активировать А2", &act_a2);
                ImGui::InputInt("Дезактивировать", &deact_a);
                ImGui::PopItemWidth();

                temp_event.act_army[0] = (uint16_t)std::max(0, act_a1);
                temp_event.act_army[1] = (uint16_t)std::max(0, act_a2);
                temp_event.deact_army = (uint16_t)std::max(0, deact_a);

                ImGui::Separator();
                ImGui::Text("Начать бой с армией (ID):");
                int bat_a = temp_event.combat_army_id;
                ImGui::PushItemWidth(120);
                ImGui::InputInt("##Battle", &bat_a);
                ImGui::PopItemWidth();
                temp_event.combat_army_id = (uint16_t)std::max(0, bat_a);

                ImGui::Separator();
                ImGui::Text("Картинка события (ID):");
                int img_id = (int)temp_event.image_id;
                ImGui::PushItemWidth(120);
                ImGui::InputInt("##ImgID", &img_id);
                ImGui::PopItemWidth();
                temp_event.image_id = (uint32_t)std::max(0, img_id);

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
        ImGui::EndGroup();


        // ==========================================
        // ПРАВАЯ ПАНЕЛЬ (СПИСОК)
        // ==========================================
        ImGui::NextColumn();
        ImGui::BeginGroup();
        ImGui::Text("Список событий");

        // Фильтры
        ImGui::RadioButton("Все", &filter_type, 0); ImGui::SameLine();
        ImGui::RadioButton("Глоб.", &filter_type, 1); ImGui::SameLine();
        ImGui::RadioButton("Лок.", &filter_type, 2);
        ImGui::RadioButton("Задания", &filter_type, 3); ImGui::SameLine();
        ImGui::RadioButton("Слухи", &filter_type, 4);

        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginListBox("##EventsList", ImVec2(0, 370))) {
            for (int i = 0; i < (int)map.events.size(); ++i) {
                // Упрощенная фильтрация: 0=Все, 1=Global(0), 2=Local(1), 3=Quest(2), 4=Gossip(3)
                int atype = map.events[i].archetype;
                if (filter_type != 0 && filter_type - 1 != atype) continue;

                std::string ename = (i < (int)map.event_texts.size()) ? cp1251_to_utf8(map.event_texts[i].name) : "Без названия";
                if (ename.empty()) ename = "Событие " + std::to_string(i + 1);

                std::string item_label = std::to_string(i + 1) + ". " + ename;

                bool is_selected = (selected_idx == i);
                if (ImGui::Selectable(item_label.c_str(), is_selected)) {
                    SaveCurrentEvent();
                    LoadEvent(i);
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndListBox();
        }

        ImGui::Dummy(ImVec2(0, 10));
        if (ImGui::Button("Выход", ImVec2(-FLT_MIN, 25))) {
            SaveCurrentEvent();
            ImGui::CloseCurrentPopup();
            open_flag = false;
            initialized = false;
        }
        ImGui::EndGroup();

        ImGui::Columns(1);
        ImGui::EndPopup();
    }

    // Снимаем стили
    ImGui::PopStyleColor(8);
}