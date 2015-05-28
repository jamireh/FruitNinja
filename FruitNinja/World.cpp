#include "World.h"
#include "PhongShader.h"
#include "TextureDebugShader.h"
#include "DebugCamera.h"
#include "Global.h"
#include "ArcheryCamera.h"
#include "GuardEntity.h"
#include "ProjectileEntity.h"
#include "ObstacleEntity.h"
#include "OctTree.h"
#include "DeferredShader.h"
#include "CollisionHandler.h"
#include "DebugShader.h"
#include <glm/gtx/rotate_vector.hpp>
#include "Skybox.h"
#include "SimpleTextureShader.h"
#include "TestSphere.h"
#include <functional>
#include <queue>
#include "LightEntity.h"
#include "FrustrumCulling.h"
#include <iostream>
#include <fstream>
#include "ParticleShader.h"
#include "MyOctree.h"

#define FILE_TO_WORLD_SCALE 6.f

using namespace std;
using namespace glm;

bool keys[1024];
float seconds_passed = 0;
float x_offset;
float y_offset;

bool debug_enabled = false;
float screen_width = SCREEN_WIDTH;
float screen_height = SCREEN_HEIGHT;

mat4 projection = mat4(perspective((float)radians(PLAYER_FOV), screen_width / screen_height, PLAYER_NEAR, PLAYER_FAR));
mat4 guard_projection = mat4(perspective((float)radians(GUARD_FOV), screen_width / screen_height, GUARD_NEAR, GUARD_FAR));

static std::shared_ptr<Camera> camera;
static shared_ptr<DebugShader> debugShader;
bool time_stopped = false;
float game_speed = 1.0f;
static vector<std::function<void()>> debugShaderQueue;

World::World()
{
    debugShader = shared_ptr<DebugShader>(new DebugShader("debugVert.glsl", "debugFrag.glsl"));
	init();
    x_offset = 0;
    y_offset = 0;
}

