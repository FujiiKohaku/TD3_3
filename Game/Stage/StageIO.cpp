#include "StageIO.h"
#include "../externals/nlohmann/json.hpp"

#include <fstream>
#include <filesystem>
#include <Windows.h>
#include <algorithm>

using json = nlohmann::json;

// ---- helpers（StageEditorSceneのやつを移植）----
static inline json ToJsonVec3(const Vector3& v) {
    return json{ {"x", v.x}, {"y", v.y}, {"z", v.z} };
}
static inline Vector3 FromJsonVec3(const json& j) {
    return Vector3{ j.at("x").get<float>(), j.at("y").get<float>(), j.at("z").get<float>() };
}

static std::wstring Utf8ToWide_(const std::string& s)
{
    if (s.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), size);
    return out;
}

static std::string WideToUtf8_(const std::wstring& ws)
{
    if (ws.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(),
        nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(),
        out.data(), size, nullptr, nullptr);
    return out;
}


static std::string SanitizeFileNameUtf8_(std::string s)
{
    const char* bad = "\\/:*?\"<>|";
    for (char& c : s) {
        if (std::strchr(bad, c)) c = '_';
    }
    if (s.find("..") != std::string::npos) s = "stage";
    while (!s.empty() && (s.back() == ' ' || s.back() == '.')) s.pop_back();
    if (s.empty()) s = "stage";
    return s;
}

static std::filesystem::path MakeStagePath_(const std::string& fileNameUtf8) {

    // ベースディレクトリ
    const std::filesystem::path baseDir = std::filesystem::path(L"resources") / L"stage";
    std::filesystem::create_directories(baseDir);

    // 入力を path として解釈（例: "_test/__test_play.json"）
    std::filesystem::path in = Utf8ToWide_(fileNameUtf8);

    // ★絶対パス禁止
    if (in.is_absolute()) {
        in = in.filename(); // 念のためファイル名だけに落とす
    }

    // ★危険な ".." を禁止（親ディレクトリへ抜けるのを防ぐ）
    for (const auto& part : in) {
        if (part == L"..") {
            in = L"stage.json"; // 強制的に安全名へ
            break;
        }
    }

    // 拡張子が無ければ .json を付ける（入力末尾に対して）
    if (!in.has_extension()) {
        in += L".json";
    }

    // ★パスの各要素をサニタイズ（フォルダ名もファイル名も）
    std::filesystem::path safeRel;
    for (const auto& part : in) {
        std::string utf8 = WideToUtf8_(part.wstring());
        utf8 = SanitizeFileNameUtf8_(utf8);   // 既存関数を流用
        safeRel /= Utf8ToWide_(utf8);
    }

    // 最終パス：resources/stage/<safeRel>
    std::filesystem::path out = baseDir / safeRel;

    // ★保存時に必要なディレクトリを作れるようにしておく
    std::filesystem::create_directories(out.parent_path());

    return out;
}



namespace StageIO
{
    bool Load(const std::string& fileName, StageData& out)
    {
        const std::filesystem::path path = MakeStagePath_(fileName);

        std::ifstream ifs(path, std::ios::binary);
        if (!ifs.is_open()) return false;

        json root;
        ifs >> root;

        out = StageData{}; // reset

        if (root.contains("version")) out.version = root["version"].get<int>();

        // goal
        if (root.contains("goal")) {
            const auto& g = root["goal"];
            if (g.contains("pos")) {
                out.goalPos = FromJsonVec3(g["pos"]);
                out.hasGoalPos = true;
            }
            if (g.contains("spawnOffset")) {
                out.goalSpawnOffset = FromJsonVec3(g["spawnOffset"]);
                out.hasGoalSpawnOffset = true;
            }
        }

        // drone
        if (root.contains("drone")) {
            const auto& d = root["drone"];
            if (d.contains("spawnPos")) out.droneSpawnPos = FromJsonVec3(d["spawnPos"]);
            if (d.contains("spawnYaw")) out.droneSpawnYaw = d["spawnYaw"].get<float>();
        }

        // gates
        out.gates.clear();
        if (root.contains("gates")) {
            for (const auto& j : root["gates"]) {
                Gate g{};
                g.pos = FromJsonVec3(j.at("pos"));
                g.rot = FromJsonVec3(j.at("rot"));
                g.scale = FromJsonVec3(j.at("scale"));
                g.perfectRadius = j.at("perfectRadius").get<float>();
                g.gateRadius = j.at("gateRadius").get<float>();
                g.thickness = j.at("thickness").get<float>();
                out.gates.push_back(g);
            }
        }

        // walls
        out.walls.clear();
        if (root.contains("walls")) {
            for (const auto& j : root["walls"]) {
                WallSystem::Wall w{};
                const std::string type = j.at("type").get<std::string>();
                w.type = (type == "OBB") ? WallSystem::Type::OBB : WallSystem::Type::AABB;
                w.center = FromJsonVec3(j.at("center"));
                w.half = FromJsonVec3(j.at("half"));
                w.rot = FromJsonVec3(j.at("rot"));
                out.walls.push_back(w);
            }
        }

        return true;
    }

    bool Save(const std::string& fileName, const StageData& in)
    {
        const std::filesystem::path path = MakeStagePath_(fileName);
        const std::filesystem::path dir = path.parent_path();

        // ★ディレクトリ自動生成（安全）
        std::filesystem::create_directories(dir);

        json root;
        root["version"] = in.version;

        root["drone"]["spawnPos"] = ToJsonVec3(in.droneSpawnPos);
        root["drone"]["spawnYaw"] = in.droneSpawnYaw;

        root["goal"]["pos"] = ToJsonVec3(in.goalPos);
        if (in.hasGoalSpawnOffset) root["goal"]["spawnOffset"] = ToJsonVec3(in.goalSpawnOffset);

        // gates
        {
            json arr = json::array();
            for (const auto& g : in.gates) {
                json j;
                j["pos"] = ToJsonVec3(g.pos);
                j["rot"] = ToJsonVec3(g.rot);
                j["scale"] = ToJsonVec3(g.scale);
                j["perfectRadius"] = g.perfectRadius;
                j["gateRadius"] = g.gateRadius;
                j["thickness"] = g.thickness;
                arr.push_back(j);
            }
            root["gates"] = arr;
        }

        // walls
        {
            json arr = json::array();
            for (const auto& w : in.walls) {
                json j;
                j["type"] = (w.type == WallSystem::Type::AABB) ? "AABB" : "OBB";
                j["center"] = ToJsonVec3(w.center);
                j["half"] = ToJsonVec3(w.half);
                j["rot"] = ToJsonVec3(w.rot);
                arr.push_back(j);
            }
            root["walls"] = arr;
        }

        std::ofstream ofs(path, std::ios::binary);
        if (!ofs.is_open()) return false;
        ofs << root.dump(2);
        return true;
    }
}
