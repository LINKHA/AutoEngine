#include "PreLoadManager.h"

namespace Auto3D {

SINGLETON_INSTANCE(PreLoadManager);

PreLoadManager::PreLoadManager(Ambient* ambient)
	:Super(ambient)
{
}


PreLoadManager::~PreLoadManager()
{
}

void PreLoadManager::AddPrefab(Prefab* prefab)
{
	_prefabs.push_back(prefab);
}
void PreLoadManager::RemovePrefab(Prefab* prefab)
{
	_prefabs.remove(prefab);
}
}