#pragma once
#include "Texture.h"
#include "Texture.h"
#include "Transform.h"
#include "Camera.h"
#include "Application.h"
#include "TextureMap.h"
#include "ShaderVariation.h"


namespace Auto3D {

class TextureNormal : public TextureMap, public EnableSharedFromThis<TextureNormal>
{
	REGISTER_OBJECT_CLASS(TextureNormal, TextureMap)
public:
	explicit TextureNormal(SharedPtr<Ambient> ambient);
	void Destory()override;
	void Start()override;
	void Draw()override;

	void SetColor(const Color& color);
	void SetColor(const Vector3& vec);
	void SetColor(float r, float g, float b, float a = 1.0f);

private:

	unsigned int _VAO{};
	unsigned int _VBO{};
	unsigned int _textureData{};
	SharedPtr<ShaderVariation> _shader;
	Color _color;
	char* _imagePath{};
	char* _imageNormalPath{};
	unsigned int _timage{};
	unsigned int _imageNormal{};

	void renderQuad();
};

}