void World::init()
{
	debug_camera = shared_ptr<Camera>(new DebugCamera());
    player_camera = shared_ptr<Camera>(new PlayerCamera());
    archery_camera = shared_ptr<Camera>(new ArcheryCamera());

    meshes.insert(pair<string, shared_ptr<MeshSet>>("tower", make_shared<MeshSet>(assetPath + "tower.dae")));
    meshes.insert(pair<string, shared_ptr<MeshSet>>("chewy", shared_ptr<MeshSet>(new MeshSet(assetPath + "ninja_final2.dae"))));
    meshes.insert(pair<string, shared_ptr<MeshSet>>("chewy_bb", shared_ptr<MeshSet>(new MeshSet(assetPath + "ninja_boundingbox.dae"))));
    meshes.insert(pair<string, shared_ptr<MeshSet>>("guard", shared_ptr<MeshSet>(new MeshSet(assetPath + "samurai2.dae"))));
    meshes.insert(pair<string, shared_ptr<MeshSet>>("arrow", make_shared<MeshSet>(assetPath + "arrow.dae")));
    meshes.insert(pair<string, shared_ptr<MeshSet>>("unit_sphere", make_shared<MeshSet>(assetPath + "UnitSphere.obj")));
    meshes.insert(pair<string, shared_ptr<MeshSet>>("lantern", shared_ptr<MeshSet>(new MeshSet(assetPath + "lantern.dae"))));
    meshes.insert(pair<string, shared_ptr<MeshSet>>("lanternPole", shared_ptr<MeshSet>(new MeshSet(assetPath + "lanternPole.dae"))));
    meshes.insert(pair<string, shared_ptr<MeshSet>>("closedBarrel", shared_ptr<MeshSet>(new MeshSet(assetPath + "closedBarrel.dae"))));
    meshes.insert(pair<string, shared_ptr<MeshSet>>("openBarrel", shared_ptr<MeshSet>(new MeshSet(assetPath + "openBarrel.dae"))));
    meshes.insert(pair<string, shared_ptr<MeshSet>>("box", shared_ptr<MeshSet>(new MeshSet(assetPath + "Box.dae"))));
    meshes.insert(pair<string, shared_ptr<MeshSet>>("skybox", shared_ptr<MeshSet>(new MeshSet(assetPath + "skybox.dae", GL_LINEAR, GL_CLAMP_TO_EDGE))));
    //meshes.insert(pair<string, shared_ptr<MeshSet>>("testsphere", shared_ptr<MeshSet>(new MeshSet(assetPath + "testsphere.dae"))));
    
    chewy = std::make_shared<ChewyEntity>(vec3(60.0, 0.0, 60.0), meshes.at("chewy"), player_camera, archery_camera);
    // chewy bounding box mesh
    chewy->largestBB = (new ChewyEntity(vec3(60.0, 0.0, 60.0), meshes.at("chewy_bb"), player_camera, archery_camera))->getOuterBoundingBox();
    chewy->sebInit();
	chewy->list = SET_HIDE(chewy->list);

	_skybox = std::make_shared<Skybox>(Skybox(&camera, meshes.at("skybox")));
	_skybox->setScale(750.f);
	_skybox->list = UNSET_OCTTREE((_skybox->list));

    shared_ptr <GameEntity> tower(new ObstacleEntity(vec3(0.0, 0.0, 0.0), meshes.at("tower")));
    tower->setScale(30.0f);
    tower->list = UNSET_OCTTREE((tower->list));

    camera = player_camera;
    player_camera->in_use = true;


    setup_level(assetPath + "first_courtyard.txt");
    setup_guard(assetPath + "first_courtyard_guard.txt");
    setup_guard(assetPath + "first_courtyard_second_guard.txt");

	entities.push_back(chewy);
    entities.push_back(tower);

	hud = HUD();

	shared_ptr<Shader> phongShader(new PhongShader("phongVert.glsl", "phongFrag.glsl"));
	shaders.insert(pair<string, shared_ptr<Shader>>("phongShader", phongShader));
	shared_ptr<Shader> defShader(new DeferredShader("DeferredVertShader.glsl", "DeferredFragShader.glsl", _skybox));
	shaders.insert(pair<string, shared_ptr<Shader>>("defShader", defShader));

	shaders.insert(pair<string, shared_ptr<Shader>>("debugShader", debugShader));

	shared_ptr<Shader> simpleShader(new SimpleTextureShader("simpleVert.glsl", "simpleFrag.glsl"));
	shaders.insert(pair<string, shared_ptr<Shader>>("simpleShader", simpleShader));

	//shared_ptr<Shader> textDebugShader(new TextureDebugShader());
	//shaders.insert(pair<string, shared_ptr<Shader>>("textureDebugShader", textDebugShader));
}

void World::setup_level(string file_path)
{
    ifstream level_file;
    level_file.open(file_path);

    string current_line;
    int current_row = 0;

    while (!level_file.eof()) // runs through every line
    {
        getline(level_file, current_line);

        for (int i = 0; i < current_line.length(); i++)
        {
            setup_token(current_line.at(i), vec3(i, 0, current_row));
        }
        current_row++;
    }
    level_file.close();
}

void World::setup_guard(string file_path)
{
    ifstream level_file;
    level_file.open(file_path);

    string current_line;
    int current_row = 0;

    vec3 control_points[10];
    vec3 starting_position;

    while (!level_file.eof()) // runs through every line
    {
        getline(level_file, current_line);

        for (int i = 0; i < current_line.length(); i++)
        {
            glm::vec3 world_position = FILE_TO_WORLD_SCALE * vec3(i, 0, current_row);

            switch (current_line.at(i))
            {
            case 'G':
                starting_position = world_position;
                break;
            case '0':
                control_points[0] = world_position;
                break;
            case '1':
                control_points[1] = world_position;
                break;
            case '2':
                control_points[2] = world_position;
                break;
            case '3':
                control_points[3] = world_position;
                break;
            case '4':
                control_points[4] = world_position;
                break;
            case '5':
                control_points[5] = world_position;
                break;
            case '6':
                control_points[6] = world_position;
                break;
            case '7':
                control_points[7] = world_position;
                break;
            case '8':
                control_points[8] = world_position;
                break;
            case '9':
                control_points[9] = world_position;
                break;
            }
        }
        current_row++;
    }

    vector<vec3> spline_points;
    spline_points.push_back(starting_position);
    for (int i = 0; i < 10; i++)
    {
        if (control_points[i] != vec3(0))
            spline_points.push_back(glm::vec3(control_points[i]));
        else
            break;
    }

    entities.push_back(std::shared_ptr<GuardEntity>(new GuardEntity(starting_position, meshes.at("guard"), spline_points, 15.f)));
    level_file.close();
}

