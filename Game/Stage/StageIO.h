#pragma once
#include <string>
#include "StageData.h"

namespace StageIO
{
    bool Load(const std::string& fileName, StageData& out);
    bool Save(const std::string& fileName, const StageData& in);
}
