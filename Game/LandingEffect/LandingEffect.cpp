#include "LandingEffect.h"
#include "Object3d.h"
#include "Object3dManager.h"
#include "Camera.h"
#include <cmath>
#include <algorithm>
#include <numbers>
LandingEffect::LandingEffect() {
}

LandingEffect::~LandingEffect() {
	for (Piece& p : pieces_) {
		delete p.obj;
		p.obj = nullptr;
	}
	pieces_.clear();
}

void LandingEffect::Initialize(Object3dManager* objManager, Camera* camera) {
	objManager_ = objManager;
	camera_ = camera;

	const int pieceCount = 8;
	pieces_.resize(pieceCount);

	for (Piece& p : pieces_) {
		p.obj = new Object3d();
		p.obj->Initialize(objManager_);
		p.obj->SetModel("cube.obj");
		p.obj->SetCamera(camera_);
	}
}

void LandingEffect::Play(const Vector3& pos) {
	if (isPlaying_) {//isPlayingがtrueだと入れない
		return;
	}

	isPlaying_ = true;//isPlayingをtrueに

	const float speed = 3.0f;
	const float upSpeed = 2.0f;
	const float lifeTime = 0.5f;

	const int pieceCount = static_cast<int>(pieces_.size());

	for (int i = 0; i < pieceCount; i++) {
		Piece& piece = pieces_[i];

		piece.scale = { 0.05f, 0.05f, 0.05f };
	

		Vector3 start = pos;
		start.y += yOffset_;
		piece.obj->SetTranslate(start);
		piece.obj->SetScale(piece.scale);

		float angle = (2.0f * std::numbers::pi_v<float> / pieceCount) * i;
		piece.velocity.x = std::cosf(angle) * speed;
		piece.velocity.z = std::sinf(angle) * speed;
		piece.velocity.y = upSpeed;

		piece.life = lifeTime;

	}
}

void LandingEffect::Update(float dt) {
	const float gravity = 9.8f;
	bool anyAlive = false;//ピースが生きているか否か

	for (Piece& p : pieces_) {
		if (p.life <= 0.0f) {//lifeが0以下ならスキップ全部チェック
			continue;
		}

		anyAlive = true;//もしピースが一個でも生きていたらtrue

		p.velocity.y -= gravity * dt;

		Vector3 pos = p.obj->GetTranslate();
		pos.x += p.velocity.x * dt;
		pos.y += p.velocity.y * dt;
		pos.z += p.velocity.z * dt;


		p.scale.x -= 0.002f;
		p.scale.y -= 0.002f;
		p.scale.z -= 0.002f;
		if (p.scale.x < 0.0f) p.scale.x = 0.0f;
		if (p.scale.y < 0.0f) p.scale.y = 0.0f;
		if (p.scale.z < 0.0f) p.scale.z = 0.0f;
		p.obj->SetTranslate(pos);
		p.obj->SetScale(p.scale);
		p.obj->Update();


		p.life -= dt;
	}

	if (!anyAlive) {
		isPlaying_ = false;
	}
}

void LandingEffect::Draw() {
	for (auto& p : pieces_) {
		if (p.life > 0.0f) {
			p.obj->Draw();
		}
	}
}
