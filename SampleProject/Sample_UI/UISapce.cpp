#include "UISapce.h"
#include "Application.h"
#include "../FreeCamera.h"
#include "level_0.h"
UISapce::UISapce(Ambient* ambient)
	:MotionSpace(ambient)
{}
UISapce::~UISapce()
{
	RemoveScene(0);
}
void UISapce::Awake()
{
	RegisterScene(new Level_0(_ambient, 0));
}

void UISapce::Start()
{
}
void UISapce::Update()
{
}
int UISapce::Launch()
{
	return INSTANCE(Application).Run(_ambient);
}
AUTO_APPLICATION_MAIN(UISapce)