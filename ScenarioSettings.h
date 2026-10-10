#pragma once
#include "MapParser.h"
#include "ResourceManager.h"
#include "TextureManager.h"
#include "ObjectManager.h"

void DrawScenarioSettingsModal(bool& open_flag, MapData& map, ResourceManager* resMgr, TextureManager* texMgr, ObjectManager* objMgr, int& focus_hero_tab);