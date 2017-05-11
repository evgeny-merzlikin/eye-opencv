#include <stdlib.h>     // atoi

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "GLFW/glfw3.h"

#include "ps3eye.h"

#define HAVE_OPENCV_IMGPROC
#include "opencv2/opencv.hpp"

#define IM_ARRAYSIZE(_ARR)  ((int)(sizeof(_ARR)/sizeof(*_ARR)))
#define IM_MAX(_A,_B)       (((_A) >= (_B)) ? (_A) : (_B))


using namespace ps3eye;
using namespace std;
using namespace cv;

// global structs

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

SimpleBlobDetector::Params blobDetectorParams;

// Global vars

GLFWwindow* window;
PS3EYECam::PS3EYERef eye;

struct GLTexture {

  void init(int _width, int _height, GLint _format) 
  {
    
    dataWidth = _width;
    dataHeight = _height; 
    format = _format;

    if ( format == GL_LUMINANCE )
      channels = 1;
    if ( format == GL_RGB || format == GL_BGR )
      channels = 3;
    if ( format == GL_RGBA || format == GL_BGRA )
      channels = 4;
   
    auto pow2 = [](unsigned v) { int p = 2; while (v >>= 1) p <<= 1; return p; };

    POTWidth = std::max(dataWidth, pow2(dataWidth));
    POTHeight = std::max(dataHeight, pow2(dataHeight));

    u = (float) dataWidth / (float) POTWidth;
    v = (float) dataHeight / (float) POTHeight;
    
  }
  
  void glCreateTexture()
  {
    glGenTextures( 1, &ID );
    
    glBindTexture( GL_TEXTURE_2D, ID ); 
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ); 

    std::vector<GLubyte> pixels(POTWidth * POTHeight * channels);

    glTexImage2D( GL_TEXTURE_2D, 0, format, POTWidth, POTHeight, 0, format, GL_UNSIGNED_BYTE, &pixels[0] ); 

