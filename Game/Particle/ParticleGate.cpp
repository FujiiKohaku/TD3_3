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

	const int pieceCount = 16;
	pieces_.resize(pieceCount);

	ModelManager::GetInstance()->LoadModel("star.obj");

	for (Piece& piece : pieces_) {
		piece.obj = new Object3d();
		piece.obj->Initialize(objManager_);
		piece.obj->SetModel("star.obj");
		piece.obj->SetCamera(camera_);
		piece.phase = Phase::None;
		piece.life = 0.0f;
		piece.scale = { 0.05f, 0.05f, 0.05f };
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

		piece.phase = Phase::Ring;
		piece.life = ringLife_;
		piece.scale = { 0.06f, 0.06f, 0.06f };

		Vector3 startPos = centerPos;
		startPos.y += yOffset_;

		piece.obj->SetTranslate(startPos);
		piece.obj->SetScale(piece.scale);

		float angle = (2.0f * std::numbers::pi_v<float> / pieceCount) * static_cast<float>(i);
		piece.velocity.x = std::cosf(angle) * ringSpeed_;
		piece.velocity.z = std::sinf(angle) * ringSpeed_;
		piece.velocity.y = 0.0f;

		piece.obj->Update();
	}
}

void ParticleGate::StartBurst(Piece& piece, int index, int count)
{
	piece.phase = Phase::Burst;
	piece.life = burstLife_;

	float angle = (2.0f * std::numbers::pi_v<float> / count) * static_cast<float>(index);

	float spread = 0.6f + 0.4f * std::sinf(static_cast<float>(index) * 1.7f);
	float horizontalSpeed = burstSpeed_ * spread;

	piece.velocity.x = std::cosf(angle) * horizontalSpeed;
	piece.velocity.z = std::sinf(angle) * horizontalSpeed;

	float up = 2.0f + 1.5f * std::cosf(static_cast<float>(index) * 2.3f);
	piece.velocity.y = up;
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

		if (piece.phase == Phase::Ring) {
			pos.y = centerPos_.y + yOffset_;

			piece.scale.x -= ringShrink_;
			piece.scale.y -= ringShrink_;
			piece.scale.z -= ringShrink_;

			if (piece.life - dt <= 0.0f) {
				StartBurst(piece, i, pieceCount);
			}
		}
		else if (piece.phase == Phase::Burst) {
			piece.velocity.y -= 9.8f * dt;

			piece.scale.x -= burstShrink_;
			piece.scale.y -= burstShrink_;
			piece.scale.z -= burstShrink_;
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
