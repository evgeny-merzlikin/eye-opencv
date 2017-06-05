#include <stdlib.h>     // atoi
#include <thread>
#include <atomic>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "ps3eye.h"
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "GL/glew.h"
#include "GLFW/glfw3.h"

#define IM_ARRAYSIZE(_ARR)  ((int)(sizeof(_ARR)/sizeof(*_ARR)))
#define IM_MAX(_A,_B)       (((_A) >= (_B)) ? (_A) : (_B))
#define IM_F32_TO_INT8_SAT(_VAL)        ((int)(ImSaturate(_VAL) * 255.0f + 0.5f))               // Saturated, always output 0..255
static inline float  ImSaturate(float f)                                        { return (f < 0.0f) ? 0.0f : (f > 1.0f) ? 1.0f : f; }

using namespace ps3eye;
using namespace std;
using namespace cv;

// global structs

PS3EYECam::PS3EYERef eye;

struct PS3EyeParameters {
 
  bool autoGain;
  bool autoExposure;    
  bool autoWB;  
  bool horizontalFlip;  
  bool verticalFlip;    

  int width;
  int height;
  int fps;
  
  int gain;
  int redBalance;
  int blueBalance;
  int greenBalance;
  int exposure;    
  int sharpness;   
  int contrast;    
  int brightness;  
  int hue;         

  int view_mode;
  bool startStreaming;

  shared_ptr<Mat> bayerRAW8, bayerRAW16;

  void update( bool updateAll = false ) 
  {
    
    if ( updateAll ) {

      autoGain = eye->getAutogain();
      autoExposure = eye->getAutoExposure();
      autoWB = eye->getAutoWhiteBalance();
      gain = eye->getGain();  
      exposure = eye->getExposure();  
      blueBalance = eye->getBlueBalance();
      greenBalance = eye->getGreenBalance();
      redBalance = eye->getRedBalance();
      sharpness = eye->getSharpness();   
      contrast = eye->getContrast();    
      brightness = eye->getBrightness();  
      hue = eye->getHue();         
      horizontalFlip = eye->getFlipH();  
      verticalFlip = eye->getFlipV();    
      
      return;
    }
      
    if ( autoGain ) 
      gain = eye->getGain();  
    
    if ( autoExposure )
      exposure = eye->getExposure();  
    
    if ( autoWB ) {
      blueBalance = eye->getBlueBalance();
      greenBalance = eye->getGreenBalance();
      redBalance = eye->getRedBalance();
    }

  }
  
} cameraParameters;



struct OpenCVParameters {
	struct Pair { int min, max; };

	// binary threshold
	Pair luminance, hue, saturation, value;

} opencvParams;

GLFWwindow* window;

struct GLTexture {
	GLTexture(): imagePtr(nullptr) {}

  void init(int _width, int _height, int _channels, GLint _format, GLenum _type) 
  {
    dataWidth = _width;
    dataHeight = _height; 
    format = _format;
	type = _type;
	channels = _channels;
	GLenum err;

	switch (type) {
	case GL_UNSIGNED_BYTE:
		imagePtr = make_shared<Mat>(dataHeight, dataWidth, CV_MAKETYPE(CV_8U,channels));
		break;

	case GL_UNSIGNED_SHORT:
		imagePtr = make_shared<Mat>(dataHeight, dataWidth, CV_MAKETYPE(CV_16U,channels));
		break;
	}

	auto pow2 = [](unsigned v) { int p = 2; while (v >>= 1) p <<= 1; return p; };
    POTWidth = std::max(dataWidth, pow2(dataWidth));
    POTHeight = std::max(dataHeight, pow2(dataHeight));
    u = (float) dataWidth / (float) POTWidth;
    v = (float) dataHeight / (float) POTHeight;
    
    glGenTextures( 1, &ID );
    glBindTexture( GL_TEXTURE_2D, ID ); 
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ); 

	void* pixels = nullptr;
	if (type == GL_UNSIGNED_BYTE) 
		pixels = calloc(POTWidth * POTHeight * channels, sizeof(uint8_t));
	else if (type == GL_UNSIGNED_SHORT) 
		pixels = calloc(POTWidth * POTHeight * channels, sizeof(uint16_t));

	//}  else  {
	//	std::vector<GLshort> pixels(POTWidth * POTHeight * channels);
	//}

	GLint internalFormat = GL_RGBA;
	if (format == GL_LUMINANCE)
		internalFormat = GL_LUMINANCE;
	
