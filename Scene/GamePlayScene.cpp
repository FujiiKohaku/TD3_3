#include "GamePlayScene.h"
#include "../Light/LightManager.h"
#include "ParticleManager.h"
#include "SphereObject.h"
#include <numbers>

#include "../externals/nlohmann/json.hpp"
#include <fstream>
#include <string>

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

// ===== RowVec * Mat4 で w除算して Vector3 にする =====
static Vector3 TransformCoord_RowVector4(const Vector4& v, const Matrix4x4& m)
{
    Vector4 o = MulRowVec4Mat4(v, m);
    if (std::abs(o.w) > 1e-6f) {
        o.x /= o.w; o.y /= o.w; o.z /= o.w;
    }
    return { o.x, o.y, o.z };
}

// ===== Screen -> World のレイ（y=0平面に当てる用）=====
// D3DのNDC: x,y = [-1..1], z = [0..1] 想定
static bool ScreenRayToPlaneY0_RowVector(
    int mouseX, int mouseY,
    float screenW, float screenH,
    const Matrix4x4& viewProj,
    Vector3& outHit
)
{
    // ViewProj の逆
    Matrix4x4 invVP = MatrixMath::Inverse(viewProj);

    // Screen -> NDC
    float ndcX = ((float)mouseX / screenW) * 2.0f - 1.0f;
    float ndcY = 1.0f - ((float)mouseY / screenH) * 2.0f;

    // near(z=0) と far(z=1) を unproject
    Vector3 pNear = TransformCoord_RowVector4({ ndcX, ndcY, 0.0f, 1.0f }, invVP);
    Vector3 pFar = TransformCoord_RowVector4({ ndcX, ndcY, 1.0f, 1.0f }, invVP);

    // ray
    Vector3 dir{ pFar.x - pNear.x, pFar.y - pNear.y, pFar.z - pNear.z };
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len < 1e-6f) return false;
    dir.x /= len; dir.y /= len; dir.z /= len;

    // 平面 y=0 と交差
    if (std::abs(dir.y) < 1e-6f) return false; // 平面と平行
    float t = (0.0f - pNear.y) / dir.y;
    if (t <= 0.0f) return false; // カメラの後ろ側

    outHit = { pNear.x + dir.x * t, 0.0f, pNear.z + dir.z * t };
    return true;
}


static bool WorldToScreen_RowVector(
    const Vector3& worldPos,
    const Matrix4x4& viewProj,
    float screenW, float screenH,
    ImVec2& outScreen
)
{
    Vector4 clip = MulRowVec4Mat4({ worldPos.x, worldPos.y, worldPos.z, 1.0f }, viewProj);

    // カメラ後ろ(またはwが小さい)は描かない
    if (clip.w <= 1e-6f) return false;

    // NDC化
    const float ndcX = clip.x / clip.w;
    const float ndcY = clip.y / clip.w;

    // 画面座標へ（左上原点）
    outScreen.x = (ndcX * 0.5f + 0.5f) * screenW;
    outScreen.y = (-ndcY * 0.5f + 0.5f) * screenH;

    return true;
}


