#include "ArmySettings.h"
#include "StringUtils.h"
#include <imgui.h>
#include <cstring>
#include <algorithm>
#include <GLFW/glfw3.h>
#include <cstdint>
#include <string>
#include <vector>
#include <cmath>
#include "MapParser.h"
#include "ObjectManager.h"
#include "ResourceManager.h"
#include "TextureManager.h"
#include <cstdio>

static bool initialized = false;
static char buf_name[256] = "";
static char buf_leader[256] = "";
static char buf_desc[1024] = "";
static ArmyObject temp_army;
static int current_army_idx = -1;

// Автоматический перенос строк: заменяет пробел на \t\n (Таб используется как скрытый маркер мягкого переноса)
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
            result[last_space_pos] = '\t'; // Маркируем место бывшего пробела
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

// Коллбэк для ImGui: переносит слова на лету, вставляя скрытый маркер \t\n
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
                data->InsertChars(last_space_byte, "\t\n"); // Заменяем пробел на невидимый таб + перенос
                i = last_space_byte + 1; // Компенсируем сдвиг цикла
                line_start_byte = last_space_byte + 2;
                char_count = 0;
                last_space_byte = -1;
            }
        }
    }
    return 0;
}

// Восстанавливает текст перед сохранением: убирает визуальные переносы (\t\n), превращая их обратно в пробелы
static std::string UnwrapString(const std::string& text) {
    std::string result;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\t') {
            result += ' '; // Возвращаем пробел на место
            // Если за табом идет \n, пропускаем его (это наш мягкий перенос)
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                i++;
            }
        }
        else {
            result += text[i];
        }
    }
    return result;
}

// ================= ФУНКЦИИ РАСЧЕТА ТАКТИЧЕСКОЙ СИЛЫ =================
struct UnitStats {
    int hits = 0;
    int max_hits = 0;
    int atk_blow = 0;
    int atk_shot = 0;
    int def_blow = 0;
    int def_shot = 0;
    int magic_pwr = 0;
    int initiative = 0;
    int manevres = 0;
    int prot_life = 0;
    int prot_death = 0;
    int prot_elem = 0;
    int vamp = 0;
    int regen = 0;
    int bonus_id = 0;
    std::string magic_type = "";
    std::string magic_dir = "";
    int row = 0;
    int cost_multipler = 100;
    int unit_class = 0;
    int strength = 0;
};

struct UnitEvaluation {
    int strength = 0;            int unit_class = 0;
};

struct ArmyStrength {
    int cumulative_strength = 0; int tactical_strength = 0;
};

static int GetInternalBonusId(const std::string& bonus) {
    if (bonus == "SpearDefense") return 1; if (bonus == "HorseAtack") return 2;
    if (bonus == "ArmorIgnore") return 3; if (bonus == "ArmyMedic") return 4;
    if (bonus == "Merchant") return 5; if (bonus == "DeathCurse") return 6;
    if (bonus == "GodStrike") return 7; if (bonus == "GodAnger") return 8;
    if (bonus == "Unvulnerabe") return 9; if (bonus == "VampirsGist") return 10;
    if (bonus == "OldVampirsGist") return 11; if (bonus == "Evasive") return 12;
    if (bonus == "Ghost") return 13; if (bonus == "Artillery") return 14;
    if (bonus == "Garrison") return 15; if (bonus == "AddPayment") return 16;
    if (bonus == "Poison") return 17; if (bonus == "Dead") return 18;
    if (bonus == "FastDead") return 19; if (bonus == "Counterblow") return 20;
    if (bonus == "FlankStrike") return 21;
    return 0;
}

static int CalculateUpkeep(int cost, int unitDiv, int globalRecrutDiv) {
    if (unitDiv <= 0) unitDiv = 2;
    if (globalRecrutDiv <= 0) globalRecrutDiv = 1;

    double baseRate = (double)cost / (double)unitDiv;
    double multiplier = 1.0;

    if (cost <= 50) multiplier = 0.25;
    else if (cost <= 100) multiplier = 0.50;
    else if (cost <= 150) multiplier = 0.75;
    else multiplier = 1.0;

    double finalValue = (baseRate / (double)globalRecrutDiv) * multiplier;
    return static_cast<int>(std::round(finalValue));
}

static int GetStatAtLevel(int base, int delta, int target_level, bool is_percent = false) {
    double val = base;
    double d = delta;
    for (int i = 0; i < target_level; ++i) {
        if (is_percent) val = val + d * (100.0 - val) / 100.0;
        else val = val + d;
    }
    return static_cast<int>(std::round(val));
}

