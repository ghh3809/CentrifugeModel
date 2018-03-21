#include <stdio.h>
#include <stdlib.h>

//#include <vector>
#include <algorithm>

#include <GL/glew.h>

#include <glfw3.h>
GLFWwindow* window;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
using namespace glm;


#include "shader.hpp"
#include "texture.hpp"
#include "controls.hpp"

// CPU representation of a particle
struct Particle {
	glm::vec3 pos, speed;
	unsigned char r, g, b, a; // Color
	float size, angle, weight;
	float life; // Remaining life of the particle. if <0 : dead and unused.
	float cameradistance; // *Squared* distance to the camera. if dead : -1.0f

	bool operator<(const Particle& that) const {
		// Sort in reverse order : far particles drawn first.
		return this->cameradistance > that.cameradistance;
	}
};
// ********** 整体控制部分 **********
const int MaxParticles = 5000;						// 爆炸颗粒数
const float timeRatio = 0.1f;						// 时间比例
const float centrifugeSpeed = 8.0f;					// 离心机转速(rad/s)
const float boomSpeed = 20.0f;						// 爆炸速率(m/s)
const float gravityAcceleraion = 9.81f * 1;			// 重力加速度
const float frictionCoefficient = 0.01f * 0;		// 摩擦系数
// ********** 整体控制部分 **********

Particle ParticlesContainer[MaxParticles];
bool startFlag = false; // Simulation start flag. If TRUE, simulation will begin

// OpenGL keyboard callback function
void onKey(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action != GLFW_PRESS) return;
	switch (key) {
	case GLFW_KEY_SPACE:
		startFlag = !startFlag;
		break;
	case GLFW_KEY_ENTER:
		setNoninertialFlag(!getNoninertialFlag());
		break;
	}
	

}

// OpenGL mouse button callback function
void onMouse(GLFWwindow* window, int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == 0) {
			setRotateFlag(false);
		}
		else if (action == 1) {
			setRotateFlag(true);
		}
	} else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		if (action == 0) {
			setMoveFlag(false);
		}
		else if (action == 1) {
			setMoveFlag(true);
		}
	}
}

// OpenGL scroll callback function
void onScroll(GLFWwindow* window, double xoffset, double yoffset) {
	AddScrollOffset(yoffset);
}

void SortParticles() {
	std::sort(&ParticlesContainer[0], &ParticlesContainer[MaxParticles]);
}

