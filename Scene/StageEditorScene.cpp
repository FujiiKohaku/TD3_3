#include "StageEditorScene.h"
#include "../Light/LightManager.h"

#include "../externals/nlohmann/json.hpp"
#include <fstream>
#include <string>
#include <algorithm>
#include <cmath>
#include <numbers>

// ---- json helpers ----
using json = nlohmann::json;

static inline json ToJsonVec3(const Vector3& v) {
    return json{ {"x", v.x}, {"y", v.y}, {"z", v.z} };
}
static inline Vector3 FromJsonVec3(const json& j) {
    return Vector3{ j.at("x").get<float>(), j.at("y").get<float>(), j.at("z").get<float>() };
}

// ===== World -> Screen (row-vector行列想定) =====
static Vector4 MulRowVec4Mat4(const Vector4& v, const Matrix4x4& m)
{
    Vector4 o{};
    o.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + v.w * m.m[3][0];
    o.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + v.w * m.m[3][1];
    o.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + v.w * m.m[3][2];
    o.w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + v.w * m.m[3][3];
    return o;
}

static Vector3 TransformCoord_RowVector4(const Vector4& v, const Matrix4x4& m)
{
    Vector4 o = MulRowVec4Mat4(v, m);
    if (std::abs(o.w) > 1e-6f) {
        o.x /= o.w; o.y /= o.w; o.z /= o.w;
    }
    return { o.x, o.y, o.z };
}

// D3D NDC: x,y=[-1..1], z=[0..1]
static bool ScreenRayToPlaneY0_RowVector(
    int mouseX, int mouseY,
    float screenW, float screenH,
    const Matrix4x4& viewProj,
    Vector3& outHit
)
{
    Matrix4x4 invVP = MatrixMath::Inverse(viewProj);

    float ndcX = ((float)mouseX / screenW) * 2.0f - 1.0f;
    float ndcY = 1.0f - ((float)mouseY / screenH) * 2.0f;

    Vector3 pNear = TransformCoord_RowVector4({ ndcX, ndcY, 0.0f, 1.0f }, invVP);
    Vector3 pFar = TransformCoord_RowVector4({ ndcX, ndcY, 1.0f, 1.0f }, invVP);

    Vector3 dir{ pFar.x - pNear.x, pFar.y - pNear.y, pFar.z - pNear.z };
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len < 1e-6f) return false;
    dir.x /= len; dir.y /= len; dir.z /= len;

    if (std::abs(dir.y) < 1e-6f) return false;
    float t = (0.0f - pNear.y) / dir.y;
    if (t <= 0.0f) return false;

    outHit = { pNear.x + dir.x * t, 0.0f, pNear.z + dir.z * t };
    return true;
}