void GamePlayScene::Initialize()
{
    camera_ = new Camera();
    camera_->Initialize();
    camera_->SetTranslate({ 0, 0, 0 });

    Object3dManager::GetInstance()->SetDefaultCamera(camera_);

    ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), camera_);

    Object3dManager::GetInstance()->SetDefaultCamera(camera_);

    sprite_ = new Sprite();
    sprite_->Initialize(SpriteManager::GetInstance(), "resources/uvChecker.png");
    sprite_->SetPosition({ 100.0f, 100.0f });
    // サウンド関連
    bgm = SoundManager::GetInstance()->SoundLoadFile("Resources/BGM.wav");
    player2_ = new Object3d();
    player2_->Initialize(Object3dManager::GetInstance());
    player2_->SetModel("cube.obj");
    //player2_->SetModel("terrain.obj"); 
    player2_->SetTranslate({ 3.0f, 0.0f, 0.0f });
    //player2_->SetRotate({ std::numbers::pi_v<float> / 2.0f, std::numbers::pi_v<float>, 0.0f });

    ParticleManager::GetInstance()->CreateParticleGroup("circle", "resources/circle.png");
    Transform t {};
    t.translate = { 0.0f, 0.0f, 0.0f };

    emitter_.Init("circle", t, 30, 0.1f);

    sphere_ = new SphereObject();
    sphere_->Initialize(DirectXCommon::GetInstance(), 16, 1.0f);

    // Transform
    sphere_->SetTranslate({ 0, 0, 0 }); // 消える？
    sphere_->SetScale({ 1.5f, 1.5f, 1.5f });

    // Material
    sphere_->SetColor({ 1, 1, 1, 1 });
    LightManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
    LightManager::GetInstance()->SetDirectional({ 1, 1, 1, 1 }, { 0, -1, 0 }, 1.0f);

    // ---- Drone Object ----
    droneObj_ = new Object3d();
    droneObj_->Initialize(Object3dManager::GetInstance());

    // ★存在するモデル名にしてね（無ければ cube.obj とか）
  //  droneObj_->SetModel("cube.obj");
    droneObj_->SetModel("cube.obj"); // ←まずこれで見えるかテスト
    droneObj_->SetTranslate({ 0.0f, 1.0f, 0.0f });
    droneObj_->SetScale({ 0.1f, 0.1f, 0.1f });

   drone_.Initialize({ 0.0f, 1.0f, 0.0f });

   //ゲート

    // 例：ゲート3つ
   gates_.resize(3);

   // Gate 1
   gates_[0].gate.rot = { 0, 0, 0 };            // まず回転なし
   gates_[0].gate.scale = { 10.0f, 10.0f, 10.0f }; // デカくする（見えるか確認）
   gates_[0].gate.pos = { 0, 2, 5 };            // 近づける
   gates_[0].gate.perfectRadius = 1.0f;
   gates_[0].gate.gateRadius = 2.5f;
   gates_[0].gate.thickness = 0.8f;
   gates_[0].Initialize(Object3dManager::GetInstance(), "cube.obj", camera_);

   // Gate 2（回転）
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
   perfectCount_ = 0;
   goodCount_ = 0;

   //壁システム初期化
   // 壁システム初期化（デバッグ表示したい場合）
   wallSys_.BuildDebug(Object3dManager::GetInstance(), "cube.obj");

   // AABB壁（回転なし）
   wallSys_.AddAABB({ 0.0f, 2.0f, 12.0f }, { 6.0f, 2.0f, 0.5f });  // 幅12, 高さ4, 厚み1
   wallSys_.AddAABB({ 8.0f, 2.0f, 10.0f }, { 0.5f, 2.0f, 6.0f });  // 右壁

   // OBB壁（回転あり）
   wallSys_.AddOBB({ -4.0f, 2.0f, 18.0f }, { 4.0f, 2.0f, 0.5f }, { 0.0f, 0.6f, 0.0f }); // yaw回転



}

