#include "ObjectManager.h"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

const uint16_t XOR_KEY = 0xAAAA;
const int SHIFT_VAL = 3;

class GraphicCommon {
public:
    static uint16_t rotateRight(uint16_t n, int d) { return (n >> d) | (n << (16 - d)); }

    static void decodePixel(uint16_t raw, uint8_t& a, uint8_t& r, uint8_t& g, uint8_t& b) {
        uint16_t val = rotateRight(raw ^ XOR_KEY, SHIFT_VAL);
        uint8_t a4 = (val & 0xF000) >> 12;
        uint8_t r4 = (val & 0x0F00) >> 8;
        uint8_t g4 = (val & 0x00F0) >> 4;
        uint8_t b4 = (val & 0x000F);
        a = (a4 << 4) | a4;
        r = (r4 << 4) | r4;
        g = (g4 << 4) | g4;
        b = (b4 << 4) | b4;
    }
};

bool ObjectManager::Load(const std::string& folder) {
    objects.clear();
    fs::path ugsPath = fs::path(folder) / "Graphics" / "Objects" / "Objects.ugs";

    // Резервный путь, если запускается напрямую из папки с exe
    if (!fs::exists(ugsPath)) {
        ugsPath = fs::path("Graphics") / "Objects" / "Objects.ugs";
        if (!fs::exists(ugsPath)) return false;
    }

    std::ifstream f(ugsPath, std::ios::binary);
    if (!f.is_open()) return false;

    while (f.peek() != EOF) {
        uint32_t check = 0;
        f.read((char*)&check, 4);
        if (f.gcount() < 4) break;

        GameObject obj;
        if (check == 0) {
            obj.isEmpty = true;
        }
        else {
            f.seekg(-4, std::ios::cur);
            obj.isEmpty = false;

            f.read((char*)&obj.header.groupId, 4);
            f.read((char*)&obj.header.objectId, 4);
            f.read((char*)&obj.header.gridSize, 4);
            f.read((char*)&obj.header.width, 4);
            f.read((char*)&obj.header.height, 4);

            uint32_t nv = 0; f.read((char*)&nv, 4);
            uint16_t cs = 0; f.read((char*)&cs, 2);

            if (cs == (uint16_t)obj.header.width && obj.header.width < 60000) {
                obj.header.type = ObjType::Decor;
                obj.header.special = nv;
                uint16_t h = 0; f.read((char*)&h, 2);
            }
            else {
                obj.header.type = ObjType::Functional;
                obj.header.mapCellsW = nv;
                uint16_t hh = 0; f.read((char*)&hh, 2);
                obj.header.mapCellsH = (uint32_t)cs | ((uint32_t)hh << 16);
                uint16_t w = 0, h = 0; f.read((char*)&w, 2); f.read((char*)&h, 2);
            }

            size_t sz = static_cast<size_t>(obj.header.width) * obj.header.height;
            if (sz > 25000000) { obj.isEmpty = true; break; }

            std::vector<uint16_t> pixels(sz);
            f.read((char*)pixels.data(), sz * sizeof(uint16_t));

            std::vector<uint8_t> rgba(sz * 4);
            for (size_t i = 0; i < sz; ++i) {
                GraphicCommon::decodePixel(pixels[i], rgba[i * 4 + 3], rgba[i * 4 + 0], rgba[i * 4 + 1], rgba[i * 4 + 2]);
            }

            glGenTextures(1, &obj.texture);
            glBindTexture(GL_TEXTURE_2D, obj.texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, obj.header.width, obj.header.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
        objects.push_back(obj);
    }
    return true;
}

void ObjectManager::Cleanup() {
    for (auto& obj : objects) {
        if (obj.texture != 0) {
            glDeleteTextures(1, &obj.texture);
            obj.texture = 0;
        }
    }
    objects.clear();
}