void StageEditorScene::Initialize()
{

    LightManager::GetInstance()->Initialize(DirectXCommon::GetInstance());


    // -------------------------
    // Camera
    // -------------------------
    camera_ = new Camera();
    camera_->Initialize();
    camera_->SetTranslate({ 0, 0, 0 });

    Object3dManager::GetInstance()->SetDefaultCamera(camera_);

    // -------------------------
    // Drone (editor gizmo)
    // -------------------------
    droneObj_ = new Object3d();
    droneObj_->Initialize(Object3dManager::GetInstance());
    droneObj_->SetModel("cube.obj");
    droneObj_->SetTranslate({ 0.0f, 1.0f, 0.0f });
    droneObj_->SetScale({ 0.1f, 0.1f, 0.1f });
    droneObj_->Update();

    drone_.Initialize({ 0.0f, 1.0f, 0.0f });

    // -------------------------
    // Gates (初期配置：空からでもOK)
    // -------------------------
    gates_.clear();
    gates_.resize(3);

    // Gate 1
    gates_[0].gate.rot = { 0, 0, 0 };
    gates_[0].gate.scale = { 10.0f, 10.0f, 10.0f };
    gates_[0].gate.pos = { 0, 2, 5 };
    gates_[0].gate.perfectRadius = 1.0f;
    gates_[0].gate.gateRadius = 2.5f;
    gates_[0].gate.thickness = 0.8f;
    gates_[0].Initialize(Object3dManager::GetInstance(), "cube.obj", camera_);

    // Gate 2
    gates_[1].gate.pos = { 5, 3, 20 };
    gates_[1].gate.rot = { 0, 0.7f, 0 };
    gates_[1].gate.scale = { 2, 2, 2 };
    gates_[1].Initialize(Object3dManager::GetInstance(), "cube.obj", camera_);

    // Gate 3
    gates_[2].gate.pos = { -3, 3, 30 };
    gates_[2].gate.rot = { 0.2f, -0.4f, 0 };
    gates_[2].gate.scale = { 2, 2, 2 };
    gates_[2].Initialize(Object3dManager::GetInstance(), "cube.obj", camera_);

    nextGate_ = 0;

    // -------------------------
    // Walls
    // -------------------------
    wallSys_.BuildDebug(Object3dManager::GetInstance(), "cube.obj");

    wallSys_.AddAABB({ 0.0f, 2.0f, 12.0f }, { 6.0f, 2.0f, 0.5f });
    wallSys_.AddAABB({ 8.0f, 2.0f, 10.0f }, { 0.5f, 2.0f, 6.0f });
    wallSys_.AddOBB({ -4.0f, 2.0f, 18.0f }, { 4.0f, 2.0f, 0.5f }, { 0.0f, 0.6f, 0.0f });

    // -------------------------
    // Goal
    // -------------------------
    goalSys_.Initialize(Object3dManager::GetInstance(), camera_);
    goalSys_.Reset();
    stageCleared_ = false;

    camPos_ = { 0.0f, 3.0f, -10.0f };
    camYaw_ = 0.0f;
    camPitch_ = 0.0f;

    // ★リセット用に保存
    camPosInit_ = camPos_;
    camYawInit_ = camYaw_;
    camPitchInit_ = camPitch_;

    camera_->SetTranslate(camPos_);
    camera_->SetRotate({ camPitch_, camYaw_, 0.0f });
    camera_->Update();

}

void StageEditorScene::Finalize()
{
    ParticleManager::GetInstance()->Finalize();

    LightManager::GetInstance()->Finalize();

    delete droneObj_;
    droneObj_ = nullptr;

    delete camera_;
    camera_ = nullptr;

    goalSys_.Finalize();

    //SoundManager::GetInstance()->SoundUnload(&bgm);
}



void StageEditorScene::Update()
{
    float dt = 1.0f / 60.0f;
    Input& input = *Input::GetInstance();

    // ドローン操作（配置用）
   // drone_.UpdateMode1(input, dt);

    UpdateFreeCamera(dt);

    // 壁衝突（編集でも欲しければ）
    {
        Vector3 pos = drone_.GetPos();
        Vector3 vel = drone_.GetVel();
        wallSys_.ResolveDroneAABB(pos, vel, droneHalf_, dt, 4);
        drone_.SetPos(pos);
        drone_.SetVel(vel);
    }

    // 描画オブジェクト反映
    if (droneObj_) {
        droneObj_->SetTranslate(drone_.GetPos());
        droneObj_->Update();
    }

    // Gate処理が終わった後あたりで
    goalSys_.Update(gates_, nextGate_, drone_.GetPos());

    if (goalSys_.IsCleared()) {
        stageCleared_ = true;
    }


    // Gateの見た目更新（色タイマー）
    for (auto& g : gates_) g.Tick(dt);

    // --- Editor UI ---
    AddGate();
    EditWallsImGui();
    StageIOImGui();
    GoalEditorImGui();

    // デバッグ更新
    if (drawWallDebug_) wallSys_.UpdateDebug();
}

void StageEditorScene::Draw2D() {}
void StageEditorScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());

    if (droneObj_) droneObj_->Draw();
    for (auto& g : gates_) g.Draw();
    goalSys_.Draw();

    if (drawWallDebug_) wallSys_.DrawDebug();
}

void StageEditorScene::DrawImGui() {}  // ★これが無いとLNK2001

// --- editor functions (GamePlaySceneから移植) ---
// --- editor functions (GamePlaySceneから移植) ---

