#include "ParticleGate.h"
#include "Object3d.h"
#include "Object3dManager.h"
#include "Camera.h"
#include <numbers>
#include <cmath>
#include"ModelManager.h"
ParticleGate::ParticleGate()
{
}

ParticleGate::~ParticleGate()
{
	for (Piece& piece : pieces_) {
		delete piece.obj;
		piece.obj = nullptr;
	}
	pieces_.clear();
}

void ParticleGate::Initialize(Object3dManager* objManager, Camera* camera)
{
	objManager_ = objManager;
	camera_ = camera;

	const int pieceCount = 32;
	pieces_.resize(pieceCount);

	ModelManager::GetInstance()->LoadModel("star.obj");

	for (Piece& piece : pieces_) {
		piece.obj = new Object3d();
		piece.obj->Initialize(objManager_);
		piece.obj->SetModel("star.obj");
		piece.obj->SetCamera(camera_);
		piece.phase = Phase::None;
		piece.life = 0.0f;
		piece.baseColor.x = RandomFloat(0.6f, 1.0f);
		piece.baseColor.y = RandomFloat(0.6f, 1.0f);
		piece.baseColor.z = RandomFloat(0.6f, 1.0f);
		piece.baseColor.w = 1.0f;
		piece.startScale = { 0.08f, 0.08f, 0.08f };
		piece.scale = piece.startScale;
		piece.obj->SetColor(piece.baseColor);
	}
}

void ParticleGate::Play(const Vector3& centerPos)
{
	if (isPlaying_) {
		return;
	}

	isPlaying_ = true;
	centerPos_ = centerPos;

	StartRing(centerPos_);
}

void ParticleGate::StartRing(const Vector3& centerPos)
{
	const int pieceCount = static_cast<int>(pieces_.size());
	for (int i = 0; i < pieceCount; i++) {
		Piece& piece = pieces_[i];

		piece.phase = Phase::Burst;
		piece.life = burstLife_;

		Vector3 startPos = centerPos_;
		startPos.y += yOffset_;
		piece.obj->SetTranslate(startPos);

		StartBurst(piece);   
		piece.obj->SetScale(piece.startScale);
		piece.obj->Update();
	}

}

void ParticleGate::StartBurst(Piece& piece)
{
	piece.phase = Phase::Burst;
	piece.life = burstLife_;
	if (piece.phase == Phase::Burst) {

		float flicker = RandomFloat(-0.15f, 0.15f);

		Vector4 color = piece.baseColor;
		color.x = std::clamp(color.x + flicker, 0.0f, 1.0f);
		color.y = std::clamp(color.y + flicker, 0.0f, 1.0f);
		color.z = std::clamp(color.z + flicker, 0.0f, 1.0f);
		float intensity = RandomFloat(0.7f, 1.3f);
		color.x = piece.baseColor.x * intensity;
		color.y = piece.baseColor.y * intensity;
		color.z = piece.baseColor.z * intensity;
		color.w = 1.0f;

		
		piece.obj->SetColor(color);
	}
	float theta = RandomFloat(0.0f, 2.0f * std::numbers::pi_v<float>);
	float phi = RandomFloat(0.0f, std::numbers::pi_v<float>);

	Vector3 dir;
	dir.x = std::sinf(phi) * std::cosf(theta);
	dir.y = std::cosf(phi);
	dir.z = std::sinf(phi) * std::sinf(theta);

	dir = Normalize(dir);

	piece.velocity.x = dir.x * burstSpeed_;
	piece.velocity.y = dir.y * burstSpeed_;
	piece.velocity.z = dir.z * burstSpeed_;

	piece.scale = { 0.08f, 0.08f, 0.08f };
}

float ParticleGate::RandomFloat(float min, float max)
{
	float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	return min + (max - min) * r;
}

void ParticleGate::Update(float dt)
{
	bool anyAlive = false;
	const int pieceCount = static_cast<int>(pieces_.size());

	for (int i = 0; i < pieceCount; i++) {
		Piece& piece = pieces_[i];

		if (piece.life <= 0.0f) {
			continue;
		}

		anyAlive = true;

		Vector3 pos = piece.obj->GetTranslate();

		pos.x += piece.velocity.x * dt;
		pos.y += piece.velocity.y * dt;
		pos.z += piece.velocity.z * dt;

		
		if (piece.phase == Phase::Burst) {

			piece.velocity.y -= 9.8f * dt;

			float t = piece.life / burstLife_; 
			if (t < 0.0f) t = 0.0f;

			piece.scale.x = piece.startScale.x * t;
			piece.scale.y = piece.startScale.y * t;
			piece.scale.z = piece.startScale.z * t;
		}


		if (piece.scale.x < 0.0f) piece.scale.x = 0.0f;
		if (piece.scale.y < 0.0f) piece.scale.y = 0.0f;
		if (piece.scale.z < 0.0f) piece.scale.z = 0.0f;

		piece.obj->SetTranslate(pos);
		piece.obj->SetScale(piece.scale);
		piece.obj->Update();

		piece.life -= dt;
	}

	if (!anyAlive) {
		isPlaying_ = false;
	}
}

void ParticleGate::Draw()
{
	for (Piece& piece : pieces_) {
		if (piece.life > 0.0f) {
			piece.obj->Draw();
		}
	}
}
