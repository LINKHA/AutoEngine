#include "VertexExplodeSpace.h"
#include "GameObject.h"
#include "../FreeCamera.h"
#include "Application.h"
#include "Mesh.h"
#include "Shader.h"
#include "LightPoint.h"
VertexExplodeSpace::VertexExplodeSpace(Ambient* ambient)
	:MotionSpace(ambient)
{
}


VertexExplodeSpace::~VertexExplodeSpace()
{
}
void VertexExplodeSpace::Start()
{
	GameObject * cameraObj = new GameObject(_ambient);
	FreeCamera * camera = new FreeCamera(_ambient);
	cameraObj->GetComponent(Transform).SetPosition(0.0f, 0.0f, 3.0f);
	cameraObj->AddComponent(camera);

	GameObject * lightObj = new GameObject(_ambient);
	Light * light = new LightPoint(_ambient);
	lightObj->AddComponent(light);


	GameObject * meshObj = new GameObject(_ambient);
	Shader shader(AtConfig::shader_path + "au_vertex_explode.auvs",
		AtConfig::shader_path + "au_vertex_explode.aufs",
		AtConfig::shader_path + "au_vertex_explode.augs");

	Mesh * mesh = new Mesh(_ambient,"../Resource/object/nanosuit/nanosuit.obj",shader);
	meshObj->AddComponent(mesh);

	GameObject * meshObj2 = new GameObject(_ambient);
	Mesh * mesh2 = new Mesh(_ambient,"../Resource/object/base/Cube.FBX");
	meshObj2->AddComponent(mesh2);
}
void VertexExplodeSpace::Update()
{}
int VertexExplodeSpace::Launch()
{
	return INSTANCE(Application).Run(_ambient);
}
AUTO_APPLICATION_MAIN(VertexExplodeSpace)