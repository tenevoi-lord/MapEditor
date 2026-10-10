#include "TextureManager.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <GLFW/glfw3.h>

// Определение GL_CLAMP_TO_EDGE для стандартного OpenGL на Windows
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace {
    const double PI = 3.14159265358979323846;

    void YCbCr_to_RGB(uint8_t y, uint8_t cb, uint8_t cr, uint8_t& r, uint8_t& g, uint8_t& b) {
        float Y = y;
        float Cb = cb - 128.0f;
        float Cr = cr - 128.0f;
        int R = (int)round(Y + 1.402f * Cr);
        int G = (int)round(Y - 0.344136f * Cb - 0.714136f * Cr);
        int B = (int)round(Y + 1.772f * Cb);
        r = (uint8_t)std::clamp(R, 0, 255);
        g = (uint8_t)std::clamp(G, 0, 255);
        b = (uint8_t)std::clamp(B, 0, 255);
    }

    void DecodePlane(const uint8_t* source, std::vector<uint8_t>& dest, int widthBlocks, int heightBlocks) {
        int totalBlocks = widthBlocks * heightBlocks;
        for (int by = 0; by < heightBlocks; ++by) {
            for (int bx = 0; bx < widthBlocks; ++bx) {
                int blockIdx = by * widthBlocks + bx;
                double block[64]{};
                for (int i = 0; i < 64; ++i) {
                    int zz = source[0x40 + i];
                    int8_t coeff = (int8_t)source[0x80 + zz * totalBlocks + blockIdx];
                    uint8_t quant = source[i];
                    block[i] = (double)(coeff * quant);
                }

                double temp[64]{};
                for (int y = 0; y < 8; ++y) {
                    for (int x = 0; x < 8; ++x) {
                        double sum = 0;
                        for (int u = 0; u < 8; ++u) {
                            double cu = (u == 0) ? 0.7071067811865475 : 1.0;
                            sum += cu * block[y * 8 + u] * cos((2.0 * x + 1.0) * u * PI / 16.0);
                        }
                        temp[y * 8 + x] = sum;
                    }
                }

                double outBlock[64]{};
                for (int x = 0; x < 8; ++x) {
                    for (int y = 0; y < 8; ++y) {
                        double sum = 0;
                        for (int v = 0; v < 8; ++v) {
                            double cv = (v == 0) ? 0.7071067811865475 : 1.0;
                            sum += cv * temp[v * 8 + x] * cos((2.0 * y + 1.0) * v * PI / 16.0);
                        }
                        outBlock[y * 8 + x] = sum;
                    }
                }

                for (int y = 0; y < 8; ++y) {
                    for (int x = 0; x < 8; ++x) {
                        int val = (int)round(outBlock[y * 8 + x] * 2.0);
                        if (val < 0) val = 0;
                        val = (val + 4) >> 3;
                        if (val > 255) val = 255;
                        dest[(by * 8 + y) * (widthBlocks * 8) + (bx * 8 + x)] = (uint8_t)val;
                    }
                }
            }
        }
    }

    void DeblockPlane(std::vector<uint8_t>& buf, int width, int height) {
        for (int y = 1; y < height; ++y) {
            for (int x = 1; x < width; ++x) {
                int left = y * width + x - 1, curr = y * width + x;
                int diff = buf[left] - buf[curr];
                int delta = (diff >= 0) ? (diff >> 2) : ((diff + 3) >> 2);
                buf[left] -= delta; buf[curr] += delta;
            }
        }
        for (int x = 1; x < width; ++x) {
            for (int y = 1; y < height; ++y) {
                int top = (y - 1) * width + x, curr = y * width + x;
                int diff = buf[top] - buf[curr];
                int delta = (diff >= 0) ? (diff >> 2) : ((diff + 3) >> 2);
                buf[top] -= delta; buf[curr] += delta;
            }
        }
    }

    void UpscalePlane(std::vector<uint8_t>& buf, int halfWidthBlocks, int halfHeightBlocks) {
        int halfW = halfWidthBlocks * 8, halfH = halfHeightBlocks * 8;
        int fullW = halfWidthBlocks * 16, fullH = halfHeightBlocks * 16;
        for (int y = halfH - 1; y >= 0; --y) {
            for (int x = halfW - 1; x >= 0; --x) {
                uint8_t val = buf[y * halfW + x];
                int idx = (y * 2) * fullW + (x * 2);
                buf[idx] = val; buf[idx + 1] = val;
                buf[idx + fullW] = val; buf[idx + fullW + 1] = val;
            }
        }
        for (int y = 0; y < fullH; ++y) {
            uint8_t prev = buf[y * fullW];
            for (int x = 1; x < fullW - 1; ++x) {
                int curr = y * fullW + x;
                int sum = (int)buf[curr] * 2 + prev + buf[curr + 1];
                prev = buf[curr];
                buf[curr] = (uint8_t)((sum >= 0) ? (sum >> 2) : ((sum + 3) >> 2));
            }
        }
        for (int x = 0; x < fullW; ++x) {
            uint8_t prev = buf[x];
            for (int y = 1; y < fullH - 1; ++y) {
                int curr = y * fullW + x;
                int sum = (int)buf[curr] * 2 + prev + buf[curr + fullW];
                prev = buf[curr];
                buf[curr] = (uint8_t)((sum >= 0) ? (sum >> 2) : ((sum + 3) >> 2));
            }
        }
    }
}

