#pragma once
#include <string>
#include <vector>

#include "../externals/nlohmann/json.hpp"

// あなたの Vector3 / Gate / WallSystem::Wall が見えるように include 調整
#include "MathStruct.h"
#include "../Game/Gate/GateVisual.h"
#include "../Game/Drone/Walls.h"

struct StageData
{
    int version = 1;

    Vector3 droneSpawnPos{};
    float   droneSpawnYaw = 0.0f;

    Vector3 goalPos{};
    bool    hasGoalPos = false;

    Vector3 goalSpawnOffset{};
    bool    hasGoalSpawnOffset = false;

    // gate は GateVisual を直接持つと依存が重いので「Gate」だけ
    std::vector<Gate> gates;

    std::vector<WallSystem::Wall> walls;
};
