#pragma once
#include "MathStruct.h"
#include <vector>
class Object3d;
class Object3dManager;
class Camera;

class ParticleGate
{
public:
	ParticleGate();
	~ParticleGate();

	void Initialize(Object3dManager* objManager, Camera* camera);
	void Play(const Vector3& centerPos);

	void Update(float dt);
	void Draw();

private:
	enum class Phase {
		None,
		Burst
	};

	struct Piece {
		Object3d* obj = nullptr;
		Vector3 velocity{};
		Vector3 scale;
		float life;
		Phase phase;
		Vector3 startScale;
		Vector4 baseColor;
	};


	void StartRing(const Vector3& centerPos);
	void StartBurst(Piece& piece);
	float RandomFloat(float min, float max);
private:
	std::vector<Piece> pieces_;

	Object3dManager* objManager_ = nullptr;
	Camera* camera_ = nullptr;

	bool isPlaying_ = false;
	Vector3 centerPos_{};

	float yOffset_ = 0.08f;

	float ringLife_ = 0.94f;
	float burstLife_ = 1.2f;

	float ringSpeed_ = 4.0f;
	float burstSpeed_ = 10.0f; 


	float ringShrink_ = 0.0018f;
	float burstShrink_ = 0.0040f;
};
