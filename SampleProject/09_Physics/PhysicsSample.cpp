
#include "PhysicsSample.h"

void PhysicsSample::Init()
{
	Super::Init();
	auto* graphics = Object::Subsystem<Graphics>();
	graphics->RenderWindow()->SetTitle("Physics Sample");
}

void PhysicsSample::Start()
{
	Super::Start();
	auto* cache = Object::Subsystem<ResourceCache>();
	auto* graphics = Object::Subsystem<Graphics>();

	graphics->RenderWindow()->SetMouseLock(true);
	graphics->RenderWindow()->SetMouseHide(true);

	scene = new Scene();
	scene->CreateChild<Octree>();
	scene->CreateChild<PhysicsWorld>();
	camera = scene->CreateChild<Camera>();
	camera->SetPosition(Vector3F(0.0f, 10.0f, -60.0f));

	StaticModel* plane = scene->CreateChild<StaticModel>();
	plane->SetModel(cache->LoadResource<Model>("Box.mdl"));
	plane->SetPosition(Vector3F(0.0f, -25.0f, 0.0f));
	plane->SetScale(Vector3F(50.0f, 50.0f, 50.0f));
	plane->SetMaterial(cache->LoadResource<Material>("Stone.json"));
	ColliderBox* colliderBox = plane->CreateChild<ColliderBox>();
	colliderBox->SetSize(Vector3F(50.0f, 50.0f, 50.0f));
	RigidBody* rigidBody = plane->CreateChild<RigidBody>();
}
void PhysicsSample::Update()
{
	Super::Update();
	auto* input = Object::Subsystem<Input>();
	auto* graphics = Object::Subsystem<Graphics>();
	auto* renderer = Object::Subsystem<Renderer>();
	auto* time = Object::Subsystem<Time>();

	pitch += input->GetMouseMove()._y * 0.25f;
	yaw += input->GetMouseMove()._x * 0.25f;
	pitch = Clamp(pitch, -90.0f, 90.0f);

	float moveSpeed = input->IsKeyDown(KEY_LSHIFT) ? 50 : 10.0f;

	camera->SetRotation(Quaternion(pitch, yaw, 0.0f));
	if (input->IsKeyDown(KEY_W))
		camera->Translate(Vector3F::FORWARD * time->GetDeltaTime() * moveSpeed);
	if (input->IsKeyDown(KEY_S))
		camera->Translate(Vector3F::BACK * time->GetDeltaTime()  * moveSpeed);
	if (input->IsKeyDown(KEY_A))
		camera->Translate(Vector3F::LEFT * time->GetDeltaTime()  * moveSpeed);
	if (input->IsKeyDown(KEY_D))
		camera->Translate(Vector3F::RIGHT * time->GetDeltaTime()  * moveSpeed);

}

void PhysicsSample::Stop()
{
	Super::Stop();
}

AUTO_APPLICATION_MAIN(PhysicsSample)