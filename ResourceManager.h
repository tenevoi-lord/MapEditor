#pragma once
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <GLFW/glfw3.h>
#include <cstdint>

struct UnitDef {
    std::string sectionName; // Имя секции в ini, например [Squire]
    int globalIndex = 0;
    std::string name;
    std::string descript;
    int iconIndex = 0;
    int cost = 100;
    int costMultipler = 100;
    int costGoldDiv = 1;
    int minMagicPower = 0;
    int manaDrain = 0;
    int startExperience = 100;
    int levelMultipler = 100;
    int surrender = 0;
    int evasion = 0;

    std::string nature;
    std::string magic;
    std::string magicDirection;
    std::string bonus;

    std::string nextUnit1; int nextUnit1Level = 0;
    std::string nextUnit2; int nextUnit2Level = 0;
    std::string nextUnit3; int nextUnit3Level = 0;

    std::map<std::string, int> stats;
    std::map<std::string, int> dStats;

    GLuint iconTex = 0;
};

struct ArtDef {
    std::string id;
    std::string name;
    int cost = 0;
    GLuint tex = 0; // 53x53 natively (Items.ugs)
};

struct SpellDef {
    std::string name;
    int costGold = 0;
    int costMana = 0;
    GLuint tex = 0; // 22x22 natively (.spi)
};

class ResourceManager {
public:
    int costRecrutDiv = 2; // Глобальный параметр

    std::vector<UnitDef> units;
    std::vector<ArtDef> artifacts;
    std::vector<SpellDef> spells;

    bool LoadResources(const std::string& projectRoot);
    void Cleanup();

private:
    std::vector<GLuint> LoadUGS(const std::filesystem::path& path);
    GLuint LoadSPI(const std::filesystem::path& path);
    void DecodePixel(uint16_t raw, uint8_t& a, uint8_t& r, uint8_t& g, uint8_t& b);
    std::string Trim(const std::string& str);
};
enum class ResType { UNIT, ARTIFACT, SPELL };

// Глобальная функция для отрисовки выпадающих списков с иконками
bool DrawGridCombo(const char* id, int& current_item, ResourceManager* resMgr, ResType rType, bool disable_change = false, int start_unit_id = 1);