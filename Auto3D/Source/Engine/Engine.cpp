#include "Engine.h"
#include "Renderer/ForwardShadingRenderer.h"
#include "UI/UI.h"
#include "Platform/PlatformDef.h"
#include "Time/Time.h"
#include "Math/Random.h"
#include "Gameplay/WorldContext.h"
#include "Gameplay/World.h"
#include "Platform/ProcessWindow.h"
#include "Application.h"
#include "Physics/Physics.h"

namespace Auto3D
{
FForwardShadingRenderer* renderer;

FEngine::FEngine()
{
	renderer = new FForwardShadingRenderer();
}

FEngine::~FEngine()
{
	delete renderer;
}

bool FEngine::Init()
{
	// Set random seeds based on time
	GTimeModule::RealTime& realTime = GTimeModule::Get().GetRealTime();

	SetRandomSeed(((unsigned)(realTime._day & 0xff) << 24) |
		((unsigned)(realTime._hour & 0xff) << 16) |
		((unsigned)(realTime._minute & 0xff) << 8) |
		((unsigned)(realTime._second & 0xff)));

	renderer->Init();
	imguiCreate();

	return true;
}

void FEngine::Exit()
{
	FWorldContext::Get().DeleteAllWorld();
	imguiDestroy();
	renderer->ShutDowm();
}

void FEngine::Render()
{
	imguiEndFrame();
	if(!IAppInstance::_useCustomRender)
		renderer->Render();
}

bool FEngine::Update()
{
	GTimeModule& time = GTimeModule::Get();
	// Begin is still dirty if you manually replace the world 
	auto world = FWorldContext::Get().GetActiveWorld();
	if (world && !world->HasBegunPlay())
		world->BeginPlay();

	time.Update();
	GPhysicsModule::Get().Update();

	if (world && world->HasBegunPlay())
		world->Tick(time.GetDeltaTime());

	if (!GProcessWindow::Get().ProcessUpdate())
	{
		/*renderer->SetBackBufferSize(TVector2F(GProcessWindow::_width, GProcessWindow::_height));
		renderer->SetDebugMode(GProcessWindow::_debug);
		renderer->SetResetMode(GProcessWindow::_reset);
*/
		imguiBeginFrame(GProcessWindow::_mouseState._mx
			, GProcessWindow::_mouseState._my
			, (GProcessWindow::_mouseState._buttons[Auto3D::MouseButton::Left] ? IMGUI_MBUT_LEFT : 0)
			| (GProcessWindow::_mouseState._buttons[Auto3D::MouseButton::Right] ? IMGUI_MBUT_RIGHT : 0)
			| (GProcessWindow::_mouseState._buttons[Auto3D::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0)
			, GProcessWindow::_mouseState._mz
			, uint16_t(GProcessWindow::_width)
			, uint16_t(GProcessWindow::_height)
		);
	}
	else
		return false;

	return true;
}

void FEngine::FrameFinish()
{
	
}

}