void GamePlayScene::Update()
{

    float dt = 1.0f / 60.0f;

    // ★入力更新（必須）
   
    Input& input = *Input::GetInstance();

    // ドローン更新（※これが無いとカメラも動かない）
    if (isDebug_) {
        drone_.UpdateDebugNoInertia(input, dt);
    } else {
        drone_.UpdateMode1(input, dt);
    }
    {
        Vector3 pos = drone_.GetPos();
        Vector3 vel = drone_.GetVel();

        wallSys_.ResolveDroneAABB(pos, vel, droneHalf_, dt, 4);

        drone_.SetPos(pos);
        drone_.SetVel(vel);
    }


    // ★ドローン実体 → 描画Object3dへ反映（毎フレーム必須）
    if (droneObj_) {
        droneObj_->SetTranslate(drone_.GetPos());
        droneObj_->SetRotate({ drone_.GetPitch(), drone_.GetYaw() + droneYawOffset, drone_.GetRoll() });
        droneObj_->Update();
    }


    // ★これを毎フレーム呼ぶ
    camera_->FollowDroneRigid(drone_, 7.5f, 1.8f, -0.18f, droneYawOffset);
 


    // 更新系
    emitter_.Update();
    ParticleManager::GetInstance()->Update();
    player2_->Update();
    sprite_->Update();
    sphere_->Update(camera_);

    // ★最後に一回
   
    if (input.IsKeyTrigger(DIK_O)) {

        isDebug_ = !isDebug_;
    }

   

   //  camera_->DebugUpdate();

   
        camera_->Update();
    
        //ゲート

           // 1) 全ゲートの見た目更新（色タイマーもここで進む）
        for (auto& g : gates_) {
            g.Tick(dt);
        }

        // 2) 次ゲートだけ判定
        if (nextGate_ < (int)gates_.size()) {
            GateResult res;
            const Vector3 dronePos = drone_.GetPos(); // ★ここはあなたのドローン取得に合わせる

            if (gates_[nextGate_].TryPass(dronePos, res)) {
                if (res == GateResult::Perfect) { perfectCount_++; nextGate_++; } else if (res == GateResult::Good) { goodCount_++; nextGate_++; } else {
                    // Miss：進まない（色は赤になる）
                }
            }
        } else {
            // クリア状態
            // perfectCount_ / goodCount_ を表示する
        }


    // ==================================
    // Lighting Panel（ライト操作パネル）
    // ==================================
    ImGui::Begin("Lighting Control");

    // ---- ライトの ON / OFF ----
    static bool lightEnabled = true;
    ImGui::Checkbox("Enable Light", &lightEnabled);

    // ---- ライトの色 ----
    static Vector4 lightColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    ImGui::ColorEdit3("Light Color", (float*)&lightColor);

    // ---- 明るさ（強さ） ----
    static float lightIntensity = 1.0f;
    ImGui::SliderFloat("Intensity", &lightIntensity, 0.0f, 5.0f);

    // ---- 光の向き ----
    static Vector3 lightDir = { 0.0f, -1.0f, 0.0f };
    ImGui::SliderFloat3("Direction", &lightDir.x, -1.0f, 1.0f);

    // ---- 正規化 ----
    Vector3 normalizedDir = Normalize(lightDir);

    float intensity = lightIntensity;
    if (!lightEnabled) {
        intensity = 0.0f; // OFF のときは光なし
    }

    LightManager::GetInstance()->SetDirectional(
        { lightColor.x, lightColor.y, lightColor.z, 1.0f },
        normalizedDir,
        intensity);

    // ---- リセットボタン（向きだけ元に戻す）----
    if (ImGui::Button("Reset Direction")) {
        lightDir = { 0.0f, -1.0f, 0.0f };
    }

    ImGui::SameLine();

    // ---- ライトを完全初期化 ----
    if (ImGui::Button("Reset Light")) {
        lightEnabled = true;
        lightColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        lightIntensity = 1.0f;
        lightDir = { 0.0f, -1.0f, 0.0f };
    }
    ImGui::Separator();
    ImGui::Text("Point Light Control");

    static bool pointEnabled = true;
    ImGui::Checkbox("Enable Point Light", &pointEnabled);

    static Vector4 pointColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    ImGui::ColorEdit3("Point Color", (float*)&pointColor);

    static Vector3 pointPos = { 0.0f, 2.0f, 0.0f };
    ImGui::SliderFloat3("Point Position", &pointPos.x, -10.0f, 10.0f);

    static float pointIntensity = 1.0f;
    ImGui::SliderFloat("Point Intensity", &pointIntensity, 0.0f, 5.0f);

    float pI = pointEnabled ? pointIntensity : 0.0f;
    static float pointRadius = 10.0f;
    static float pointDecay = 1.0f;

    ImGui::SliderFloat("Point Radius", &pointRadius, 0.1f, 30.0f);
    ImGui::SliderFloat("Point Decay", &pointDecay, 0.1f, 5.0f);

    LightManager::GetInstance()->SetPointRadius(pointRadius);
    LightManager::GetInstance()->SetPointDecay(pointDecay);
    LightManager::GetInstance()->SetPointLight(pointColor,pointPos,pI);

    ImGui::End();

    // ==================================
    // Sphere Control
    // ==================================
    ImGui::Begin("Sphere Control");

    // ---- このオブジェクトだけ ライティングする？ ----
    // OFF にすると「フラット表示」になる
    ImGui::Checkbox("Enable Lighting", &sphereLighting);

    // ---- 位置 ----
    ImGui::SliderFloat3("Position", &spherePos.x, -10.0f, 10.0f);

    // ---- 回転 ----
    ImGui::SliderFloat3("Rotate", &sphereRotate.x, -3.14f, 3.14f);

     ImGui::SliderFloat3("Scale", &sphereScale.x, 1.0f, 10.0f);
    // ---- テカり具合（鏡面反射の鋭さ） ----
    static float shininess = 32.0f;
    ImGui::SliderFloat("Shininess", &shininess, 1.0f, 128.0f);

    ImGui::End();

    ImGui::Begin("Camera Debug");

    const Vector3& camPos = camera_->GetTranslate();
    const Vector3& camRot = camera_->GetRotate();

    ImGui::Text("Pos : %.2f  %.2f  %.2f", camPos.x, camPos.y, camPos.z);
    ImGui::Text("Rot(rad): %.3f  %.3f  %.3f", camRot.x, camRot.y, camRot.z);

    // 見やすいように度数も
    const float rad2deg = 180.0f / std::numbers::pi_v<float>;
    ImGui::Text("Rot(deg): %.1f  %.1f  %.1f",
        camRot.x * rad2deg, camRot.y * rad2deg, camRot.z * rad2deg);

    ImGui::Text("FovY     : %.3f", camera_->GetFovY());
    ImGui::Text("Near/Far : %.2f / %.2f", camera_->GetNearClip(), camera_->GetFarClip());

    ImGui::End();

    ImGui::Begin("Drone Tuning");
    ImGui::SliderAngle("Yaw Offset", &droneYawOffset); // -pi～+pi を度で触れる

    ImGui::Text("yaw(rad)=%.3f  yaw(deg)=%.1f",
        drone_.GetYaw(), drone_.GetYaw() * 180.0f / std::numbers::pi_v<float>);
    ImGui::Text("pos=%.2f %.2f %.2f", drone_.GetPos().x, drone_.GetPos().y, drone_.GetPos().z);

    ImGui::Text("nextGate=%d / %d", nextGate_, (int)gates_.size());
    ImGui::Text("Perfect=%d Good=%d", perfectCount_, goodCount_);

    // ===== ゲート番号（画面上にオーバーレイ表示）=====
    {
        // 画面サイズ（あなたのWinApp定数があるならそれを使う）
        // 例：WinApp::kClientWidth / kClientHeight がある想定
        const float W = (float)WinApp::kClientWidth;
        const float H = (float)WinApp::kClientHeight;

        // カメラの ViewProjection を取得（あなたのCameraにある想定）
        const Matrix4x4& vp = camera_->GetViewProjectionMatrix();

        auto* dl = ImGui::GetForegroundDrawList();

        for (int i = 0; i < (int)gates_.size(); ++i) {

            // 「ゲート中心」 = gate.pos（真ん中）
            ImVec2 p;
            if (!WorldToScreen_RowVector(gates_[i].gate.pos, vp, W, H, p)) {
                continue;
            }

            // 次のゲートだけ目立たせる
            const bool isNext = (i == nextGate_);
            const ImU32 col = isNext
                ? IM_COL32(255, 255, 0, 255)   // 黄色
                : IM_COL32(255, 255, 255, 200); // 白薄め

            // 文字を中心に寄せる（だいたい）
            const std::string s = std::to_string(i + 1);
            ImVec2 size = ImGui::CalcTextSize(s.c_str());

            // 少し上にずらす（ゲート中心に重なると見づらいので）
            p.y -= 12.0f;

            dl->AddText(ImVec2(p.x - size.x * 0.5f, p.y - size.y * 0.5f), col, s.c_str());
        }
    }


    ImGui::End();

    AddGate();

    EditWallsImGui();

    StageIOImGui();

    // 反映
    sphere_->SetEnableLighting(sphereLighting);
    sphere_->SetTranslate(spherePos);
    sphere_->SetRotate(sphereRotate);
    sphere_->SetScale(sphereScale);
    sphere_->SetShininess(shininess);

    if (drawWallDebug_) {
        wallSys_.UpdateDebug();
    }

}

void GamePlayScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());
    player2_->Draw();
    if (droneObj_) droneObj_->Draw();

    for (auto& g : gates_) {
        g.Draw();
    }

    if (drawWallDebug_) {
        wallSys_.DrawDebug();
    }

    sphere_->Draw(DirectXCommon::GetInstance()->GetCommandList());
    ParticleManager::GetInstance()->PreDraw();
    ParticleManager::GetInstance()->Draw();
}

void GamePlayScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();

   // sprite_->Draw();
}

void GamePlayScene::DrawImGui()
{
#ifdef USE_IMGUI

#endif
}

void GamePlayScene::Finalize()
{
    ParticleManager::GetInstance()->Finalize();

    LightManager::GetInstance()->Finalize();

    delete droneObj_;
    droneObj_ = nullptr;

    delete sprite_;
    sprite_ = nullptr;

    delete sphere_;
    sphere_ = nullptr;

    delete player2_;
    player2_ = nullptr;

    delete camera_;
    camera_ = nullptr;

    SoundManager::GetInstance()->SoundUnload(&bgm);
}

void GamePlayScene::AddGate()
{

    // ================================
// Gate Editor (Add/Remove/Reorder)
// ================================
    ImGui::Begin("Gate Editor");

    static int editGate = 0;
    const int gateCount = (int)gates_.size();

    if (gateCount == 0) {
        ImGui::Text("No gates. Add one!");
        if (ImGui::Button("Add Gate")) {
            GateVisual gv;
            gv.gate.pos = drone_.GetPos();          // いまのドローン付近に置く
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
        return; // gateCount==0 の時はここで終わり（下のUIが参照できないため）
    }

    // 範囲外にならないように
    editGate = std::clamp(editGate, 0, gateCount - 1);

    ImGui::Text("Gates: %d", gateCount);
    ImGui::SliderInt("Edit Gate Index", &editGate, 0, gateCount - 1);

    ImGui::Separator();

    // ---- Add / Duplicate / Remove ----
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
        // Gateデータだけコピーして、Visualは新規初期化
        Gate copy = gates_[editGate].gate;

        GateVisual gv;
        gv.gate = copy;
        gv.gate.pos.z += 3.0f; // 少しずらす（重なり防止）
        gv.Initialize(Object3dManager::GetInstance(), "cube.obj", camera_);
        gates_.insert(gates_.begin() + (editGate + 1), std::move(gv));
        editGate++;
        // nextGate_は「順番」なので、基本はそのまま（必要なら後で調整）
    }

    ImGui::SameLine();

    if (ImGui::Button("Remove")) {
        gates_.erase(gates_.begin() + editGate);

        // index補正
        if (editGate >= (int)gates_.size()) editGate = (int)gates_.size() - 1;
        if (editGate < 0) editGate = 0;

        // nextGate_補正（消した位置より後なら詰まる）
        if (nextGate_ > editGate) nextGate_--;
        nextGate_ = std::clamp(nextGate_, 0, (int)gates_.size());
    }

    ImGui::Separator();

    // ---- Reorder ----
    bool canUp = (editGate > 0);
    bool canDown = (editGate < (int)gates_.size() - 1);

    if (!canUp) ImGui::BeginDisabled();
    if (ImGui::Button("Up")) {
        std::swap(gates_[editGate], gates_[editGate - 1]);
        // nextGate_ がこの2つを指してた場合は追従させる
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

    // ---- Parameter edit ----
    Gate& g = gates_[editGate].gate;

    // ================================
// Mouse Place (Plane Y=0)
// ================================
    static bool placeMode = false;
    ImGui::Separator();
    ImGui::Checkbox("Mouse Place Mode (Y=0 Plane)", &placeMode);
    ImGui::Text("LClick: place selected gate on ground (y=0)");
    ImGui::Text("Selected Gate = %d", editGate);

    if (placeMode) {

        // ImGuiの上をクリックしてる時は無視（UI操作と衝突しない）
        if (!ImGui::GetIO().WantCaptureMouse) {

            // 左クリックで配置
            if (Input::GetInstance()->IsMouseTrigger(0)) {

                POINT mp = Input::GetInstance()->GetMousePos();

                const float W = (float)WinApp::kClientWidth;
                const float H = (float)WinApp::kClientHeight;

                const Matrix4x4& vp = camera_->GetViewProjectionMatrix();

                Vector3 hit{};
                if (ScreenRayToPlaneY0_RowVector(mp.x, mp.y, W, H, vp, hit)) {
                    // ★ここで「選択中ゲート」を地面に置く
                    gates_[editGate].gate.pos = hit;

                    // 置いた瞬間が分かりやすいようにちょいログ（任意）
                    // ImGui::Text("Placed at %.2f %.2f %.2f", hit.x, hit.y, hit.z);
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
        perfectCount_ = 0;
        goodCount_ = 0;
    }

    if (ImGui::Button("Add Gate (After)")) {
        GateVisual gv;

        const int insertIndex = editGate + 1;

        // 今のゲートをベースにする
        gv.gate = gates_[editGate].gate;

        // 少し前にずらす（重なり防止）※回転しててもとりあえずZでOK
        gv.gate.pos.z += 3.0f;

        gv.Initialize(Object3dManager::GetInstance(), "cube.obj", camera_);

        gates_.insert(gates_.begin() + insertIndex, std::move(gv));

        // nextGate_補正：挿入位置より後ろを指してたら1つずらす
        if (nextGate_ >= insertIndex) {
            nextGate_++;
        }

        editGate = insertIndex; // 追加したゲートを選択状態に
    }

    ImGui::End();

}

void GamePlayScene::EditWallsImGui()
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
        }
        ImGui::End();
        return;
    }

    editWall = std::clamp(editWall, 0, wallCount - 1);
    ImGui::SliderInt("Edit Wall Index", &editWall, 0, wallCount - 1);

    ImGui::Separator();

    // -------------------------
    // Add / Duplicate / Remove
    // -------------------------
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
        copy.center.z += 2.0f; // 少しずらす
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

    // -------------------------
    // Reorder
    // -------------------------
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

    // -------------------------
    // Edit params
    // -------------------------
    WallSystem::Wall& w = walls[editWall];

    int typeInt = (w.type == WallSystem::Type::AABB) ? 0 : 1;
    if (ImGui::RadioButton("AABB", typeInt == 0)) typeInt = 0;
    ImGui::SameLine();
    if (ImGui::RadioButton("OBB", typeInt == 1)) typeInt = 1;
    w.type = (typeInt == 0) ? WallSystem::Type::AABB : WallSystem::Type::OBB;

    // マウス配置（y=0平面）
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

                    // 1) 選択中を配置
                    w.center = hit;

                    // 2) 置いたら次を自動追加
                    if (autoAddOnPlace) {
                        WallSystem::Wall next = w;       // 型・half・rot を引き継ぐ
                        next.center.z += autoAddZStep;   // ちょい前にずらす（重なり防止）

                        walls.insert(walls.begin() + (editWall + 1), next);
                        editWall++; // 追加した壁を次の編集対象にする

                        wallSys_.BuildDebug(Object3dManager::GetInstance(), "cube.obj");

                    }
                }

            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Transform");
    ImGui::DragFloat3("Center", &w.center.x, 0.1f);

    // ★壁は half が当たり判定そのもの
    ImGui::DragFloat3("Half Size", &w.half.x, 0.1f, 0.05f, 100.0f);

    if (w.type == WallSystem::Type::OBB) {
        ImGui::DragFloat3("Rotation (rad)", &w.rot.x, 0.02f);
    } else {
        // AABBにした瞬間回転を無効化したいなら
        w.rot = { 0,0,0 };
    }

    ImGui::End();

    wallSys_.SetSelectedIndex(editWall);

    // 変更後はデバッグ表示更新
    // ※あなたは Update()の最後で wallSys_.UpdateDebug() してるので、
    //   ここで呼ばなくても良い。確実にしたいなら呼んでもOK。
}