	glTexImage2D( GL_TEXTURE_2D, 0, internalFormat, POTWidth, POTHeight, 0, format, type, pixels );
	if (pixels)
		free(pixels);
	err = glGetError();
    glBindTexture( GL_TEXTURE_2D, 0 );
  }
  
  
  void glUpdate()
  {
	if ( imagePtr && displayTexture ) {
		glBindTexture( GL_TEXTURE_2D, ID );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, dataWidth, dataHeight, format, type, imagePtr->data ); 
		glBindTexture( GL_TEXTURE_2D, 0 );
	}
  }
  
  void glRenderTexture(float width, float height)
  {
    if ( imagePtr && displayTexture ) {
		glBindTexture( GL_TEXTURE_2D, ID );
		glBegin( GL_QUADS ); 
		  glTexCoord2f( 0.f, 0.f ); glVertex2f( 0.0f, 0.0f ); 
		  glTexCoord2f( u, 0.f ); glVertex2f( width, 0.0f ); 
		  glTexCoord2f( u, v ); glVertex2f( width, height); 
		  glTexCoord2f( 0.f, v ); glVertex2f( 0.0f, height ); 
		glEnd();
		glBindTexture( GL_TEXTURE_2D, 0 );
	}
  }
  
  void glDestroyTexture()
  {
    if ( ID )
      glDeleteTextures( 1, &ID );  
  }
  
  // closest point-of-two width and height
  int POTWidth, POTHeight; 
  // pixel buffer width and height
  int dataWidth, dataHeight; 
  int channels;
  // clamped [0-1] 
  //  u = (float) dataWidth / POTWidth
  //  v = (float) dataHeight / POTHeight
  GLfloat u, v;

  GLint format;
  GLenum type;
  GLuint ID;    
  shared_ptr<Mat> imagePtr;
  bool displayTexture;

} grayTexture, rgbTexture;

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
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
  
  bool fullscreen = false;
  if (fullscreen)
	window = glfwCreateWindow(mode->width, mode->height, "PS3Eye OpenCV2 example", monitor, nullptr);
  else
	window = glfwCreateWindow(800, 600, "PS3Eye OpenCV2 example", nullptr, nullptr);
  
  glfwMakeContextCurrent(window);
  
  ImGui_ImplGlfw_Init(window, true);
  ImGuiIO& io = ImGui::GetIO();
  io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 25.0f);
  
}

void terminateOpenGL()
{
  ImGui_ImplGlfw_Shutdown();
  glfwTerminate();
}

