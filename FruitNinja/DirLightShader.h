#pragma once

#include "Shader.h"		
#include "StencilShader.h"
#include "DirShadowMapBuffer.h"

class DirLightShader : public Shader
{
public:
	DirLightShader(GBuffer* gbuffer, DirShadowMapBuffer *shadowMapBuf, glm::vec3 position, glm::mat4& shadowVP);
	~DirLightShader();
	virtual void draw(std::shared_ptr<Camera> camera, std::vector<std::shared_ptr<GameEntity>> ents, std::vector<Light*> lights) override;
	void draw(glm::mat4& view_mat, std::shared_ptr<GameEntity> entity) override;
	void pass(std::shared_ptr<Camera> camera);

private:
	GBuffer* gbuffer;
	DirShadowMapBuffer *shadowMapBuffer;
	glm::vec3 pos;
	glm::mat4 shadowVP;
	GLuint VAO;
	GLuint VBO;
	GLuint IND;

	GLuint model_handle;
	GLuint view_handle;
	GLuint proj_handle;
	GLuint shadow_map_handle;

	GLuint pos_map_handle;
	GLuint color_map_handle;
	GLuint normal_map_handle;

	GLuint eye_handle;
	GLuint size_handle;
	GLuint position_handle;

	GLuint shadowvp_handle;
};