#include "UnitSettings.h"
#include "StringUtils.h"
#include <imgui.h>
#include <string>
#include <algorithm>
#include <cmath>

static bool initialized = false;
static int selected_unit_idx = -1;
static char buf_name[256] = "";
static char buf_desc[1024] = "";
static char search_buf[256] = "";


// --- АЛГОРИТМ РАСЧЕТА ТАКТИЧЕСКОЙ СИЛЫ ---

static int GetInternalBonusId(const std::string& bonus) {
    if (bonus == "SpearDefense") return 1;
    if (bonus == "HorseAtack") return 2;
    if (bonus == "ArmorIgnore") return 3;
    if (bonus == "ArmyMedic") return 4;
    if (bonus == "Merchant") return 5;
    if (bonus == "DeathCurse") return 6;
    if (bonus == "GodStrike") return 7;
    if (bonus == "GodAnger") return 8;
    if (bonus == "Unvulnerabe") return 9;
    if (bonus == "VampirsGist") return 10;
    if (bonus == "OldVampirsGist") return 11;
    if (bonus == "Evasive") return 12;
    if (bonus == "Ghost") return 13;
    if (bonus == "Artillery") return 14;
    if (bonus == "Garrison") return 15;
    if (bonus == "AddPayment") return 16;
    if (bonus == "Poison") return 17;
    if (bonus == "Dead") return 18;
    if (bonus == "FastDead") return 19;
    if (bonus == "Counterblow") return 20;
    if (bonus == "FlankStrike") return 21;
    return 0; // Остальные не влияют на расчет
}

static int GetStatAtLevel(int base, int delta, int target_level, bool is_percent = false) {
    double val = base;
    double d = delta;
    for (int i = 1; i < target_level; ++i) {
        if (is_percent) val = val + d * (100.0 - val) / 100.0;
        else val = val + d;
    }
    // ИСПРАВЛЕНИЕ: Движок использует математическое округление (round), а не просто отсечение (int)
    return static_cast<int>(std::round(val));
}

struct CoreStats {
    double strength;
    double pot_attack;
    double pot_defense;
};

