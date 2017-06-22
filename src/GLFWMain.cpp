#include "GLFWMain.h"

// global vars

OpenCVParameters opencvParams;
PS3EyeParameters cameraParameters;
GUIParameters guiParams;
ps3eye::PS3EYECam::PS3EYERef eye;
GLFWwindow* window;
LPVM_MIDI_PORT MIDIport;
GLTexture grayTexture, rgbTexture;

void startCamera()
{
		if ( !eye )
			return;
			
		eye->init( cameraParameters.width, cameraParameters.height, (uint16_t) cameraParameters.fps, ps3eye::PS3EYECam::EOutputFormat::RAW8 );

		eye->start(); // start camera thread
		cameraParameters.update( true ); // refresh all camera parameters

		// init and create texture
		grayTexture.init( eye->getWidth(), eye->getHeight(), GL_LUMINANCE );
		rgbTexture.init( eye->getWidth(), eye->getHeight(), GL_RGB );

		grayTexture.glCreateTexture();
		rgbTexture.glCreateTexture();

		// start opencv thread
		startOpenCVThread();

}

void stopCamera()
{
	if ( !eye )
		return;

	if ( eye->isStreaming() ) {
		eye->stop();
		eye.reset();
		grayTexture.glDestroyTexture();
		rgbTexture.glDestroyTexture();
	}

}

void findCamera()
{
	std::vector<ps3eye::PS3EYECam::PS3EYERef> devices = ps3eye::PS3EYECam::getDevices(true);

	if ( !eye ) {
		if ( devices.size() > 0 ) {
			eye = devices.at(0);
		}
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
	
	stopCamera();



	terminateOpenGL();
	destroyMIDIPort();

	return 0;
}