static UnitEvaluation CalculateUnitCoreStats(const UnitStats& u, int forced_mode = 0, int global_ranged_threshold = 9999) {
    double hits = std::max(1, u.hits);
    double manevres = std::max(1, u.manevres);

    // 1. Определение класса для EHP (в ассемблере sub_57D188 делитель 2.0: flt_57D730)
    int ehp_class = 4;
    if (u.atk_shot > u.atk_blow && u.atk_shot > u.magic_pwr) ehp_class = 7;
    if (u.magic_pwr >= (u.atk_blow / 2.0) && u.magic_pwr >= (u.atk_shot / 2.0)) ehp_class = 17;

    double ehp_bonus = 0.0;
    if (ehp_class == 4) {
        double v3 = u.def_blow / 21.5;
        double v6 = (v3 < 100.0) ? (std::exp(v3) / 1.17) : 999999.0;
        double v4 = (u.regen * hits / 100.0) + u.def_shot;
        ehp_bonus = (std::exp(v4 / 30.3) / 1.07) + v6;
    }
    else {
        double v9 = (u.def_blow > 5.0) ? (u.def_blow - 5.0) : 0.0;
        double v6 = ((v9 / 21.5) < 100.0) ? (std::exp(v9 / 21.5) / 1.17) : 999999.0;
        double v2 = (u.regen * hits / 100.0) + u.def_shot + 5.0;
        ehp_bonus = (std::exp(v2 / 30.3) / 1.07) + v6;
    }
    ehp_bonus = std::round(ehp_bonus * hits);

    if (u.bonus_id == 15) ehp_bonus *= 2.0;
    else if (u.bonus_id == 1) ehp_bonus *= 1.15;
    else if (u.bonus_id == 9 || u.bonus_id == 13) ehp_bonus = hits * 10.0;
    else if (u.bonus_id == 10 || u.bonus_id == 11 || u.bonus_id == 12) ehp_bonus *= 1.5;

    double base_ehp = hits + ehp_bonus;
    if (u.bonus_id == 18 || u.bonus_id == 19) base_ehp *= 1.7;

    double resist_factor = (u.prot_life + u.prot_death + u.prot_elem * 1.5) / 560.0;
    double pot_defense = base_ehp * (1.0 + resist_factor);

    // 2. Выбор активной атаки: forced_mode 1 = только мили, 2 = только магия/стрельба, 0 = авто
    int act_blow = (forced_mode == 2) ? 0 : u.atk_blow;
    int act_shot = (forced_mode == 1) ? 0 : u.atk_shot;
    int act_magic = (forced_mode == 1) ? 0 : u.magic_pwr;

    double eff_attack = 0.0;
    int attack_class = 4;

    if (act_blow >= act_shot && act_blow >= act_magic) {
        eff_attack = act_blow;
        attack_class = 4;
    }
    else if (act_shot >= act_blow && act_shot >= act_magic) {
        eff_attack = act_shot * 1.4;
        attack_class = 7;
    }
    else {
        // ВАЖНО: Модификаторы ToEnemy / ToAll применяются ТОЛЬКО к магии!
        eff_attack = act_magic;
        attack_class = 17;
        if (manevres > 0) {
            if (u.magic_dir == "ToEnemy") {
                eff_attack = (0.8 * eff_attack * (manevres - 1.0) / manevres) + (eff_attack * 0.2);
                if (u.magic_type == "ElementalMagic") eff_attack += 25.0;
            }
            else if (u.magic_dir == "ToAll") {
                eff_attack *= 1.2;
            }
        }
    }

    if (u.bonus_id == 3 || u.bonus_id == 10 || u.bonus_id == 11 || u.bonus_id == 14) eff_attack += 18.0;

    if (u.bonus_id == 15) eff_attack *= 2.0;
    else if (u.bonus_id == 7) eff_attack += 10.0;
    else if (u.bonus_id == 8) eff_attack += 20.0;
    else if (u.bonus_id == 20) eff_attack += act_blow;
    else if (u.bonus_id == 21) eff_attack += (act_blow / 3.0);

    double var_38 = (u.bonus_id == 2 || u.bonus_id == 11 || u.bonus_id == 19) ? (eff_attack * 0.15) : 0.0;
    eff_attack = (eff_attack * manevres) + var_38;

    if (u.initiative > 0) eff_attack = eff_attack + (eff_attack * (u.initiative / 100.0));
    else eff_attack = 0.0;

    double final_ehp = pot_defense + pot_defense * (u.vamp / 100.0) * (eff_attack / hits);
    double raw_strength = (3.2 * final_ehp * (eff_attack + 1.0)) / 200.0;

    if (u.bonus_id == 6 || u.bonus_id == 13) raw_strength += 150.0;
    if (u.bonus_id == 17) raw_strength *= 1.1;
    if (raw_strength > 2147483647.0 || std::isnan(raw_strength)) raw_strength = 2147483647.0;

    raw_strength = (raw_strength * u.cost_multipler) / 100.0;

    int final_strength = static_cast<int>(std::round(raw_strength));
    //if (attack_class == 7 && act_shot >= global_ranged_threshold && u.bonus_id != 14) final_strength /= 3;
    if (final_strength <= 0) final_strength = 1;

    return { final_strength, attack_class };
}

static ArmyStrength CalculateArmyStrength(std::vector<UnitStats>& army) {
    if (army.empty()) return { 0, 0 };
    int cumulative = 0;
    double row_power[4] = { 0.0, 0.0, 0.0, 0.0 };

    // 1. Прямой расчет базовой силы каждого юнита
    for (auto& unit : army) {
        UnitEvaluation eval = CalculateUnitCoreStats(unit);
        unit.unit_class = eval.unit_class;
        unit.strength = eval.strength;

        // Кумулятивная сила — это чистая сумма всех базовых сил без модификаторов рядов
        cumulative += unit.strength;
    }

    // 2. Сортировка: Воины (Класс 4) вперед, внутри класса — по убыванию силы
    std::sort(army.begin(), army.end(), [](const UnitStats& a, const UnitStats& b) {
        if (a.unit_class != b.unit_class) return a.unit_class < b.unit_class;
        return a.strength > b.strength;
        });

    // 3. Авторасстановка по рядам
    const int ROW_CAPACITY[4] = { 0, 6, 4, 2 };
    int row_counts[4] = { 0, 0, 0, 0 };
    auto try_place = [&](int unit_idx, int target_row) -> bool {
        if (row_counts[target_row] < ROW_CAPACITY[target_row]) {
            army[unit_idx].row = target_row;
            row_counts[target_row]++;
            return true;
        }
        return false;
        };

    try_place(0, 1); // 1-й юнит всегда во фронте
    for (size_t i = 1; i < army.size(); ++i) {
        if (army[i].unit_class == 4) {
            if (!try_place(i, 1)) { if (!try_place(i, 3)) try_place(i, 2); }
        }
        else {
            if (!try_place(i, 2)) { if (!try_place(i, 3)) try_place(i, 1); }
        }
    }

    // 4. Подсчет тактической мощности рядов (с удвоением стрелков/магов во 2 ряду)
    for (const auto& unit : army) {
        int r = std::clamp(unit.row, 1, 3);
        double power = static_cast<double>(unit.strength);

        if (r == 2 && unit.unit_class != 4) {
            power *= 2.0; // Удвоение мощи заднего ряда
        }
        row_power[r] += power;
    }

    // 5. Вычисление коэффициента прикрытия
    double cover_ratio = 1.0;
    if (row_power[2] > 0.0) {
        cover_ratio = (row_power[1] * 0.8 / row_power[2]) + 0.2;
        if (cover_ratio > 1.0) cover_ratio = 1.0;
    }

    double total_tactical = (row_power[2] * cover_ratio) + (row_power[3] + row_power[1]);
    int tactical_strength = static_cast<int>(std::round(total_tactical));

    // Штраф одиночки (если юнит всего 1 и это не воин)
    if (army.size() == 1 && army[0].unit_class != 4) {
        tactical_strength /= 5;
    }

    if (tactical_strength <= 0) tactical_strength = 1;

    return { cumulative, tactical_strength };
}
// ====================================================================