void World::setup_token(char obj_to_place, glm::vec3 file_index)
{
    glm::vec3 placement_position = FILE_TO_WORLD_SCALE * file_index;

    switch (obj_to_place)
    {
    case 'X':
        entities.push_back(std::make_shared<ObstacleEntity>(ObstacleEntity(placement_position, meshes.at("box"))));
        entities.back()->setScale(3.f);
        break;
    case 'O':
        entities.push_back(std::make_shared<ObstacleEntity>(ObstacleEntity(placement_position, meshes.at("openBarrel"))));
        entities.back()->setScale(3.f);
        entities.back()->list = SET_HIDE((entities.back()->list));
        break;
    case 'C':
        entities.push_back(std::make_shared<ObstacleEntity>(ObstacleEntity(placement_position, meshes.at("closedBarrel"))));
        entities.back()->setScale(3.f);
        break;
    case 'l': // Lantern Pole with Lantern
        entities.push_back(std::make_shared<LightEntity>(LightEntity(placement_position + vec3(0.f, 7.f, 1.2f), meshes.at("lantern"), 500.f, meshes.at("unit_sphere"))));
		vec3 rots = entities.back()->getRotations();
        rots.y = M_PI_2;
		entities.back()->setRotations(rots);
        entities.push_back(std::make_shared<ObstacleEntity>(ObstacleEntity(placement_position, meshes.at("lanternPole"))));
        break;
    }
}

void World::shootArrows()
{

	static bool held = false;
	bool shot = false;
	for (auto it = entities.begin(); it != entities.end(); ++it) {
		if (typeid(*(*it)) == typeid(ProjectileEntity)) {
			shot = true;
		}
	}
	if (keys[GLFW_KEY_E] && archery_camera->in_use && !held && !shot)
	{
		held = true;
	}
	if (held && !keys[GLFW_KEY_E])
	{
		entities.push_back(make_shared<ProjectileEntity>(meshes["arrow"], archery_camera));
		held = false;
	}
}

