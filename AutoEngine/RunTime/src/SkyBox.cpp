#include "SkyBox.h"
#include "LoadResource.h"
#include "GLWindow.h"
#include "Camera.h"
AUTO_BEGIN

float skyboxVertices[] = {      
	-1.0f,  1.0f, -1.0f,
	-1.0f, -1.0f, -1.0f,
	1.0f, -1.0f, -1.0f,
	1.0f, -1.0f, -1.0f,
	1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,

	-1.0f, -1.0f,  1.0f,
	-1.0f, -1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f,  1.0f,
	-1.0f, -1.0f,  1.0f,

	1.0f, -1.0f, -1.0f,
	1.0f, -1.0f,  1.0f,
	1.0f,  1.0f,  1.0f,
	1.0f,  1.0f,  1.0f,
	1.0f,  1.0f, -1.0f,
	1.0f, -1.0f, -1.0f,

	-1.0f, -1.0f,  1.0f,
	-1.0f,  1.0f,  1.0f,
	1.0f,  1.0f,  1.0f,
	1.0f,  1.0f,  1.0f,
	1.0f, -1.0f,  1.0f,
	-1.0f, -1.0f,  1.0f,

	-1.0f,  1.0f, -1.0f,
	1.0f,  1.0f, -1.0f,
	1.0f,  1.0f,  1.0f,
	1.0f,  1.0f,  1.0f,
	-1.0f,  1.0f,  1.0f,
	-1.0f,  1.0f, -1.0f,

	-1.0f, -1.0f, -1.0f,
	-1.0f, -1.0f,  1.0f,
	1.0f, -1.0f, -1.0f,
	1.0f, -1.0f, -1.0f,
	-1.0f, -1.0f,  1.0f,
	1.0f, -1.0f,  1.0f
};

SkyBox::SkyBox()
{
}


SkyBox::~SkyBox()
{
}
void SkyBox::Start()
{
	m_shader = Shader(AtConfig::shader_path + "au_skybox.auvs", AtConfig::shader_path + "au_skybox.aufs");
	glGenVertexArrays(1, &m_skyboxVAO);
	glGenBuffers(1, &m_skyboxVBO);
	glBindVertexArray(m_skyboxVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_skyboxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	_VECTOR(_String) faces
	{
	"Resource/skybox/arrakisday_ft.tga",
	"Resource/skybox/arrakisday_bk.tga",
	"Resource/skybox/arrakisday_up.tga",
	"Resource/skybox/arrakisday_dn.tga",
	"Resource/skybox/arrakisday_rt.tga",
	"Resource/skybox/arrakisday_lf.tga"
	};
	m_cubemapTexture = LoadCubemap(faces);
}
void SkyBox::Draw(Camera * cam)
{
	if (cam == nullptr)
	{
		WarningString("Fail to find camera");
		return;
	}
	glm::mat4 viewMat = cam->GetViewMatrix(); 
	RectInt rect = INSTANCE(GLWindow).GetWindowRectInt();
	glm::mat4 projectionMat = glm::perspective(cam->Zoom,
		((float)rect.width * (float)cam->ViewRect.width) /
		((float)rect.height * (float)cam->ViewRect.height),
		cam->Near, cam->Far);
	glDepthFunc(GL_LEQUAL);  
	m_shader.Use();
	viewMat = glm::mat4(glm::mat3(cam->GetViewMatrix()));
	m_shader.SetMat4("view", viewMat);
	m_shader.SetMat4("projection", projectionMat);
	glBindVertexArray(m_skyboxVAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemapTexture);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
	glDepthFunc(GL_LESS);
}
AUTO_END