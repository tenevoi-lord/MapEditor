#include "ResourceManager.h"
#include "StringUtils.h"
//#include "stb_image.h" // Используем для кораблей
#include <fstream>
#include <imgui.h>
#include <algorithm>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <string>
#include <filesystem>
#include <cstdlib>
#include <cfloat>
#include <cctype>

void ResourceManager::DecodePixel(uint16_t raw, uint8_t& a, uint8_t& r, uint8_t& g, uint8_t& b) {
    uint16_t temp = raw ^ 0xAAAA;
    uint16_t val = (temp >> 3) | (temp << (16 - 3));

    uint8_t a4 = (val & 0xF000) >> 12;
    uint8_t r4 = (val & 0x0F00) >> 8;
    uint8_t g4 = (val & 0x00F0) >> 4;
    uint8_t b4 = (val & 0x000F);

    a = (a4 << 4) | a4;
    r = (r4 << 4) | r4;
    g = (g4 << 4) | g4;
    b = (b4 << 4) | b4;
}

std::vector<GLuint> ResourceManager::LoadUGS(const std::filesystem::path& path) {
    std::vector<GLuint> textures;
    if (!std::filesystem::exists(path)) return textures;
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return textures;

    while (f.peek() != EOF) {
        uint16_t h = 0, w = 0;
        f.read((char*)&h, 2); if (f.gcount() < 2) break;
        f.read((char*)&w, 2); if (f.gcount() < 2) break;

        // Защита от поврежденных файлов
        if (w == 0 || h == 0 || w > 2000 || h > 2000) break;

        size_t sz = w * h;
        if (sz > 4000000) break;

        std::vector<uint16_t> pixels(sz);
        f.read((char*)pixels.data(), sz * 2);
        if (f.gcount() < (std::streamsize)(sz * 2)) break;

        std::vector<uint8_t> rgba(sz * 4);
        for (size_t i = 0; i < sz; ++i) {
            DecodePixel(pixels[i], rgba[i * 4 + 3], rgba[i * 4 + 0], rgba[i * 4 + 1], rgba[i * 4 + 2]);
        }

        GLuint tex; glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        textures.push_back(tex);
    }
    return textures;
}

GLuint ResourceManager::LoadSPI(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return 0;
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return 0;

    std::vector<uint8_t> buffer(1936);
    f.read((char*)buffer.data(), 1936);
    if (f.gcount() < 1936) return 0;

    for (size_t i = 0; i < 1936; i += 4) {
        uint8_t temp = buffer[i];
        buffer[i] = buffer[i + 2];
        buffer[i + 2] = temp;
    }

    GLuint tex; glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 22, 22, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return tex;
}