    glBindTexture( GL_TEXTURE_2D, 0 );

  }
  
  
  void glUpdate()
  {

    glBindTexture( GL_TEXTURE_2D, ID );

    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, dataWidth, dataHeight, format, GL_UNSIGNED_BYTE, data ); 
    
    glBindTexture( GL_TEXTURE_2D, 0 );
  
  }
  
  void glRenderTexture(float x, float y, float width, float height)
  {
    
    glBindTexture( GL_TEXTURE_2D, ID );

    glBegin( GL_QUADS ); 
      glTexCoord2f( 0.f, 0.f ); glVertex2f( x, y ); 
      glTexCoord2f( u, 0.f ); glVertex2f( x + w, y ); 
      glTexCoord2f( u, v ); glVertex2f( x + w, y + h); 
      glTexCoord2f( 0.f, v ); glVertex2f( x, y + h ); 
    glEnd();

    glBindTexture( GL_TEXTURE_2D, 0 );
  
  }
  
  void glDestroyTexture()
  {
    if ( ID != 0 )
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
  
  GLvoid* data;
  GLint format; // filled in Opencv thread
  GLuint ID;    // filled in create_textures

} texture;

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

  window = glfwCreateWindow(mode->width, mode->height, "PS3Eye OpenCV2 example", monitor, NULL);

  glfwMakeContextCurrent(window);
  
  // wait for vsync
  // glfwSwapInterval(1);
  
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
    // new popup (modal window)
    ImGui::BeginPopupModal("Please connect PS3 EYE camera");
    ImGui::Text("Please connect PS3 EYE camera to PC to start streaming\n\n");
    ImGui::EndPopup();
    return;
  }
  
  // camera connected and not streaming
  
  if ( eye && !eye->isStreaming() ) {

    ImGui::BeginPopupModal("Select PS3 EYE camera settings");
    
    ImGui::Text("Please select camera parameters to start streaming:\n\n");
    
    // camera resolution
    static int camera_res = 0; // VGA
    
    ImGui::Combo("Camera resolution", &camera_res, "VGA\0QVGA\0");
    
    // camera fps
        
    const char* vga_rates[] = { "2", "3", "5", "8", "10", "15", "20", "25", "30", "40", "50", "60", "75", "83" };    
    
    const char* qvga_rates[] = { "2", "3", "5", "7", "10", "12", "15", "17", "30", "37", "40", "50", "60", "75", "90", "100", "125", "137", "150", "187", "205", "290" };

    int camera_fps = 10;
    
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
      
      eye->init( cameraParameters.width, cameraParameters.height, (uint16_t) cameraParameters.fps, PS3EYECam::EOutputFormat::Bayer );
      
      eye->start(); // start camera thread
      cameraParameters.update( true ); // refresh all camera parameters
      
      // init and create texture
      texture.init( cameraParameters.width, cameraParameters.height, GL_BGR );
  
      texture.glCreateTexture();
     
      // start opencv thread
      startOpenCVThread();
      
    } // ImGui::Button("Start streaming")
    
    ImGui::EndPopup(); // end modal dialog
    return;
  }
  
  
  // camera connected and streaming
  if ( eye && eye->isStreaming() ) {
    
    // show camera parameters window
    ImGui::Begin("PS3EYE parameters");
    {
    
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
      
      if ( ImGui::SliderInt("camera gain", &cameraParameters.gain, 0, 63) )
        eye->setGain((uint8_t)cameraParameters.gain);
        
      if ( ImGui::SliderInt("camera exposure", &cameraParameters.exposure, 0, 255) )
        eye->setExposure((uint8_t)cameraParameters.exposure);
      
      if ( ImGui::SliderInt("camera red balance", &cameraParameters.redBalance, 0, 255) )
        eye->setRedBalance((uint8_t)cameraParameters.redBalance);
      
      if ( ImGui::SliderInt("camera blue balance", &cameraParameters.blueBalance, 0, 255) )
        eye->setBlueBalance((uint8_t)cameraParameters.blueBalance);
      
      if ( ImGui::SliderInt("camera green balance", &cameraParameters.greenBalance, 0, 255) )
        eye->setGreenBalance((uint8_t)cameraParameters.greenBalance);

      if ( ImGui::SliderInt("camera sharpness", &cameraParameters.sharpness, 0, 63) )   
        eye->setSharpness((uint8_t)cameraParameters.sharpness);
      
      if ( ImGui::SliderInt("camera contrast", &cameraParameters.contrast, 0, 255) )    
        eye->setContrast((uint8_t)cameraParameters.contrast);
      
      if ( ImGui::SliderInt("camera brightness", &cameraParameters.brightness, 0, 255) )  
        eye->setBrightness((uint8_t)cameraParameters.brightness);
      
      if ( ImGui::SliderInt("camera hue", &cameraParameters.hue, 0, 255) )         
        eye->setHue((uint8_t)cameraParameters.hue);
    }
    ImGui::End(); // PS3EYE parameters window
    
    // OpenCV Simple BLOB detector parameters window
    ImGui::Begin("OpenCV Simple BLOB detector");
    {    
      
      ImGui::DragFloatRange2("Threshold", &blobDetectorParams.minThreshold, &blobDetectorParams.maxThreshold);

      ImGui::DragFloat("Threshold step", &blobDetectorParams.thresholdStep);
      
      ImGui::DragInt("Minimum Repeatability", &blobDetectorParams.minRepeatability);
      
      ImGui::DragFloat("Minimum Distance Between Blobs", &blobDetectorParams.minDistBetweenBlobs);

      ImGui::Checkbox("Filter by color", &blobDetectorParams.filterByColor);
      
      int blobColor = blobDetectorParams.blobColor;
      ImGui::DragInt("BLOB Color", &blobColor);
      blobDetectorParams.blobColor = (uchar) blobColor;

      ImGui::Checkbox("Filter by area", &blobDetectorParams.filterByArea);
      ImGui::DragFloatRange2("Area min/max", &blobDetectorParams.minArea, &blobDetectorParams.maxArea);

      ImGui::Checkbox("Filter by circularity", &blobDetectorParams.filterByCircularity);
      ImGui::DragFloatRange2("Circularity min/max", &blobDetectorParams.minCircularity, &blobDetectorParams.maxCircularity);

      ImGui::Checkbox("Filter by inertia", &blobDetectorParams.filterByInertia);
      ImGui::DragFloatRange2("Inertia ratio min/max", &blobDetectorParams.minInertiaRatio, &blobDetectorParams.maxInertiaRatio);

      ImGui::Checkbox("Filter by convexity", &blobDetectorParams.filterByConvexity);
      ImGui::DragFloatRange2("Convexity min/max", &blobDetectorParams.minConvexity, &blobDetectorParams.maxConvexity);      
      
    }
    ImGui::End(); // BLOB detector parameters window

    
    return;
  }
  
}


