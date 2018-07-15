#pragma once
#include "Texture.h"

AUTO_BEGIN
class Texture2D : public Texture
{
	REGISTER_DERIVED_ABSTRACT_CLASS(Texture2D, Texture);
	DECLARE_OBJECT_SERIALIZE(Texture2D);
public:
	typedef struct _TexParams {
		GLuint    minFilter;
		GLuint    magFilter;
		GLuint    wrapS;
		GLuint    wrapT;
	}TexParams;

public:

	Texture2D();
	
	virtual void Awake() {}
	virtual void Start() {}
	virtual void Update() {}
	virtual void FixUpdate() {}
	virtual void Finish() {}
	virtual void Draw(Camera* camera = nullptr) {}

};

AUTO_END