void StageEditorScene::AddGate()
{
    ImGui::Begin("Gate Editor");

    static int editGate = 0;
    const int gateCount = (int)gates_.size();

    if (gateCount == 0) {
        ImGui::Text("No gates. Add one!");
        if (ImGui::Button("Add Gate")) {
            GateVisual gv;
            gv.gate.pos = drone_.GetPos();
            gv.gate.pos.z += 10.0f;
            gv.gate.rot = { 0,0,0 };
            gv.gate.perfectRadius = 1.0f;
            gv.gate.gateRadius = 2.5f;
            gv.gate.thickness = 0.8f;
            gv.Initialize(Object3dManager::GetInstance(), "cube.obj", camera_);
            gates_.push_back(std::move(gv));
            editGate = 0;
            nextGate_ = 0;
        }
        ImGui::End();
        return;
    }

    editGate = std::clamp(editGate, 0, gateCount - 1);

    ImGui::Text("Gates: %d", gateCount);
    ImGui::SliderInt("Edit Gate Index", &editGate, 0, gateCount - 1);

    ImGui::Separator();

    if (ImGui::Button("Add Gate (End)")) {
        GateVisual gv;
        gv.gate.pos = drone_.GetPos();
        gv.gate.pos.z += 10.0f;
        gv.gate.rot = { 0,0,0 };
        gv.gate.perfectRadius = 1.0f;
        gv.gate.gateRadius = 2.5f;
        gv.gate.thickness = 0.8f;
        gv.Initialize(Object3dManager::GetInstance(), "cube.obj", camera_);
        gates_.push_back(std::move(gv));
        editGate = (int)gates_.size() - 1;
    }

    ImGui::SameLine();

    if (ImGui::Button("Duplicate")) {
        Gate copy = gates_[editGate].gate;

        GateVisual gv;
        gv.gate = copy;
        gv.gate.pos.z += 3.0f;
        gv.Initialize(Object3dManager::GetInstance(), "cube.obj", camera_);
        gates_.insert(gates_.begin() + (editGate + 1), std::move(gv));
        editGate++;
    }

    ImGui::SameLine();

    if (ImGui::Button("Remove")) {
        gates_.erase(gates_.begin() + editGate);

        if (editGate >= (int)gates_.size()) editGate = (int)gates_.size() - 1;
        if (editGate < 0) editGate = 0;

        if (nextGate_ > editGate) nextGate_--;
        nextGate_ = std::clamp(nextGate_, 0, (int)gates_.size());
    }

    ImGui::Separator();

    bool canUp = (editGate > 0);
    bool canDown = (editGate < (int)gates_.size() - 1);

    if (!canUp) ImGui::BeginDisabled();
    if (ImGui::Button("Up")) {
        std::swap(gates_[editGate], gates_[editGate - 1]);
        if (nextGate_ == editGate) nextGate_ = editGate - 1;
        else if (nextGate_ == editGate - 1) nextGate_ = editGate;
        editGate--;
    }
    if (!canUp) ImGui::EndDisabled();

    ImGui::SameLine();

    if (!canDown) ImGui::BeginDisabled();
    if (ImGui::Button("Down")) {
        std::swap(gates_[editGate], gates_[editGate + 1]);
        if (nextGate_ == editGate) nextGate_ = editGate + 1;
        else if (nextGate_ == editGate + 1) nextGate_ = editGate;
        editGate++;
    }
    if (!canDown) ImGui::EndDisabled();

    ImGui::Separator();

    Gate& g = gates_[editGate].gate;

    // Mouse Place (Plane Y=0)
    static bool placeMode = false;
    ImGui::Separator();
    ImGui::Checkbox("Mouse Place Mode (Y=0 Plane)", &placeMode);
    ImGui::Text("LClick: place selected gate on ground (y=0)");

    if (placeMode) {
        if (!ImGui::GetIO().WantCaptureMouse) {
            if (Input::GetInstance()->IsMouseTrigger(0)) {
                POINT mp = Input::GetInstance()->GetMousePos();
                const float W = (float)WinApp::kClientWidth;
                const float H = (float)WinApp::kClientHeight;
                const Matrix4x4& vp = camera_->GetViewProjectionMatrix();

                Vector3 hit{};
                if (ScreenRayToPlaneY0_RowVector(mp.x, mp.y, W, H, vp, hit)) {
                    g.pos = hit;
                }
            }
        }
    }

    ImGui::Text("Transform");
    ImGui::DragFloat3("Position", &g.pos.x, 0.1f);
    ImGui::DragFloat3("Rotation (rad)", &g.rot.x, 0.05f);

    ImGui::Text("Gate Params");
    ImGui::DragFloat("Perfect Radius", &g.perfectRadius, 0.05f, 0.1f, g.gateRadius);
    ImGui::DragFloat("Gate Radius", &g.gateRadius, 0.05f, g.perfectRadius, 50.0f);
    ImGui::DragFloat("Thickness", &g.thickness, 0.05f, 0.05f, 10.0f);

    ImGui::Separator();
    ImGui::Text("NextGate = %d", nextGate_);
    if (ImGui::Button("Set NextGate = Edit")) {
        nextGate_ = editGate;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset NextGate")) {
        nextGate_ = 0;
        goalSys_.Reset();
        stageCleared_ = false;
    }

    if (ImGui::Button("Add Gate (After)")) {
        GateVisual gv;
        const int insertIndex = editGate + 1;

        gv.gate = gates_[editGate].gate;
        gv.gate.pos.z += 3.0f;

        gv.Initialize(Object3dManager::GetInstance(), "cube.obj", camera_);
        gates_.insert(gates_.begin() + insertIndex, std::move(gv));

        if (nextGate_ >= insertIndex) nextGate_++;

        editGate = insertIndex;
    }



    ImGui::End();
}

