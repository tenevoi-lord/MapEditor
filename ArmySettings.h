#pragma once
#include "MapParser.h"
#include "ResourceManager.h"
#include "TextureManager.h"
#include "ObjectManager.h"
#include <vector>

void DrawArmySettingsModal(bool& open_flag, std::vector<ArmyObject>& armies, MapData& map, int army_index,
    ResourceManager* resMgr, TextureManager* texMgr, ObjectManager* objMgr);