int main(void)
{
	// Initialise GLFW
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		getchar();
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow(1024, 768, "Gravity", NULL, NULL);
	if (window == NULL) {
		fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n");
		getchar();
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		getchar();
		glfwTerminate();
		return -1;
	}

	// Ensure we can capture the key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, GL_TRUE);

	glfwSetKeyCallback(window, onKey);
	glfwSetMouseButtonCallback(window, onMouse);
	glfwSetScrollCallback(window, onScroll);

	// Black background
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS);


	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);


	// Create and compile our GLSL program from the shaders
	GLuint programID = LoadShaders("Particle.vertexshader", "Particle.fragmentshader");

	// Vertex shader
	GLuint CameraRight_worldspace_ID = glGetUniformLocation(programID, "CameraRight_worldspace");
	GLuint CameraUp_worldspace_ID = glGetUniformLocation(programID, "CameraUp_worldspace");
	GLuint ViewProjMatrixID = glGetUniformLocation(programID, "VP");

	// fragment shader
	GLuint TextureID = glGetUniformLocation(programID, "myTextureSampler");
	GLuint Texture = loadDDS("particle.DDS");


	static GLfloat* g_particule_position_size_data = new GLfloat[MaxParticles * 4];
	static GLubyte* g_particule_color_data = new GLubyte[MaxParticles * 4];

	// The VBO containing the 4 vertices of the particles.
	// Thanks to instancing, they will be shared by all particles.
	static const GLfloat g_vertex_buffer_data[] = {
		-0.5f, -0.5f, 0.0f,
		0.5f, -0.5f, 0.0f,
		-0.5f,  0.5f, 0.0f,
		0.5f,  0.5f, 0.0f,
	};
	GLuint billboard_vertex_buffer;
	glGenBuffers(1, &billboard_vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, billboard_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

	// The VBO containing the positions and sizes of the particles
	GLuint particles_position_buffer;
	glGenBuffers(1, &particles_position_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, particles_position_buffer);
	// Initialize with empty (NULL) buffer : it will be updated later, each frame.
	glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);

	// The VBO containing the colors of the particles
	GLuint particles_color_buffer;
	glGenBuffers(1, &particles_color_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, particles_color_buffer);
	// Initialize with empty (NULL) buffer : it will be updated later, each frame.
	glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLubyte), NULL, GL_STREAM_DRAW);


	// Create and compile our GLSL program from the shaders
	GLuint programID2 = LoadShaders("Line.vertexshader", "SimpleFragmentShader.fragmentshader");

	// Vertex shader
	GLuint MatrixID2 = glGetUniformLocation(programID2, "MVP");

	GLuint vertexbuffer2;
	GLfloat g_vertex_buffer_data2[18] = {0,0,0, 1,0,0, 0,0,0, 0,1,0, 0,0,0, 0,0,1};

	glGenBuffers(1, &vertexbuffer2);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer2);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data2), g_vertex_buffer_data2, GL_STATIC_DRAW);

	float radius = getCentrifugeRadius();
	float angle = getCentrifugeAngle();

	for (int i = 0; i < MaxParticles - 1; i++) {
		int particleIndex = i;
		
		ParticlesContainer[particleIndex].pos = glm::vec3(radius*sin(angle), radius*cos(angle), 0);

		// Generate a random color
		ParticlesContainer[particleIndex].r = rand() % 256;
		ParticlesContainer[particleIndex].g = rand() % 256;
		ParticlesContainer[particleIndex].b = rand() % 256;
		ParticlesContainer[particleIndex].a = rand() % 256;

		ParticlesContainer[particleIndex].size = (rand() % 1000) / 2000.0f + 0.1f;
		ParticlesContainer[particleIndex].size = 0.2f;
		ParticlesContainer[particleIndex].life = 1000.0f; // This particle will live 5 seconds.

	}

	ParticlesContainer[MaxParticles - 1].pos = glm::vec3(0, 0, 0);
	ParticlesContainer[MaxParticles - 1].speed = glm::vec3(0, 0, 0);
	ParticlesContainer[MaxParticles - 1].r = 255;
	ParticlesContainer[MaxParticles - 1].g = 255;
	ParticlesContainer[MaxParticles - 1].b = 255;
	ParticlesContainer[MaxParticles - 1].a = 255;
	ParticlesContainer[MaxParticles - 1].size = 0.2f;
	ParticlesContainer[MaxParticles - 1].life = 1000.0f;


	double lastTime = glfwGetTime();
	bool boomFlag = false;
	do
	{
		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		double currentTime = glfwGetTime();
		double delta = (currentTime - lastTime) * timeRatio;
		lastTime = currentTime;


		computeMatricesFromInputs();
		glm::mat4 ProjectionMatrix = getProjectionMatrix();
		glm::mat4 ViewMatrix = getViewMatrix();

		// We will need the camera's position in order to sort the particles
		// w.r.t the camera's distance.
		// There should be a getCameraPosition() function in common/controls.cpp, 
		// but this works too.
		glm::vec3 CameraPosition(getCameraPosition());

		glm::mat4 ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;

		// Simulate all particles
		int ParticlesCount = 0;
		angle += centrifugeSpeed * (float)delta;
		setCentrifugeAngle(angle);
		if (!boomFlag && startFlag) {
			for (int i = 0; i < MaxParticles; i++) {
				Particle& p = ParticlesContainer[i]; // shortcut

				float speed = boomSpeed * pow((rand() % 10000) / 10000.0f, 0.3);
				float longitude = 2.0f * 3.1416f * (rand() % 10000) / 10000.0f;
				float latitude = acos((rand() % 20000 - 10000.0f) / 10000.0f);

				glm::vec3 randomdir = glm::vec3(
					speed * sin(longitude) * sin(latitude),
					speed * cos(longitude) * sin(latitude),
					speed * cos(latitude)
				);

				p.speed += randomdir;
			}
			boomFlag = true;
		}
		for (int i = 0; i < MaxParticles; i++) {
			Particle& p = ParticlesContainer[i]; // shortcut
			if (p.life > 0.0f) {
				p.life -= delta;
				if (p.life > 0.0f) {

					if (i != MaxParticles - 1) {
						if (!startFlag) {
							p.speed = centrifugeSpeed * radius * glm::vec3(cos(angle), -sin(angle), 0);
							p.pos = glm::vec3(radius*sin(angle), radius*cos(angle), 0);
							p.cameradistance = glm::length2(p.pos - CameraPosition);
						} else {
							glm::vec3 startSpeed = p.speed;
							glm::vec3 boxSpeed = centrifugeSpeed * radius * glm::vec3(cos(angle), -sin(angle), 0);
							glm::vec3 relativeSpeed = startSpeed - boxSpeed;
							float relativeSpeedValue = sqrt(relativeSpeed[0] * relativeSpeed[0] + relativeSpeed[1] * relativeSpeed[1] + relativeSpeed[2] * relativeSpeed[2]);
							glm::vec3 gravity = glm::vec3(0.0f, 0.0f, -gravityAcceleraion); // Gravity acceleration
							glm::vec3 friction = (float)(frictionCoefficient * relativeSpeedValue / p.size) * startSpeed; // Friction acceleration, f=kSv^2, where we set k=0.01
							glm::vec3 endSpeed = startSpeed + (float)delta * (gravity + friction);
							glm::vec3 aveSpeed = (startSpeed + endSpeed) * 0.5f;
							p.speed = endSpeed;
							p.pos += p.speed * (float)delta;
							p.cameradistance = glm::length2(p.pos - CameraPosition);
						}
					}

					// Fill the GPU buffer
					g_particule_position_size_data[4 * ParticlesCount + 0] = p.pos.x;
					g_particule_position_size_data[4 * ParticlesCount + 1] = p.pos.y;
					g_particule_position_size_data[4 * ParticlesCount + 2] = p.pos.z;
					g_particule_position_size_data[4 * ParticlesCount + 3] = p.size;

					g_particule_color_data[4 * ParticlesCount + 0] = p.r;
					g_particule_color_data[4 * ParticlesCount + 1] = p.g;
					g_particule_color_data[4 * ParticlesCount + 2] = p.b;
					g_particule_color_data[4 * ParticlesCount + 3] = p.a;

				}
				else {
					// Particles that just died will be put at the end of the buffer in SortParticles();
					p.cameradistance = -1.0f;
				}

				ParticlesCount++;

			}
		}

		SortParticles();


		//printf("%d ",ParticlesCount);


		// Update the buffers that OpenGL uses for rendering.
		// There are much more sophisticated means to stream data from the CPU to the GPU, 
		// but this is outside the scope of this tutorial.
		// http://www.opengl.org/wiki/Buffer_Object_Streaming

		
		glBindBuffer(GL_ARRAY_BUFFER, particles_position_buffer);
		glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW); // Buffer orphaning, a common way to improve streaming perf. See above link for details.
		glBufferSubData(GL_ARRAY_BUFFER, 0, ParticlesCount * sizeof(GLfloat) * 4, g_particule_position_size_data);

		glBindBuffer(GL_ARRAY_BUFFER, particles_color_buffer);
		glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLubyte), NULL, GL_STREAM_DRAW); // Buffer orphaning, a common way to improve streaming perf. See above link for details.
		glBufferSubData(GL_ARRAY_BUFFER, 0, ParticlesCount * sizeof(GLubyte) * 4, g_particule_color_data);


		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// Use our shader
		glUseProgram(programID);

		// Bind our texture in Texture Unit 0
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, Texture);
		// Set our "myTextureSampler" sampler to use Texture Unit 0
		glUniform1i(TextureID, 0);

		// Same as the billboards tutorial
		glUniform3f(CameraRight_worldspace_ID, ViewMatrix[0][0], ViewMatrix[1][0], ViewMatrix[2][0]);
		glUniform3f(CameraUp_worldspace_ID, ViewMatrix[0][1], ViewMatrix[1][1], ViewMatrix[2][1]);

		glUniformMatrix4fv(ViewProjMatrixID, 1, GL_FALSE, &ViewProjectionMatrix[0][0]);

		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, billboard_vertex_buffer);
		glVertexAttribPointer(
			0,                  // attribute. No particular reason for 0, but must match the layout in the shader.
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
		);

		// 2nd attribute buffer : positions of particles' centers
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, particles_position_buffer);
		glVertexAttribPointer(
			1,                                // attribute. No particular reason for 1, but must match the layout in the shader.
			4,                                // size : x + y + z + size => 4
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
		);

		// 3rd attribute buffer : particles' colors
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, particles_color_buffer);
		glVertexAttribPointer(
			2,                                // attribute. No particular reason for 1, but must match the layout in the shader.
			4,                                // size : r + g + b + a => 4
			GL_UNSIGNED_BYTE,                 // type
			GL_TRUE,                          // normalized?    *** YES, this means that the unsigned char[4] will be accessible with a vec4 (floats) in the shader ***
			0,                                // stride
			(void*)0                          // array buffer offset
		);

		// These functions are specific to glDrawArrays*Instanced*.
		// The first parameter is the attribute buffer we're talking about.
		// The second parameter is the "rate at which generic vertex attributes advance when rendering multiple instances"
		// http://www.opengl.org/sdk/docs/man/xhtml/glVertexAttribDivisor.xml
		glVertexAttribDivisor(0, 0); // particles vertices : always reuse the same 4 vertices -> 0
		glVertexAttribDivisor(1, 1); // positions : one per quad (its center)                 -> 1
		glVertexAttribDivisor(2, 1); // color : one per quad                                  -> 1

									 // Draw the particules !
									 // This draws many times a small triangle_strip (which looks like a quad).
									 // This is equivalent to :
									 // for(i in ParticlesCount) : glDrawArrays(GL_TRIANGLE_STRIP, 0, 4), 
									 // but faster.
		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, ParticlesCount);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);


		glUseProgram(programID2);
		glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &ViewProjectionMatrix[0][0]);

		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(9);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer2);
		glVertexAttribPointer(
			9,                  // attribute. No particular reason for 0, but must match the layout in the shader.
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
		);

		// Draw the line !
		glDisable(GL_BLEND);
		//glDisable(GL_LINE_SMOOTH);
		glLineWidth(3.0f);
		glDrawArrays(GL_LINES, 0, 6); // 2*3 indices starting at 0 -> 2 triangles

		glDisableVertexAttribArray(9);

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
		glfwWindowShouldClose(window) == 0);


	delete[] g_particule_position_size_data;

	// Cleanup VBO and shader
	glDeleteBuffers(1, &particles_color_buffer);
	glDeleteBuffers(1, &particles_position_buffer);
	glDeleteBuffers(1, &billboard_vertex_buffer);
	glDeleteProgram(programID);
	glDeleteTextures(1, &Texture);
	glDeleteVertexArrays(1, &VertexArrayID);


	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}
