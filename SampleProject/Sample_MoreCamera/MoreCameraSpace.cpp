#include "MoreCameraSpace.h"
#include "Level_0.h"
#include "FileSystem.h"
#include "ResourceSystem.h"

MoreCameraSpace::MoreCameraSpace(Ambient* ambient)
	:MotionSpace(ambient)
{
}
MoreCameraSpace::~MoreCameraSpace()
{
	RemoveScene(0);
}

void MoreCameraSpace::Awake()
{
	STRING ResourceDir = GetSubSystem<FileSystem>()->GetProgramDir() + "../../SampleProject/Resource/";
	GetSubSystem<ResourceSystem>()->AddResourceDir(ResourceDir);
	RegisterScene(new Level_0(_ambient, 0));
}

AUTO_APPLICATION_MAIN(MoreCameraSpace)