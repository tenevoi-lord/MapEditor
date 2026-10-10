#pragma once
#include "MapParser.h"
#include "ResourceManager.h"
#include "TextureManager.h"
#include "ObjectManager.h"
#include <vector>

void DrawBuildingSettingsModal(bool& open_flag, std::vector<BuildingObject>& buildings, MapData& map, int building_index,
    ResourceManager* resMgr, TextureManager* texMgr, ObjectManager* objMgr);