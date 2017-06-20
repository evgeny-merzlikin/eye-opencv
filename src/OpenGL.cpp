#include "GLFWMain.h"


static void error_callback(int error, const char* description)
{
	MessageBox(0, description, "Error", MB_OK);
}


void initOpenGL() 
{

	// Setup window
	glfwSetErrorCallback(error_callback);

	if (!glfwInit())
		exit(1);

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);

	glfwWindowHint(GLFW_RED_BITS, mode->redBits);
	glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
	glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
	glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

	bool fullscreen = false;
	if (fullscreen)
		window = glfwCreateWindow(mode->width, mode->height, "PS3Eye OpenCV2 example", monitor, nullptr);
	else
		window = glfwCreateWindow(800, 600, "PS3Eye OpenCV2 example", nullptr, nullptr);

	glfwMakeContextCurrent(window);

	// wait for vsync
	glfwSwapInterval(5);


	ImGui_ImplGlfw_Init(window, true);

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 25.0f);

}

void terminateOpenGL()
{

	ImGui_ImplGlfw_Shutdown();
	glfwTerminate();

}

void render()
{

	// Rendering
	int display_w, display_h;
	glfwGetFramebufferSize(window, &display_w, &display_h);

	glEnable( GL_TEXTURE_2D );

	glViewport(0, 0, display_w, display_h);
	glMatrixMode( GL_PROJECTION ); 
	glLoadIdentity(); 
	glOrtho( 0.0, display_w, display_h, 0.0, 1.0, -1.0 ); 

	//Initialize Modelview Matrix 
	glMatrixMode( GL_MODELVIEW ); 
	glLoadIdentity(); 

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glTranslatef( 0.0f, 0.0f, 0.0f );

	grayTexture.glRenderTexture((float)display_w, (float)display_h );  
	rgbTexture.glRenderTexture((float)display_w, (float)display_h );

	// render GUI
	ImGui::Render();

	// finally swap buffers
	glfwSwapBuffers(window);

}