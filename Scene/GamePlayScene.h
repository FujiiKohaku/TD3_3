#pragma once
#include "BaseScene.h"
#include "Camera.h"
#include "ModelManager.h"
#include "Object3d.h"
#include "Object3dManager.h"
#include "ParticleEmitter.h"
#include "ParticleManager.h"
#include "SoundManager.h"
#include "Sprite.h"
#include "SpriteManager.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "../Game/Stage/StageIO.h"
#include "StageSelectScene.h"

//ゲームプレイ用
#include "Input.h"
#include "../Game/Drone/Drone.h"
#include "../Game/Gate/Gate.h"
#include "../Game/Gate/GateVisual.h"
#include "../Game/Drone/Walls.h"
#include "../Game/Goal/GoalSystem.h"
#include"../Game/LandingEffect/LandingEffect.h"
#include "../Game/Particle/ParticleGate.h"
#include "BitmapFont.h"
class SphereObject;
class GamePlayScene : public BaseScene {
public:
	void Initialize() override;

	void Finalize() override;

	void Update() override;

	void Draw2D() override;
	void Draw3D() override;
	void DrawImGui() override;

	/*  void AddGate();

	  void EditWallsImGui();

	  bool LoadStageJson(const std::string& fileName);

	  bool SaveStageJson(const std::string& fileName);

	  void StageIOImGui();*/


private:
	// ------------------------------
	// サウンド
	// ------------------------------
	SoundData bgm;
	Sprite* sprite_ = nullptr;
	std::vector<Sprite*> sprites_;
	Object3d* player2_;

	ParticleEmitter emitter_;

	// ------------------------------
	// メッシュ
	// ------------------------------
	SphereObject* sphere_ = nullptr;

	Camera* camera_;

	bool sphereLighting = true;
	Vector3 spherePos = { 0.0f, 0.0f, 0.0f };
	Vector3 sphereRotate = { 0.0f, 0.0f, 0.0f }; // ラジアン想定
	Vector3 sphereScale = { 1.0f, 1.0f, 1.0f };
	float lightIntensity = 1.0f;
	Vector3 lightDir = { 0.0f, -1.0f, 0.0f };

	// --- Drone ---
	Object3d* droneObj_ = nullptr;
	Drone drone_;

	// --- 追従カメラ（後ろから見る） ---
	float camDist_ = 8.0f;
	float camHeight_ = 3.0f;
	float camPitch_ = -0.20f;

	int isDebug_ = false;

	float droneYawOffset = 0.0f; // rad

	//ゲート(リング)

	std::vector<GateVisual> gates_;
	int nextGate_ = 0;
	int perfectCount_ = 0;
	int goodCount_ = 0;

	BitmapFont gateNum_;  // ゲート番号描画用

	void DrawGateIndices2D_();

	//壁
	WallSystem wallSys_;
	Vector3 droneHalf_ = { 0.1f, 0.1f, 0.1f }; // ドローン当たり判定（半サイズ）
	bool drawWallDebug_ = true;

	//ゴール
	GoalSystem goalSys_;
	bool stageCleared_ = false; // クリア後の演出用

	bool requestBackToSelect_ = false;

	//天球
	std::unique_ptr<Object3d> skydome_ = nullptr;
	//地面
	std::unique_ptr<Object3d> ground_ = nullptr;

	LandingEffect landingEffect_;
	ParticleGate particleGate_;
	//gateのフラグ
	bool isPaused_ = false;
	bool requestBackToTitle_ = false;
	void	UpdateDronePointLight();

	//コンパス表示
	Sprite* compassA_ = nullptr;
	Sprite* compassB_ = nullptr;
	// マーカーは後で
	// Sprite* compassMarker_ = nullptr;

	std::string compassPath_ = "resources/ui/compass_strip.png";
	Vector2 compassPos_ = { 640.0f, 40.0f };
	Vector2 compassSize_ = { 520.0f, 48.0f };
	bool compassInit_ = false;
	bool compassDrawB_ = false;

	void InitCompass_();
	void UpdateCompass_();
//	void DrawCompass_();

	//高さ表示

	bool altInit_ = false;

	Sprite altBarBg_;      // 背景
	Sprite altBarFill_;    // 中のバー（任意）
	Sprite altTick_;       // 目盛り線用（白1px画像とか推奨。無ければuvCheckerでも一旦OK）
	Sprite altPointer_;    // 中央ポインタ（任意。無ければ細い線でもOK）

	BitmapFont font_;      // ←あなたのBitmapFont

	std::string altTickTex_ = "resources/white.png";     // 1x1白PNG推奨
	std::string altFontTex_ = "resources/ui/ascii_font_16x6_cell32_first32.png";      // ビットマップフォント
	Vector2 altPos_ = { 50.0f, 240.0f };                // 左上
	Vector2 altSize_ = { 120.0f, 220.0f };               // 全体サイズ

	float altHalfRange_ = 20.0f;   // 現在高度±20を表示
	float altStep_ = 10.0f;    // 5刻みで数字
	float altMinorStep_ = 1.0f;    // 細かい目盛り

	void InitAltimeter_();
	void DrawAltimeter_();
	void DrawSpeedSimple_();

	//マーカー描画
	// --- Compass marker ---
	Sprite* compassMarker_ = nullptr;
	std::string compassMarkerPath_ = "resources/ui/marker.png"; // 作ったPNG
	Vector2 compassMarkerPos_ = { 100.0f, 32.0f };  // コンパス中心と同じあたり
	Vector2 compassMarkerSize_ = { 18.0f, 18.0f };   // 好きに調整

	Sprite* altMarker_ = nullptr;
	std::string altMarkerPath_ = "resources/ui/marker.png"; // 既にあるなら同じでOK
	Vector2 altMarkerSize_ = { 16.0f, 16.0f };              // 調整
	float altMarkerOffsetX_ = 6.0f;                         // バーからの距離


};