void createGUI()
{
  // camera not connected or not streaming
  if ( !eye ) {
	ImGui::Begin("Please connect PS3 EYE camera");
    ImGui::Text("Please connect PS3 EYE camera to PC to start streaming\n\n");
	ImGui::End();
    return;
  }
  // camera connected and not streaming
  if ( eye && !eye->isStreaming() ) {
    ImGui::Begin("Select PS3 EYE camera settings");
    ImGui::Text("Please select camera parameters to start streaming:\n\n");
    // camera resolution
    static int camera_res = 0; // VGA
    static int camera_fps = 10;
    if ( ImGui::Combo("Camera resolution", &camera_res, "VGA\0QVGA\0") )
		camera_fps = 10;
    // camera fps
	const char* vga_rates[] = { "2", "3", "5", "8", "10", "15", "20", "25", "30", "40", "50", "60", "75", "83" };    
    const char* qvga_rates[] = { "2", "3", "5", "7", "10", "12", "15", "17", "30", "37", "40", "50", "60", "75", "90", "100", "125", "137", "150", "187", "205", "290" };
    if ( camera_res == 0 ) // VGA
      ImGui::Combo("Camera fps: ", &camera_fps, vga_rates, IM_ARRAYSIZE(vga_rates));
    if ( camera_res == 1 ) // QVGA
      ImGui::Combo("Camera fps: ", &camera_fps, qvga_rates, IM_ARRAYSIZE(qvga_rates));
    // start streaming button
    if ( ImGui::Button("Start streaming") ) {
      switch (camera_res) {
      case 0: // VGA
        cameraParameters.width = 640;
        cameraParameters.height = 480;
        cameraParameters.fps = atoi( vga_rates[camera_fps] );
        break;
      case 1: // QVGA
        cameraParameters.width = 320;
        cameraParameters.height = 240;
        cameraParameters.fps = atoi( qvga_rates[camera_fps] );
        break;
      }
	  cameraParameters.startStreaming = true;     
    } // ImGui::Button("Start streaming")
    ImGui::End(); // end modal dialog
    return;
  }
  // camera connected and streaming
  if ( eye && eye->isStreaming() ) {
    // show camera parameters window
    ImGui::Begin("PS3EYE parameters");
    {
    	int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		ImGui::Text("Display: %d x %d, fps %.0f", display_w, display_h, ImGui::GetIO().Framerate);
		ImGui::Text("Camera: %d x %d, fps %d", cameraParameters.width, cameraParameters.height, cameraParameters.fps);
		ImGui::Combo("Mode", &cameraParameters.view_mode, "Gray\0Color\0Luminance threshold\0Color threshold (HSV)\0");
      if ( ImGui::Checkbox("Autogain", &cameraParameters.autoGain) )
        eye->setAutogain(cameraParameters.autoGain);
      if ( ImGui::Checkbox("Auto white balance", &cameraParameters.autoWB) )
        eye->setAutoWhiteBalance(cameraParameters.autoWB);
      if ( ImGui::Checkbox("Auto exposure", &cameraParameters.autoExposure) )
        eye->setAutoExposure(cameraParameters.autoExposure);
      if ( ImGui::Checkbox("Horizontal flip", &cameraParameters.horizontalFlip) )
        eye->setFlip( cameraParameters.horizontalFlip, cameraParameters.verticalFlip );
      if ( ImGui::Checkbox("Vertical flip", &cameraParameters.verticalFlip) )
        eye->setFlip( cameraParameters.horizontalFlip, cameraParameters.verticalFlip );
      if ( ImGui::SliderInt("Gain", &cameraParameters.gain, 0, 63) )
        eye->setGain((uint8_t)cameraParameters.gain);
      if ( ImGui::SliderInt("Exposure", &cameraParameters.exposure, 0, 255) )
        eye->setExposure((uint8_t)cameraParameters.exposure);
      if ( ImGui::SliderInt("Red", &cameraParameters.redBalance, 0, 255) )
        eye->setRedBalance((uint8_t)cameraParameters.redBalance);
      if ( ImGui::SliderInt("Blue", &cameraParameters.blueBalance, 0, 255) )
        eye->setBlueBalance((uint8_t)cameraParameters.blueBalance);
      if ( ImGui::SliderInt("Green", &cameraParameters.greenBalance, 0, 255) )
        eye->setGreenBalance((uint8_t)cameraParameters.greenBalance);
      if ( ImGui::SliderInt("Sharpness", &cameraParameters.sharpness, 0, 63) )   
        eye->setSharpness((uint8_t)cameraParameters.sharpness);
      if ( ImGui::SliderInt("Contrast", &cameraParameters.contrast, 0, 255) )    
        eye->setContrast((uint8_t)cameraParameters.contrast);
      if ( ImGui::SliderInt("Brightness", &cameraParameters.brightness, 0, 255) )  
        eye->setBrightness((uint8_t)cameraParameters.brightness);
      if ( ImGui::SliderInt("Hue", &cameraParameters.hue, 0, 255) )         
        eye->setHue((uint8_t)cameraParameters.hue);
    }
    ImGui::End(); // PS3EYE parameters window
    
	// Luminance threshold
	if ( cameraParameters.view_mode == 2 ) { 
		ImGui::Begin("Luminance Threshold");
		ImGui::DragIntRange2("Luminance", &opencvParams.luminance.min, &opencvParams.luminance.max);
		ImGui::End(); 
	}

	// Color threshold (HSV)
	if ( cameraParameters.view_mode == 3 ) { 
		ImGui::Begin("Color Threshold (HSV)");
		ImGui::DragIntRange2("Hue", &opencvParams.hue.min, &opencvParams.hue.max);
		ImGui::DragIntRange2("Saturation", &opencvParams.saturation.min, &opencvParams.saturation.max);
		ImGui::DragIntRange2("Value", &opencvParams.value.min, &opencvParams.value.max);
		
		
		
		
		float hsv1[3] = { 
			(float)opencvParams.hue.min / 255.0f, 
			(float)opencvParams.saturation.min / 255.0f,
			(float)opencvParams.value.min / 255.0f 
		};
        float hsv2[3] = { 
			(float)opencvParams.hue.max / 255.0f, 
			(float)opencvParams.saturation.max / 255.0f,
			(float)opencvParams.value.max / 255.0f 
		};
		float rgb1[3] = {0.0f, 0.0f, 0.0f};
		float rgb2[3] = {0.0f, 0.0f, 0.0f};
		ImGui::ColorConvertHSVtoRGB( hsv1[0], hsv1[1], hsv1[2], rgb1[0], rgb1[1], rgb1[2] );
		ImGui::ColorConvertHSVtoRGB( hsv2[0], hsv2[1], hsv2[2], rgb2[0], rgb2[1], rgb2[2] );

        ImGui::ColorEdit3("Min HSV", rgb1);
		ImGui::ColorEdit3("Max HSV", rgb2);
		ImGui::End(); 
	}
	return;
  } // end if ( eye && eye->isStreaming() ) 
}

