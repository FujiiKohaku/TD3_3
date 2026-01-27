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
	return json{ { "x", v.x }, { "y", v.y }, { "z", v.z } };
}
static inline Vector3 FromJsonVec3(const json& j) {
	return Vector3{ j.at("x").get<float>(), j.at("y").get<float>(), j.at("z").get<float>() };
}

// ===== World -> Screen (row-vector行列想定) =====
static Vector4 MulRowVec4Mat4(const Vector4& v, const Matrix4x4& m) {
	Vector4 o{};
	o.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + v.w * m.m[3][0];
	o.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + v.w * m.m[3][1];
	o.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + v.w * m.m[3][2];
	o.w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + v.w * m.m[3][3];
	return o;
}

// ===== RowVec * Mat4 で w除算して Vector3 にする =====
static Vector3 TransformCoord_RowVector4(const Vector4& v, const Matrix4x4& m) {
	Vector4 o = MulRowVec4Mat4(v, m);
	if (std::abs(o.w) > 1e-6f) {
		o.x /= o.w;
		o.y /= o.w;
		o.z /= o.w;
	}
	return { o.x, o.y, o.z };
}

// ===== Screen -> World のレイ（y=0平面に当てる用）=====
// D3DのNDC: x,y = [-1..1], z = [0..1] 想定
static bool ScreenRayToPlaneY0_RowVector(
	int mouseX, int mouseY,
	float screenW, float screenH,
	const Matrix4x4& viewProj,
	Vector3& outHit) {
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
	if (len < 1e-6f)
		return false;
	dir.x /= len;
	dir.y /= len;
	dir.z /= len;

	// 平面 y=0 と交差
	if (std::abs(dir.y) < 1e-6f)
		return false; // 平面と平行
	float t = (0.0f - pNear.y) / dir.y;
	if (t <= 0.0f)
		return false; // カメラの後ろ側

	outHit = { pNear.x + dir.x * t, 0.0f, pNear.z + dir.z * t };
	return true;
}

static bool WorldToScreen_RowVector(
	const Vector3& worldPos,
	const Matrix4x4& viewProj,
	float screenW, float screenH,
	ImVec2& outScreen) {
	Vector4 clip = MulRowVec4Mat4({ worldPos.x, worldPos.y, worldPos.z, 1.0f }, viewProj);

	// カメラ後ろ(またはwが小さい)は描かない
	if (clip.w <= 1e-6f)
		return false;

	// NDC化
	const float ndcX = clip.x / clip.w;
	const float ndcY = clip.y / clip.w;

	// 画面座標へ（左上原点）
	outScreen.x = (ndcX * 0.5f + 0.5f) * screenW;
	outScreen.y = (-ndcY * 0.5f + 0.5f) * screenH;

	return true;
}

void GamePlayScene::Initialize() {
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
	// player2_->SetModel("terrain.obj");
	player2_->SetTranslate({ 3.0f, 0.0f, 0.0f });
	// player2_->SetRotate({ std::numbers::pi_v<float> / 2.0f, std::numbers::pi_v<float>, 0.0f });

	ParticleManager::GetInstance()->CreateParticleGroup("circle", "resources/circle.png");
	Transform t{};
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

	// 存在するモデル名にしてね（無ければ cube.obj とか）
	//  droneObj_->SetModel("cube.obj");
	droneObj_->SetModel("cube.obj"); // ←まずこれで見えるかテスト

	droneObj_->SetScale({ 0.1f, 0.1f, 0.1f });

	// ---- Stage Load (from StageSelect) ----
	StageData stage{};
	{
		const std::string& fileUtf8 = SceneManager::GetInstance()->GetSelectedStageFile();

		const bool ok = StageIO::Load(fileUtf8, stage);
		if (!ok) {
			// ここで fall back したいなら、今までのハードコード配置にする
			// ひとまず「最低限」置いておく
			stage = StageData{};
			stage.gates.clear();
			// 例：最低限1つゲート置く
			Gate g{};
			g.pos = { 0, 2, 5 };
			g.rot = { 0, 0, 0 };
			g.scale = { 2, 2, 2 };
			g.perfectRadius = 1.0f;
			g.gateRadius = 2.5f;
			g.thickness = 0.8f;
			stage.gates.push_back(g);

			stage.droneSpawnPos = { 0, 1, 0 };
			stage.droneSpawnYaw = 0.0f;
			stage.hasGoalPos = false;
			stage.hasGoalSpawnOffset = false;
		}
	}



	drone_.Initialize(stage.droneSpawnPos);

	// yaw も使うなら（Drone に SetYaw がある想定。無ければ Initialize に含めるか、メンバへ直接）
	drone_.SetYaw(stage.droneSpawnYaw); // 無いならコメントアウト

	droneObj_->SetTranslate(stage.droneSpawnPos);

	// ---- gates build ----
	gates_.clear();
	gates_.resize((int)stage.gates.size());

	for (int i = 0; i < (int)stage.gates.size(); ++i) {

		// StageData::gates は Gate 型（あなたの StageIO と同じ Gate）
		// ただし Visual 側 gates_[i].gate に代入する
		gates_[i].gate = stage.gates[i];

		// 見た目モデル（今は cube 固定でOK）
		gates_[i].Initialize(Object3dManager::GetInstance(), "Gate.obj", camera_);
	}

	nextGate_ = 0;
	perfectCount_ = 0;
	goodCount_ = 0;

	// ---- walls build ----
	wallSys_.Clear(); // ★無いなら追加（後述）
	wallSys_.BuildDebug(Object3dManager::GetInstance(), "cube.obj");

	for (const auto& w : stage.walls) {
		if (w.type == WallSystem::Type::AABB) {
			wallSys_.AddAABB(w.center, w.half);
		}
		else {
			wallSys_.AddOBB(w.center, w.half, w.rot);
		}
	}

	goalSys_.Initialize(Object3dManager::GetInstance(), camera_);
	goalSys_.Reset();
	stageCleared_ = false;

	if (stage.hasGoalPos) {
		goalSys_.SetFixedGoalPos(stage.goalPos);
	}
	else {
		goalSys_.ClearFixedGoalPos();
	}

	ModelManager::GetInstance()->LoadModel("skydome.obj");
	TextureManager::GetInstance()->LoadTexture("resources/skydome.png");
	skydome_ = std::make_unique<Object3d>();
	skydome_->Initialize(Object3dManager::GetInstance());
	skydome_->SetModel("skydome.obj");
	skydome_->SetCamera(camera_);
	skydome_->SetEnableLighting(false);

	landingEffect_.Initialize(Object3dManager::GetInstance(),camera_);
}

