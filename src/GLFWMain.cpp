#include "GLFWMain.h"
namespace cv {};;
// global vars

OpenCVParameters opencvParams;
PS3EyeParameters cameraParameters;
ps3eye::PS3EYECam::PS3EYERef eye;
GLFWwindow* window;
LPVM_MIDI_PORT MIDIport;
std::mutex mtx;  
int framerate;
int framecount;
GLTexture grayTexture, rgbTexture;

void startCamera()
{


}

void findCamera()
{
	std::vector<ps3eye::PS3EYECam::PS3EYERef> devices = ps3eye::PS3EYECam::getDevices(true);

	if ( devices.size() > 0 ) {
		eye = devices.at(0);
		startCamera();
	}
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}


int main(int, char**)
{
	double time = 0;

	// init opengl and imgui
	initOpenGL();

	// create midi port
	createMIDIPort();

	//set keyboard callback
	glfwSetKeyCallback(window, key_callback);

	// create ps3 class
	findCamera();

	// Main loop
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		ImGui_ImplGlfw_NewFrame();
		if ( glfwGetTime() - time >= 1.0 ) {
			if ( !eye ) 
				findCamera();

			if ( eye && eye->isStreaming() ) 
				cameraParameters.update();

			time = glfwGetTime();
		}

		createGUI();
		grayTexture.glUpdate();
		rgbTexture.glUpdate();
		render();
		
	}
	
	if ( eye && eye->isStreaming() )
		eye->stop();

	grayTexture.glDestroyTexture();
	rgbTexture.glDestroyTexture();

	terminateOpenGL();
	destroyMIDIPort();


	return 0;
}