std::string ResourceManager::Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool ResourceManager::LoadResources(const std::string& projectRoot) {
    units.clear(); artifacts.clear(); spells.clear();
    units.reserve(512); artifacts.reserve(512); spells.reserve(512);

    units.push_back(UnitDef());
    artifacts.push_back(ArtDef());
    spells.push_back(SpellDef());

    std::filesystem::path root(projectRoot);

    // --- Загрузка _Global.ini ---
    costRecrutDiv = 2;
    std::ifstream gFile(root / "_Global.ini");
    if (gFile.is_open()) {
        std::string gLine;
        while (std::getline(gFile, gLine)) {
            size_t eq = gLine.find('=');
            if (eq != std::string::npos) {
                std::string k = Trim(gLine.substr(0, eq));
                std::string v = Trim(gLine.substr(eq + 1));
                if (k == "CostRecrutDiv") {
                    try { costRecrutDiv = std::stoi(v); }
                    catch (...) {}
                }
            }
        }
    }
    if (costRecrutDiv <= 0) costRecrutDiv = 1;

    // --- Загрузка Юнитов ---
    auto icons = LoadUGS(root / "Graphics" / "Objects" / "Icons.ugs");
    std::ifstream ui(root / "Rus_Units.ini");
    if (ui.is_open()) {
        UnitDef* cur = nullptr; std::string line;
        while (std::getline(ui, line)) {
            size_t c = line.find("//"); if (c != std::string::npos) line = line.substr(0, c);
            size_t c2 = line.find(";"); if (c2 != std::string::npos) line = line.substr(0, c2);

            std::string rawLine = line;
            line = Trim(line); if (line.empty()) continue;

            if (line.front() == '[') {
                units.push_back(UnitDef());
                cur = &units.back();
                size_t endBr = line.find(']');
                if (endBr != std::string::npos) {
                    cur->sectionName = line.substr(1, endBr - 1);
                }
                continue;
            }
            if (cur) {
                size_t eq = rawLine.find('='); if (eq == std::string::npos) continue;
                std::string k = Trim(rawLine.substr(0, eq));
                std::string v = Trim(rawLine.substr(eq + 1));

                try {
                    if (k == "GlobalIndex") cur->globalIndex = std::stoi(v);
                    else if (k == "Name") cur->name = cp1251_to_utf8(v);
                    else if (k == "Descript") cur->descript = cp1251_to_utf8(v);
                    else if (k == "IconIndex") cur->iconIndex = std::stoi(v);
                    else if (k == "Cost") cur->cost = std::stoi(v);
                    else if (k == "CostMultipler") cur->costMultipler = std::stoi(v);
                    else if (k == "CostGoldDiv") cur->costGoldDiv = std::stoi(v);
                    else if (k == "Nature") cur->nature = v;
                    else if (k == "Magic") cur->magic = v;
                    else if (k == "MinMagicPower") cur->minMagicPower = std::stoi(v);
                    else if (k == "ManaDrain") cur->manaDrain = std::stoi(v);
                    else if (k == "MagicDirection") cur->magicDirection = v;
                    else if (k == "Bonus") cur->bonus = v;
                    else if (k == "Surrender") cur->surrender = std::stoi(v);
                    else if (k == "Evasion") cur->evasion = std::stoi(v);
                    else if (k == "StartExpirience") cur->startExperience = std::stoi(v);
                    else if (k == "LevelMultipler") cur->levelMultipler = std::stoi(v);
                    else if (k == "NextUnit1") cur->nextUnit1 = cp1251_to_utf8(v);
                    else if (k == "NextUnit1Level") cur->nextUnit1Level = std::stoi(v);
                    else if (k == "NextUnit2") cur->nextUnit2 = cp1251_to_utf8(v);
                    else if (k == "NextUnit2Level") cur->nextUnit2Level = std::stoi(v);
                    else if (k == "NextUnit3") cur->nextUnit3 = cp1251_to_utf8(v);
                    else if (k == "NextUnit3Level") cur->nextUnit3Level = std::stoi(v);
                    else if (k.find("d-") == 0) cur->dStats[k.substr(2)] = std::stoi(v);
                    else if (!k.empty() && isupper(k[0])) cur->stats[k] = std::stoi(v);
                }
                catch (...) {}
            }
        }
    }
    // Раздаем текстуры (с учетом смещения +1, так как 0-й элемент мы зарезервировали пустым)
    for (size_t i = 1; i < units.size(); i++) {
        if (i - 1 < icons.size()) units[i].iconTex = icons[i - 1];
    }

    // --- Загрузка Артефактов ---
    auto items = LoadUGS(root / "Graphics" / "Objects" / "Items.ugs");
    std::ifstream ai(root / "Rus_Artefacts.ini");
    if (ai.is_open()) {
        ArtDef* cur = nullptr; std::string line;
        while (std::getline(ai, line)) {
            size_t c = line.find("//"); if (c != std::string::npos) line = line.substr(0, c);
            line = Trim(line); if (line.empty()) continue;
            if (line.front() == '[' && line.back() == ']') { artifacts.push_back(ArtDef()); cur = &artifacts.back(); continue; }
            if (cur) {
                size_t eq = line.find('='); if (eq == std::string::npos) continue;
                std::string k = Trim(line.substr(0, eq)); std::string v = Trim(line.substr(eq + 1));

                if (k == "Name") cur->name = cp1251_to_utf8(v);
                else if (k == "Cost") { cur->cost = std::atoi(v.c_str()); }
            }
        }
    }
    for (size_t i = 1; i < artifacts.size(); i++) {
        if (i - 1 < items.size()) artifacts[i].tex = items[i - 1];
    }

    // --- Загрузка Заклинаний ---
    std::ifstream si(root / "Rus_Spells.ini");
    if (si.is_open()) {
        SpellDef* cur = nullptr; std::string line; int splIdx = 0;
        while (std::getline(si, line)) {
            size_t c = line.find("//"); if (c != std::string::npos) line = line.substr(0, c);
            line = Trim(line); if (line.empty()) continue;
            if (line.front() == '[') {
                if (line.find("MapEditorSpecialOptions") != std::string::npos) { cur = nullptr; continue; }
                spells.push_back(SpellDef()); cur = &spells.back();
                cur->tex = LoadSPI(root / "Graphics" / "Editor" / (std::to_string(splIdx++) + ".spi"));
                continue;
            }
            if (cur) {
                size_t eq = line.find('='); if (eq == std::string::npos) continue;
                std::string k = Trim(line.substr(0, eq)); std::string v = Trim(line.substr(eq + 1));

                if (k == "Name") cur->name = cp1251_to_utf8(v);
                else if (k == "CostGold") { cur->costGold = std::atoi(v.c_str()); }
                else if (k == "CostMana") { cur->costMana = std::atoi(v.c_str()); }
            }
        }
    }

    //if (units.size() < 256) units.resize(256);
    //if (artifacts.size() < 256) artifacts.resize(256);
    if (spells.size() < 256) spells.resize(256);

    return true;
}

