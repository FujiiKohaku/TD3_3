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

static std::string Vec3Str(const Vector3& v)
{
    char buf[128];
    std::snprintf(buf, sizeof(buf), "(%.2f, %.2f, %.2f)", v.x, v.y, v.z);
    return std::string(buf);
}

static Vector3 TransformCoord_RowVector4(const Vector4& v, const Matrix4x4& m)
{
    Vector4 o = MulRowVec4Mat4(v, m);
    if (std::abs(o.w) > 1e-6f) {
        o.x /= o.w; o.y /= o.w; o.z /= o.w;
    }
    return { o.x, o.y, o.z };
}

static float DistSq3(const Vector3& a, const Vector3& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

static int FindNearestGateIndex(const std::vector<GateVisual>& gates, const Vector3& hit, float maxDist)
{
    if (gates.empty()) return -1;

    const float maxDistSq = maxDist * maxDist;
    int best = -1;
    float bestD = maxDistSq;

    for (int i = 0; i < (int)gates.size(); ++i) {
        const Vector3& p = gates[i].gate.pos;
        const float d = DistSq3(p, hit);
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best; // -1 なら「近くに無い」
}

static int FindNearestWallIndex(const std::vector<WallSystem::Wall>& walls, const Vector3& hit, float maxDist)
{
    if (walls.empty()) return -1;

    const float maxDistSq = maxDist * maxDist;
    int best = -1;
    float bestD = maxDistSq;

    for (int i = 0; i < (int)walls.size(); ++i) {
        const Vector3& c = walls[i].center;
        const float d = DistSq3(c, hit);
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
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

static bool WorldToScreen_RowVector(
    const Vector3& worldPos,
    const Matrix4x4& viewProj,
    float screenW, float screenH,
    Vector2& outScreen
)
{
    // world -> clip
    Vector4 clip = MulRowVec4Mat4({ worldPos.x, worldPos.y, worldPos.z, 1.0f }, viewProj);

    // カメラ後ろ（w<=0）は弾く（ここ重要）
    if (clip.w <= 1e-6f) return false;

    // clip -> ndc
    const float ndcX = clip.x / clip.w;
    const float ndcY = clip.y / clip.w;
    const float ndcZ = clip.z / clip.w; // D3Dなら 0..1 期待

    // 画面外を弾きたいなら
    if (ndcX < -1.0f || ndcX > 1.0f) return false;
    if (ndcY < -1.0f || ndcY > 1.0f) return false;
    if (ndcZ < 0.0f || ndcZ > 1.0f) return false;

    // ndc -> screen
    outScreen.x = (ndcX * 0.5f + 0.5f) * screenW;
    outScreen.y = (1.0f - (ndcY * 0.5f + 0.5f)) * screenH; // 上が0

    return true;
}

static std::string WideToUtf8(const std::wstring& ws)
{
    if (ws.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(),
        nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(),
        out.data(), size, nullptr, nullptr);
    return out;
}

static std::string SanitizeFileNameUtf8(std::string s)
{
    const char* bad = "\\/:*?\"<>|";
    for (char& c : s) {
        if (std::strchr(bad, c)) c = '_';
    }

    // パスっぽいのは拒否（最低限）
    if (s.find("..") != std::string::npos) s = "stage";

    while (!s.empty() && (s.back() == ' ' || s.back() == '.')) s.pop_back();
    if (s.empty()) s = "stage";
    return s;
}

static std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), size);
    return out;
}

static void RenderTextToRGBA_GDI(
    const std::wstring& text,
    uint32_t width,
    uint32_t height,
    std::vector<uint8_t>& outRgba,
    int fontSizePx = 32
)
{
    outRgba.assign((size_t)width * height * 4, 0); // 透明で初期化

    // 何も無ければ透明のまま
    if (width == 0 || height == 0) return;

    // 32bit DIB（BGRA）を作る
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = (LONG)width;
    bmi.bmiHeader.biHeight = -(LONG)height; // ★上が0になるトップダウン
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC hdc = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(hdc);

    HBITMAP dib = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(memDC, dib);

    // 背景を透明（=黒で塗って後でα0にする）
    PatBlt(memDC, 0, 0, (int)width, (int)height, BLACKNESS);

    // フォント
    HFONT font = CreateFontW(
        fontSizePx, 0, 0, 0,
        FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Meiryo" // 日本語が無難（無ければMS Gothic等に）
    );
    HGDIOBJ oldFont = SelectObject(memDC, font);

    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(255, 255, 255)); // 白文字

    RECT rc{ 0, 0, (LONG)width, (LONG)height };

    // 左上寄せで描画（必要ならDT_CENTER等に変更）
    DrawTextW(memDC, text.c_str(), (int)text.size(), &rc, DT_LEFT | DT_TOP | DT_NOPREFIX);

    // bits は BGRA（B,G,R,Aなし）なので RGBA に変換しつつ αを作る
    // 今回は「黒以外の画素＝文字」とみなして α=255 にする簡易版
    const uint8_t* src = reinterpret_cast<const uint8_t*>(bits);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const size_t i = ((size_t)y * width + x) * 4;

            uint8_t B = src[i + 0];
            uint8_t G = src[i + 1];
            uint8_t R = src[i + 2];

            // 文字色（白）以外もアンチエイリアスで灰色になるので、その明るさをαにする
            const uint8_t a = (uint8_t)std::clamp<int>((int)std::max<int>({ R, G, B }), 0, 255);

            outRgba[i + 0] = 255; // 文字色を白に固定
            outRgba[i + 1] = 255;
            outRgba[i + 2] = 255;
            outRgba[i + 3] = a;   // 明るさをαに
        }
    }

    // 後始末
    SelectObject(memDC, oldFont);
    DeleteObject(font);

    SelectObject(memDC, oldBmp);
    DeleteObject(dib);

    DeleteDC(memDC);
    ReleaseDC(nullptr, hdc);
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
    gates_[0].Initialize(Object3dManager::GetInstance(), "Gate.obj", camera_);

    // Gate 2
    gates_[1].gate.pos = { 5, 3, 20 };
    gates_[1].gate.rot = { 0, 0.7f, 0 };
    gates_[1].gate.scale = { 2, 2, 2 };
    gates_[1].Initialize(Object3dManager::GetInstance(), "Gate.obj", camera_);

    // Gate 3
    gates_[2].gate.pos = { -3, 3, 30 };
    gates_[2].gate.rot = { 0.2f, -0.4f, 0 };
    gates_[2].gate.scale = { 2, 2, 2 };
    gates_[2].Initialize(Object3dManager::GetInstance(), "Gate.obj", camera_);

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
    goalSys_.SetGoalAlpha(goalAlpha_);
    goalSys_.SetEditorAlwaysVisible(true); 
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
    

    goalAlpha_ = 0.25f;
    goalSys_.SetGoalAlpha(goalAlpha_);

    modeHud_ = new Sprite();
    modeHud_->Initialize(SpriteManager::GetInstance(), "resources/uvChecker.png"); // 何でもOK
    modeHud_->SetPosition({ 12.0f, 12.0f });
    modeHud_->SetSize({ 24.0f, 24.0f });
    modeHud_->SetColor({ 1,1,1,1 });
    modeHud_->Update();


    font_.Initialize(SpriteManager::GetInstance(),
        "resources/ui/ascii_font_16x6_cell32_first32.png",
        16, 6, 32, 32, 32);

    font_.SetColor({ 1,1,1,1 });

    gateNum.Initialize(SpriteManager::GetInstance(),
        "resources/ui/ascii_font_16x6_cell32_first32.png",
        16, 6, 32, 32, 32);
    
    gateNum.SetColor({ 1,1,0,1 }); // 見やすい色（好きに）

    // 動的テクスチャを1回だけ作る
    TextureManager::GetInstance()->CreateDynamicTextureRGBA8(kNameTexKey, kNameTexW, kNameTexH);

    // 表示用スプライト
    stageNameSprite_ = new Sprite();
    stageNameSprite_->Initialize(SpriteManager::GetInstance(), kNameTexKey);
    stageNameSprite_->SetPosition({ 16.0f, 16.0f });
    stageNameSprite_->SetSize({1280, 720.0f }); // 必要なら調整


}

