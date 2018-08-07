#include "MeshShadowPoint.h"
#include "Renderer.h"
#include "Light.h"
#include "VertexData.h"
#include "ShadowTest.h"
#include "Camera.h"
#include "Configs.h"
#include "LoadResource.h"

namespace Auto3D {

MeshShadowPoint::MeshShadowPoint(Ambient* ambient)
	: RenderComponent(ambient)
	, _shader(shader_path + "au_point_shadows.auvs"
		, shader_path + "au_point_shadows.aufs")
{
	RegisterShadow(this);
	RegisterOpaque(this);
}
MeshShadowPoint::MeshShadowPoint(Ambient* ambient, int i)
	: RenderComponent(ambient)
	, _shader(Shader(shader_path + "au_point_shadows.auvs"
		, shader_path + "au_point_shadows.aufs"))
{
	k = i;
	RegisterShadow(this);
	RegisterOpaque(this);
}

MeshShadowPoint::~MeshShadowPoint()
{
	UnloadShadow(this);
	UnloadOpaque(this);
}
void MeshShadowPoint::DrawReady()
{
	_model = LocalModelLoad("../Resource/object/base/Cube.FBX");

	_woodTexture = LocalTextureLoad("../Resource/texture/wood.jpg");
	_shader.Use();
	_shader.SetInt("diffuseTexture", 0);
	_shader.SetInt("shadowMap", 1);
}

void MeshShadowPoint::DrawShadow()
{
	Shader& shadowShader = GetSubSystem<Renderer>()->GetShadowRenderer()->GetPointDepthMapShader();
	glm::mat4 model;
	switch (k)
	{
	case 0:
		model = glm::scale(model, glm::vec3(5.0f));
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, 2.0));
		shadowShader.SetMat4("model", model);
		glDisable(GL_CULL_FACE); // note that we disable culling here since we render 'inside' the cube instead of the usual 'outside' which throws off the normal culling methods.
		shadowShader.SetInt("reverse_normals", 1); // A small little hack to invert normals when drawing cube from the inside so lighting still works.
		_model->Draw(shadowShader);
		shadowShader.SetInt("reverse_normals", 0); // and of course disable it
		glEnable(GL_CULL_FACE);
		break;
	case 1:
		model = glm::mat4();
		model = glm::translate(model, glm::vec3(4.0f, -3.5f, 0.0));
		model = glm::scale(model, glm::vec3(0.5f));
		shadowShader.SetMat4("model", model);
		_model->Draw(shadowShader);
		break;
	case 2:
		model = glm::mat4();
		model = glm::translate(model, glm::vec3(2.0f, 3.0f, 1.0));
		model = glm::scale(model, glm::vec3(0.75f));
		shadowShader.SetMat4("model", model);
		_model->Draw(shadowShader);
		break;
	case 3:
		model = glm::mat4();
		model = glm::translate(model, glm::vec3(-3.0f, -1.0f, 0.0));
		model = glm::scale(model, glm::vec3(0.5f));
		shadowShader.SetMat4("model", model);
		_model->Draw(shadowShader);
		break;
	case 4:
		model = glm::mat4();
		model = glm::translate(model, glm::vec3(-1.5f, 1.0f, 1.5));
		model = glm::scale(model, glm::vec3(0.5f));
		shadowShader.SetMat4("model", model);
		_model->Draw(shadowShader);
	case 5:
		model = glm::mat4();
		model = glm::translate(model, glm::vec3(-1.5f, 2.0f, -3.0));
		model = glm::rotate(model, glm::radians(60.0f), glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
		model = glm::scale(model, glm::vec3(0.75f));
		shadowShader.SetMat4("model", model);
		_model->Draw(shadowShader);
	}

}
void MeshShadowPoint::Draw()
{

	Camera* camera = GetSubSystem<Renderer>()->GetCurrentCameraPtr();
	glm::vec3 lightPos;
	//!!! Temp use one light,and must need light
#pragma warning
	_VECTOR(Light*)& lights = GetSubSystem<Renderer>()->GetLightContainer()->GetAllLights();
	//!!!Temp
	Assert(lights.size() != 0);
	for (_VECTOR(Light*)::iterator it = lights.begin(); it != lights.end(); it++)
	{
		lightPos = (*it)->GetLightPosition();
		unsigned depthMap = (*it)->GetShadowAssist()->GetDepthMap();

		_shader.Use();
		glm::mat4 projection = camera->GetProjectionMatrix();
		glm::mat4 view = camera->GetViewMatrix();
		_shader.SetMat4("projection", projection);
		_shader.SetMat4("view", view);
		// set lighting uniforms
		_shader.SetVec3("viewPos", camera->GetPosition());
		_shader.SetVec3("lightPos", lightPos);
		_shader.SetInt("shadows", true); // enable/disable shadows by pressing 'SPACE'
		_shader.SetFloat("far_plane", (*it)->GetFarPlane());
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, _woodTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_CUBE_MAP, depthMap);
		glm::mat4 model;
		switch (k)
		{
		case 0:
			model = glm::scale(model, glm::vec3(5.0f));
			_shader.SetMat4("model", model);
			glDisable(GL_CULL_FACE); // note that we disable culling here since we render 'inside' the cube instead of the usual 'outside' which throws off the normal culling methods.
			_shader.SetInt("reverse_normals", 1); // A small little hack to invert normals when drawing cube from the inside so lighting still works.
			_model->Draw(_shader);
			_shader.SetInt("reverse_normals", 0); // and of course disable it
			glEnable(GL_CULL_FACE);
			break;
		case 1:
			model = glm::mat4();
			model = glm::translate(model, glm::vec3(4.0f, -3.5f, 0.0));
			model = glm::scale(model, glm::vec3(0.5f));
			_shader.SetMat4("model", model);
			_model->Draw(_shader);
			break;
		case 2:
			model = glm::mat4();
			model = glm::translate(model, glm::vec3(2.0f, 3.0f, 1.0));
			model = glm::scale(model, glm::vec3(0.75f));
			_shader.SetMat4("model", model);
			_model->Draw(_shader);
			break;
		case 3:
			model = glm::mat4();
			model = glm::translate(model, glm::vec3(-3.0f, -1.0f, 0.0));
			model = glm::scale(model, glm::vec3(0.5f));
			_shader.SetMat4("model", model);
			_model->Draw(_shader);
			break;
		case 4:
			model = glm::mat4();
			model = glm::translate(model, glm::vec3(-1.5f, 1.0f, 1.5));
			model = glm::scale(model, glm::vec3(0.5f));
			_shader.SetMat4("model", model);
			_model->Draw(_shader);
		case 5:
			model = glm::mat4();
			model = glm::translate(model, glm::vec3(-1.5f, 2.0f, -3.0));
			model = glm::rotate(model, glm::radians(60.0f), glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
			model = glm::scale(model, glm::vec3(0.75f));
			_shader.SetMat4("model", model);
			_model->Draw(_shader);
		}
	}
}

}