void GamePlayScene::Update() {

	float dt = 1.0f / 60.0f;



	Input& input = *Input::GetInstance();

	// ドローン更新（※これが無いとカメラも動かない）
	if (isDebug_) {
		drone_.UpdateDebugNoInertia(input, dt);
	}
	else {
		drone_.UpdateMode1(input, dt);
	}
	{
		Vector3 pos = drone_.GetPos();
		Vector3 vel = drone_.GetVel();

		wallSys_.ResolveDroneAABB(pos, vel, droneHalf_, dt, 4);

		drone_.SetPos(pos);
		drone_.SetVel(vel);
	}

	if (drone_.HasJustLanded()) {
		landingEffect_.Play(drone_.GetPos());

	}
	landingEffect_.Update(dt);

	// ドローン実体 → 描画Object3dへ反映（毎フレーム必須）
	if (droneObj_) {
		droneObj_->SetTranslate(drone_.GetPos());
		droneObj_->SetRotate({ drone_.GetPitch(), drone_.GetYaw() + droneYawOffset, drone_.GetRoll() });
		droneObj_->Update();
	}

	// これを毎フレーム呼ぶ
	camera_->FollowDroneRigid(drone_, 7.5f, 1.8f, -0.18f, droneYawOffset);



	// 更新系
	emitter_.Update();
	ParticleManager::GetInstance()->Update();
	player2_->Update();
	sprite_->Update();
	sphere_->Update(camera_);
	droneObj_->Update();
	skydome_->Update();

	// ★最後に一回

	if (input.IsKeyTrigger(DIK_O)) {

		isDebug_ = !isDebug_;
	}

	//  camera_->DebugUpdate();


	camera_->Update();

	// ゲート

	// 1) 全ゲートの見た目更新（色タイマーもここで進む）
	for (auto& g : gates_) {
		g.Tick(dt);
	}

	// 2) 次ゲートだけ判定
	if (nextGate_ < (int)gates_.size()) {
		GateResult res;
		const Vector3 dronePos = drone_.GetPos(); // ★ここはあなたのドローン取得に合わせる

		if (gates_[nextGate_].TryPass(dronePos, res)) {
			if (res == GateResult::Perfect) {
				perfectCount_++;
				nextGate_++;
			}
			else if (res == GateResult::Good) {
				goodCount_++;
				nextGate_++;
			}
			else {
				// Miss：進まない（色は赤になる）
			}
		}
	}
	else {
		// ---- GoalSystem update ----
		goalSys_.Update(gates_, nextGate_, drone_.GetPos());

		if (goalSys_.IsCleared()) {
			stageCleared_ = true;

			// ここで「リザルトへ遷移」「SE」「フェード」等を入れる
			// 例：次シーンへ
			// sceneManager_->ChangeScene(new ResultScene());
		}
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
	LightManager::GetInstance()->SetPointLight(pointColor, pointPos, pI);

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

	ImGui::Text("yaw(rad)=%.3f  yaw(deg)=%.1f", drone_.GetYaw(), drone_.GetYaw() * 180.0f / std::numbers::pi_v<float>);
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
				? IM_COL32(255, 255, 0, 255) // 黄色
				: IM_COL32(255, 255, 255, 200); // 白薄め

			// 文字を中心に寄せる（だいたい）
			const std::string s = std::to_string(i + 1);
			ImVec2 size = ImGui::CalcTextSize(s.c_str());

			// 少し上にずらす（ゲート中心に重なると見づらいので）
			p.y -= 12.0f;

			dl->AddText(ImVec2(p.x - size.x * 0.5f, p.y - size.y * 0.5f), col, s.c_str());
		}
	}

	// ================================
	// GOAL overlay (ImGui)
	// ================================
	if (stageCleared_) {

		// Enterで戻る（トリガー）
		if (stageCleared_ && input.IsKeyTrigger(DIK_RETURN)) {
			requestBackToSelect_ = true;
		}

		// 画面中央に出す
		ImGuiIO& io = ImGui::GetIO();
		const float W = io.DisplaySize.x;
		const float H = io.DisplaySize.y;

		const char* msg = "GOAL!!";
		ImVec2 textSize = ImGui::CalcTextSize(msg);

		// 少し上に出す
		ImVec2 pos((W - textSize.x) * 0.5f, (H * 0.35f) - textSize.y * 0.5f);

		auto* dl = ImGui::GetForegroundDrawList();

		// 影（見やすく）
		dl->AddText(ImVec2(pos.x + 2, pos.y + 2), IM_COL32(0, 0, 0, 200), msg);

		// 本体
		dl->AddText(pos, IM_COL32(255, 255, 0, 255), msg);

		// ついでに小さく案内（任意）
		const char* sub = "Press Enter to continue";
		ImVec2 subSize = ImGui::CalcTextSize(sub);
		ImVec2 subPos((W - subSize.x) * 0.5f, pos.y + 40.0f);
		dl->AddText(ImVec2(subPos.x + 1, subPos.y + 1), IM_COL32(0, 0, 0, 180), sub);
		dl->AddText(subPos, IM_COL32(255, 255, 255, 230), sub);
	}

	ImGui::Begin("Gate Debug");

	if (nextGate_ < (int)gates_.size()) {
		const Gate& g = gates_[nextGate_].gate;

		ImGui::Text("=== Next Gate ===");
		ImGui::Text("Local Pos : x=%.2f y=%.2f z=%.2f",
			g.dbgLocalPos.x, g.dbgLocalPos.y, g.dbgLocalPos.z);

		ImGui::Text("PrevZ     : %.2f", g.dbgPrevZ);

		ImGui::Separator();

		ImGui::Text("Crossed   : %s", g.dbgCrossed ? "YES" : "NO");
		ImGui::Text("Thickness : %s", g.dbgInThickness ? "IN" : "OUT");

		ImGui::Text("Radius    : %.2f", g.dbgRadius);
		ImGui::Text("Perfect R : %.2f", g.perfectRadius);
		ImGui::Text("Good R    : %.2f", g.gateRadius);

		if (g.dbgRadius <= g.perfectRadius)
			ImGui::TextColored(ImVec4(0, 1, 1, 1), "=> PERFECT ZONE");
		else if (g.dbgRadius <= g.gateRadius)
			ImGui::TextColored(ImVec4(0, 1, 0, 1), "=> GOOD ZONE");
		else
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "=> MISS ZONE");
	}

	ImGui::End();

	ImGui::End();

	// 反映
	sphere_->SetEnableLighting(sphereLighting);
	sphere_->SetTranslate(spherePos);
	sphere_->SetRotate(sphereRotate);
	sphere_->SetScale(sphereScale);
	sphere_->SetShininess(shininess);

	if (drawWallDebug_) {
		wallSys_.UpdateDebug();
	}

	if (requestBackToSelect_) {
		requestBackToSelect_ = false;
		SceneManager::GetInstance()->SetNextScene(new StageSelectScene());
		// ★ここでは return してOK（もうImGuiは全部閉じた後だから）
		return;
	}

}

void GamePlayScene::Draw3D() {
	Object3dManager::GetInstance()->PreDraw();
	LightManager::GetInstance()->Bind(DirectXCommon::GetInstance()->GetCommandList());
	//	player2_->Draw();
	if (droneObj_) droneObj_->Draw();
	if (skydome_) skydome_->Draw();

	for (auto& g : gates_) {
		g.Draw();
	}

	goalSys_.Draw();

	if (drawWallDebug_) {
		wallSys_.DrawDebug();
	}
	landingEffect_.Draw();
	//sphere_->Draw(DirectXCommon::GetInstance()->GetCommandList());
	ParticleManager::GetInstance()->PreDraw();
	ParticleManager::GetInstance()->Draw();
}

void GamePlayScene::Draw2D() {
	SpriteManager::GetInstance()->PreDraw();

	// sprite_->Draw();
}

void GamePlayScene::DrawImGui() {
#ifdef USE_IMGUI

#endif
}

void GamePlayScene::Finalize() {
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

	goalSys_.Finalize();

	SoundManager::GetInstance()->SoundUnload(&bgm);
}