void ResourceManager::Cleanup() {
    for (auto& u : units) { if (u.iconTex) glDeleteTextures(1, &u.iconTex); }
    for (auto& a : artifacts) { if (a.tex) glDeleteTextures(1, &a.tex); }
    for (auto& s : spells) { if (s.tex) glDeleteTextures(1, &s.tex); }
    units.clear(); artifacts.clear(); spells.clear();
}

static char grid_search_filter[256] = ""; // Глобальный фильтр для списков

bool DrawGridCombo(const char* id, int& current_item, ResourceManager* resMgr, ResType rType, bool disable_change, int start_unit_id) {
    bool changed = false;
    ImGui::PushID(id);

    if (disable_change) ImGui::BeginDisabled();

    float width = ImGui::CalcItemWidth();
    float height = 40.0f;
    ImVec2 pos = ImGui::GetCursorScreenPos();

    if (ImGui::InvisibleButton("##btn", ImVec2(width, height))) {
        ImGui::OpenPopup("GridPopup");
        grid_search_filter[0] = '\0';
    }

    bool is_hovered = ImGui::IsItemHovered();
    bool is_active = ImGui::IsItemActive();
    ImU32 bg_col = ImGui::GetColorU32(is_active ? ImGuiCol_FrameBgActive : is_hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), bg_col, ImGui::GetStyle().FrameRounding);
    draw_list->AddRect(pos, ImVec2(pos.x + width, pos.y + height), ImGui::GetColorU32(ImGuiCol_Border), ImGui::GetStyle().FrameRounding);

    float a_sz = 10.0f;
    draw_list->AddTriangleFilled(
        ImVec2(pos.x + width - a_sz - 6, pos.y + height / 2 - a_sz / 4),
        ImVec2(pos.x + width - 6, pos.y + height / 2 - a_sz / 4),
        ImVec2(pos.x + width - a_sz / 2 - 6, pos.y + height / 2 + a_sz / 2),
        ImGui::GetColorU32(ImGuiCol_Text)
    );

    GLuint p_tex = 0; std::string p_name = "Отсутствует"; int p_cost = 0; std::string cost_label = "Стоимость:";

    if (current_item > 0) {
        if (rType == ResType::UNIT && current_item < (int)resMgr->units.size()) {
            p_tex = resMgr->units[current_item].iconTex; p_name = resMgr->units[current_item].name; p_cost = resMgr->units[current_item].cost;
        }
        else if (rType == ResType::ARTIFACT && current_item < (int)resMgr->artifacts.size()) {
            p_tex = resMgr->artifacts[current_item].tex; p_name = resMgr->artifacts[current_item].name; p_cost = resMgr->artifacts[current_item].cost;
        }
        else if (rType == ResType::SPELL && current_item < (int)resMgr->spells.size()) {
            p_tex = resMgr->spells[current_item].tex; p_name = resMgr->spells[current_item].name; p_cost = resMgr->spells[current_item].costMana; cost_label = "Мана:";
        }
        if (p_name.empty()) p_name = "ID " + std::to_string(current_item);
    }

    if (current_item == 0) {
        draw_list->AddText(ImVec2(pos.x + 8, pos.y + height / 2 - ImGui::GetFontSize() / 2), ImGui::GetColorU32(ImGuiCol_TextDisabled), "Отсутствует");
    }
    else {
        if (p_tex) draw_list->AddImage((ImTextureID)(intptr_t)p_tex, ImVec2(pos.x + 4, pos.y + 4), ImVec2(pos.x + 36, pos.y + 36));
        draw_list->AddText(ImVec2(pos.x + 42, pos.y + 4), ImGui::GetColorU32(ImGuiCol_Text), p_name.c_str());

        char costBuf[64]; snprintf(costBuf, sizeof(costBuf), "%s %d", cost_label.c_str(), p_cost);
        draw_list->AddText(ImVec2(pos.x + 42, pos.y + 20), ImGui::GetColorU32(ImGuiCol_TextDisabled), costBuf);
    }

    //ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + height + ImGui::GetStyle().ItemSpacing.y));
    if (disable_change) ImGui::EndDisabled();

    ImGui::SetNextWindowSize(ImVec2(600, 280));
    if (ImGui::BeginPopup("GridPopup")) {
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##search", "Поиск (начните вводить название)...", grid_search_filter, sizeof(grid_search_filter));
        ImGui::Separator();

        ImGui::BeginChild("##scroll_area", ImVec2(0, 0), false);
        if (utf8_contains_icase("Отсутствует", grid_search_filter)) {
            if (ImGui::Selectable("Отсутствует", current_item == 0, 0, ImVec2(0, 30))) {
                current_item = 0; changed = true; ImGui::CloseCurrentPopup();
            }
        }

        size_t max_items = 0;
        if (rType == ResType::UNIT) max_items = resMgr->units.size();
        else if (rType == ResType::ARTIFACT) max_items = resMgr->artifacts.size();
        else if (rType == ResType::SPELL) max_items = resMgr->spells.size();
        // ОПРЕДЕЛЯЕМ СТАРТОВЫЙ ИНДЕКС: для юнитов берем start_unit_id (например, 4), для остальных — 1
        size_t start_i = (rType == ResType::UNIT) ? static_cast<size_t>(std::max(1, start_unit_id)) : 1;

        if (ImGui::BeginTable("##grid", 3, ImGuiTableFlags_SizingStretchProp)) {
            for (size_t i = start_i; i < max_items; ++i) {
                GLuint i_tex = 0; std::string i_name; int i_cost = 0;

                if (rType == ResType::UNIT) {
                    if (resMgr->units[i].name.empty() && resMgr->units[i].iconTex == 0) continue;
                    i_tex = resMgr->units[i].iconTex; i_name = resMgr->units[i].name; i_cost = resMgr->units[i].cost;
                }
                else if (rType == ResType::ARTIFACT) {
                    if (resMgr->artifacts[i].name.empty() && resMgr->artifacts[i].tex == 0) continue;
                    i_tex = resMgr->artifacts[i].tex; i_name = resMgr->artifacts[i].name; i_cost = resMgr->artifacts[i].cost;
                }
                else if (rType == ResType::SPELL) {
                    if (resMgr->spells[i].name.empty() && resMgr->spells[i].tex == 0) continue;
                    i_tex = resMgr->spells[i].tex; i_name = resMgr->spells[i].name; i_cost = resMgr->spells[i].costMana;
                }

                if (i_name.empty()) i_name = "ID " + std::to_string(i);
                if (!utf8_contains_icase(i_name, grid_search_filter)) continue;

                ImGui::TableNextColumn();
                ImGui::PushID((int)i);

                ImVec2 p_min = ImGui::GetCursorScreenPos();
                float cell_h = 42.0f;

                bool is_selected = (current_item == (int)i);
                if (ImGui::Selectable("##sel", is_selected, 0, ImVec2(0, cell_h))) {
                    current_item = (int)i; changed = true; ImGui::CloseCurrentPopup();
                }
                if (is_selected) ImGui::SetItemDefaultFocus();

                ImDrawList* dl = ImGui::GetWindowDrawList();
                if (i_tex) dl->AddImage((ImTextureID)(intptr_t)i_tex, ImVec2(p_min.x + 4, p_min.y + 4), ImVec2(p_min.x + 38, p_min.y + 38));
                dl->AddText(ImVec2(p_min.x + 44, p_min.y + 4), ImGui::GetColorU32(ImGuiCol_Text), i_name.c_str());
                char cBuf[64]; snprintf(cBuf, sizeof(cBuf), "%s %d", cost_label.c_str(), i_cost);
                dl->AddText(ImVec2(p_min.x + 44, p_min.y + 22), ImGui::GetColorU32(ImGuiCol_TextDisabled), cBuf);

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return changed;
}