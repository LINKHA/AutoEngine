#include "DepthTestSpace.h"
#include "Application.h"
#include "Level_0.h"

DepthTestSpace::DepthTestSpace(Ambient* ambient)
	:MotionSpace(ambient)
{}
DepthTestSpace::~DepthTestSpace()
{
	RemoveScene(0);
}
void DepthTestSpace::Awake()
{
	RegisterScene(new Level_0(_ambient, 0));
}

int DepthTestSpace::Launch()
{
	return INSTANCE(Application).Run(_ambient);
}
AUTO_APPLICATION_MAIN(DepthTestSpace)