void StageEditorScene::Finalize()
{
    ParticleManager::GetInstance()->ClearAllParticles();

    LightManager::GetInstance()->Finalize();

    delete droneObj_;  droneObj_ = nullptr;
    delete camera_;    camera_ = nullptr;
    delete modeHud_;   modeHud_ = nullptr;

    goalSys_.Finalize();

    delete stageNameSprite_; stageNameSprite_ = nullptr;
}




void StageEditorScene::Update()
{
    float dt = 1.0f / 60.0f;
    Input& input = *Input::GetInstance();

    // =========================
    // ★命名入力は Update の最優先で処理する
    // =========================
    if (input.IsKeyTrigger(DIK_F2)) {

        if (!isNaming_) {
            // === 命名開始 ===
            isNaming_ = true;

            TextInput::GetInstance()->Clear();
            stageNameW_.clear();

            // 表示を消す（任意）
            nameRgba_.assign((size_t)kNameTexW * kNameTexH * 4, 0);
            TextureManager::GetInstance()->UpdateDynamicTextureRGBA8(
                kNameTexKey, nameRgba_.data(), kNameTexW, kNameTexH);

        }
        else {
            // === 命名キャンセル（F2 2回目）===
            isNaming_ = false;

            // 変換中も含めて掃除したいなら（任意）
            TextInput::GetInstance()->Clear();
            stageNameW_.clear();

            // 表示も消したいなら（任意）
            nameRgba_.assign((size_t)kNameTexW * kNameTexH * 4, 0);
            TextureManager::GetInstance()->UpdateDynamicTextureRGBA8(
                kNameTexKey, nameRgba_.data(), kNameTexW, kNameTexH);
        }
    }


    if (isNaming_) {
        // 表示用（確定+変換中）
        stageNameW_ = TextInput::GetInstance()->GetDisplayString();

        // 文字更新があったフレームだけテクスチャ更新
        if (TextInput::GetInstance()->ConsumeDirty()) {
            RenderTextToRGBA_GDI(stageNameW_, kNameTexW, kNameTexH, nameRgba_);
            TextureManager::GetInstance()->UpdateDynamicTextureRGBA8(
                kNameTexKey, nameRgba_.data(), kNameTexW, kNameTexH);
        }

        // 確定（Enter）
        if (input.IsKeyTrigger(DIK_RETURN)) {
            stageNameUtf8_ = WideToUtf8(stageNameW_);
            stageNameUtf8_ = SanitizeFileNameUtf8(stageNameUtf8_);
            stageFile_ = stageNameUtf8_ + ".json";
            isNaming_ = false;
        }
        // ★命名中は “ここで終了”。移動/編集/カメラ処理に行かせない
        return;
    }

    // =========================
    // ここから下が通常更新
    // =========================

    UpdateFreeCamera(dt);

    {
        Vector3 pos = drone_.GetPos();
        Vector3 vel = drone_.GetVel();
        wallSys_.ResolveDroneAABB(pos, vel, droneHalf_, dt, 4);
        drone_.SetPos(pos);
        drone_.SetVel(vel);
    }

    if (droneObj_) {
        droneObj_->SetTranslate(drone_.GetPos());
        droneObj_->Update();
    }

    goalSys_.Update(gates_, nextGate_, drone_.GetPos());
    if (goalSys_.IsCleared()) stageCleared_ = true;

    for (auto& g : gates_) g.Tick(dt);

    for (int i = 0; i < (int)gates_.size(); ++i) {
        gates_[i].SetSelected(editMode_ == EditMode::Gate && i == selectedGate_);
    }

    UpdateEditorInput(dt);

    if (drawWallDebug_) wallSys_.UpdateDebug();

    if (input.IsKeyTrigger(DIK_H)) {
        showHelp_ = !showHelp_;
    }
}

void StageEditorScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();

    if (!modeHud_) return;

    Vector4 col =
        (editMode_ == EditMode::Gate) ? Vector4{ 1,0,0,1 } :
        (editMode_ == EditMode::Wall) ? Vector4{ 0,1,0,1 } :
        Vector4{ 0,0,1,1 };

    modeHud_->SetColor(col);
    modeHud_->Update();
    modeHud_->Draw();

    font_.BeginFrame();

    gateNum.BeginFrame();

    if (showHelp_) {

        font_.SetColor({ 1,1,1,1 });

        // =========================
        // 左カラム（常時表示）
        // =========================
        std::string left =
            "=== STAGE EDITOR HELP ===\n"
            "[H] Toggle Help\n"
            "\n"
            "Mode:\n"
            "  [1] Gate\n"
            "  [2] Wall\n"
            "  [3] Goal\n"
            "\n"
            "Camera:\n"
            "  RMB Drag : Look\n"
            "  WASD     : Move\n"
            "  Q/E      : Down/Up\n"
            "  LSHIFT   : Fast\n"
            "  Y        : Reset Camera\n"
            "\n"
            "Common:\n"
            "  LMB Click : Select\n"
            "  Hold P + LMB Click : Place (Y=0)\n"
			"  F2 : Name Stage or Name Finishn"
            "  F5 : Save\n"
            "  F9 : Load\n";

        font_.DrawString(
            helpPosLeft_.x,
            helpPosLeft_.y,
            left,
            helpScale_
        );

        // =========================
        // 右カラム（モード別）
        // =========================
        std::string right;

        if (editMode_ == EditMode::Gate) {
            right =
                "[Gate Mode]\n"
                "  [ / ] : Select Gate\n"
                "  N : Add\n"
                "  C : Duplicate\n"
                "  DEL : Remove\n"
                "\n"
                "Move:\n"
                "  Arrow : XZ\n"
                "  PgUp/PgDn : Y\n"
                "\n"
                "Rotate:\n"
                "  I/K : RotX\n"
                "  J/L : RotY\n";
        } else if (editMode_ == EditMode::Wall) {
            right =
                "[Wall Mode]\n"
                "  [ / ] : Select Wall\n"
                "  N / C : Add / Duplicate\n"
                "  DEL   : Remove\n"
                "  T : AABB <-> OBB\n"
                "\n"
                "Move:\n"
                "  Arrow : XZ\n"
                "  PgUp/PgDn : Y\n"
                "\n"
                "Size:\n"
                "  U/O : X\n"
                "  I/K : Y\n"
                "  J/L : Z\n"
                "\n"
                "Rotate (OBB):\n"
                "  R/F : RotY\n";
        } else if (editMode_ == EditMode::Goal) {
            right =
                "[Goal Mode]\n"
                "Move:\n"
                "  Arrow : XZ\n"
                "  PgUp/PgDn : Y\n"
                "\n";
        }

        font_.DrawString(
            helpPosRight_.x,
            helpPosRight_.y,
            right,
            helpScale_
        );
    }

    // ★追加：パラメータHUD（常時でも、showHelp_中だけでもOK）
    DrawEditorParamHud_();

    DrawGateIndices_();

    SpriteManager::GetInstance()->PreDraw();

    if (stageNameSprite_) {
        stageNameSprite_->Update();
        stageNameSprite_->Draw();
    }


}

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