void render()
{
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

void findCamera()
{
  vector<PS3EYECam::PS3EYERef> devices = PS3EYECam::getDevices(true);
  if ( devices.size() > 0 ) 
    eye = devices.at(0);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

int main(int, char**)
{
    double time = 0;
	void* buf = nullptr;
	
	// init opengl and imgui
    initOpenGL();
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
        
		if ( cameraParameters.startStreaming ) {
			eye->init( cameraParameters.width, cameraParameters.height, (uint16_t) cameraParameters.fps, PS3EYECam::EOutputFormat::RAW8 );
			//buf = (void*)calloc(cameraParameters.width * cameraParameters.height * eye->getOutputBytesPerPixel(), sizeof(uint8_t));
			rgbTexture.init( cameraParameters.width, cameraParameters.height, 3, GL_RGB, GL_UNSIGNED_BYTE);
			grayTexture.init( cameraParameters.width, cameraParameters.height, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE);
			cameraParameters.bayerRAW8 = make_shared<Mat>(cameraParameters.height, cameraParameters.width, CV_MAKETYPE(CV_8U, 1));

			eye->start(); 
			cameraParameters.update( true ); // refresh all camera parameters
			cameraParameters.startStreaming = false;
		}

		if ( eye && eye->isStreaming() ) {
			
			rgbTexture.displayTexture = false;
			grayTexture.displayTexture = false;

			eye->getFrame( (uint8_t*)cameraParameters.bayerRAW8->data );
			//eye->getFrame( (uint8_t*)buf );

			switch ( cameraParameters.view_mode) { 
			case 0: // Gray
				grayTexture.displayTexture = true;
				rgbTexture.displayTexture = false;
				cvtColor( *cameraParameters.bayerRAW8, *grayTexture.imagePtr, COLOR_BayerGB2GRAY );

				break;

			case 1: // color
				grayTexture.displayTexture = false;
				rgbTexture.displayTexture = true;
				cvtColor( *cameraParameters.bayerRAW8, *rgbTexture.imagePtr, COLOR_BayerGB2RGB );

				break;

			case 2: // luminance threshold
				grayTexture.displayTexture = true;
				rgbTexture.displayTexture = false;
				cvtColor( *cameraParameters.bayerRAW8, *grayTexture.imagePtr, COLOR_BayerGB2GRAY );
				threshold(*grayTexture.imagePtr, *grayTexture.imagePtr, opencvParams.luminance.min, opencvParams.luminance.max, THRESH_TOZERO);
				break;

			case 3: // color threshold
				
				grayTexture.displayTexture = true;
				rgbTexture.displayTexture = false;
				cvtColor( *cameraParameters.bayerRAW8, *rgbTexture.imagePtr, COLOR_BayerGB2RGB );
				Mat HSVImage = Mat( rgbTexture.imagePtr->rows, rgbTexture.imagePtr->cols, CV_MAKETYPE(CV_8U, 3));
				cvtColor( *rgbTexture.imagePtr, HSVImage, COLOR_RGB2HSV);
				inRange( HSVImage, Scalar(opencvParams.hue.min, opencvParams.saturation.min, opencvParams.value.min), Scalar(opencvParams.hue.max, opencvParams.saturation.max, opencvParams.value.max), *grayTexture.imagePtr);
				break;


			} 
			
			grayTexture.glUpdate();
			rgbTexture.glUpdate();
		}
        render();
    }
	if( buf )
		free(buf);

	if ( eye && eye->isStreaming() )
		eye->stop();
    
	grayTexture.glDestroyTexture();
	rgbTexture.glDestroyTexture();
    terminateOpenGL();
    return 0;
}
