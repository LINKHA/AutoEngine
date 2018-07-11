#ifndef SHADOW_H_
#define SHADOW_H_
#include "Auto.h"
#include "Shader.h"
#include "GameObject.h"
AUTO_BEGIN
class Shadow : public Component
{
public:
	Shadow();
	virtual ~Shadow();
	void Start()override;
	void Draw(Camera* camera = nullptr)override;
private:
	Shader m_ShadowMap;
	Shader m_ShadowMapDepth;
	glm::vec3 lightPos;
	unsigned int woodTexture;
	unsigned int planeVAO;
	unsigned int loadTexture(const char *path);
	void renderScene(const Shader &shader);
	void renderCube();
};
AUTO_END
#endif //!SHADOW_H_