bool GamePlayScene::SaveStageJson(const std::string& fileName)
{
    // fileName: "stage01.json" みたいに ImGui で入力したやつ
    // 保存先：resources/stage/
    const std::string path = "resources/stage/" + fileName;

    json root;
    root["version"] = 1;

    // ---- drone spawn ----
    root["drone"]["spawnPos"] = ToJsonVec3(drone_.GetPos());
    root["drone"]["spawnYaw"] = drone_.GetYaw();

    // ---- gates ----
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

    // ---- walls ----
    {
        json arr = json::array();
        for (const auto& w : wallSys_.Walls()) {
            json j;
            j["type"] = (w.type == WallSystem::Type::AABB) ? "AABB" : "OBB";
            j["center"] = ToJsonVec3(w.center);
            j["half"] = ToJsonVec3(w.half);
            j["rot"] = ToJsonVec3(w.rot); // AABBでも入れてOK（0,0,0）
            arr.push_back(j);
        }
        root["walls"] = arr;
    }

    std::ofstream ofs(path);
    if (!ofs.is_open()) return false;

    ofs << root.dump(2); // 2=見やすいインデント
    return true;
}

bool GamePlayScene::LoadStageJson(const std::string& fileName)
{
    const std::string path = "resources/stage/" + fileName;

    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;

    json root;
    ifs >> root;

    // ---- drone spawn ----
    if (root.contains("drone")) {
        const auto& d = root["drone"];
        if (d.contains("spawnPos")) {
            drone_.SetPos(FromJsonVec3(d["spawnPos"]));
        }
        if (d.contains("spawnYaw")) {
            // yaw_ 直接代入の setter がないなら SetYaw を用意するのがベスト
            // とりあえず Drone に SetYaw(float) を追加してね
            drone_.SetYaw(d["spawnYaw"].get<float>());
        }
        // 速度リセット（ロード直後に暴れないため）
        drone_.SetVel({ 0,0,0 });
    }

    // ---- gates ----
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

            // ★Visualはロード時に再生成
            gv.Initialize(Object3dManager::GetInstance(), "cube.obj", camera_);
            gates_.push_back(std::move(gv));
        }
    }

    // ---- walls ----
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

    // 状態リセット（ゲーム進行用）
    nextGate_ = 0;
    perfectCount_ = 0;
    goodCount_ = 0;

    // デバッグ表示は walls_ 個数が変わるので更新が必要
    // あなたの BuildDebug は dirtyDebug_ を立てて再生成する設計なので
    // ここで dirtyDebug_ を立てたいが private なので、手っ取り早いのは BuildDebug 呼び直し
    wallSys_.BuildDebug(Object3dManager::GetInstance(), "cube.obj");

    return true;
}

void GamePlayScene::StageIOImGui()
{
    ImGui::Begin("Stage IO");

    static char fileName[128] = "stage01.json";
    ImGui::InputText("File", fileName, IM_ARRAYSIZE(fileName));

    // 拡張子付け忘れ防止（任意）
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