void StageEditorScene::EditWallsImGui()
{
    ImGui::Begin("Wall Editor");

    auto& walls = wallSys_.Walls();
    static int editWall = 0;

    const int wallCount = (int)walls.size();
    ImGui::Text("Walls: %d", wallCount);

    if (wallCount == 0) {
        ImGui::Text("No walls. Add one!");
        if (ImGui::Button("Add AABB Wall")) {
            WallSystem::Wall w;
            w.type = WallSystem::Type::AABB;
            w.center = drone_.GetPos();
            w.center.y = 2.0f;
            w.center.z += 8.0f;
            w.half = { 2.0f, 2.0f, 0.5f };
            walls.push_back(w);
            wallSys_.BuildDebug(Object3dManager::GetInstance(), "cube.obj");
        }
        ImGui::SameLine();
        if (ImGui::Button("Add OBB Wall")) {
            WallSystem::Wall w;
            w.type = WallSystem::Type::OBB;
            w.center = drone_.GetPos();
            w.center.y = 2.0f;
            w.center.z += 8.0f;
            w.half = { 2.0f, 2.0f, 0.5f };
            w.rot = { 0,0,0 };
            walls.push_back(w);
            wallSys_.BuildDebug(Object3dManager::GetInstance(), "cube.obj");
        }
        ImGui::End();
        return;
    }

    editWall = std::clamp(editWall, 0, wallCount - 1);
    ImGui::SliderInt("Edit Wall Index", &editWall, 0, wallCount - 1);

    ImGui::Separator();

    if (ImGui::Button("Add AABB (End)")) {
        WallSystem::Wall w;
        w.type = WallSystem::Type::AABB;
        w.center = drone_.GetPos(); w.center.y = 2.0f; w.center.z += 8.0f;
        w.half = { 2.0f, 2.0f, 0.5f };
        walls.push_back(w);
        editWall = (int)walls.size() - 1;
        wallSys_.BuildDebug(Object3dManager::GetInstance(), "cube.obj");
    }
    ImGui::SameLine();
    if (ImGui::Button("Add OBB (End)")) {
        WallSystem::Wall w;
        w.type = WallSystem::Type::OBB;
        w.center = drone_.GetPos(); w.center.y = 2.0f; w.center.z += 8.0f;
        w.half = { 2.0f, 2.0f, 0.5f };
        w.rot = { 0,0,0 };
        walls.push_back(w);
        editWall = (int)walls.size() - 1;
        wallSys_.BuildDebug(Object3dManager::GetInstance(), "cube.obj");
    }

    ImGui::SameLine();

    if (ImGui::Button("Duplicate")) {
        WallSystem::Wall copy = walls[editWall];
        copy.center.z += 2.0f;
        walls.insert(walls.begin() + (editWall + 1), copy);
        editWall++;
        wallSys_.BuildDebug(Object3dManager::GetInstance(), "cube.obj");
    }

    ImGui::SameLine();

    if (ImGui::Button("Remove")) {
        walls.erase(walls.begin() + editWall);
        if (walls.empty()) { editWall = 0; ImGui::End(); return; }
        editWall = std::clamp(editWall, 0, (int)walls.size() - 1);
        wallSys_.BuildDebug(Object3dManager::GetInstance(), "cube.obj");
    }

    ImGui::Separator();

    bool canUp = (editWall > 0);
    bool canDown = (editWall < (int)walls.size() - 1);

    if (!canUp) ImGui::BeginDisabled();
    if (ImGui::Button("Up")) { std::swap(walls[editWall], walls[editWall - 1]); editWall--; }
    if (!canUp) ImGui::EndDisabled();

    ImGui::SameLine();

    if (!canDown) ImGui::BeginDisabled();
    if (ImGui::Button("Down")) { std::swap(walls[editWall], walls[editWall + 1]); editWall++; }
    if (!canDown) ImGui::EndDisabled();

    ImGui::Separator();

    WallSystem::Wall& w = walls[editWall];

    int typeInt = (w.type == WallSystem::Type::AABB) ? 0 : 1;
    if (ImGui::RadioButton("AABB", typeInt == 0)) typeInt = 0;
    ImGui::SameLine();
    if (ImGui::RadioButton("OBB", typeInt == 1)) typeInt = 1;
    w.type = (typeInt == 0) ? WallSystem::Type::AABB : WallSystem::Type::OBB;

    static bool placeMode = false;
    ImGui::Checkbox("Mouse Place Mode (Y=0 Plane)", &placeMode);
    ImGui::Text("LClick: place selected wall center on ground (y=0)");

    static bool autoAddOnPlace = true;
    ImGui::Checkbox("Auto Add Next On Place", &autoAddOnPlace);
    ImGui::SameLine();
    static float autoAddZStep = 2.0f;
    ImGui::DragFloat("Z Step", &autoAddZStep, 0.1f, 0.0f, 50.0f);

    if (placeMode) {
        if (!ImGui::GetIO().WantCaptureMouse) {
            if (Input::GetInstance()->IsMouseTrigger(0)) {
                POINT mp = Input::GetInstance()->GetMousePos();
                const float W = (float)WinApp::kClientWidth;
                const float H = (float)WinApp::kClientHeight;
                const Matrix4x4& vp = camera_->GetViewProjectionMatrix();

                Vector3 hit{};
                if (ScreenRayToPlaneY0_RowVector(mp.x, mp.y, W, H, vp, hit)) {
                    w.center = hit;

                    if (autoAddOnPlace) {
                        WallSystem::Wall next = w;
                        next.center.z += autoAddZStep;
                        walls.insert(walls.begin() + (editWall + 1), next);
                        editWall++;
                        wallSys_.BuildDebug(Object3dManager::GetInstance(), "cube.obj");
                    }
                }
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Transform");
    ImGui::DragFloat3("Center", &w.center.x, 0.1f);
    ImGui::DragFloat3("Half Size", &w.half.x, 0.1f, 0.05f, 100.0f);

    if (w.type == WallSystem::Type::OBB) {
        ImGui::DragFloat3("Rotation (rad)", &w.rot.x, 0.02f);
    } else {
        w.rot = { 0,0,0 };
    }

    ImGui::End();

    wallSys_.SetSelectedIndex(editWall);
}

bool StageEditorScene::SaveStageJson(const std::string& fileName)
{
    const std::string path = "resources/stage/" + fileName;

    json root;
    root["version"] = 1;

    root["drone"]["spawnPos"] = ToJsonVec3(drone_.GetPos());
    root["drone"]["spawnYaw"] = drone_.GetYaw();
    root["goal"]["pos"] = ToJsonVec3(goalSys_.GetGoalPos());

    // gates
    {
        json arr = json::array();
        for (const auto& gv : gates_) {
            const Gate& g = gv.gate;
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
        for (const auto& w : wallSys_.Walls()) {
            json j;
            j["type"] = (w.type == WallSystem::Type::AABB) ? "AABB" : "OBB";
            j["center"] = ToJsonVec3(w.center);
            j["half"] = ToJsonVec3(w.half);
            j["rot"] = ToJsonVec3(w.rot);
            arr.push_back(j);
        }
        root["walls"] = arr;
    }

    std::ofstream ofs(path);
    if (!ofs.is_open()) return false;

    ofs << root.dump(2);
    return true;
}

bool StageEditorScene::LoadStageJson(const std::string& fileName)
{
    const std::string path = "resources/stage/" + fileName;

    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;

    json root;
    ifs >> root;

    // goal
    bool hasGoalPos = false;
    Vector3 goalPos{};

    bool hasGoalOffset = false;
    Vector3 goalOfs{};

    if (root.contains("goal")) {
        const auto& g = root["goal"];
        if (g.contains("pos")) { hasGoalPos = true; goalPos = FromJsonVec3(g["pos"]); }
        if (g.contains("spawnOffset")) { hasGoalOffset = true; goalOfs = FromJsonVec3(g["spawnOffset"]); }
    }

    // drone
    if (root.contains("drone")) {
        const auto& d = root["drone"];
        if (d.contains("spawnPos")) drone_.SetPos(FromJsonVec3(d["spawnPos"]));
        if (d.contains("spawnYaw")) drone_.SetYaw(d["spawnYaw"].get<float>());
        drone_.SetVel({ 0,0,0 });
    }

    // gates
    gates_.clear();
    if (root.contains("gates")) {
        for (const auto& j : root["gates"]) {
            GateVisual gv;
            gv.gate.pos = FromJsonVec3(j.at("pos"));
            gv.gate.rot = FromJsonVec3(j.at("rot"));
            gv.gate.scale = FromJsonVec3(j.at("scale"));
            gv.gate.perfectRadius = j.at("perfectRadius").get<float>();
            gv.gate.gateRadius = j.at("gateRadius").get<float>();
            gv.gate.thickness = j.at("thickness").get<float>();
            gv.Initialize(Object3dManager::GetInstance(), "cube.obj", camera_);
            gates_.push_back(std::move(gv));
        }
    }

    // walls
    wallSys_.Walls().clear();
    if (root.contains("walls")) {
        for (const auto& j : root["walls"]) {
            WallSystem::Wall w;
            const std::string type = j.at("type").get<std::string>();
            w.type = (type == "OBB") ? WallSystem::Type::OBB : WallSystem::Type::AABB;
            w.center = FromJsonVec3(j.at("center"));
            w.half = FromJsonVec3(j.at("half"));
            w.rot = FromJsonVec3(j.at("rot"));
            wallSys_.Walls().push_back(w);
        }
    }

    nextGate_ = 0;

    goalSys_.Reset();
    stageCleared_ = false;

    if (hasGoalOffset) goalSys_.SetSpawnOffset(goalOfs);
    if (hasGoalPos) goalSys_.SetGoalPos(goalPos);

    wallSys_.BuildDebug(Object3dManager::GetInstance(), "cube.obj");
    return true;
}

void StageEditorScene::StageIOImGui()
{
    ImGui::Begin("Stage IO");

    static char fileName[128] = "stage01.json";
    ImGui::InputText("File", fileName, IM_ARRAYSIZE(fileName));

    bool hasJson = (std::string(fileName).find(".json") != std::string::npos);
    if (!hasJson) {
        ImGui::TextColored(ImVec4(1, 0.7f, 0.2f, 1), "Tip: add .json");
    }

    if (ImGui::Button("Save")) {
        const bool ok = SaveStageJson(fileName);
        ImGui::SameLine();
        ImGui::Text(ok ? "Saved." : "Save FAILED.");
    }

    ImGui::SameLine();

    if (ImGui::Button("Load")) {
        const bool ok = LoadStageJson(fileName);
        ImGui::SameLine();
        ImGui::Text(ok ? "Loaded." : "Load FAILED.");
    }

    ImGui::End();
}

void StageEditorScene::UpdateFreeCamera(float dt)
{
    Input& input = *Input::GetInstance();

    // =========================
    // マウスで視点回転（右クリック中）
    // =========================
    if (input.IsMousePressed(1)) {
        POINT d = input.GetMouseDelta();
        camYaw_ += (float)d.x * camMouseSens_;
        camPitch_ += (float)d.y * camMouseSens_;

        const float limit = std::numbers::pi_v<float> *0.49f;
        camPitch_ = std::clamp(camPitch_, -limit, limit);
    }

    POINT d = input.GetMouseDelta();

    ImGui::Begin("Mouse Debug");
    ImGui::Text("RButton=%d", input.IsMousePressed(1));
    ImGui::Text("WantCaptureMouse=%d", ImGui::GetIO().WantCaptureMouse ? 1 : 0);
    ImGui::Text("dx=%ld dy=%ld", d.x, d.y);
    ImGui::Text("camYaw=%.3f camPitch=%.3f", camYaw_, camPitch_);
    ImGui::End();


    // =========================
    // 向きベクトル計算
    // =========================
    const float cy = std::cos(camYaw_);
    const float sy = std::sin(camYaw_);
    const float cp = std::cos(camPitch_);
    const float sp = std::sin(camPitch_);

    Vector3 right{ cy, 0.0f, -sy };
    Vector3 up{ 0.0f, 1.0f, 0.0f };

    float speed = camMoveSpeed_;
    if (input.GetInstance()->IsKeyPressed(DIK_LSHIFT)) {
        speed *= 3.0f;
    }

    // ★符号はここが一般的（上が+なら -sp、下が+なら +sp）
    Vector3 forward{ sy * cp, -sp, cy * cp };

    // =========================
    // キーボード移動
    // =========================
    if (input.IsKeyPressed(DIK_W)) camPos_ += forward * speed * dt;
    if (input.IsKeyPressed(DIK_S)) camPos_ -= forward * speed * dt;
    if (input.IsKeyPressed(DIK_D)) camPos_ += right * speed * dt;
    if (input.IsKeyPressed(DIK_A)) camPos_ -= right * speed * dt;
    if (input.IsKeyPressed(DIK_E)) camPos_ += up * speed * dt;
    if (input.IsKeyPressed(DIK_Q)) camPos_ -= up * speed * dt;

    Vector3 target = camPos_ + forward;

    // ★LookAtで view を作る（rotateは使わない）
    Matrix4x4 view = MatrixMath::MakeLookAtMatrix(camPos_, target, { 0,1,0 });
    camera_->SetTranslate(camPos_);     // GPUに渡すworldPosition用
    camera_->SetCustomView(view);
    camera_->Update();

    if (input.IsKeyTrigger(DIK_Y)) {
        camPos_ = camPosInit_;
        camYaw_ = camYawInit_;
        camPitch_ = camPitchInit_;

        // ついでに即反映（この後の処理でも上書きされない）
        camera_->SetTranslate(camPos_);
        camera_->SetRotate({ camPitch_, camYaw_, 0.0f });
        camera_->Update();
        return; // ★好み：このフレームは他の移動/回転を無視
    }

}

void StageEditorScene::GoalEditorImGui()
{
    ImGui::Begin("Goal Editor");

    static bool preview = false;
    static bool prevPreview = false;

    ImGui::Checkbox("Preview (ForceSpawn)", &preview);

    if (preview && !prevPreview) {
        goalSys_.ForceSpawn(nullptr);
    }
    if (!preview && prevPreview) {
        goalSys_.ClearForceSpawn();
    }
    prevPreview = preview;

    Vector3 gp = goalSys_.GetGoalPos();
    if (ImGui::DragFloat3("Goal Pos", &gp.x, 0.1f)) {
        goalSys_.SetGoalPos(gp);
    }

    static float a = 0.25f;
    if (ImGui::SliderFloat("Goal Alpha", &a, 0.0f, 1.0f)) {
        goalSys_.SetGoalAlpha(a);
    }

    ImGui::End();
}