void World::draw()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(0);
	static bool usePhong = false;

	if (keys[GLFW_KEY_6])
	{
		usePhong = true;
	}
	if (keys[GLFW_KEY_7])
	{
		usePhong = false;
	}


	if (usePhong) {
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glDepthMask(GL_TRUE);


		glUseProgram(shaders.at("simpleShader")->getProgramID());
		glViewport(0, 0, screen_width, screen_height);
		shaders.at("simpleShader")->draw(camera->getViewMatrix(), _skybox);
	}

	shared_ptr<DebugCamera> d_test = dynamic_pointer_cast<DebugCamera>(camera);
	vector<shared_ptr<GameEntity>> in_view;
    if (d_test != nullptr)
    {
        in_view = get_objects_in_view(entities, player_camera->getViewMatrix());
	}
	else
	{
		in_view = get_objects_in_view(entities, camera->getViewMatrix());
	}

	shared_ptr<PlayerCamera> p_test = dynamic_pointer_cast<PlayerCamera>(camera);
	if (p_test != nullptr)
	{
		p_test->reorient(entities, chewy);
	}
	glUseProgram(0);


	if (usePhong) {
		glUseProgram(shaders.at("phongShader")->getProgramID());
		glViewport(0, 0, screen_width, screen_height);
		for (int i = 0; i < in_view.size(); i++)
			shaders.at("phongShader")->draw(camera->getViewMatrix(), in_view.at(i));
	}
	//otherwise deferred rendering
	else {
		vector<Light*> lights;
		for (int i = 0; i < entities.size(); i++) {
			//even if lantern culled still need light from it
			if (typeid(*entities[i]) == typeid(LightEntity) && SHOULD_DRAW(entities[i]->list)) {
				shared_ptr<LightEntity> le = dynamic_pointer_cast<LightEntity>(entities[i]);
				lights.push_back(&le->light);
			}
			//if there's an arrow have archery camera follow it and make game slow-mo
			if (typeid(*entities[i]) == typeid(ProjectileEntity)) {
				archery_camera->cameraPosition = entities[i]->getPosition() - archery_camera->cameraFront;
			}

			if (!SHOULD_DRAW(entities[i]->list)) {
				entities.erase(entities.begin() + i);
				i--;
			}
		}
		for (int i = 0; i < in_view.size(); i++)
		{
			if (!SHOULD_DRAW(in_view[i]->list)) {
				in_view.erase(in_view.begin() + i);
				i--;
			}
		}
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(shaders.at("defShader")->getProgramID());
		glViewport(0, 0, screen_width, screen_height);
		shaders.at("defShader")->draw(camera, in_view, lights);
	}

    glUseProgram(0);
	if (debug_enabled)
	{
		glUseProgram(debugShader->getProgramID());
		for (int i = 0; i < in_view.size(); i++)
		{
			shared_ptr<BoundingBox> box = in_view.at(i)->getTransformedOuterBoundingBox();
			shared_ptr<vector<pair<vec3, vec3>>> points = box->get_line_segments();
			for (int j = 0; j < points->size(); j++)
			{
				draw_line(points->at(j).first, points->at(j).second, vec3(1.f, 0.f, 0.f));
			}
			vector<pair<glm::vec3, glm::vec3>> planes = box->getPlanes();
			for (int k = 0; k < planes.size(); k++)
			{
				draw_line(planes.at(k).first, planes.at(k).first + box->getMaxWidth(5.0f) * planes.at(k).second, vec3(0, 1.f, 0));
			}
			draw_sphere(in_view.at(i)->getCenter(), in_view.at(i)->getRadius(), vec3(1.f, 1.f, 0.f), 3.f);
		}
		glUseProgram(0);
	}

	glUseProgram(debugShader->getProgramID());
	for (int i = debugShaderQueue.size() - 1; i >= 0; i--)
	{
		debugShaderQueue.at(i)();
	}
	if (!time_stopped)
	{
		debugShaderQueue.clear();
	}
	glUseProgram(0);

	hud.draw();
}

void World::key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (action == GLFW_PRESS)
        keys[key] = true;
    else if (action == GLFW_RELEASE)
        keys[key] = false;
}

void World::mouse_callback(GLFWwindow* window, double x_position, double y_position)
{
    x_offset = x_position;
    y_offset = -1.f * y_position;

    glfwSetCursorPos(window, 0, 0);
}

void World::change_camera()
{
    if (keys[GLFW_KEY_1])
    {
        camera = debug_camera;
        debug_camera->in_use = true;
        player_camera->in_use = false;
        archery_camera->in_use = false;

    }
    if (keys[GLFW_KEY_2])
    {
        camera = player_camera;
        debug_camera->in_use = false;
        player_camera->in_use = true;
        archery_camera->in_use = false;
    }
    if (keys[GLFW_KEY_3])
    {
        camera = archery_camera;
        debug_camera->in_use = false;
        player_camera->in_use = false;
        archery_camera->in_use = true;
    }
}

void World::enable_debugging()
{
	if (keys[GLFW_KEY_Z])
		debug_enabled = true;
	if (keys[GLFW_KEY_X])
		debug_enabled = false;
}

void World::update_key_callbacks()
{
    camera->movement(chewy);
    change_camera();
	enable_debugging();
	stop_time();
    x_offset = 0;
    y_offset = 0;
}

void World::stop_time()
{
	if (keys[GLFW_KEY_T])
		time_stopped = true;
	if (keys[GLFW_KEY_Y])
		time_stopped = false;
}