//ステージセーブ
bool StageEditorScene::SaveStageJson(const std::string& fileName)
{
    std::string safe = SanitizeFileNameUtf8(fileName);

    // "resources/stage/" は wide で持つ（安定）
    std::filesystem::path dir = std::filesystem::path(L"resources") / L"stage";
    std::filesystem::path path = dir / Utf8ToWide(safe);

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

    // ★ path を直接渡す（MSVCならこれで日本語OK）
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs.is_open()) return false;

    ofs << root.dump(2);
    return true;
}

//ステージロード
bool StageEditorScene::LoadStageJson(const std::string& fileName)
{
    StageData data;
    if (!StageIO::Load(fileName, data)) return false;

    // drone
    drone_.SetPos(data.droneSpawnPos);
    drone_.SetYaw(data.droneSpawnYaw);
    drone_.SetVel({ 0,0,0 });

    // gates
    gates_.clear();
    for (const auto& g : data.gates) {
        GateVisual gv;
        gv.gate = g;
        gv.Initialize(Object3dManager::GetInstance(), "cube.obj", camera_);
        gates_.push_back(std::move(gv));
    }

    // walls
    wallSys_.Walls() = data.walls;

    // goal
    goalSys_.Reset();
    stageCleared_ = false;
    if (data.hasGoalSpawnOffset) goalSys_.SetSpawnOffset(data.goalSpawnOffset);
    if (data.hasGoalPos) goalSys_.SetGoalPos(data.goalPos);

    wallSys_.BuildDebug(Object3dManager::GetInstance(), "cube.obj");
    nextGate_ = 0;
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
    static bool showMouseDbg = false;
    if (Input::GetInstance()->IsKeyTrigger(DIK_F1)) {
        showMouseDbg = !showMouseDbg;
    }

    if (showMouseDbg) {
        ImGui::Begin("Mouse Debug");
        ImGui::Text("RButton=%d", input.IsMousePressed(1));
        ImGui::Text("WantCaptureMouse=%d", ImGui::GetIO().WantCaptureMouse ? 1 : 0);
        ImGui::Text("dx=%ld dy=%ld", d.x, d.y);
        ImGui::Text("camYaw=%.3f camPitch=%.3f", camYaw_, camPitch_);
        ImGui::End();
    }



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

void StageEditorScene::UpdateEditorInput(float dt)
{
    Input& input = *Input::GetInstance();

    // モード切替（1/2/3）
    if (input.IsKeyTrigger(DIK_1)) editMode_ = EditMode::Gate;
    if (input.IsKeyTrigger(DIK_2)) editMode_ = EditMode::Wall;
    if (input.IsKeyTrigger(DIK_3)) editMode_ = EditMode::Goal;

    const bool fast = input.IsKeyPressed(DIK_LSHIFT);
    const float mv = moveStep_ * (fast ? 5.0f : 1.0f);
    const float rv = rotStep_ * (fast ? 5.0f : 1.0f);
    const float sv = sizeStep_ * (fast ? 5.0f : 1.0f);

    // クリック配置（Y=0平面）
    auto PlaceByMouse = [&](Vector3& inoutPos)
        {
            if (input.IsMouseTrigger(0)) {
                POINT mp = input.GetMousePos();
                const float W = (float)WinApp::kClientWidth;
                const float H = (float)WinApp::kClientHeight;
                const Matrix4x4& vp = camera_->GetViewProjectionMatrix();
                Vector3 hit{};
                if (ScreenRayToPlaneY0_RowVector(mp.x, mp.y, W, H, vp, hit)) {
                    inoutPos = hit;
                }
            }
        };

    // ========== クリック選択（P押してない時は選択、P押してる時は配置） ==========
    {
        // 右ドラッグでカメラ回してる最中は選択しない（誤爆防止）
        const bool cameraRotating = input.IsMousePressed(1);

        // P押し中は「配置」なので選択はしない
        const bool placing = input.IsKeyPressed(DIK_P);

        if (!cameraRotating && !placing && input.IsMouseTrigger(0))
        {
            POINT mp = input.GetMousePos();
            const float W = (float)WinApp::kClientWidth;
            const float H = (float)WinApp::kClientHeight;
            const Matrix4x4& vp = camera_->GetViewProjectionMatrix();

            Vector3 hit{};
            if (ScreenRayToPlaneY0_RowVector(mp.x, mp.y, W, H, vp, hit))
            {
                if (editMode_ == EditMode::Gate) {
                    const int idx = FindNearestGateIndex(gates_, hit, /*maxDist=*/6.0f);
                    if (idx >= 0) selectedGate_ = idx;
                } else if (editMode_ == EditMode::Wall) {
                    auto& walls = wallSys_.Walls();
                    const int idx = FindNearestWallIndex(walls, hit, /*maxDist=*/8.0f);
                    if (idx >= 0) {
                        selectedWall_ = idx;
                        wallSys_.SetSelectedIndex(selectedWall_);
                    }
                } else if (editMode_ == EditMode::Goal) {
                    // クリックでゴール移動したいなら：
                    // goalSys_.SetGoalPos(hit);
                }
            }
        }
    }



    // 保存/ロード（F5/F9）
    if (input.IsKeyTrigger(DIK_F5)) SaveStageJson(stageFile_);
    if (input.IsKeyTrigger(DIK_F9)) LoadStageJson(stageFile_);

    // ========== Gate編集 ==========
    if (editMode_ == EditMode::Gate)
    {
        if (!gates_.empty()) {
            selectedGate_ = std::clamp(selectedGate_, 0, (int)gates_.size() - 1);

            // 選択（[ / ]）
            if (input.IsKeyTrigger(DIK_LBRACKET)) selectedGate_ = std::max<float>(0, selectedGate_ - 1);
            if (input.IsKeyTrigger(DIK_RBRACKET)) selectedGate_ = std::min<float>((int)gates_.size() - 1, selectedGate_ + 1);

            Gate& g = gates_[selectedGate_].gate;

            // 追加（N）/複製（C）/削除（Delete）
            if (input.IsKeyTrigger(DIK_N)) {
                GateVisual gv;
                gv.gate = g;                 // 現在のをベース
                gv.gate.pos.z += 3.0f;
                gv.Initialize(Object3dManager::GetInstance(), "cube.obj", camera_);
                gates_.insert(gates_.begin() + (selectedGate_ + 1), std::move(gv));
                selectedGate_++;
            }
            if (input.IsKeyTrigger(DIK_C)) {
                GateVisual gv;
                gv.gate = g;
                gv.gate.pos.z += 3.0f;
                gv.Initialize(Object3dManager::GetInstance(), "cube.obj", camera_);
                gates_.insert(gates_.begin() + (selectedGate_ + 1), std::move(gv));
                selectedGate_++;
            }
          /*  if (input.IsKeyTrigger(DIK_DELETE)) {
                gates_.erase(gates_.begin() + selectedGate_);
                if (!gates_.empty())
                    selectedGate_ = std::clamp(selectedGate_, 0, (int)gates_.size() - 1);
                else
                    selectedGate_ = 0;
            }*/

            if (input.IsKeyTrigger(DIK_DELETE)) {
                if ((int)gates_.size() > 1) {
                    gates_.erase(gates_.begin() + selectedGate_);
                    selectedGate_ = std::clamp(selectedGate_, 0, (int)gates_.size() - 1);
                }
                // else: 1個しかないので消さない
            }



            // マウス配置（Pでトグルでもいいけど、まずは常時でもOK）
            if (input.IsKeyPressed(DIK_P)) {
                PlaceByMouse(g.pos);
            }

            // 移動（矢印 + PageUp/PageDown）
            if (input.IsKeyPressed(DIK_UP))    g.pos.z += mv * dt * 60.0f;
            if (input.IsKeyPressed(DIK_DOWN))  g.pos.z -= mv * dt * 60.0f;
            if (input.IsKeyPressed(DIK_RIGHT)) g.pos.x += mv * dt * 60.0f;
            if (input.IsKeyPressed(DIK_LEFT))  g.pos.x -= mv * dt * 60.0f;
            if (input.IsKeyPressed(DIK_PGUP))  g.pos.y += mv * dt * 60.0f;
            if (input.IsKeyPressed(DIK_PGDN))  g.pos.y -= mv * dt * 60.0f;

            // 回転（I/K/J/L）
            if (input.IsKeyPressed(DIK_I)) g.rot.x -= rv * dt * 60.0f;
            if (input.IsKeyPressed(DIK_K)) g.rot.x += rv * dt * 60.0f;
            if (input.IsKeyPressed(DIK_J)) g.rot.y -= rv * dt * 60.0f;
            if (input.IsKeyPressed(DIK_L)) g.rot.y += rv * dt * 60.0f;

            // パラメータ（1/2: perfect, 3/4: gateRadius, 5/6: thickness）
            if (input.IsKeyPressed(DIK_4)) g.perfectRadius = std::max<float>(0.1f, g.perfectRadius - 0.05f);
            if (input.IsKeyPressed(DIK_5)) g.perfectRadius = std::min<float>(g.gateRadius, g.perfectRadius + 0.05f);

            if (input.IsKeyPressed(DIK_6)) g.gateRadius = std::max<float>(g.perfectRadius, g.gateRadius - 0.05f);
            if (input.IsKeyPressed(DIK_7)) g.gateRadius = g.gateRadius + 0.05f;

            if (input.IsKeyPressed(DIK_8)) g.thickness = std::max<float>(0.05f, g.thickness - 0.05f);
            if (input.IsKeyPressed(DIK_9)) g.thickness = g.thickness + 0.05f;
        }
    }

    // ========== Wall編集 ==========
    if (editMode_ == EditMode::Wall)
    {
        auto& walls = wallSys_.Walls();

        // 壁が無い
        if (walls.empty()) {
            selectedWall_ = 0;
            wallSys_.SetSelectedIndex(-1);
        } else
        {
            // まず同期（クリック選択後も確実に反映）
            selectedWall_ = std::clamp(selectedWall_, 0, (int)walls.size() - 1);
            wallSys_.SetSelectedIndex(selectedWall_);

            // 選択（[ / ]）
            if (input.IsKeyTrigger(DIK_LBRACKET)) selectedWall_ = std::max<float>(0, selectedWall_ - 1);
            if (input.IsKeyTrigger(DIK_RBRACKET)) selectedWall_ = std::min<float>((int)walls.size() - 1, selectedWall_ + 1);

            // ここで再同期（[ ] で動いた場合）
            wallSys_.SetSelectedIndex(selectedWall_);

            auto& w = walls[selectedWall_];

            // 追加（N）/複製（C）
            if (input.IsKeyTrigger(DIK_N) || input.IsKeyTrigger(DIK_C)) {
                WallSystem::Wall nw = w;
                nw.center.z += 2.0f;
                walls.insert(walls.begin() + (selectedWall_ + 1), nw);
                selectedWall_++;

                wallSys_.SetSelectedIndex(selectedWall_); // ★追加後も同期
                wallSys_.BuildDebug(Object3dManager::GetInstance(), "cube.obj");
            }

            // 削除（Delete）
            if (input.IsKeyTrigger(DIK_DELETE)) {
                if ((int)walls.size() > 1) {
                    walls.erase(walls.begin() + selectedWall_);
                    selectedWall_ = std::clamp(selectedWall_, 0, (int)walls.size() - 1);
                    wallSys_.SetSelectedIndex(selectedWall_);
                    wallSys_.BuildDebug(Object3dManager::GetInstance(), "cube.obj");
                }
                // else: 1個しかないので消さない
            }


            // （削除で walls が空になった可能性があるのでガード）
            if (!walls.empty())
            {
                auto& w2 = walls[selectedWall_];

                // タイプ切替（T）
                if (input.IsKeyTrigger(DIK_T)) {
                    w2.type = (w2.type == WallSystem::Type::AABB) ? WallSystem::Type::OBB : WallSystem::Type::AABB;
                    wallSys_.BuildDebug(Object3dManager::GetInstance(), "cube.obj");
                }

                // マウス配置（P押しながら）
                if (input.IsKeyPressed(DIK_P)) {
                    PlaceByMouse(w2.center);
                }

                // 移動
                if (input.IsKeyPressed(DIK_UP))    w2.center.z += mv * dt * 60.0f;
                if (input.IsKeyPressed(DIK_DOWN))  w2.center.z -= mv * dt * 60.0f;
                if (input.IsKeyPressed(DIK_RIGHT)) w2.center.x += mv * dt * 60.0f;
                if (input.IsKeyPressed(DIK_LEFT))  w2.center.x -= mv * dt * 60.0f;
                if (input.IsKeyPressed(DIK_PGUP))  w2.center.y += mv * dt * 60.0f;
                if (input.IsKeyPressed(DIK_PGDN))  w2.center.y -= mv * dt * 60.0f;

                // サイズ
                if (input.IsKeyPressed(DIK_U)) w2.half.x = std::max<float>(0.05f, w2.half.x - sv * dt * 60.0f);
                if (input.IsKeyPressed(DIK_O)) w2.half.x = w2.half.x + sv * dt * 60.0f;
                if (input.IsKeyPressed(DIK_I)) w2.half.y = std::max<float>(0.05f, w2.half.y - sv * dt * 60.0f);
                if (input.IsKeyPressed(DIK_K)) w2.half.y = w2.half.y + sv * dt * 60.0f;
                if (input.IsKeyPressed(DIK_J)) w2.half.z = std::max<float>(0.05f, w2.half.z - sv * dt * 60.0f);
                if (input.IsKeyPressed(DIK_L)) w2.half.z = w2.half.z + sv * dt * 60.0f;

                // OBB回転（R/F）
                if (w2.type == WallSystem::Type::OBB) {
                    if (input.IsKeyPressed(DIK_R)) w2.rot.y -= rv * dt * 60.0f;
                    if (input.IsKeyPressed(DIK_F)) w2.rot.y += rv * dt * 60.0f;
                } else {
                    w2.rot = { 0,0,0 };
                }

                wallSys_.SetSelectedIndex(selectedWall_);
            }
        }
    }

    goalSys_.SetHighlighted(editMode_ == EditMode::Goal);

    // ========== Goal編集 ==========
    if (editMode_ == EditMode::Goal)
    {
        Vector3 gp = goalSys_.GetGoalPos();

        // ★P押しながらクリックで地面(Y=0)に配置
        if (input.IsKeyPressed(DIK_P) && input.IsMouseTrigger(0)) {
            POINT mp = input.GetMousePos();
            const float W = (float)WinApp::kClientWidth;
            const float H = (float)WinApp::kClientHeight;
            const Matrix4x4& vp = camera_->GetViewProjectionMatrix();
            Vector3 hit{};
            if (ScreenRayToPlaneY0_RowVector(mp.x, mp.y, W, H, vp, hit)) {
                goalSys_.SetGoalPos(hit);
                gp = hit;
            }
        }

        // 位置操作（例）
        if (input.IsKeyPressed(DIK_UP))    gp.z += mv * dt * 60.0f;
        if (input.IsKeyPressed(DIK_DOWN))  gp.z -= mv * dt * 60.0f;
        if (input.IsKeyPressed(DIK_RIGHT)) gp.x += mv * dt * 60.0f;
        if (input.IsKeyPressed(DIK_LEFT))  gp.x -= mv * dt * 60.0f;
        if (input.IsKeyPressed(DIK_PGUP))  gp.y += mv * dt * 60.0f;
        if (input.IsKeyPressed(DIK_PGDN))  gp.y -= mv * dt * 60.0f;

        goalSys_.SetGoalPos(gp);

        // アルファ操作（- / +）
        if (input.IsKeyPressed(DIK_MINUS))  goalAlpha_ = std::max<float>(0.0f, goalAlpha_ - 0.01f);
        if (input.IsKeyPressed(DIK_EQUALS)) goalAlpha_ = std::min<float>(1.0f, goalAlpha_ + 0.01f);

        goalSys_.SetGoalAlpha(goalAlpha_);
    }

}

void StageEditorScene::DrawEditorParamHud_()
{
    // HUD表示位置（右上寄せなど好きに）
    const float x = 800.0f;
    const float y = 360.0f;

    std::string s;
    s.reserve(1024);

    // 共通情報
    s += "=== EDITOR PARAMS ===\n";
    s += "File : " + std::string(stageFile_) + "\n";

    // Mode 表示
    s += "Mode : ";
    s += (editMode_ == EditMode::Gate) ? "Gate\n" :
        (editMode_ == EditMode::Wall) ? "Wall\n" : "Goal\n";

    // 選択状況
    if (editMode_ == EditMode::Gate) {
        s += "SelectedGate : " + std::to_string(selectedGate_) + " / " + std::to_string((int)gates_.size()) + "\n";
        s += "NextGate     : " + std::to_string(nextGate_) + "\n";

        if (!gates_.empty() && 0 <= selectedGate_ && selectedGate_ < (int)gates_.size()) {
            const Gate& g = gates_[selectedGate_].gate;
            s += "\n[Gate]\n";
            s += "pos   : " + Vec3Str(g.pos) + "\n";
            s += "rot   : " + Vec3Str(g.rot) + " (rad)\n";
            s += "scale : " + Vec3Str(g.scale) + "\n";
            s += "perfectRadius : " + std::to_string(g.perfectRadius) + "\n";
            s += "gateRadius    : " + std::to_string(g.gateRadius) + "\n";
            s += "thickness     : " + std::to_string(g.thickness) + "\n";
        }
    } else if (editMode_ == EditMode::Wall) {
        auto& walls = wallSys_.Walls();
        s += "SelectedWall : " + std::to_string(selectedWall_) + " / " + std::to_string((int)walls.size()) + "\n";

        if (!walls.empty() && 0 <= selectedWall_ && selectedWall_ < (int)walls.size()) {
            const auto& w = walls[selectedWall_];
            s += "\n[Wall]\n";
            s += "type  : ";
            s += (w.type == WallSystem::Type::AABB) ? "AABB\n" : "OBB\n";
            s += "center: " + Vec3Str(w.center) + "\n";
            s += "half  : " + Vec3Str(w.half) + "\n";
            if (w.type == WallSystem::Type::OBB) {
                s += "rot   : " + Vec3Str(w.rot) + " (rad)\n";
            }
        }
    } else if (editMode_ == EditMode::Goal) {
        s += "\n[Goal]\n";
        s += "pos   : " + Vec3Str(goalSys_.GetGoalPos()) + "\n";
        s += "alpha : " + std::to_string(goalAlpha_) + "\n";
        s += "cleared : ";
        s += stageCleared_ ? "YES\n" : "NO\n";
    }

    // 文字描画
    font_.SetColor({ 1,1,1,1 });
    font_.DrawString(x, y, s, 0.5f);
}

void StageEditorScene::DrawGateIndices_()
{
    const float W = (float)WinApp::kClientWidth;
    const float H = (float)WinApp::kClientHeight;
    const Matrix4x4& vp = camera_->GetViewProjectionMatrix();

  

    for (int i = 0; i < (int)gates_.size(); ++i) {
        const Vector3 p = gates_[i].gate.pos;

        // 少し上に出す（ゲート中心に被るのが嫌なら）
        Vector3 labelPos = p;
        labelPos.y += 0.0f;

        Vector2 screen{};
        if (!WorldToScreen_RowVector(labelPos, vp, W, H, screen)) {
            continue;
        }

        // 文字列は "0", "1", ...
        const std::string txt = std::to_string(i);

        // 中央寄せしたいなら少し左にずらす（1文字想定の簡易）
        gateNum.DrawString(screen.x - 8.0f, screen.y - 8.0f, txt, 0.8f);
    }
}