void render()
{

  // Rendering
  int display_w, display_h;
  
  glfwGetFramebufferSize(window, &display_w, &display_h);
  glViewport(0, 0, display_w, display_h);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  
  //Initialize Projection Matrix 
	glMatrixMode( GL_PROJECTION ); 
  glLoadIdentity(); 
	glOrtho( 0.0, display_w, display_h, 0.0, 1.0, -1.0 ); 

	//Initialize Modelview Matrix 
	glMatrixMode( GL_MODELVIEW ); 
	glLoadIdentity(); 
	glTranslatef( 0.0f, 0.0f, 0.0f ); 

  texture.glRenderTexture( 0.0f, 0.0f, (float)display_w, (float)display_h );  
  
  // render GUI
  ImGui::Render();
  
  // finally swap buffers
  glfwSwapBuffers(window);
  
}

void findCamera()
{
  vector<PS3EYECam::PS3EYERef> devices = PS3EYECam::getDevices();
  
  if ( devices.size() > 0 ) {
    eye = devices.at(0);
  }
}


void startOpenCVThread()
{
	std::thread cv_thread(openCVThreadFunc);
  cv_thread.detach();
}

void openCVThreadFunc()
{

  Mat bayerRaw( Size(cameraParameters.height, cameraParameters.width), CV_8UC1 ); 
  
  Mat GreyImage( Size(cameraParameters.height, cameraParameters.width), CV_8UC1 );

  // BGR !!!
  Mat ProcessedImage( Size(cameraParameters.height, cameraParameters.width), CV_8UC3 );
  
  texture.data = ProcessedImage.data;
  
  SimpleBlobDetector detector(blobDetectorParams);
  
  while ( eye->isStreaming() ) {
    eye->getFrame( (uint8_t*)bayerRaw.data );

    //cvtColor( bayerRaw, RGBImage, CV_BayerGB2RGB );
    cvtColor( bayerRaw, GreyImage, CV_BayerGB2GRAY );    
    
    // Storage for blobs
    std::vector<KeyPoint> keypoints;

    // Detect blobs
    detector.detect( GreyImage, keypoints);

    drawKeypoints( GreyImage, keypoints, ProcessedImage, Scalar(0,0,255), DrawMatchesFlags::DRAW_RICH_KEYPOINTS );
    
  }

}


int main(int, char**)
{
    // init opengl and imgui
    initOpenGL();
    
    // create ps3 class
    findCamera();
    
    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplGlfw_NewFrame();
        
        if ( glfwGetTime() >= 1.0 ) {
          if ( !eye ) 
            findCamera();
          
          if ( eye && eye->isStreaming() ) 
            cameraParameters.update();

          glfwSetTime( 0.0 );
        }
      
        
        createGUI();
        
        texture.glUpdate();
        
        // render and swap buffers
        render();
        
    }

    texture.glDestroyTexture();
    terminateOpenGL();
    
    return 0;
}
