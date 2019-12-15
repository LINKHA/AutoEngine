#pragma once
#include "Collider.h"

namespace Auto3D
{

class AUTO_API AColliderCapsule : public ACollider
{
	DECLARE_CLASS(AColliderCapsule, ACollider)

	DECLARE_CLASS_NEW(AColliderCapsule, ACollider)
public:
	/// Construct
	AColliderCapsule();
	/// Destructer
	~AColliderCapsule();
	/// Register object factory.
	static void RegisterObject();

	/// Set size radius and height
	void SetSize(float radius,float height);
	/// Set radius with this shape.
	void SetRadius(float radius);
	/// Set height with this shape.
	void SetHeight(float height);

	float GetRadius() { return _radius; }
	float GetHeight() { return _height; }
private:
	/// Resize form TVector3
	void Resize(float radius, float height);

	/// Capsule radius.
	float _radius;
	/// Capsule height.
	float _height;
};

}