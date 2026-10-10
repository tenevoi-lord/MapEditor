#include "ArtifactSettings.h"
#include "StringUtils.h"
#include <imgui.h>
#include <map>
#include <string>
#include <algorithm>

// Локальная структура для хранения расширенных параметров (которых пока нет в ArtDef)
struct ExtendedArtData {
    int type = 0;
    int max_stats[8] = { 0 }; // hits, init, sp, manevres, ab, as, db, ds
    int abs_stats[8] = { 0 };
    int prc_stats[8] = { 0 };
    int prot_life = 0, prot_death = 0, prot_elem = 0;
    int vampirism = 0, regeneration = 0;
    int magic_type = 0, bonus = 0;
};
static std::map<int, ExtendedArtData> ext_art_db;

static bool initialized = false;
static int selected_art_idx = -1;
static char buf_name[256] = "";
static char buf_desc[1024] = "";
static int temp_cost = 0;

void DrawArtifactSettingsModal(bool& open_flag, ResourceManager* resMgr) {
    if (!open_flag || resMgr->artifacts.empty()) {
        initialized = false;
        return;
    }

    auto LoadArtifact = [&](int idx) {
        selected_art_idx = std::clamp(idx, 0, (int)resMgr->artifacts.size() - 1);
        auto& art = resMgr->artifacts[selected_art_idx];

        snprintf(buf_name, sizeof(buf_name), "%s", art.name.c_str());  //cp1251_to_utf8
        buf_desc[0] = '\0'; // В ArtDef пока нет поля описания
        temp_cost = art.cost;
        };

    auto SaveArtifact = [&]() {
        if (selected_art_idx >= 0 && selected_art_idx < (int)resMgr->artifacts.size()) {
            resMgr->artifacts[selected_art_idx].name = buf_name;  //utf8_to_cp1251
            resMgr->artifacts[selected_art_idx].cost = temp_cost;
            // Описание можно будет сохранять, если добавить descript в ArtDef
        }
        };

    if (!initialized) {
        ImGui::OpenPopup("Настройка артефактов");
        LoadArtifact(1); // 0 - обычно "Отсутствует"
        initialized = true;
    }

    ImGui::SetNextWindowSize(ImVec2(800, 680), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal("Настройка артефактов", &open_flag, ImGuiWindowFlags_NoSavedSettings)) {

        ImGui::Columns(2, "ArtCols", false);
        ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() - 250);

        // ================= ЛЕВАЯ ПАНЕЛЬ (НАСТРОЙКИ) =================
        ImGui::BeginChild("ArtSettings", ImVec2(0, 0), false);

        if (selected_art_idx >= 0) {
            auto& art = resMgr->artifacts[selected_art_idx];
            ExtendedArtData& ext = ext_art_db[selected_art_idx];

            ImGui::Text("Артефакт № %d", selected_art_idx);
            ImGui::Separator();

            ImGui::BeginGroup();
            if (art.tex) ImGui::Image((ImTextureID)(intptr_t)art.tex, ImVec2(53, 53));
            else ImGui::Dummy(ImVec2(53, 53));
            ImGui::EndGroup();

            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::PushItemWidth(300);
            ImGui::InputText("Название", buf_name, sizeof(buf_name));
            ImGui::InputTextMultiline("Описание", buf_desc, sizeof(buf_desc), ImVec2(300, 60));
            ImGui::PopItemWidth();
            ImGui::EndGroup();

            ImGui::Dummy(ImVec2(0, 5));
            ImGui::PushItemWidth(150);
            const char* art_types[] = { "Оружие", "Щит", "Броня", "Шлем", "Кольцо", "Амулет", "Обувь", "Зелье" };
            ImGui::Combo("Тип артефакта", &ext.type, art_types, IM_ARRAYSIZE(art_types));
            ImGui::SameLine(300);
            ImGui::InputInt("Стоимость", &temp_cost, 25);
            ImGui::PopItemWidth();

            auto DrawStatsTable = [&](const char* title, int* stats) {
                ImGui::SeparatorText(title);
                if (ImGui::BeginTable(title, 4, ImGuiTableFlags_SizingStretchProp)) {
                    const char* labels[] = { "Пункты жизни", "Рукопашная атака", "Инициатива", "Дистанц. атака",
                                             "Магическая сила", "Рукопашная защита", "Маневры", "Дистанц. защита" };
                    for (int i = 0; i < 4; ++i) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%s", labels[i * 2]);
                        ImGui::TableNextColumn(); ImGui::PushID(i * 2); ImGui::PushItemWidth(50); ImGui::InputInt("##s1", &stats[i * 2], 0); ImGui::PopItemWidth(); ImGui::PopID();
                        ImGui::TableNextColumn(); ImGui::Text("%s", labels[i * 2 + 1]);
                        ImGui::TableNextColumn(); ImGui::PushID(i * 2 + 1); ImGui::PushItemWidth(50); ImGui::InputInt("##s2", &stats[i * 2 + 1], 0); ImGui::PopItemWidth(); ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
                };

            DrawStatsTable("Максимальные значения", ext.max_stats);
            DrawStatsTable("Абсолютные изменения", ext.abs_stats);
            DrawStatsTable("Процентные изменения", ext.prc_stats);

            ImGui::SeparatorText("Прочие");
            if (ImGui::BeginTable("OtherTable", 4, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Защита магия жизни");
                ImGui::TableNextColumn(); ImGui::PushItemWidth(50); ImGui::InputInt("##p1", &ext.prot_life, 0); ImGui::PopItemWidth();
                ImGui::TableNextColumn(); ImGui::Text("Вампиризм");
                ImGui::TableNextColumn(); ImGui::PushItemWidth(50); ImGui::InputInt("##v1", &ext.vampirism, 0); ImGui::PopItemWidth();

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Защита магия смерти");
                ImGui::TableNextColumn(); ImGui::PushItemWidth(50); ImGui::InputInt("##p2", &ext.prot_death, 0); ImGui::PopItemWidth();
                ImGui::TableNextColumn(); ImGui::Text("Регенерация");
                ImGui::TableNextColumn(); ImGui::PushItemWidth(50); ImGui::InputInt("##v2", &ext.regeneration, 0); ImGui::PopItemWidth();

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Защита магия стихий");
                ImGui::TableNextColumn(); ImGui::PushItemWidth(50); ImGui::InputInt("##p3", &ext.prot_elem, 0); ImGui::PopItemWidth();
                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        // ================= ПРАВАЯ ПАНЕЛЬ (СПИСОК) =================
        ImGui::NextColumn();
        ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
        ImGui::Text("Список артефактов");
        ImGui::Separator();

        ImGui::BeginChild("ArtList", ImVec2(0, -45), true);
        for (int i = 1; i < (int)resMgr->artifacts.size(); ++i) {
            std::string name = resMgr->artifacts[i].name; //cp1251_to_utf8
            if (name.empty()) name = "Артефакт #" + std::to_string(i);

            bool is_sel = (selected_art_idx == i);
            if (ImGui::Selectable(name.c_str(), is_sel)) {
                SaveArtifact();
                LoadArtifact(i);
            }
        }
        ImGui::EndChild();

        if (ImGui::Button("Сохранить", ImVec2(90, 30))) { SaveArtifact(); open_flag = false; initialized = false; }
        ImGui::SameLine();
        if (ImGui::Button("Выход", ImVec2(90, 30))) { open_flag = false; initialized = false; }

        ImGui::EndChild();
        ImGui::Columns(1);
        ImGui::EndPopup();
    }
}