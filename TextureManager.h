#pragma once
#include <string>
#include <GLFW/glfw3.h>

struct EditorTexture {
    GLuint id = 0;
    int w = 0;
    int h = 0;
};

class TextureManager {
public:
    GLuint terrain_textures[16] = { 0 };
    GLuint cursor_texture = 0;
    EditorTexture tex_editor[20]; // Текстуры для армий/событий с размерами
    GLuint tex_hero[3] = { 0 }; // 0 = Knight, 1 = Archmage, 2 = Ranger
    GLuint tex_scenario_icons[5] = { 0 }; // Иконки сценариев для кампаний
    // Иконки кнопок тулбара [0] = выкл (красный), [1] = вкл (зеленый)
    GLuint tex_btn_grid[2] = { 0 };
    GLuint tex_btn_patrol[2] = { 0 };
    GLuint tex_btn_fog[2] = { 0 };
    void LoadAll();
    void Cleanup() const;
    GLuint LoadLitTextureFromMemory(const uint8_t* data, size_t size, int* out_w = nullptr, int* out_h = nullptr);
private:
    GLuint LoadTexture(const std::string& path, int* out_w = nullptr, int* out_h = nullptr);
    GLuint LoadLitTexture(const std::string& path, int* out_w = nullptr, int* out_h = nullptr);
};