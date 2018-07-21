#pragma once
#include "Texture.h"
#include "Texture.h"
#include "LoadResource.h"
#include "Transform.h"
#include "Camera.h"
#include "Application.h"
#include "GameObject.h"
#include "TextureMap.h"
AUTO_BEGIN

class TextureNormal : public TextureMap
{
	REGISTER_DERIVED_CLASS(TextureNormal, TextureMap);
	DECLARE_OBJECT_SERIALIZE(TextureNormal);
public:
	TextureNormal();
	TextureNormal(char* imagePath);
	TextureNormal(char* imagePath, const Shader& shader);
	void Start()override;
	void Draw()override;

	void SetColor(const Color& color);
	void SetColor(const Vector3& vec);
	void SetColor(float r, float g, float b, float a = 1.0f);

private:

	unsigned int _VBO, _VAO;
	unsigned int _textureData;

	Shader _shader;
	Color _color;
	Ptr(char, _imagePath);
	Ptr(char, _imageNormalPath);
	unsigned int _image;
	unsigned int _imageNormal;

	void renderQuad();
};

AUTO_END
