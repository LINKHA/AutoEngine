#include "FrameBuffersSpace.h"
#include "Application.h"
#include "Level_0.h"

FrameBuffersSpace::FrameBuffersSpace(Ambient* ambient)
	:MotionSpace(ambient)
{
}
FrameBuffersSpace::~FrameBuffersSpace()
{
	RemoveScene(0);
}

void FrameBuffersSpace::Awake()
{
	RegisterScene(new Level_0(_ambient, 0));
}

int FrameBuffersSpace::Launch()
{
	return INSTANCE(Application).Run(_ambient);
}
AUTO_APPLICATION_MAIN(FrameBuffersSpace)