#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <GLFW/glfw3.h>

enum class ObjType { Decor, Functional };

struct ObjectHeader {
    uint32_t groupId;
    uint32_t objectId;
    uint32_t gridSize;
    uint32_t width;
    uint32_t height;
    ObjType type;
    uint32_t special; // Для холмов/деревьев
    uint32_t mapCellsW; // Для строений
    uint32_t mapCellsH; // Для строений
};

struct GameObject {
    bool isEmpty = true;
    ObjectHeader header;
    GLuint texture = 0;
};

class ObjectManager {
public:
    std::vector<GameObject> objects;

    bool Load(const std::string& folder);
    void Cleanup();
};