#pragma pack(push, 1)
struct LitHeader {
    char magic[4];
    uint32_t width;
    uint32_t height;
    uint32_t flags;
};
#pragma pack(pop)

GLuint TextureManager::LoadLitTextureFromMemory(const uint8_t* data, size_t size, int* out_w, int* out_h) {
    if (!data || size < sizeof(LitHeader)) return 0;

    LitHeader header{};
    std::memcpy(&header, data, sizeof(LitHeader));
    if (header.magic[0] != 'L' || header.magic[1] != 'I' || header.magic[2] != 'T') return 0;
    if (header.width == 0 || header.height == 0 || header.width > 4096 || header.height > 4096) return 0;

    bool isUncompressed = (header.flags & 0x04) != 0;
    bool isSubsampled = (header.flags & 0x02) != 0;
    bool hasAlpha = (header.flags & 0x08) != 0;

    std::vector<uint8_t> rgbaOut(header.width * header.height * 4, 255);
    int align = isSubsampled ? 16 : 8;
    int alignedW = (header.width + align - 1) & ~(align - 1);
    int alignedH = (header.height + align - 1) & ~(align - 1);

    size_t mem_offset = sizeof(LitHeader);

    if (isUncompressed) {
        size_t rawSize = static_cast<size_t>(header.width) * header.height * 3;
        if (mem_offset + rawSize > size) return 0;
        const uint8_t* rawData = data + mem_offset;
        for (size_t i = 0, out_i = 0; i < rawSize; i += 3, out_i += 4) {
            YCbCr_to_RGB(rawData[i], rawData[i + 1], rawData[i + 2], rgbaOut[out_i], rgbaOut[out_i + 1], rgbaOut[out_i + 2]);
        }
    }
    else {
        int blocksW = alignedW / 8, blocksH = alignedH / 8;
        int halfBlocksW = blocksW / (isSubsampled ? 2 : 1), halfBlocksH = blocksH / (isSubsampled ? 2 : 1);

        int sizeY = alignedW * alignedH + 0x80;
        int sizeUV = (alignedW / (isSubsampled ? 2 : 1)) * (alignedH / (isSubsampled ? 2 : 1)) + 0x80;

        if (mem_offset + sizeY + sizeUV * 2 > size) return 0;

        const uint8_t* rawY = data + mem_offset; mem_offset += sizeY;
        const uint8_t* rawCb = data + mem_offset; mem_offset += sizeUV;
        const uint8_t* rawCr = data + mem_offset; mem_offset += sizeUV;

        std::vector<uint8_t> planeY(alignedW * alignedH), planeCb(alignedW * alignedH), planeCr(alignedW * alignedH);
        DecodePlane(rawY, planeY, blocksW, blocksH);
        DecodePlane(rawCb, planeCb, halfBlocksW, halfBlocksH);
        DecodePlane(rawCr, planeCr, halfBlocksW, halfBlocksH);

        DeblockPlane(planeCb, halfBlocksW * 8, halfBlocksH * 8);
        DeblockPlane(planeCr, halfBlocksW * 8, halfBlocksH * 8);

        if (isSubsampled) {
            UpscalePlane(planeCb, halfBlocksW, halfBlocksH);
            UpscalePlane(planeCr, halfBlocksW, halfBlocksH);
        }

        for (uint32_t y = 0; y < header.height; ++y) {
            for (uint32_t x = 0; x < header.width; ++x) {
                int out_idx = (y * header.width + x) * 4;
                int plane_idx = y * alignedW + x;
                YCbCr_to_RGB(planeY[plane_idx], planeCb[plane_idx], planeCr[plane_idx], rgbaOut[out_idx], rgbaOut[out_idx + 1], rgbaOut[out_idx + 2]);
            }
        }
    }

    if (hasAlpha && !isUncompressed) {
        int sizeA = alignedW * alignedH + 0x80;
        if (mem_offset + sizeA <= size) {
            const uint8_t* rawA = data + mem_offset;
            std::vector<uint8_t> planeA(alignedW * alignedH);
            DecodePlane(rawA, planeA, alignedW / 8, alignedH / 8);

            for (uint32_t y = 0; y < header.height; ++y) {
                for (uint32_t x = 0; x < header.width; ++x) {
                    rgbaOut[(y * header.width + x) * 4 + 3] = planeA[y * alignedW + x];
                }
            }
        }
    }

    if (out_w) *out_w = header.width;
    if (out_h) *out_h = header.height;

    GLuint texture; glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, header.width, header.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaOut.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return texture;
}

