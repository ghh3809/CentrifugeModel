#ifndef CONTROLS_HPP
#define CONTROLS_HPP

void computeMatricesFromInputs();
glm::mat4 getViewMatrix();
glm::mat4 getProjectionMatrix();
glm::vec3 getCameraPosition();
bool getRotateFlag();
void setRotateFlag(bool flag);
bool getMoveFlag();
void setMoveFlag(bool flag);
bool getNoninertialFlag();
void setNoninertialFlag(bool flag);
float getCentrifugeAngle();
void setCentrifugeAngle(float angle);
float getCentrifugeRadius();
void AddScrollOffset(double offset);

#endif