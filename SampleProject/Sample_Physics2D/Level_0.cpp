#include "Level_0.h"
#include "Node.h"
#include "../FreeCamera2D.h"
#include "PhysicsWorld2D.h"
#include "RigidBody2D.h"
#include "ColliderBox2D.h"
#include "ResourceSystem.h"
#include "SpriteRenderer.h"


void Level_0::Awake()
{
	Super::Awake();
}

void Level_0::Start()
{
	Super::Start();

	GameNode camObj = CreateNode();

	GameNode groundNode = CreateNode();
	groundNode->SetPosition(0.0f,4.0f);
	groundNode->CreateComponent<RigidBody2D>();
	groundNode->CreateComponent<ColliderBox2D>()->SetSize(100.0f, 20.0f);


	bodyNode = CreateNode();
	bodyNode->SetPosition(0.0f, -10.0f);
	bodyNode->CreateComponent<RigidBody2D>()->SetBodyType(BodyType2D::kDynamic);
	auto bodyCollider = bodyNode->CreateComponent<ColliderBox2D>();
	bodyCollider->SetSize(2.0f, 2.0f);
	bodyCollider->SetDensity(1.0f);
	bodyCollider->SetFriction(0.3f);

}

void Level_0::Update()
{
	Super::Update();
	Vector3 pos = bodyNode->GetPosition();
	AutoCout << pos.x <<"   "<< pos.y << "   " << pos.z << AutoEndl;
}

