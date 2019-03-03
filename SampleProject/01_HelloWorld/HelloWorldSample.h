#pragma once

#include "Source/Application.h"

using namespace Auto3D;

class HelloWorldSample : public Application
{
	REGISTER_OBJECT_CLASS(HelloWorldSample, Application)
public:
	void Init()override;
	void Start()override;
	void Update()override;
	void Stop()override;
	void HandleCloseRequest(Event& /* event */)
	{
		Subsystem<Graphics>()->Close();
	}
	float yaw = 0.0f, pitch = 20.0f;
	Camera* camera;
	SharedPtr<Scene> scene;
	UICamera* uiCamera;
	SharedPtr<Canvas> canvas;
};