void World::update()
{
	static float start_time = 0.0;

	shootArrows();

	for (int i = 0; i < entities.size(); i++)
	{
		entities[i]->update();
		shared_ptr<GuardEntity> guard_temp = dynamic_pointer_cast<GuardEntity>(entities[i]);
		if (guard_temp != nullptr)
		{
			guard_temp->check_view(chewy, entities);
		}
	}
	if (!time_stopped)
	{
		seconds_passed = (glfwGetTime() - start_time) * game_speed;
	}
	else
	{
		seconds_passed = 0.f;
	}
	start_time = glfwGetTime();
	OctTree* world_oct_tree = new OctTree(Voxel(vec3(-1000.f, -1000.f, -1000.f), vec3(1000.f, 1000.f, 1000.f)), entities, nullptr);
	collision_handler(world_oct_tree->collision_pairs);
	//MyOctree* world_oct_tree = new MyOctree(Voxel(vec3(-1000.f, -1000.f, -1000.f), vec3(1000.f, 1000.f, 1000.f)), entities);
	//world_oct_tree->handle_collisions();
	//delete world_oct_tree;
    update_key_callbacks();
	_skybox->update();
}

void World::scroll_callback(GLFWwindow* window, double x_pos, double y_pos)
{
    shared_ptr<PlayerCamera> radius_changer = dynamic_pointer_cast<PlayerCamera>(camera);
    if (radius_changer)
        radius_changer->update_radius(y_pos);
}

void World::draw_line(vec3 p1, vec3 p2, vec3 color)
{
	glUseProgram(debugShader->getProgramID());
	debugShaderQueue.push_back([=](){debugShader->drawLine(p1, p2, color, camera->getViewMatrix()); });
	glUseProgram(0);
}

void World::draw_point(vec3 p, vec3 color, float radius)
{
	glUseProgram(debugShader->getProgramID());
	debugShaderQueue.push_back([=](){debugShader->drawPoint(p, color, radius, camera->getViewMatrix());  });
	glUseProgram(0);
}

void World::draw_box(shared_ptr<BoundingBox> box, vec3 color)
{
	shared_ptr<vector<pair<vec3, vec3>>> points = box->get_line_segments();
	for (int j = 0; j < points->size(); j++)
	{
		draw_line(points->at(j).first, points->at(j).second, vec3(1.f, 0.f, 0.f));
	}
}

void World::draw_sphere(vec3 center, float radius, vec3 color, float delta)
{
	glUseProgram(debugShader->getProgramID());
	vector<float> points;
	for (float theta = 0.f; theta < 360.f; theta +=delta)
	{
		float x = radius * cos(radians(theta)) * cos(radians(0.f));
		points.push_back(x + center.x);
		float y = radius * sin(radians(0.f));
		points.push_back(y + center.y);
		float z = radius * sin(radians(theta)) * cos(radians(0.f));
		points.push_back(z + center.z);
	}
	//assert(points.size() == 360.f / delta);
	debugShaderQueue.push_back([=](){debugShader->drawPoints(points, color, camera->getViewMatrix()); });
	points.clear();
	for (float phi = 0.f; phi < 360.f; phi +=delta)
	{
		float x = radius * cos(radians(0.f)) * cos(radians(phi));
		points.push_back(x + center.x);
		float y = radius * sin(radians(phi));
		points.push_back(y + center.y);
		float z = radius * sin(radians(0.f)) * cos(radians(phi));
		points.push_back(z + center.z);
	}
	//assert(points.size() == 360.f / delta);
	debugShaderQueue.push_back([=](){debugShader->drawPoints(points, color, camera->getViewMatrix()); });
	points.clear();
	for (float phi = -180.f; phi < 180.f; phi += delta)
	{
		float x = radius * cos(radians(90.f)) * cos(radians(phi));
		points.push_back(x + center.x);
		float y = radius * sin(radians(phi));
		points.push_back(y + center.y);
		float z = radius * sin(radians(90.f)) * cos(radians(phi));
		points.push_back(z + center.z);
	}
	//assert(points.size() == 360.f / delta);
	debugShaderQueue.push_back([=](){debugShader->drawPoints(points, color, camera->getViewMatrix()); });
	glUseProgram(0);
}