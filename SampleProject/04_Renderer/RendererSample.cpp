#include "RendererSample.h"

void RendererSample::Init()
{
	auto* graphics = Object::Subsystem<Graphics>();
	graphics->RenderWindow()->SetTitle("Renderer Sample");

}
void RendererSample::Start()
{
	auto* cache = Object::Subsystem<ResourceCache>();
	auto* graphics = Object::Subsystem<Graphics>();
	auto* renderer = Object::Subsystem<Renderer>();
	auto* input = Object::Subsystem<Input>();
	auto* profiler = Object::Subsystem<Profiler>();

	graphics->RenderWindow()->SetMouseLock(true);
	graphics->RenderWindow()->SetMouseHide(true);

	SubscribeToEvent(graphics->RenderWindow()->closeRequestEvent, &RendererSample::HandleCloseRequest);


	scene = new Scene();
	scene->CreateChild<Octree>();
	camera = scene->CreateChild<Camera>();
	camera->SetPosition(Vector3F(0.0f, 20.0f, -75.0f));
	camera->SetAmbientColor(Color(0.1f, 0.1f, 0.1f));
	// Register scene to scene system use to render
	Object::Subsystem<RegisteredBox>()->RegisterScene(scene, camera);

	for (int y = -5; y <= 5; ++y)
	{
		for (int x = -5; x <= 5; ++x)
		{
			StaticModel* object = scene->CreateChild<StaticModel>();
			object->SetPosition(Vector3F(10.5f * x, -0.1f, 10.5f * y));
			object->SetScale(Vector3F(10.0f, 0.1f, 10.0f));
			object->SetModel(cache->LoadResource<Model>("Box.mdl"));
			object->SetMaterial(cache->LoadResource<Material>("Stone.json"));
		}
	}

	for (unsigned i = 0; i < 435; ++i)
	{
		StaticModel* object = scene->CreateChild<StaticModel>();
		object->SetPosition(Vector3F(Random() * 100.0f - 50.0f, 1.0f, Random() * 100.0f - 50.0f));
		object->SetScale(1.5f);
		object->SetModel(cache->LoadResource<Model>("Mushroom.mdl"));
		object->SetMaterial(cache->LoadResource<Material>("Mushroom.json"));
		object->SetCastShadows(true);
		object->SetLodBias(2.0f);
	}

	for (unsigned i = 0; i < 10; ++i)
	{
		Light* light = scene->CreateChild<Light>();
		light->SetLightType(LightType::POINT);
		light->SetCastShadows(true);
		Vector3F colorVec = 2.0f * Vector3F(Random(), Random(), Random()).Normalized();
		light->SetColor(Color(colorVec._x, colorVec._y, colorVec._z));
		light->SetFov(90.0f);
		light->SetRange(20.0f);
		light->SetPosition(Vector3F(Random() * 120.0f - 60.0f, 7.0f, Random() * 120.0f - 60.0f));
		light->SetDirection(Vector3F(0.0f, -1.0f, 0.0f));
		light->SetShadowMapSize(256);
	}
	canvas = new Canvas();
	uiCamera = canvas->CreateChild<UICamera>();
	uiCamera->SetOrthographic(true);
	uiCamera->SetPosition(Vector3F(0.0f, 0.0f, -100.0f));
	Subsystem<RegisteredBox>()->RegisterCanvas(canvas, uiCamera);

	CreateLogo();
}
void RendererSample::Update()
{
	auto* input = Object::Subsystem<Input>();
	auto* graphics = Object::Subsystem<Graphics>();
	auto* renderer = Object::Subsystem<Renderer>();
	auto* time = Object::Subsystem<Time>();

	pitch += input->GetMouseMove()._y * 0.25f;
	yaw += input->GetMouseMove()._x * 0.25f;
	pitch = Clamp(pitch, -90.0f, 90.0f);

	float moveSpeed = input->IsKeyDown(KEY_LSHIFT) ? 200.0f : 50.0f;

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

void RendererSample::Stop()
{
}
void RendererSample::CreateLogo()
{
	auto* cache = Object::Subsystem<ResourceCache>();
	Sprite* logoLong = canvas->CreateChild<Sprite>();
	logoLong->SetTexture(cache->LoadResource<Texture>("LogoLong.png"));
	logoLong->SetScale(Vector3F(3.0f, 0.8f, 1.0f));
	logoLong->SetPosition(Vector3F(7.0f, -9.2f, -0.1f));
}
AUTO_APPLICATION_MAIN(RendererSample)