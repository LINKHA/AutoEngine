#pragma once
#include "Shader.h"
#include "GLMeshEnable.h"
#include "RenderComponent.h"

namespace Auto3D {

class Model;
class Camera;
class MeshShadowPoint : public RenderComponent, public GLMeshEnable
{
public:
	explicit MeshShadowPoint(Ambient* ambient);
	MeshShadowPoint(Ambient* ambient,bool enable);
	~MeshShadowPoint();
	void DrawReady()override;
	void Draw()override;
	void DrawShadow()override;
	void DisableCull() { _cullEnable = true; }
private:
	Shader _shader;
	Shader _hardShader;
	SharedPtr<Model> _model;
	bool _cullEnable;
	unsigned int _woodTexture;
	unsigned int _VAO;
	unsigned int _VBO;

};

}