#include "MeshLight.h"
#include "Graphics.h"
#include "Renderer.h"
#include "BaseMesh.h"
#include "Light.h"
#include "ResourceSystem.h"
#include "Config.h"
#include "NewDef.h"
namespace Auto3D {

MeshLight::MeshLight(SharedPtr<Ambient> ambient)
	: RenderComponent(ambient)
{
	auto shader = GetSubSystem<ResourceSystem>()->GetResource<Shader>("shader/au_lighting.glsl");
	_shader = MakeShared<ShaderVariation>(shader);
	_shader->Create();
}

MeshLight::~MeshLight()
{
	
}

void MeshLight::Destory()
{
	UnloadOpaque(SharedFromThis());
}
#define HDR_DEBUG 0
void MeshLight::Start()
{
	STRING woodDir = system_content_dictionary;
	woodTexture = GetSubSystem<ResourceSystem>()->TextureLoad((woodDir + "texture/bricks.jpg").CStr());

	_shader->Use();
	_shader->SetInt("diffuseTexture", 0);
	RegisterOpaque(SharedFromThis());
}

void MeshLight::Draw()
{

	glm::mat4 projection = GetSubSystem<Renderer>()->GetCurrentCamera().GetProjectionMatrix();
	glm::mat4 view = GetSubSystem<Renderer>()->GetCurrentCamera().GetViewMatrix();
	_shader->Use();
	_shader->SetMat4("projection", projection);
	_shader->SetMat4("view", view);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, woodTexture);
	// set lighting uniforms

	VECTOR<SharedPtr<Light> > lights = GetSubSystem<Renderer>()->GetLightContainer()->GetAllLights();
	int lightNum = 0;
	for (VECTOR<SharedPtr<Light> >::iterator it = lights.begin(); it != lights.end(); it++)
	{
		_shader->SetVec3("lights[" + KhSTL::ToString(lightNum) + "].Position", (*it)->GetNode()->GetPosition());
		_shader->SetVec3("lights[" + KhSTL::ToString(lightNum) + "].Color", (*it)->GetColorToVec());
		lightNum++;
	}

	_shader->SetVec3("viewPos", GetSubSystem<Renderer>()->GetCurrentCamera().GetPosition());
	// render tunnel
	glm::mat4 model = glm::mat4();

	if (GetNode())		//if gameObject not empty
		model = GetNode()->GetComponent<Transform>()->GetTransformMat();
	else
		model = Matrix4x4::identity;

	_shader->SetMat4("model", model);
	_shader->SetInt("inverse_normals", true);
	renderCube(&_vao, &_vbo);
}

}