static CoreStats GetCoreStats(int hits_in, int atk_blow, int atk_shot, int def_blow, int def_shot,
    int magic_pwr, int initiative, int manevres_in,
    int prot_death, int prot_life, int prot_elem,
    int vamp, int regen, int bonus_id,
    const std::string& magic_type, const std::string& magic_dir) {
    double hits = std::max(1, hits_in);
    double manevres = std::max(1, manevres_in);

    int unit_class = 4;
    if (atk_shot > atk_blow && atk_shot > magic_pwr) unit_class = 7;
    if (magic_pwr >= (atk_blow / 2.0) && magic_pwr >= (atk_shot / 2.0)) unit_class = 17;

    double ehp_bonus = 0.0;
    if (unit_class == 4) {
        double v3 = def_blow / 21.5;
        double v6 = (v3 < 100.0) ? (std::exp(v3) / 1.17) : 999999.0;
        double v4 = (regen * hits / 100.0) + def_shot;
        ehp_bonus = (std::exp(v4 / 30.3) / 1.07 + v6);
    }
    else {
        double v9 = (def_blow > 5.0) ? (def_blow - 5.0) : 0.0;
        double v2 = (regen * hits / 100.0) + def_shot + 5.0;
        double v6 = ((v9 / 21.5) < 100.0) ? (std::exp(v9 / 21.5) / 1.17) : 999999.0; //std::round((std::exp(v2 / 30.3) / 1.07 + v6) * hits);
        ehp_bonus = (std::exp(v2 / 30.3) / 1.07 + v6); //ehp_bonus = (std::exp(v2 / 30.3) / 1.07) + v6;
    }
    ehp_bonus = std::round(ehp_bonus * hits);

    if (bonus_id == 15) ehp_bonus *= 2.0;
    else if (bonus_id == 1) ehp_bonus *= 1.15;
    else if (bonus_id == 9 || bonus_id == 13) ehp_bonus = hits * 10.0;
    else if (bonus_id == 10 || bonus_id == 11 || bonus_id == 12) ehp_bonus *= 1.5;

    double base_ehp = hits + ehp_bonus;
    if (bonus_id == 18 || bonus_id == 19) base_ehp *= 1.7;

    double resist_factor = (prot_life + prot_death + prot_elem * 1.5) / 560.0;
    double pot_defense = base_ehp * (1.0 + resist_factor);

    double eff_attack = 0.0;
    if (atk_blow >= atk_shot && atk_blow >= magic_pwr) eff_attack = atk_blow;
    else if (atk_shot >= atk_blow && atk_shot >= magic_pwr) eff_attack = atk_shot * 1.4;
    else {
        // ВАЖНО: Модификаторы ToEnemy / ToAll применяются ТОЛЬКО к магии
        eff_attack = magic_pwr;
        if (manevres > 0) {
            if (magic_dir == "ToEnemy") {
                eff_attack = (0.8 * eff_attack * (manevres - 1.0) / manevres) + (eff_attack * 0.2);
                if (magic_type == "ElementalMagic") eff_attack += 25.0;
            }
            else if (magic_dir == "ToAll") {
                eff_attack *= 1.2;
            }
        }
    }

    if (bonus_id == 3 || bonus_id == 10 || bonus_id == 11 || bonus_id == 14) eff_attack += 18.0;

    if (bonus_id == 15) eff_attack *= 2.0;
    if (bonus_id == 7) eff_attack += 10.0;
    if (bonus_id == 8) eff_attack += 20.0;
    if (bonus_id == 20) eff_attack += atk_blow;
    if (bonus_id == 21) eff_attack += (atk_blow / 3.0);

    double var_38 = (bonus_id == 2 || bonus_id == 11 || bonus_id == 19) ? (eff_attack * 0.15) : 0.0;
    eff_attack = (manevres * eff_attack) + var_38;

    if (initiative > 0) {
        eff_attack = eff_attack + (eff_attack * (initiative / 100.0));
    }
    else {
        eff_attack = 0.0;
    }
    double pot_attack = eff_attack;
    // Вампиризм увеличивает защитный потенциал юнита (loc_57D65E)
    pot_defense = pot_defense + pot_defense * (vamp / 100.0) * (pot_attack / hits);
    double strength = (3.2 * pot_defense * (pot_attack + 1.0)) / 200.0;

    if (bonus_id == 6 || bonus_id == 13) strength += 150.0;
    if (bonus_id == 17) strength *= 1.1;

    if (std::isnan(strength) || std::isinf(strength)) strength = 2147483647.0;

    return { strength, pot_attack, pot_defense };  //return { strength, pot_attack, pot_defense };
}
// --------------------------------------------------------

// ================= ФУНКЦИИ АВТОПЕРЕНОСА ТЕКСТА =================
static std::string AutoWrapString(const std::string& text, int max_chars = 75) {
    std::string result;
    int char_count = 0;
    size_t last_space_pos = std::string::npos;
    size_t line_start_pos = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\r') continue;
        if (c == '\n') {
            result += '\n';
            char_count = 0;
            line_start_pos = result.size();
            last_space_pos = std::string::npos;
            continue;
        }

        if ((c & 0xC0) != 0x80) char_count++;
        if (c == ' ') last_space_pos = result.size();

        result += (char)c;

        if (char_count >= max_chars && last_space_pos != std::string::npos && last_space_pos >= line_start_pos) {
            result[last_space_pos] = '\t';
            result.insert(last_space_pos + 1, "\n");
            line_start_pos = last_space_pos + 2;
            char_count = 0;
            for (size_t k = line_start_pos; k < result.size(); ++k) {
                if (((unsigned char)result[k] & 0xC0) != 0x80) char_count++;
            }
            last_space_pos = std::string::npos;
        }
    }
    return result;
}

static int AutoWrapCallback(ImGuiInputTextCallbackData* data) {
    const int MAX_LINE_CHARS = 75;
    int char_count = 0;
    int line_start_byte = 0;
    int last_space_byte = -1;

    for (int i = 0; i < data->BufTextLen; ++i) {
        unsigned char c = (unsigned char)data->Buf[i];
        if (c == '\n') {
            char_count = 0;
            line_start_byte = i + 1;
            last_space_byte = -1;
            continue;
        }

        if ((c & 0xC0) != 0x80) char_count++;
        if (c == ' ') last_space_byte = i;

        if (char_count >= MAX_LINE_CHARS) {
            if (last_space_byte != -1 && last_space_byte >= line_start_byte) {
                data->DeleteChars(last_space_byte, 1);
                data->InsertChars(last_space_byte, "\t\n");
                i = last_space_byte + 1;
                line_start_byte = last_space_byte + 2;
                char_count = 0;
                last_space_byte = -1;
            }
        }
    }
    return 0;
}

