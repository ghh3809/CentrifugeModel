// Include GLFW
#include <glfw3.h>
extern GLFWwindow* window;

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include "controls.hpp"

glm::mat4 ViewMatrix;
glm::mat4 ProjectionMatrix;
glm::vec3 CameraPosition;
bool rotateFlag = false; // Rotation start flag. If TRUE, rotation will begin.
bool moveFlag = false; // Move start flag. If TRUE, move will begin.
bool noninertialFlag = false; // Noninertial view flag. If TRUE, the view will from the centrifuge.
float centrifugeAngle = 0.0f; // The position of centrifuge;
const float centrifugeRadius = 5.0f; // The radius of centrifuge;

glm::mat4 getViewMatrix() {
	return ViewMatrix;
}
glm::mat4 getProjectionMatrix() {
	return ProjectionMatrix;
}
glm::vec3 getCameraPosition() {
	return CameraPosition;
}
bool getRotateFlag() {
	return rotateFlag;
}
void setRotateFlag(bool flag) {
	rotateFlag = flag;
}
bool getMoveFlag() {
	return moveFlag;
}
void setMoveFlag(bool flag) {
	moveFlag = flag;
}
bool getNoninertialFlag() {
	return noninertialFlag;
}
void setNoninertialFlag(bool flag) {
	noninertialFlag = flag;
}
float getCentrifugeAngle() {
	return centrifugeAngle;
}
void setCentrifugeAngle(float angle) {
	centrifugeAngle = angle;
}
float getCentrifugeRadius() {
	return centrifugeRadius;
}

// Initial horizontal angle : toward -Z
float horizontalAngle = 3.14f;
// Initial vertical angle : none
float verticalAngle = 0.0f;
// Initial Field of View
float initialFoV = 45.0f;
// Initial view center point
glm::vec3 centerPoint = glm::vec3(0, 0, 0);
// Initial view distance
float viewDistance = 30.0f;

// ********** Control parameter of mouse and keyboard **********
float rotateSpeed = 0.005f;
float moveSpeed = 0.001f;
float distanceSpeed = 0.3f;
// ********** Control parameter of mouse and keyboard **********

void AddScrollOffset(double offset) {
	viewDistance -= distanceSpeed * offset;
}

void computeMatricesFromInputs() {

	// glfwGetTime and glfwGetCursorPos is called only once, the first time this function is called
	static bool firstCall = true;
	static double lastxpos, lastypos;
	if (firstCall) {
		glfwGetCursorPos(window, &lastxpos, &lastypos);
		firstCall = false;
	}

	// Get mouse position
	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);

	// Compute new orientation
	if (rotateFlag) {
		horizontalAngle += rotateSpeed * float(lastxpos - xpos);
		verticalAngle += rotateSpeed * float(lastypos - ypos);
	}

	// Direction : Spherical coordinates to Cartesian coordinates conversion
	// The default direction is (0, 0, -1)
	glm::vec3 direction(
		cos(verticalAngle) * sin(horizontalAngle),
		sin(verticalAngle),
		cos(verticalAngle) * cos(horizontalAngle)
	);

	// Right vector
	// The default value is (1, 0, 0)
	glm::vec3 right = glm::vec3(
		sin(horizontalAngle - 3.14f / 2.0f),
		0,
		cos(horizontalAngle - 3.14f / 2.0f)
	);

	// Up vector
	// The default value is (0, 1, 0)
	glm::vec3 up = glm::cross(right, direction);

	if (moveFlag) {
		float moveRight = float(lastxpos - xpos);
		float moveUp = float(lastypos - ypos);
		centerPoint += moveSpeed * viewDistance * (up * -moveUp + right * moveRight);
	}

	if (noninertialFlag) {
		centerPoint = glm::vec3(centrifugeRadius*sin(centrifugeAngle), centrifugeRadius*cos(centrifugeAngle), 0);
		CameraPosition = centerPoint + glm::vec3(0, 0, viewDistance);
		up = glm::vec3(-sin(centrifugeAngle), -cos(centrifugeAngle), 0);
	}
	
	// Projection matrix : 45?Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
	ProjectionMatrix = glm::perspective(glm::radians(initialFoV), 4.0f / 3.0f, 0.1f, 1000.0f);

	// Camera matrix
	CameraPosition = direction * (-viewDistance) + centerPoint;
	ViewMatrix = glm::lookAt(
		CameraPosition,           // Camera is here
		centerPoint, // and looks here : at the same position, plus "direction"
		up                  // Head is up (set to 0,-1,0 to look upside-down)
	);

	// Reset the position
	lastxpos = xpos;
	lastypos = ypos;
}