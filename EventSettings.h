#pragma once
#include "MapParser.h"
#include "ResourceManager.h"
#include "TextureManager.h"
#include "ObjectManager.h"
#include <vector>

void DrawEventSettingsModal(bool& open_flag, MapData& map, int& selected_idx,
    ResourceManager* resMgr, TextureManager* texMgr, ObjectManager* objMgr);