GLuint TextureManager::LoadLitTexture(const std::string& path, int* out_w, int* out_h) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return 0;
    std::streamsize sz = file.tellg();
    file.seekg(0, std::ios::beg);
    if (sz <= 0) return 0;
    std::vector<uint8_t> buffer(static_cast<size_t>(sz));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), sz)) return 0;
    return LoadLitTextureFromMemory(buffer.data(), buffer.size(), out_w, out_h);
}

GLuint TextureManager::LoadTexture(const std::string& path, int* out_w, int* out_h) {
    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!data) return 0;
    if (out_w) *out_w = width;
    if (out_h) *out_h = height;
    GLuint texture; glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    stbi_image_free(data);
    return texture;
}

void TextureManager::LoadAll() {
    // === 1. Тайлы ландшафта (с новыми названиями и поддержкой .lit/.tga) ===
    const char* terrain_names[16] = {
        "Shallow", "Water", "DeepWater", "FlameLand", "Road", "LowLand", "land", "plain",
        "Swamp", "DeepSwamp", "Desert", "Badground", "Rock", "Dust", "Snow", "Ice"
    };

    for (int i = 0; i < 16; ++i) {
        std::string base_name = "Graphics/Textures/" + std::string(terrain_names[i]);

        // Приоритет загрузки: сначала .lit, затем .tga, .png, .bmp
        terrain_textures[i] = LoadLitTexture(base_name + ".lit");
        if (!terrain_textures[i]) terrain_textures[i] = LoadTexture(base_name + ".png");
        if (!terrain_textures[i]) terrain_textures[i] = LoadTexture(base_name + ".bmp");

        // Если не найдено, пробуем старые универсальные названия (terrain_0 и т.д.)
        if (!terrain_textures[i]) terrain_textures[i] = LoadTexture("Graphics/Textures/terrain_" + std::to_string(i) + ".png");
        if (!terrain_textures[i]) terrain_textures[i] = LoadTexture("Graphics/Textures/terrain_" + std::to_string(i) + ".bmp");

        // Если текстура так и не загружена, создаём одно-пиксельный фолбэк
        if (!terrain_textures[i]) {
            unsigned char px[4] = { 255, 0, 255, 255 }; // магента как индикатор отсутствия
            glGenTextures(1, &terrain_textures[i]);
            glBindTexture(GL_TEXTURE_2D, terrain_textures[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
    }

    // === 2. Курсор (с вырезкой 32x22 из левого верхнего угла) ===
    auto LoadCursorCropped = [&](const std::string& path) -> GLuint {
        int w, h, c;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &c, 4);
        if (!data) return 0;

        // Требуемый размер кусочка
        int out_w = 32, out_h = 22;
        // Заполняем прозрачностью по умолчанию, на случай если текстура меньше 32x22
        std::vector<unsigned char> sub_data(out_w * out_h * 4, 0);

        int copy_w = std::min(w, out_w);
        int copy_h = std::min(h, out_h);

        // Копируем построчно левый верхний угол
        for (int cy = 0; cy < copy_h; ++cy) {
            std::memcpy(&sub_data[cy * out_w * 4], &data[cy * w * 4], copy_w * 4);
        }
        stbi_image_free(data);

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, out_w, out_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, sub_data.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        return tex;
        };

    // Пробуем форматы: сначала .tga, как указано
    cursor_texture = LoadCursorCropped("Graphics/Editor/Cursor.tga");
    if (!cursor_texture) cursor_texture = LoadCursorCropped("Graphics/Editor/Cursor.png");

    if (!cursor_texture) {
        unsigned char px[4] = { 255, 255, 255, 255 };
        glGenTextures(1, &cursor_texture);
        glBindTexture(GL_TEXTURE_2D, cursor_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    // === 3. Иконки редактора и события (с поддержкой .tga) ===
    const char* ed_files[20] = { "GUARD", "MAGE", "archer", "ENEMY", "ROGUE", "PEASANT", "E1", "E2", "E3", "ship", "Undead", "Ghost", "zombi", "E4", "E5", "E6",
        "E7", "SHIP1", "SHIP2", "SHIP3" };

    for (int i = 0; i < 20; ++i) {
        int w = 0, h = 0;
        std::string base_path = "Graphics/Editor/" + std::string(ed_files[i]);

        // Добавлена поддержка .tga
        tex_editor[i].id = LoadTexture(base_path + ".tga", &w, &h);
        if (!tex_editor[i].id) tex_editor[i].id = LoadTexture(base_path + ".png", &w, &h);

        if (!tex_editor[i].id) {
            unsigned char px[4] = { 255, 0, 255, 255 };
            glGenTextures(1, &tex_editor[i].id);
            glBindTexture(GL_TEXTURE_2D, tex_editor[i].id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            w = 1; h = 1;
        }
        tex_editor[i].w = w;
        tex_editor[i].h = h;
    }

    // Подгрузка иконок героев (с фолбэками)
    for (int i = 0; i < 3; ++i) {
        std::string base_hero = "Graphics/Editor/hero" + std::to_string(i);
        tex_hero[i] = LoadTexture(base_hero + ".bmp");
        if (!tex_hero[i]) tex_hero[i] = LoadTexture(base_hero + ".png");

        if (!tex_hero[i]) {
            unsigned char px[4] = { 255, 0, 255, 255 }; // Фолбэк-пустышка
            glGenTextures(1, &tex_hero[i]);
            glBindTexture(GL_TEXTURE_2D, tex_hero[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
    }

    // === 4. Загрузка иконок тулбара ===
    auto LoadIcon = [&](const std::string& path) -> GLuint {
        int w, h, c;
        unsigned char* data = stbi_load((path + ".tga").c_str(), &w, &h, &c, 4);
        if (!data) data = stbi_load((path + ".png").c_str(), &w, &h, &c, 4);

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        if (data) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else {
            unsigned char px[4] = { 100, 100, 100, 255 };
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
        }

        // ВАЖНО: GL_LINEAR спасает 1-пиксельные линии от исчезновения при сдвиге интерфейса
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return tex;
        };

    tex_btn_grid[0] = LoadIcon("Graphics/Editor/btn_grid_off");
    tex_btn_grid[1] = LoadIcon("Graphics/Editor/btn_grid_on");
    tex_btn_patrol[0] = LoadIcon("Graphics/Editor/btn_patrol_off");
    tex_btn_patrol[1] = LoadIcon("Graphics/Editor/btn_patrol_on");
    tex_btn_fog[0] = LoadIcon("Graphics/Editor/btn_fog_off");
    tex_btn_fog[1] = LoadIcon("Graphics/Editor/btn_fog_on");

    const char* si_names[5] = { "SI_Castle", "SI_Helm", "SI_Swords", "SI_Skull", "SI_Tutorial" };
    for (int i = 0; i < 5; ++i) {
        tex_scenario_icons[i] = LoadLitTexture("Graphics/Windows/" + std::string(si_names[i]) + ".lit");
        if (!tex_scenario_icons[i]) {
            unsigned char px[4] = { 255, 0, 255, 255 };
            glGenTextures(1, &tex_scenario_icons[i]);
            glBindTexture(GL_TEXTURE_2D, tex_scenario_icons[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
    }
}

void TextureManager::Cleanup() const {
    for (int i = 0; i < 16; ++i) if (terrain_textures[i]) glDeleteTextures(1, &terrain_textures[i]);
    for (int i = 0; i < 20; ++i) if (tex_editor[i].id) glDeleteTextures(1, &tex_editor[i].id);
    if (cursor_texture) glDeleteTextures(1, &cursor_texture);
    for (int i = 0; i < 2; ++i) {
        if (tex_btn_grid[i]) glDeleteTextures(1, &tex_btn_grid[i]);
        if (tex_btn_patrol[i]) glDeleteTextures(1, &tex_btn_patrol[i]);
        if (tex_btn_fog[i]) glDeleteTextures(1, &tex_btn_fog[i]);
    }
    for (int i = 0; i < 3; ++i) if (tex_hero[i]) glDeleteTextures(1, &tex_hero[i]);
    for (int i = 0; i < 5; ++i) if (tex_scenario_icons[i]) glDeleteTextures(1, &tex_scenario_icons[i]);
}