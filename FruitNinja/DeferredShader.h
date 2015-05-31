#pragma once

#include "Shader.h"
#include "DeferredRenderer.h"
#include "SimpleTextureShader.h"
#include "ParticleShader.h"
#include "FlameEmitter.h"
#include "FireEmitter.h"

//#include <glm/gtc/type_ptr.hpp>
//#include <glm/gtc/matrix_transform.hpp>

enum DisplayMode
{
	deferred,
	four_screen
};

class DeferredShader : public Shader
{
public:
	DeferredShader(std::string vertShader, std::string fragShader, std::shared_ptr<Skybox> skybox);
	virtual void draw(std::shared_ptr<Camera> camera, std::vector<std::shared_ptr<GameEntity>> ents, std::vector<Light*> lights);
	void draw(glm::mat4& view_mat, std::shared_ptr<GameEntity> entity);


private:
	void geomPass(glm::mat4& view_mat, std::vector<std::shared_ptr<GameEntity>> ents);
	void lightPass();
	GBuffer gbuffer;
	void startLightPasses();
	void finalPass();
	void skyboxPass(std::shared_ptr<Camera> camera);
	void particlePass(std::shared_ptr<Camera> camera, std::vector<Light*> lights);
	std::shared_ptr<Skybox> skybox;
	SimpleTextureShader skyShader;
	ParticleShader fireShader;
	vector<Emitter*> emitters;

	DeferredRenderer renderer;
	DisplayMode disp_mode;

	GLint uViewMatrixHandle;
	GLint uModelMatrixHandle;
	GLint uProjMatrixHandle;
	GLint UtexHandle;
	GLint UflagHandle;
	GLint uBoneFlagHandle;
	GLint uBonesHandle;
	GLint UdColorHandle;

};