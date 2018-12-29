#include "ModelSapce.h"
#include "ResourceSystem.h"
#include "FileSystem.h"
#include "level_0.h"

AUTO_APPLICATION_MAIN(ModelSapce)

void ModelSapce::Init()
{
	STRING ResourceDir = GetSubSystem<FileSystem>()->GetProgramDir() + "../../SampleProject/Resource/";
	GetSubSystem<ResourceSystem>()->AddResourceDir(ResourceDir);
	RegisterScene(new Level_0(_ambient, 0));
}
void ModelSapce::Destruct()
{
	RemoveScene(0);
}