static std::string UnwrapString(const std::string& text) {
    std::string result;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\t') {
            result += ' ';
            if (i + 1 < text.size() && text[i + 1] == '\n') i++;
        }
        else {
            result += text[i];
        }
    }
    return result;
}
// ===============================================================

// Вспомогательная функция для статов ИЗ СЛОВАРЯ (С поуровневой прибавкой)
static void DrawStatLine(const char* label, const char* key, UnitDef& u, bool has_lvl, ImVec4 color) {
    ImGui::PushID(key);

    // Поле базового значения
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushItemWidth(95);
    ImGui::InputInt("##base", &u.stats[key], 1);
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();

    ImGui::SameLine();
    if (has_lvl) {
        ImGui::PushItemWidth(95);
        ImGui::InputInt("##lvl", &u.dStats[key], 1);
        ImGui::PopItemWidth();
    }
    else {
        ImGui::Dummy(ImVec2(95, 1));
    }

    ImGui::SameLine();
    ImGui::TextColored(color, "%s", label);

    ImGui::PopID();
}

// Вспомогательная функция для ПРЯМЫХ статов (Без поуровневой прибавки)
static void DrawStatLineDirect(const char* label, int* base_val, ImVec4 color) {
    ImGui::PushID(label);

    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushItemWidth(95);
    ImGui::InputInt("##base", base_val, 1);
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(95, 1)); // Отступ для выравнивания колонки

    ImGui::SameLine();
    ImGui::TextColored(color, "%s", label);

    ImGui::PopID();
}