void DrawArmySettingsModal(bool& open_flag, std::vector<ArmyObject>& armies, MapData& map, int army_index, ResourceManager* resMgr, TextureManager* texMgr, ObjectManager* objMgr) {
    if (!open_flag || armies.empty()) {
        initialized = false;
        current_army_idx = -1;
        return;
    }

    // Лямбда для сохранения текущей армии и её текстов
    auto SaveCurrentArmy = [&]() {
        if (current_army_idx >= 0 && current_army_idx < (int)armies.size()) {
            armies[current_army_idx] = temp_army;
            if (current_army_idx >= (int)map.army_texts.size()) {
                map.army_texts.resize(current_army_idx + 1);
            }
            map.army_texts[current_army_idx].name = utf8_to_cp1251(buf_name);
            map.army_texts[current_army_idx].leader = utf8_to_cp1251(buf_leader);
            map.army_texts[current_army_idx].desc = utf8_to_cp1251(UnwrapString(buf_desc));
        }
        };

    // Лямбда для загрузки армии по индексу
    auto LoadArmy = [&](int idx) {
        current_army_idx = std::clamp(idx, 0, (int)armies.size() - 1);
        temp_army = armies[current_army_idx];

        if (current_army_idx < (int)map.army_texts.size()) {
            std::string tmp;
            tmp = cp1251_to_utf8(map.army_texts[current_army_idx].name);
            strncpy_s(buf_name, sizeof(buf_name), tmp.c_str(), _TRUNCATE);
            tmp = cp1251_to_utf8(map.army_texts[current_army_idx].leader);
            strncpy_s(buf_leader, sizeof(buf_leader), tmp.c_str(), _TRUNCATE);

            tmp = cp1251_to_utf8(map.army_texts[current_army_idx].desc);
            tmp = AutoWrapString(tmp, 75);
            strncpy_s(buf_desc, sizeof(buf_desc), tmp.c_str(), _TRUNCATE);
        }
        else {
            buf_name[0] = '\0'; buf_leader[0] = '\0'; buf_desc[0] = '\0';
        }
        };

    // Первичная инициализация при открытии модалки
    if (!initialized) {
        ImGui::OpenPopup("Настройка армий");
        LoadArmy(army_index);
        initialized = true;
    }

    ImGui::SetNextWindowSize(ImVec2(810, 560), ImGuiCond_FirstUseEver);

    // --- Применяем белую/светлую тему только для этого окна ---
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

    if (ImGui::BeginPopupModal("Настройка армий", NULL, ImGuiWindowFlags_NoSavedSettings)) {

        // --- ЗАГОЛОВОК С КНОПКАМИ ПЕРЕКЛЮЧЕНИЯ (ПО КРУГУ) ---
        if (ImGui::Button("^", ImVec2(25, 25))) {
            if (!armies.empty()) {
                SaveCurrentArmy();
                int next_idx = (current_army_idx < (int)armies.size() - 1) ? (current_army_idx + 1) : 0;
                LoadArmy(next_idx);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("v", ImVec2(25, 25))) {
            if (!armies.empty()) {
                SaveCurrentArmy();
                int prev_idx = (current_army_idx > 0) ? (current_army_idx - 1) : ((int)armies.size() - 1);
                LoadArmy(prev_idx);
            }
        }
        ImGui::SameLine(); 

        // Выводим ID текущей армии
        ImGui::Text("#%d", temp_army.id);

        ImGui::SameLine(120); ImGui::Text("Название"); ImGui::SameLine();
        ImGui::PushItemWidth(300);
        ImGui::InputText("##Name", buf_name, sizeof(buf_name));
        ImGui::PopItemWidth();

        ImGui::Separator();

        // --- ОБНОВЛЕНИЕ МОДЕЛИ АРМИИ ---
        int calc_leader_id = temp_army.units[0]; // Проверяем главного предводителя (слот 0)
        
            if (calc_leader_id > 0 && static_cast<size_t>(calc_leader_id) < resMgr->units.size()) {
                        // Если предводитель задан, проверяем его ID и натуру
                if (calc_leader_id == 43 || calc_leader_id == 44 || calc_leader_id == 47 || calc_leader_id == 96) {
                temp_army.model = 12; // Зомби
                
            }
             else if (calc_leader_id == 51 || calc_leader_id == 52 || calc_leader_id == 53 || calc_leader_id == 92) {
                temp_army.model = 11; // Призраки
                
            }
             else if (resMgr->units[calc_leader_id].nature == "Undead") {
                temp_army.model = 10; // Мертвые
                
            }
             else {
                                // Если предводитель есть, но не попадает под undead-исключения, модель берется по стилю поведения
                    if (temp_army.behavior == 1) temp_army.model = 5;
                else if (temp_army.behavior == 2) temp_army.model = 6;
                else temp_army.model = 4;
                
            }
            
        }
         else {
                        // Если предводителя нет (класс 0), модель строго по стилю поведения
                if (temp_army.behavior == 1) temp_army.model = 5;      // Разбойничье
            else if (temp_army.behavior == 2) temp_army.model = 6; // Крестьянское
            else temp_army.model = 4;                              // Феодальное
            
        }

        if (ImGui::BeginTabBar("ArmyTabs")) {
            // ------------------ ВКЛАДКА 1: ПРЕДВОДИТЕЛЬ ------------------
            if (ImGui::BeginTabItem("Предводитель")) {
                ImGui::BeginGroup(); // Блоки 1, 2, 3 (Левая колонка)

                // Блок 1: Класс и Портрет
                GLuint portrait = 0;
                if (calc_leader_id > 0 && static_cast<size_t>(calc_leader_id) < resMgr->units.size()) {
                    portrait = resMgr->units[calc_leader_id].iconTex;
                }

                // Отрисовка портрета 52х52 без учета альфа-канала
                if (portrait) {
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    ImVec2 p_min = ImGui::GetCursorScreenPos();
                    ImVec2 p_max = ImVec2(p_min.x + 52, p_min.y + 52);

                    draw_list->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
                        glDisable(GL_BLEND);
                        }, nullptr);

                    draw_list->AddImage((ImTextureID)(intptr_t)portrait, p_min, p_max);
                    draw_list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

                    ImGui::Dummy(ImVec2(52, 52));
                }
                else {
                    ImGui::Dummy(ImVec2(52, 52));
                }

                ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::Text("Имя героя"); ImGui::SameLine(100);
                ImGui::PushItemWidth(200);
                ImGui::InputText("##Leader", buf_leader, sizeof(buf_leader));
                ImGui::PopItemWidth();

                ImGui::Text("Базовый класс"); ImGui::SameLine(100);
                int b_class = temp_army.units[0];

                // Считаем число обычных войск в отряде
                int follower_units = 0;
                for (int i = 0; i < 6; i++) {
                    int idx = 2 + i * 3;
                    if (temp_army.units[idx] != 0) follower_units += temp_army.units[idx + 2];
                }
                // Блокируем выбор, если войск уже 12/12 и предводитель еще не выбран (0)
                bool disable_leader_select = (follower_units >= 12 && b_class == 0);

                ImGui::PushItemWidth(200);
                if (DrawGridCombo("##BaseClass", b_class, resMgr, ResType::UNIT, disable_leader_select)) {
                    temp_army.units[0] = static_cast<uint8_t>(b_class);
                    temp_army.model = static_cast<uint8_t>(b_class);
                }
                ImGui::PopItemWidth();
                ImGui::EndGroup();

                bool use_named = temp_army.named_hero_id > 0;
                if (ImGui::Checkbox("Взять именной персонаж", &use_named)) temp_army.named_hero_id = use_named ? 1 : 0;
                ImGui::SameLine();
                int named_id = temp_army.named_hero_id;
                ImGui::PushItemWidth(160);

                std::string n_preview = "Отсутствует";
                if (use_named && named_id > 0 && static_cast<size_t>(named_id - 1) < map.named_units_texts.size()) {
                    n_preview = cp1251_to_utf8(map.named_units_texts[named_id - 1]);
                }

                if (use_named) {
                    if (ImGui::BeginCombo("##NamedHero", n_preview.c_str())) {
                        if (ImGui::Selectable("Отсутствует", named_id == 0)) { temp_army.named_hero_id = 0; }
                        for (size_t i = 0; i < map.named_units_texts.size(); ++i) {
                            ImGui::PushID((int)i);
                            if (ImGui::Selectable(cp1251_to_utf8(map.named_units_texts[i]).c_str(), named_id == static_cast<int>(i + 1))) {
                                temp_army.named_hero_id = static_cast<uint16_t>(i + 1);
                            }
                            ImGui::PopID();
                        }
                        ImGui::EndCombo();
                    }
                }
                else {
                    ImGui::BeginDisabled();
                    if (ImGui::BeginCombo("##NamedHero", "Отсутствует")) ImGui::EndCombo();
                    ImGui::EndDisabled();
                }
                ImGui::PopItemWidth();

                // Блок 2: Уровень и Параметры найма
                ImGui::Dummy(ImVec2(0, 10));
                ImGui::Text("Характеристики на начало сценария");
                ImGui::Text("Стартовый уровень"); ImGui::SameLine(150); ImGui::Text("Запас золота");

                int lvl = temp_army.units[1];
                ImGui::PushItemWidth(80);
                if (ImGui::InputInt("##Lvl", &lvl, 1, 10)) temp_army.units[1] = static_cast<uint8_t>(std::clamp(lvl, 0, 255));
                ImGui::PopItemWidth();

                ImGui::SameLine(150);
                int gld = temp_army.gold_reserve;
                ImGui::PushItemWidth(100);
                if (ImGui::InputInt("##Gold", &gld, 100, 1000)) temp_army.gold_reserve = static_cast<uint16_t>(std::clamp(gld, 0, 65535));
                ImGui::PopItemWidth();

                // Блок 3: Корабль
                ImGui::Dummy(ImVec2(0, 10));
                ImGui::Text("Корабль");
                int ship = temp_army.ship_type;
                ImGui::BeginGroup();
                GLuint s_tex = 0;
                if (ship >= 1 && ship <= 3) {
                    s_tex = texMgr->tex_editor[16 + ship].id; // SHIP1 (17), SHIP2 (18), SHIP3 (19)
                }

                if (s_tex) ImGui::Image((ImTextureID)(intptr_t)s_tex, ImVec2(64, 64));
                else ImGui::Dummy(ImVec2(64, 64));

                ImGui::SameLine();
                ImGui::BeginGroup();
                if (ImGui::Button("^##s1", ImVec2(30, 16))) ship = std::clamp(ship + 1, 0, 3);
                if (ImGui::Button("v##s2", ImVec2(30, 16))) ship = std::clamp(ship - 1, 0, 3);
                ImGui::EndGroup();
                ImGui::EndGroup();
                temp_army.ship_type = ship;

                ImGui::EndGroup(); // Конец левой колонки

                ImGui::SameLine(380); // Блок 4 (Правая колонка)
                ImGui::BeginGroup();

                ImGui::Text("Стартовое строение");
                int s_build = temp_army.start_building_id;
                ImGui::PushItemWidth(250);

                // 1. Формируем превью выбранного строения (s_build - это 1-based индекс: 1 = map.buildings[0])
                std::string b_preview = "Отсутствует";
                if (s_build > 0 && static_cast<size_t>(s_build - 1) < map.buildings.size()) {
                    size_t b_idx = static_cast<size_t>(s_build - 1);
                    b_preview = "Строение #" + std::to_string(s_build);
                    if (b_idx < map.building_texts.size() && !map.building_texts[b_idx].name.empty()) {
                        b_preview += " - " + cp1251_to_utf8(map.building_texts[b_idx].name);
                    }
                }

                // 2. Выпадающий список выбора строения
                if (ImGui::BeginCombo("##StartBld", b_preview.c_str())) {
                    if (ImGui::Selectable("Отсутствует", s_build == 0)) {
                        temp_army.start_building_id = 0;
                    }
                    for (size_t i = 0; i < map.buildings.size(); ++i) {
                        int b_num = static_cast<int>(i + 1); // Реальный номер строения: 1, 2, 3...
                        std::string b_name = "Строение #" + std::to_string(b_num);
                        if (i < map.building_texts.size() && !map.building_texts[i].name.empty()) {
                            b_name += " - " + cp1251_to_utf8(map.building_texts[i].name);
                        }

                        ImGui::PushID((int)i);
                        if (ImGui::Selectable(b_name.c_str(), s_build == b_num)) {
                            temp_army.start_building_id = static_cast<uint8_t>(b_num);
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();

                // 3. Превью здания и координаты (берем напрямую по индексу s_build - 1)
                int bx = temp_army.x, by = temp_army.y;
                GLuint bld_tex = 0;
                if (s_build > 0 && static_cast<size_t>(s_build - 1) < map.buildings.size()) {
                    const auto& bo = map.buildings[s_build - 1];
                    bx = bo.x;
                    by = bo.y;
                    if (objMgr) {
                        for (const auto& o : objMgr->objects) {
                            // Обязательная проверка на то, что это строение, а не декор с тем же ID
                            if (o.header.type == ObjType::Functional && (o.header.groupId * 256 + o.header.objectId) == bo.id) {
                                bld_tex = o.texture;
                                break;
                            }
                        }
                    }
                }

                ImGui::BeginGroup();
                ImGui::Text("Координаты:\nX = %d  Y = %d", bx, by);
                ImGui::EndGroup();
                ImGui::SameLine(180);
                if (bld_tex) ImGui::Image((ImTextureID)(intptr_t)bld_tex, ImVec2(53, 53));
                else ImGui::Dummy(ImVec2(53, 53));

                ImGui::Dummy(ImVec2(0, 10));
                ImGui::Text("Наличные артефакты");
                ImGui::PushItemWidth(300);
                for (int i = 0; i < 3; i++) {
                    int a_id = temp_army.arts[i];
                    ImGui::PushID(i + 100);
                    if (DrawGridCombo("##Art", a_id, resMgr, ResType::ARTIFACT)) temp_army.arts[i] = a_id;
                    ImGui::PopID();
                }

                ImGui::Dummy(ImVec2(0, 5));
                ImGui::Text("Наложенное заклинание");
                int spl = temp_army.active_spell;
                if (DrawGridCombo("##Spell", spl, resMgr, ResType::SPELL)) temp_army.active_spell = spl;
                ImGui::PopItemWidth();

                ImGui::EndGroup();
                ImGui::EndTabItem();
            }

            // ------------------ ВКЛАДКА 2: ВОЙСКА ------------------
            if (ImGui::BeginTabItem("Войска")) {
                // ПРАВИЛЬНЫЙ ПОДСЧЕТ: Игнорируем пустой слот лидера
                int total_units = (temp_army.units[0] != 0) ? 1 : 0;
                for (int i = 0; i < 6; i++) {
                    int idx = 2 + i * 3;
                    if (temp_army.units[idx] != 0) total_units += temp_army.units[idx + 2];
                }

                // --- АВТОРАСЧЕТ ТАКТИЧЕСКОЙ СИЛЫ И СТОИМОСТИ ---
                std::vector<UnitStats> raw_army;
                int total_gold_cost = 0;
                int total_upkeep = 0;

                auto add_to_army = [&](int id, int lvl, int count, bool free_leader_upkeep) {
                    if (id <= 0 || id >= (int)resMgr->units.size()) return;
                    auto& u = resMgr->units[id];

                    UnitStats st;
                    st.hits = GetStatAtLevel(u.stats["Hits"], u.dStats["Hits"], lvl, false);
                    st.max_hits = st.hits;
                    st.atk_blow = GetStatAtLevel(u.stats["AttackBlow"], u.dStats["AttackBlow"], lvl, false);
                    st.atk_shot = GetStatAtLevel(u.stats["AttackShot"], u.dStats["AttackShot"], lvl, false);
                    st.def_blow = GetStatAtLevel(u.stats["DefenceBlow"], u.dStats["DefenceBlow"], lvl, false);
                    st.def_shot = GetStatAtLevel(u.stats["DefenceShot"], u.dStats["DefenceShot"], lvl, false);
                    st.magic_pwr = GetStatAtLevel(u.stats["MagicPower"], u.dStats["MagicPower"], lvl, false);
                    st.initiative = GetStatAtLevel(u.stats["Initiative"], u.dStats["Initiative"], lvl, false);
                    st.manevres = GetStatAtLevel(u.stats["Manevres"], u.dStats["Manevres"], lvl, false);
                    st.prot_life = GetStatAtLevel(u.stats["ProtectLife"], u.dStats["ProtectLife"], lvl, true);
                    st.prot_death = GetStatAtLevel(u.stats["ProtectDeath"], u.dStats["ProtectDeath"], lvl, true);
                    st.prot_elem = GetStatAtLevel(u.stats["ProtectElemental"], u.dStats["ProtectElemental"], lvl, true);
                    st.vamp = GetStatAtLevel(u.stats["Vampirizm"], u.dStats["Vampirizm"], lvl, true);
                    st.regen = GetStatAtLevel(u.stats["Regen"], u.dStats["Regen"], lvl, true);
                    st.bonus_id = GetInternalBonusId(u.bonus);
                    st.magic_type = u.magic;
                    st.magic_dir = u.magicDirection;
                    st.cost_multipler = u.costMultipler;
                    st.row = 0;

                    int unit_upkeep = CalculateUpkeep(u.cost, u.costGoldDiv, resMgr->costRecrutDiv);

                    for (int i = 0; i < count; ++i) {
                        raw_army.push_back(st);
                        total_gold_cost += u.cost;
                        // Предводитель (ровно 1 юнит) освобождается от выплаты
                        if (free_leader_upkeep && i == 0) {
                            // Выплата 0
                        }
                        else {
                            total_upkeep += unit_upkeep;
                        }
                    }
                    };

                bool has_main_leader = (temp_army.units[0] != 0);
                if (has_main_leader) {
                    add_to_army(temp_army.units[0], temp_army.units[1], 1, true); // Предводитель не платит
                }

                bool leader_found = has_main_leader;

                // Проверяем слоты по порядку: 0..2 (Поля 1, 2, 3 слева) и 3..5 (Поля 4, 5, 6 справа)
                for (int i = 0; i < 6; i++) {
                    int idx = 2 + i * 3;
                    if (temp_army.units[idx] != 0 && temp_army.units[idx + 2] > 0) {
                        bool free_first_unit = false;
                        if (!leader_found) {
                            free_first_unit = true; // Самый верхний юнит становится предводителем
                            leader_found = true;
                        }
                        add_to_army(temp_army.units[idx], temp_army.units[idx + 1], temp_army.units[idx + 2], free_first_unit);
                    }
                }

                ArmyStrength calc_str = CalculateArmyStrength(raw_army);
                temp_army.tac_cost_1 = static_cast<uint16_t>(std::clamp(calc_str.cumulative_strength, 0, 65535));
                temp_army.tac_cost_2 = static_cast<uint16_t>(std::clamp(calc_str.tactical_strength, 0, 65535));
                // --------------------------------------------------

                ImGui::BeginGroup();
                ImGui::Text("Описание армии");
                ImGui::InputTextMultiline("##Desc", buf_desc, sizeof(buf_desc), ImVec2(550, 100),
                    ImGuiInputTextFlags_CallbackEdit, AutoWrapCallback);
                ImGui::EndGroup();

                ImGui::SameLine(580);
                ImGui::BeginGroup();
                ImGui::Text("Тактическая стоимость");
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%d / %d", temp_army.tac_cost_1, temp_army.tac_cost_2);
                ImGui::Text("Стоимость");
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "%d / %d", total_gold_cost, total_upkeep);
                ImGui::Text("Золотой доход");
                int inc = temp_army.gold_income * 10;
                ImGui::PushItemWidth(100);
                if (ImGui::InputInt("##Inc", &inc, 10, 50)) {
                    if (inc < 0) inc = 0;
                    // Округляем до ближайших десятков и делим на 10 (диапазон 0..2550 -> 0..255)
                    int raw_income = (inc + 5) / 10;
                    temp_army.gold_income = static_cast<uint8_t>(std::clamp(raw_income, 0, 250));
                }
                ImGui::PopItemWidth();
                ImGui::EndGroup();

                ImGui::Dummy(ImVec2(0, 10));

                ImGui::Columns(2, "ArmyCols", false);
                for (int col = 0; col < 2; col++) {
                    // Скорректированы отступы заголовков
                    ImGui::Text("Начальные войска"); ImGui::SameLine(180); ImGui::Text("Уровень"); ImGui::SameLine(270); ImGui::Text("Число");
                    for (int row = 0; row < 3; row++) {
                        int slot = col * 3 + row;
                        int idx = 2 + slot * 3; // Смещение 2 байта героя + 3 байта на спутника
                        int u_id = temp_army.units[idx];
                        int u_lvl = temp_army.units[idx + 1];
                        int u_cnt = temp_army.units[idx + 2];

                        ImGui::PushID(idx);

                        ImGui::PushItemWidth(165);
                        bool is_disabled = (total_units >= 12 && u_id == 0);
                        if (DrawGridCombo("##UID", u_id, resMgr, ResType::UNIT, is_disabled, 4)) {
                            temp_army.units[idx] = static_cast<uint16_t>(u_id);
                            if (u_id != 0 && u_cnt == 0) {
                                temp_army.units[idx + 2] = 1;
                                total_units++;
                            }
                            if (u_id == 0) {
                                total_units -= u_cnt;
                                temp_army.units[idx + 2] = 0;
                                temp_army.units[idx + 1] = 0;
                            }
                        }
                        ImGui::PopItemWidth();

                        // Блокируем изменение уровня и количества, если юнит не выбран
                        bool no_unit = (u_id == 0);
                        if (no_unit) ImGui::BeginDisabled();

                        // Поле уровня
                        ImGui::SameLine(180);
                        ImGui::PushItemWidth(80);
                        if (ImGui::InputInt("##Lvl", &u_lvl, 1)) {
                            temp_army.units[idx + 1] = static_cast<uint8_t>(std::clamp(u_lvl, 0, 255));
                        }
                        ImGui::PopItemWidth();

                        // Поле количества
                        ImGui::SameLine(270);
                        ImGui::PushItemWidth(72);
                        int pre_cnt = u_cnt;
                        if (ImGui::InputInt("##Cnt", &u_cnt, 1)) {
                            u_cnt = std::max(0, u_cnt);
                            if (u_cnt > pre_cnt && total_units >= 12) {
                                u_cnt = pre_cnt + (12 - total_units);
                            }
                            total_units += (u_cnt - pre_cnt);
                            temp_army.units[idx + 2] = static_cast<uint8_t>(u_cnt);
                            if (u_cnt == 0) {
                                temp_army.units[idx] = 0;
                                temp_army.units[idx + 1] = 0;
                            }
                        }
                        ImGui::PopItemWidth();

                        if (no_unit) ImGui::EndDisabled();

                        ImGui::PopID();
                    }
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);

                ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.0f, 1.0f), "Всего юнитов (макс 12): %d", total_units);
                ImGui::EndTabItem();
            }

            // ------------------ ВКЛАДКА 3: Настройка AI ------------------
            if (ImGui::BeginTabItem("Настройка AI")) {
                // Разделяем вкладку на левую и правую части
                ImGui::BeginChild("AI_Left_Pane", ImVec2(390, -45), false);

                // --- БЛОК 1 и 2 ---
                ImGui::BeginChild("Blocks_1_2", ImVec2(0, 115), false);
                ImGui::Columns(2, "bl12", false);
                ImGui::SetColumnWidth(0, 170);

                // Блок 1: Стиль поведения
                ImGui::SeparatorText("Стиль поведения");
                int b_mod = temp_army.behavior;
                ImGui::RadioButton("Феодальное", &b_mod, 0);
                ImGui::RadioButton("Разбойничье", &b_mod, 1);
                ImGui::RadioButton("Крестьянское", &b_mod, 2);
                temp_army.behavior = b_mod;

                ImGui::NextColumn();

                // Блок 2: Найм и гарнизон
                ImGui::SeparatorText("Найм и гарнизон");

                // 1. Добавить опыт: шаг 10, ускоренный 50, диапазон 0..1000
                int exp_rec = temp_army.exp_recruit;
                ImGui::AlignTextToFramePadding(); ImGui::Text("Добавить опыт"); ImGui::SameLine(120);
                ImGui::PushItemWidth(88);
                // Передаем шаг 10 и ускоренный шаг 50 (Ctrl + клик)
                if (ImGui::InputInt("##expr", &exp_rec, 10, 50)) {
                    temp_army.exp_recruit = (uint16_t)std::clamp(exp_rec, 0, 1000); // Ограничиваем от 0 до 1000
                }
                ImGui::PopItemWidth();

                ImGui::Checkbox("Как у игрока", &temp_army.exp_as_player);

                // 2. Сила гарнизона: шаг 5, ускоренный 20, диапазон 0..90
                int gar_str = temp_army.garrison_str;
                ImGui::AlignTextToFramePadding(); ImGui::Text("Сила гарнизона"); ImGui::SameLine(120);
                ImGui::PushItemWidth(88);
                // Передаем шаг 5 и ускоренный шаг 20 (Ctrl + клик)
                if (ImGui::InputInt("##gars", &gar_str, 5, 20)) {
                    temp_army.garrison_str = (uint8_t)std::clamp(gar_str, 0, 90); // Ограничиваем от 0 до 90
                }
                ImGui::PopItemWidth();

                ImGui::Columns(1);
                ImGui::EndChild();

                // --- БЛОК 3: Передвижение ---
                ImGui::SeparatorText("Передвижение");
                bool not_act = (temp_army.activity != 0);
                ImGui::Checkbox("Армия не активна", &not_act); ImGui::SameLine(170);
                ImGui::Checkbox("Патрулирование", &temp_army.has_patrol);
                temp_army.activity = not_act ? 1 : 0;

                ImGui::BeginGroup();
                ImGui::BeginGroup(); ImGui::Text("Коррекция\nскорости"); ImGui::EndGroup();
                ImGui::SameLine(85);
                ImGui::BeginGroup();
                ImGui::Dummy(ImVec2(0, 4)); // Безопасный сдвиг вниз
                int s_mod = (int8_t)temp_army.speed_mod;
                ImGui::PushItemWidth(90);
                if (ImGui::InputInt("##smod", &s_mod, 1)) {
                    temp_army.speed_mod = (uint8_t)std::clamp(s_mod, -3, 5);
                } 
                ImGui::PopItemWidth();
                ImGui::EndGroup();
                ImGui::EndGroup();

                ImGui::SameLine(190);

                ImGui::BeginGroup();
                ImGui::BeginGroup(); ImGui::Text("Радиус\nпатруля"); ImGui::EndGroup();
                ImGui::SameLine(60);
                ImGui::BeginGroup();
                ImGui::Dummy(ImVec2(0, 4));
                int p_rad = temp_army.patrol_data;
                ImGui::PushItemWidth(90);
                if (ImGui::InputInt("##prad", &p_rad, 5, 25)) {
                    temp_army.patrol_data = (uint8_t)std::clamp(p_rad, 0, 250);
                }
                ImGui::PopItemWidth();
                ImGui::EndGroup();
                ImGui::EndGroup();

                ImGui::Dummy(ImVec2(0, 10));

                // --- БЛОК 4 и 5 ---
                ImGui::BeginChild("Blocks_4_5", ImVec2(0, 115), false);
                ImGui::Columns(2, "bl45", false);
                ImGui::SetColumnWidth(0, 190);

                // Блок 4: Воскрешение
                ImGui::SeparatorText("Воскрешение");
                ImGui::BeginGroup();
                ImGui::BeginGroup(); ImGui::Text("Время\nвоскрешения"); ImGui::EndGroup();
                ImGui::SameLine(95);
                ImGui::BeginGroup();
                ImGui::Dummy(ImVec2(0, 4));
                int r_time = temp_army.respawn_time;
                ImGui::PushItemWidth(80);
                if (ImGui::InputInt("##rtime", &r_time, 1)) {
                    temp_army.respawn_time = (uint8_t)std::clamp(r_time, 0, 30);
                }
                ImGui::PopItemWidth();
                ImGui::EndGroup();
                ImGui::EndGroup();
                ImGui::Checkbox("Воскрешение всех", &temp_army.respawn_all);

                ImGui::NextColumn();

                // Блок 5: Трофеи
                ImGui::SeparatorText("Трофеи");
                ImGui::BeginGroup();
                ImGui::BeginGroup(); ImGui::Text("Коррекция\nопыта"); ImGui::EndGroup();
                ImGui::SameLine(85);
                ImGui::BeginGroup();
                ImGui::Dummy(ImVec2(0, 4));
                int ex_mod = temp_army.exp_mod;
                ImGui::PushItemWidth(88);
                if (ImGui::InputInt("##exmod", &ex_mod, 5, 25)) {
                    temp_army.exp_mod = (uint8_t)std::clamp(ex_mod, 10, 250);
                }
                ImGui::PopItemWidth();
                ImGui::EndGroup();
                ImGui::EndGroup();
                ImGui::Checkbox("Юниты без денег", &temp_army.free_units);

                ImGui::Columns(1);
                ImGui::EndChild();

                ImGui::EndChild(); // Конец левой части

                // ==============================================
                ImGui::SameLine(410);
                // ==============================================

                ImGui::BeginChild("AI_Right_Pane", ImVec2(0, -45), false);

                // --- БЛОК 6: Группировки и союзы ---
                ImGui::SeparatorText("Группировки и союзы");
                ImGui::Columns(2, "bl6", false);
                ImGui::SetColumnWidth(0, 160);

                ImGui::TextDisabled("Выбор группировки");
                int grp = temp_army.group_id;
                int old_grp = grp; // Запоминаем текущую группировку перед кликом

                // 1. Зеленые (игрок)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.55f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.0f, 0.55f, 0.0f, 1.0f));
                ImGui::RadioButton("Зеленые (игрок)", &grp, 1);
                ImGui::PopStyleColor(2);

                // 2. Синие (союзник)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.4f, 0.9f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.0f, 0.4f, 0.9f, 1.0f));
                ImGui::RadioButton("Синие (союзник)", &grp, 2);
                ImGui::PopStyleColor(2);

                // 3. Желтые (сосед)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.5f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.7f, 0.5f, 0.0f, 1.0f));
                ImGui::RadioButton("Желтые (сосед)", &grp, 3);
                ImGui::PopStyleColor(2);

                // 4. Красные (враг)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.15f, 0.15f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.85f, 0.15f, 0.15f, 1.0f));
                ImGui::RadioButton("Красные (враг)", &grp, 4);
                ImGui::PopStyleColor(2);

                // Если группировку переключили — применяем отношения из параметров сценария
                if (grp != old_grp) {
                    temp_army.group_id = std::clamp(grp, 1, 4);
                    int base_idx = (temp_army.group_id - 1) * 4;
                    for (int i = 0; i < 4; ++i) {
                        // Если в функции объект карты называется loaded_map, замените map на loaded_map
                        temp_army.relations[i] = (uint8_t)map.relations[base_idx + i];
                    }
                }
                else {
                    temp_army.group_id = std::clamp(grp, 1, 4);
                }

                ImGui::NextColumn();

                ImGui::TextDisabled("Дружелюбие и мир");
                ImVec4 colors[] = {
                    ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
                    ImVec4(0.2f, 0.5f, 1.0f, 1.0f),
                    ImVec4(0.8f, 0.8f, 0.0f, 1.0f),
                    ImVec4(1.0f, 0.3f, 0.3f, 1.0f)
                };
                int pct_vals[] = { 0, 16, 33, 50, 66, 83, 100 }; // Маппинг от -3 до +3

                for (int i = 0; i < 4; i++) {
                    int rel = (int8_t)temp_army.relations[i];
                    ImGui::PushID(i);

                    if (ImGui::Button("-", ImVec2(20, 20))) rel = std::max(-3, rel - 1);

                    ImGui::SameLine();
                    // std::clamp предотвращает вылет за границы массива
                    int pct = pct_vals[std::clamp(rel + 3, 0, 6)];
                    char buf[16]; snprintf(buf, sizeof(buf), "%d%%", pct);

                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, colors[i]);
                    ImGui::ProgressBar(pct / 100.0f, ImVec2(130, 20), buf);
                    ImGui::PopStyleColor();

                    ImGui::SameLine();
                    if (ImGui::Button("+", ImVec2(20, 20))) rel = std::min(3, rel + 1);

                    temp_army.relations[i] = (uint8_t)rel;
                    ImGui::PopID();
                }
                ImGui::Columns(1);
                ImGui::Dummy(ImVec2(0, 10));

                // --- БЛОК 7: Настройка выбора целей армии ---
                ImGui::SeparatorText("Настройка выбора целей армии");

                // Верхняя часть блока 7
                ImGui::BeginGroup();
                ImGui::Text("Модель поведения");
                const char* models[] = { "Стандартная", "Агрессивная", "Пассивная", "Накопительная", "Торговая" };
                int bmod = temp_army.behavior_model;
                ImGui::PushItemWidth(150);
                ImGui::Combo("##bmod", &bmod, models, 5);
                ImGui::PopItemWidth();
                temp_army.behavior_model = bmod;
                ImGui::EndGroup();

                ImGui::SameLine(180);

                ImGui::BeginGroup();
                ImGui::Checkbox("Игнорируется AI", &temp_army.ignore_ai);
                ImGui::Checkbox("Преследует игрока", &temp_army.pursue_player);
                ImGui::EndGroup();

                ImGui::Dummy(ImVec2(0, 10));

                // Нижняя часть блока 7
                ImGui::BeginGroup();
                ImGui::BeginGroup(); ImGui::Text("Агрессия\nармии"); ImGui::EndGroup();
                ImGui::SameLine(80);
                ImGui::BeginGroup();
                ImGui::Dummy(ImVec2(0, 4));
                int aggr = (int8_t)temp_army.aggression;
                ImGui::PushItemWidth(88);
                if (ImGui::InputInt("##aggr", &aggr, 10, 20)) {
                    temp_army.aggression = (uint8_t)std::clamp(aggr, -100, 100);
                }
                ImGui::PopItemWidth();
                ImGui::EndGroup();
                ImGui::EndGroup();

                ImGui::SameLine(180);

                ImGui::BeginGroup();
                ImGui::Checkbox("Нет случайных целей", &temp_army.no_random_targets);
                ImGui::Checkbox("Нет общения с армиями", &temp_army.no_chat);
                ImGui::Checkbox("Нет интереса к зданиям", &temp_army.no_buildings);
                ImGui::EndGroup();

                ImGui::EndChild(); // Конец правой части

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        // --- ПОДВАЛ (КНОПКИ) ---
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 40);
        ImGui::Separator();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 220);

        if (ImGui::Button("Сохранить", ImVec2(100, 25))) {
            SaveCurrentArmy();
            ImGui::CloseCurrentPopup();
            open_flag = false;
            initialized = false;
            current_army_idx = -1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Выход", ImVec2(100, 25))) {
            ImGui::CloseCurrentPopup();
            open_flag = false;
            initialized = false;
            current_army_idx = -1;
        }

        ImGui::EndPopup();
    }

    // Снимаем заданную ранее белую тему
    ImGui::PopStyleColor(18);
}