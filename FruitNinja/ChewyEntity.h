#include "GameEntity.h"
#include "ChewyMovementComponent.h"

class ChewyEntity : public GameEntity
{
public:
	ChewyEntity();
	ChewyEntity(glm::vec3 position, std::shared_ptr<MeshSet> mesh, std::shared_ptr<Camera> camera);
	void update();
private:
	ChewyMovementComponent moveComponent;
};