void DrawUnitSettingsModal(bool& open_flag, ResourceManager* resMgr) {
    if (!open_flag || resMgr->units.empty()) {
        initialized = false;
        return;
    }

    auto LoadUnit = [&](int idx) {
        selected_unit_idx = std::clamp(idx, 0, (int)resMgr->units.size() - 1);
        auto& u = resMgr->units[selected_unit_idx];

        snprintf(buf_name, sizeof(buf_name), "%s", u.name.c_str());

        // Применяем автоперенос к загруженному описанию
        std::string wrapped_desc = AutoWrapString(u.descript, 75);
        snprintf(buf_desc, sizeof(buf_desc), "%s", wrapped_desc.c_str());
        };

    auto SaveUnit = [&]() {
        if (selected_unit_idx >= 0 && selected_unit_idx < (int)resMgr->units.size()) {
            resMgr->units[selected_unit_idx].name = buf_name;
            resMgr->units[selected_unit_idx].descript = UnwrapString(buf_desc); // Разворачиваем перед сохранением
        }
        };

    if (!initialized) {
        ImGui::OpenPopup("Настройка отрядов");
        LoadUnit(1);
        search_buf[0] = '\0';
        initialized = true;
    }

    ImGui::SetNextWindowSize(ImVec2(1100, 750), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal("Настройка отрядов", &open_flag, ImGuiWindowFlags_NoSavedSettings)) {

        ImGui::Columns(2, "UnitMainCols", false);
        ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() - 280); // Левая панель 

        // ======================= ЛЕВАЯ ПАНЕЛЬ =======================
        ImGui::BeginChild("LeftPanel", ImVec2(0, 0), false);

        if (selected_unit_idx >= 0) {
            auto& u = resMgr->units[selected_unit_idx];

            // ----------------- БЛОК 1 -----------------
            ImGui::Text("Название отряда:"); ImGui::SameLine();
            ImGui::PushItemWidth(250);
            ImGui::InputText("##Name", buf_name, sizeof(buf_name));
            ImGui::PopItemWidth();
            ImGui::SameLine(); ImGui::Text("#%d", selected_unit_idx);

            ImGui::Dummy(ImVec2(0, 2));
            ImGui::Text("Описание отряда:");

            ImGui::BeginGroup();
            if (u.iconTex) {
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                ImVec2 p_min = ImGui::GetCursorScreenPos();
                ImVec2 p_max = ImVec2(p_min.x + 92, p_min.y + 92);

                // Отключаем альфа-блендинг перед отрисовкой
                draw_list->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
                    glDisable(GL_BLEND);
                    }, nullptr);

                draw_list->AddImage((ImTextureID)(intptr_t)u.iconTex, p_min, p_max);

                // Возвращаем стандартное состояние рендера ImGui
                draw_list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

                ImGui::Dummy(ImVec2(92, 92));
            }
            else {
                ImGui::Dummy(ImVec2(92, 92));
            }
            ImGui::EndGroup();

            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::InputTextMultiline("##Desc", buf_desc, sizeof(buf_desc), ImVec2(600, 92),
                ImGuiInputTextFlags_CallbackEdit, AutoWrapCallback);
            ImGui::EndGroup();
            ImGui::Separator();

            // ----------------- БЛОКИ 2 и 3 -----------------
            ImGui::BeginChild("MiddleBlocks", ImVec2(0, 180), false);
            ImGui::Columns(2, "Blocks23", false);
            ImGui::SetColumnWidth(0, 480);

            // -- БЛОК 2 (Слева) --
            ImGui::TextDisabled("Постоянные характеристики");

            // --- Вызов алгоритма ---
            int bonus_id = GetInternalBonusId(u.bonus);
            CoreStats cs = GetCoreStats(
                u.stats["Hits"], u.stats["AttackBlow"], u.stats["AttackShot"], u.stats["DefenceBlow"], u.stats["DefenceShot"],
                u.stats["MagicPower"], u.stats["Initiative"], u.stats["Manevres"],
                u.stats["ProtectDeath"], u.stats["ProtectLife"], u.stats["ProtectElemental"],
                u.stats["Vampirizm"], u.stats["Regen"], bonus_id, u.magic, u.magicDirection
            );

            double tactic_cost_float = (cs.strength * u.costMultipler) / 100.0;
            int calc_str = static_cast<int>(std::round(tactic_cost_float));
            int pot_atk = static_cast<int>(std::round(cs.pot_attack));
            int pot_def = static_cast<int>(std::round(cs.pot_defense));

            double gold_div = (u.costGoldDiv > 0) ? u.costGoldDiv : 1.0;
            double gold_float = (tactic_cost_float / 2.2) + 21.0;
            int rec_cost = static_cast<int>(std::round(gold_float / gold_div));

            if (rec_cost < 100) rec_cost = 5 * (rec_cost / 5);
            else if (rec_cost < 251) rec_cost = 10 * (rec_cost / 10);
            else if (rec_cost < 501) rec_cost = 20 * (rec_cost / 20);
            else if (rec_cost < 5001) rec_cost = 50 * (rec_cost / 50);
            else rec_cost = 100 * (rec_cost / 100);

            if (u.globalIndex == 1 || u.globalIndex == 2 || u.globalIndex == 3) rec_cost *= 2;
            // ------------------------

            ImGui::Text("Сила:"); ImGui::SameLine(50); ImGui::Text("%d", calc_str); ImGui::SameLine(130);
            ImGui::Text("Коррекция:"); ImGui::SameLine();
            ImGui::PushItemWidth(90);
            ImGui::InputInt("##strc", &u.costMultipler, 1); ImGui::SameLine();
            ImGui::InputInt("##cstc", &u.costGoldDiv, 1);
            ImGui::PopItemWidth();

            ImGui::Text("Потенциал:"); ImGui::SameLine(130);
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Защ %d", pot_def); ImGui::SameLine();
            ImGui::Text("/"); ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Атк %d", pot_atk);

            ImGui::Text("Стоимость найма:"); ImGui::SameLine(130);
            ImGui::TextDisabled("(рек. %d)", rec_cost); ImGui::SameLine();
            ImGui::PushItemWidth(100);
            ImGui::InputInt("##Cost", &u.cost, 10);
            ImGui::PopItemWidth();

            ImGui::Text("Тип отряда:"); ImGui::SameLine(130);
            const char* nature_keys[] = { "", "People", "Rogue", "Undead", "Animal", "Elemental" };
            const char* nature_names[] = { "Обычный", "Люди", "Разбойники", "Нежить", "Животные", "Элементали" };
            int n_idx = 0; for (int i = 0; i < 6; i++) { if (u.nature == nature_keys[i]) n_idx = i; }
            ImGui::PushItemWidth(150);
            if (ImGui::Combo("##Nature", &n_idx, nature_names, 6)) u.nature = nature_keys[n_idx];
            ImGui::PopItemWidth();

            ImGui::Text("Магия:"); ImGui::SameLine(130);
            const char* mag_keys[] = { "", "LifeMagic", "ElementalMagic", "DeathMagic" };
            const char* mag_names[] = { "Нет", "Жизни", "Стихий", "Смерти" };
            const char* mdir_keys[] = { "", "ToAll", "ToEnemy", "ToAlly", "Curse", "Attack", "Bless", "Heal" };
            const char* mdir_names[] = { "Нет", "На всех", "На чужих", "На своих", "Проклятие", "Атакующая", "Благость", "Лечащая" };

            int m_idx = 0; for (int i = 0; i < 4; i++) { if (u.magic == mag_keys[i]) m_idx = i; }
            int md_idx = 0; for (int i = 0; i < 8; i++) { if (u.magicDirection == mdir_keys[i]) md_idx = i; }

            ImGui::PushItemWidth(100);
            if (ImGui::Combo("##Magic", &m_idx, mag_names, 4)) u.magic = mag_keys[m_idx];
            ImGui::SameLine();
            if (ImGui::Combo("##MagicDir", &md_idx, mdir_names, 8)) u.magicDirection = mdir_keys[md_idx];
            ImGui::PopItemWidth();

            ImGui::Text("Базовый опыт:"); ImGui::SameLine(130);
            ImGui::PushItemWidth(95);
            ImGui::InputInt("##expb", &u.startExperience, 10); ImGui::SameLine();
            ImGui::Text("на %%"); ImGui::SameLine();
            ImGui::InputInt("##expp", &u.levelMultipler, 10);
            ImGui::PopItemWidth();

            // -- БЛОК 3 (Справа) --
            ImGui::NextColumn();
            ImGui::TextDisabled("Развитие отряда");
            ImGui::Dummy(ImVec2(0, 10));

            // Лямбда-функция для отрисовки комбо-бокса развития и ровного счетчика уровня
            auto DrawNextUnit = [&](const char* id_str, std::string& nextUnitStr, int& reqLevel) {
                ImGui::PushID(id_str);
                int u_id = 0;
                if (!nextUnitStr.empty()) {
                    for (size_t i = 1; i < resMgr->units.size(); ++i) {
                        if (utf8_contains_icase(resMgr->units[i].sectionName, nextUnitStr) ||
                            utf8_contains_icase(resMgr->units[i].name, nextUnitStr)) {
                            u_id = (int)i;
                            break;
                        }
                    }
                }

                // 1. Блок комбобокса (высота 40px)
                ImGui::BeginGroup();
                ImGui::PushItemWidth(180);
                if (DrawGridCombo("##upg", u_id, resMgr, ResType::UNIT)) {
                    nextUnitStr = (u_id > 0) ? resMgr->units[u_id].sectionName : "";
                }
                ImGui::PopItemWidth();
                ImGui::EndGroup();

                ImGui::SameLine();

                // 2. Изолированный блок счетчика уровня (центрируется ровно по высоте)
                ImGui::BeginGroup();
                float offset_y = (40.0f - ImGui::GetFrameHeight()) * 0.5f;
                if (offset_y > 0.0f) {
                    ImGui::Dummy(ImVec2(0.0f, offset_y)); // Ровный вертикальный отступ
                }
                ImGui::PushItemWidth(95);
                ImGui::InputInt("##lvl", &reqLevel, 1);
                ImGui::PopItemWidth();
                ImGui::EndGroup();

                ImGui::PopID();
                };

            DrawNextUnit("U1", u.nextUnit1, u.nextUnit1Level);
            DrawNextUnit("U2", u.nextUnit2, u.nextUnit2Level);
            DrawNextUnit("U3", u.nextUnit3, u.nextUnit3Level);

            ImGui::Columns(1);
            ImGui::EndChild();
            ImGui::Separator();

            // ----------------- БЛОК 4 (Низ) -----------------
            std::string proj_texts = "";
            for (int lvl = 2; lvl <= 6; ++lvl) {
                CoreStats c_lvl = GetCoreStats(
                    GetStatAtLevel(u.stats["Hits"], u.dStats["Hits"], lvl),
                    GetStatAtLevel(u.stats["AttackBlow"], u.dStats["AttackBlow"], lvl),
                    GetStatAtLevel(u.stats["AttackShot"], u.dStats["AttackShot"], lvl),
                    GetStatAtLevel(u.stats["DefenceBlow"], u.dStats["DefenceBlow"], lvl),
                    GetStatAtLevel(u.stats["DefenceShot"], u.dStats["DefenceShot"], lvl),
                    GetStatAtLevel(u.stats["MagicPower"], u.dStats["MagicPower"], lvl),
                    GetStatAtLevel(u.stats["Initiative"], u.dStats["Initiative"], lvl),
                    GetStatAtLevel(u.stats["Manevres"], u.dStats["Manevres"], lvl),
                    GetStatAtLevel(u.stats["ProtectDeath"], u.dStats["ProtectDeath"], lvl, true),
                    GetStatAtLevel(u.stats["ProtectLife"], u.dStats["ProtectLife"], lvl, true),
                    GetStatAtLevel(u.stats["ProtectElemental"], u.dStats["ProtectElemental"], lvl, true),
                    GetStatAtLevel(u.stats["Vampirizm"], u.dStats["Vampirizm"], lvl, true),
                    GetStatAtLevel(u.stats["Regen"], u.dStats["Regen"], lvl, true),
                    bonus_id, u.magic, u.magicDirection
                );

                int c_tac = static_cast<int>(std::round((c_lvl.strength * u.costMultipler) / 100.0));
                int pct = (calc_str > 0) ? (c_tac * 100 / calc_str) : 0;

                if (lvl > 2) proj_texts += " | ";
                proj_texts += std::to_string(c_tac) + ":" + std::to_string(pct) + "%";
            }

            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Прогноз силы (Ур 2-6): %s", proj_texts.c_str());
            ImGui::Dummy(ImVec2(0, 5));

            float text_w = ImGui::CalcTextSize("Переменные характеристики").x;
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - text_w) * 0.5f);
            ImGui::TextDisabled("Переменные характеристики");

            // Цветовая палитра
            ImVec4 cRed = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            ImVec4 cGreen = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
            ImVec4 cBlue = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
            ImVec4 cDef = ImGui::GetStyleColorVec4(ImGuiCol_Text);

            if (ImGui::BeginTable("VarStats", 2, ImGuiTableFlags_None)) {
                ImGui::TableNextColumn();
                // --- Левый столбец ---
                DrawStatLine("Атака рукопашная", "AttackBlow", u, true, cRed);
                DrawStatLine("Атака стрелковая", "AttackShot", u, true, cRed);
                DrawStatLine("Защита рукопашная", "DefenceBlow", u, true, cGreen);
                DrawStatLine("Защита стрелковая", "DefenceShot", u, true, cGreen);
                DrawStatLine("Магическая сила", "MagicPower", u, true, cRed);
                DrawStatLine("Инициатива в бою", "Initiative", u, true, cBlue);
                DrawStatLine("Маневры", "Manevres", u, true, cBlue);
                DrawStatLineDirect("Уклонение", &u.evasion, cDef);

                ImGui::TableNextColumn();
                // --- Правый столбец ---
                DrawStatLine("Число хитов", "Hits", u, true, cBlue);
                DrawStatLine("% иммунитет магия жизни", "ProtectLife", u, true, cGreen);
                DrawStatLine("% иммунитет магия стихий", "ProtectElemental", u, true, cGreen);
                DrawStatLine("% иммунитет магия смерти", "ProtectDeath", u, true, cGreen);
                DrawStatLine("% вампиризм", "Vampirizm", u, true, cDef);
                DrawStatLine("% регенерация", "Regen", u, true, cDef);
                DrawStatLineDirect("Минимум силы", &u.minMagicPower, cDef);
                DrawStatLineDirect("Расход силы", &u.manaDrain, cDef);

                ImGui::EndTable();
            }

            ImGui::Dummy(ImVec2(0, 10));
            ImGui::Text("Бонус:"); ImGui::SameLine();
            ImGui::PushItemWidth(200);

            const char* bonus_keys[] = { nullptr, "ArmorIgnore", "Artillery", "ArmyMedic", "GodAnger", "GodStrike", "HorseAtack",
        "Dead", "FastDead", "VampirsGist", "OldVampirsGist", "Evasive", "Unvulnerabe", "Counterblow", "Merchant", "Garrison",
        "Poison", "Ghost", "SpearDefense", "DeathCurse", "AddPayment", "FlankStrike", "Hunger",
        "Berserk", "Exhaustion", "Drying", "CtrPoison", "Suicide", "Caster", "Splash", "Fortify", "Dominate", "Concentration",
        "PoisonS", "Stun", "Potent", "FirstShot", "Bastion", "Flying", "Flock", "Bleed", "HoldLine", "ArmorBreaker", "PoisonArmorIgnore", "FasterAttack",
        "NoHeal", "PreventiveStrike", "Neutralize", "KillingStrike", "BloodThrist", "Assault", "EternalGift", "FateGift" };

            const char* bonus_names[] = {
         "Нет", "Проникающий Удар", "Шквальная Атака", "Лекарское Умение", "Кара Господня",
         "Гнев Господня", "Быстрая Атака", "Мертвец", "Быстрый Мертвец", "Темный Дар",
         "Темное Искусство", "Увертливость", "Неуязвимость", "Контрудар", "Торговец-Эксперт",
         "Гарнизон", "Отравленное Оружие", "Яростный Дух", "Длинное Оружие", "Проклятие Смерти",
         "Тыловая Служба", "Фланговый Удар", "Голод", "Берсерк", "Истощение", "Иссушение",
         "Ядовитый", "Смертник", "Колдовство", "Размашистый удар", "Укрепление", "Доминация",
         "Концентрация", "Сильный Яд", "Оглушающий удар", "Мощная магия", "Первый удар",
         "Бастион", "Летающий", "Стая", "Кровотечение", "Держать строй", "Разрушение брони",
         "Ядовитый прокол", "Молниеносная атака", "Калечащий удар", "Упреждающий выстрел",
         "Нейтрализация", "Добивание", "Жажда крови", "Штурмовик", "Дар вечности", "Подарок Судьбы" };

            int b_idx = 0; for (int i = 0; i < 53; i++) { if (bonus_keys[i] && u.bonus == bonus_keys[i]) b_idx = i; }
            if (ImGui::Combo("##bonus", &b_idx, bonus_names, 53)) u.bonus = (b_idx == 0) ? "" : bonus_keys[b_idx];
            ImGui::PopItemWidth();

            ImGui::SameLine(360);
            ImGui::Text("Плен:"); ImGui::SameLine();
            ImGui::PushItemWidth(95);
            ImGui::InputInt("##surr", &u.surrender, 1);
            ImGui::PopItemWidth();
        }
        ImGui::EndChild();

        // ======================= ПРАВАЯ ПАНЕЛЬ =======================
        ImGui::NextColumn();
        ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);

        ImGui::Text("Список отрядов");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##Filter", "Введите имя...", search_buf, sizeof(search_buf));
        ImGui::Separator();

        ImGui::BeginChild("UnitList", ImVec2(0, -45), false);
        for (int i = 1; i < (int)resMgr->units.size(); ++i) {
            std::string name = resMgr->units[i].name;
            if (name.empty()) name = "Отряд #" + std::to_string(i);

            if (search_buf[0] != '\0' && !utf8_contains_icase(name, search_buf)) continue;

            bool is_sel = (selected_unit_idx == i);

            // ДОБАВЛЕН НЕВИДИМЫЙ ИНДЕКС: теперь стрелочки не будут зависать на одинаковых именах!
            std::string label_id = name + "##" + std::to_string(i);

            if (ImGui::Selectable(label_id.c_str(), is_sel)) {
                SaveUnit();
                LoadUnit(i);
            }
        }
        ImGui::EndChild();

        if (ImGui::Button("Сохранить", ImVec2(100, 30))) { SaveUnit(); open_flag = false; initialized = false; }
        ImGui::SameLine();
        if (ImGui::Button("Выход", ImVec2(100, 30))) { open_flag = false; initialized = false; }

        ImGui::EndChild();
        ImGui::Columns(1);
        ImGui::EndPopup();
    }
    else {
        initialized = false;
    }
}