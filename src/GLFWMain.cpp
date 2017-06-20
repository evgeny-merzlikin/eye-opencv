#include <stdlib.h>     // atoi
#include <thread>
#include <mutex>
#include <atomic>

#include "imgui.h"
#include "imgui_impl_glfw.h"

#include "ps3eye.h"
#include "GLFW/glfw3.h"
#include "teVirtualMIDI.h"

#include "opencv2/opencv.hpp";;

#define MAX_SYSEX_BUFFER	65535

#define IM_ARRAYSIZE(_ARR)  ((int)(sizeof(_ARR)/sizeof(*_ARR)))
#define IM_MAX(_A,_B)       (((_A) >= (_B)) ? (_A) : (_B))
#define ROUND_2_INT(f) ((int)(f >= 0.0 ? (f + 0.5) : (f - 0.5)))

using namespace ps3eye;
using namespace std;
using namespace cv;


inline double round(double value) { return value < 0 ? -std::floor(0.5 - value) : std::floor(0.5 + value); }

inline double round(float value) { return value < 0 ? -std::floor(0.5 - value) : std::floor(0.5 + value); }

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
	template <typename T> struct Threshold { T min, max ;};
	struct GaussThreshold {	int maxValue, blockSize; float floatC;};

	Threshold<int> binary, contoursArea;
	GaussThreshold gauss;
	int resizeInterpolation;

} opencvParams;

GLFWwindow* window;
LPVM_MIDI_PORT MIDIport;
mutex mtx;  
int framerate;
int framecount;

struct GLTexture {
	GLTexture(): data(nullptr) {}

	void init(int _width, int _height, GLint _format) 
	{

		dataWidth = _width;
		dataHeight = _height; 
		format = _format;

		if ( format == GL_LUMINANCE )
			channels = 1;
		if ( format == GL_RGB )
			channels = 3;
		if ( format == GL_RGBA )
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
		if ( !data )
			return;

		glBindTexture( GL_TEXTURE_2D, ID );
		// lock mutex
		mtx.lock();

		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, dataWidth, dataHeight, format, GL_UNSIGNED_BYTE, data ); 
		// lock mutex
		mtx.unlock();

		glBindTexture( GL_TEXTURE_2D, 0 );

	}

	void glRenderTexture(float width, float height)
	{
		if ( !data )
			return;

		glBindTexture( GL_TEXTURE_2D, ID );

		glBegin( GL_QUADS ); 
		glTexCoord2f( 0.f, 0.f ); glVertex2f( 0.0f, 0.0f ); 
		glTexCoord2f( u, 0.f ); glVertex2f( width, 0.0f ); 
		glTexCoord2f( u, v ); glVertex2f( width, height); 
		glTexCoord2f( 0.f, v ); glVertex2f( 0.0f, height ); 
		glEnd();


		glBindTexture( GL_TEXTURE_2D, 0 );

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

	GLfloat u, v;


	GLvoid* data;
	GLint format; 
	GLuint ID;    

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

void createMIDIPort()
{
	virtualMIDILogging( TE_VM_LOGGING_MISC | TE_VM_LOGGING_RX | TE_VM_LOGGING_TX );

	MIDIport = virtualMIDICreatePortEx2( L"EyeMidiPort", NULL, 0, MAX_SYSEX_BUFFER, TE_VM_FLAGS_PARSE_RX );

	if ( !MIDIport )
		printf("virtualMIDICreatePortEx2 error: %d\n", GetLastError());
}



void terminateOpenGL()
{

	ImGui_ImplGlfw_Shutdown();
	glfwTerminate();

}

void openCVThreadFunc()
{

	Mat bayerRaw( eye->getHeight(), eye->getWidth(), CV_8UC1 ); 
	//Mat bayerRaw( 480, 640, CV_8UC1 ); 
	//Mat greyImage( eye->getHeight(), eye->getWidth(), CV_8UC1 );
	Mat temp( eye->getHeight(), eye->getWidth(), CV_8UC1 );
	Mat temp2( eye->getHeight(), eye->getWidth(), CV_8UC3 );
	Mat greyImage( 480, 640, CV_8UC1 );
	//Mat greyMask( eye->getHeight(), eye->getWidth(), CV_8UC1 );
	Mat greyMask( 480, 640, CV_8UC1 );
	//Mat rgbImage( eye->getHeight(), eye->getWidth(), CV_8UC3 );
	Mat rgbImage( 480, 640, CV_8UC3 );
	//Mat hsvImage( 480, 640, CV_8UC3 );
	vector<vector<Point> > contours;
	vector<vector<Point> > circles;

	grayTexture.data = nullptr;
	rgbTexture.data = nullptr;

	while ( eye && eye->isStreaming() ) {
		eye->getFrame( (uint8_t*)bayerRaw.data );


		switch ( cameraParameters.view_mode ) {
		case 0: // show bayer image
			if (eye->getWidth() == 320) {
				resize(bayerRaw, greyImage, greyImage.size(), 0, 0, static_cast<InterpolationFlags>(opencvParams.resizeInterpolation));
				grayTexture.data = greyImage.data;
			} else {
				grayTexture.data = bayerRaw.data;
			}
			rgbTexture.data = nullptr;
			break;

		case 1: // show grey image
			cvtColor( bayerRaw, temp, COLOR_BayerGB2GRAY );



			if (eye->getWidth() == 320) {
				resize(temp, greyImage, greyImage.size(), 0, 0, static_cast<InterpolationFlags>(opencvParams.resizeInterpolation));
			} else {
				greyImage = temp;
			}

			grayTexture.data = greyImage.data;
			rgbTexture.data = nullptr;
			break;

		case 2:	 // show bgr color image
			cvtColor( bayerRaw, temp2, COLOR_BayerGB2RGB );

			if (eye->getWidth() == 320) {
				resize(temp2, rgbImage, greyImage.size(), 0, 0, static_cast<InterpolationFlags>(opencvParams.resizeInterpolation));
			} else {
				rgbImage = temp2;
			}

			grayTexture.data = nullptr;
			rgbTexture.data = rgbImage.data;
			break;

		case 3: // binary threshold of grey image
			cvtColor( bayerRaw, temp, COLOR_BayerGB2GRAY );

			// lock mutex
			mtx.lock();
			if (eye->getWidth() == 320) {
				resize(temp, greyImage, greyImage.size(), 0, 0, static_cast<InterpolationFlags>(opencvParams.resizeInterpolation));
			} else {
				greyImage = temp;
			}
			threshold( greyImage, greyMask, opencvParams.binary.min, opencvParams.binary.max, THRESH_BINARY );
			contours.clear(); circles.clear();
			
			findContours( greyMask, contours, RETR_LIST , CHAIN_APPROX_SIMPLE);
			for( size_t i = 0; i< contours.size(); i++ ) // iterate through each contour.
				{
        int area = ROUND_2_INT( contourArea( contours[i] ) );  //  Find the area of contour
				
        if (( opencvParams.contoursArea.min < area ) && ( opencvParams.contoursArea.max > area ))
        {
					circles.push_back(contours[i]);
        }
    }

    drawContours( greyImage, circles, -1, Scalar( 0, 255, 0 ), 2 ); // Draw the largest contour using previously stored index.


			grayTexture.data = greyImage.data;
			rgbTexture.data = nullptr;

			// un lock mutex
			mtx.unlock();
			break;

		case 4: // adaptive threshold of grey image
			cvtColor( bayerRaw, temp, COLOR_BayerGB2GRAY );
			// lock mutex
			mtx.lock();

			if (eye->getWidth() == 320) {
				resize(temp, greyImage, greyImage.size(), 0, 0, static_cast<InterpolationFlags>(opencvParams.resizeInterpolation));
			} else {
				greyImage = temp;
			}
			adaptiveThreshold( greyImage, greyMask, opencvParams.gauss.maxValue, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, opencvParams.gauss.blockSize, opencvParams.gauss.floatC );
			grayTexture.data = greyMask.data;
			rgbTexture.data = nullptr;

			// unlock mutex
			mtx.unlock();
			break;

		case 5: // red color threshold 

			cvtColor( bayerRaw, temp2, COLOR_BayerGB2RGB );

			if (eye->getWidth() == 320) {
				resize(temp2, rgbImage, greyImage.size(), 0, 0, static_cast<InterpolationFlags>(opencvParams.resizeInterpolation));
			} else {
				rgbImage = temp2;
			}

			medianBlur(rgbImage, rgbImage, 3);

			Mat hsvImage;
			cvtColor(rgbImage, hsvImage, COLOR_RGB2HSV);

			Mat lower_red_hue_range;
			Mat upper_red_hue_range;
			inRange(hsvImage, Scalar(0, 100, 100), Scalar(10, 255, 255), lower_red_hue_range);
			inRange(hsvImage, Scalar(160, 100, 100), Scalar(179, 255, 255), upper_red_hue_range);

			//Mat red_hue_image;
			addWeighted(lower_red_hue_range, 1.0, upper_red_hue_range, 1.0, 0.0, greyMask);

			GaussianBlur(greyMask, greyMask, cv::Size(9, 9), 2, 2);
			/*
			vector<Vec3f> circles;
			HoughCircles(red_hue_image, circles, HOUGH_GRADIENT, 1, red_hue_image.rows/8, 100, 20, 0, 0);


			for(size_t current_circle = 0; current_circle < circles.size(); ++current_circle) {
			Point center(round(circles[current_circle][0]), round(circles[current_circle][1]));
			int radius = round(circles[current_circle][2]);

			circle(rgbImage, center, radius, Scalar(0, 255, 0), 5);
			}
			*/

			grayTexture.data = greyMask.data;
			rgbTexture.data = nullptr; //rgbImage.data;
			break;

		} // end switch

	} // end while

	//keypoints.clear();
	grayTexture.data = nullptr;
	rgbTexture.data = nullptr;

}

void startOpenCVThread()
{
	std::thread cv_thread(openCVThreadFunc);
	cv_thread.detach();
}

void sendMidiNote(unsigned char channel, unsigned char pitch, unsigned char volume) {

	if ( !MIDIport )
		return;

	unsigned char message [] = {channel, pitch, volume};
	if ( !virtualMIDISendData(MIDIport, message, 3) )
		printf("virtualMIDISendData error: %d\n", GetLastError());

}

void sendMidiNoteOn(unsigned char channel, unsigned char pitch, unsigned char volume) {
	sendMidiNote(0x90 & channel, pitch, volume);
}

void createGUI()
{

	if ( MIDIport ) {
		static int channel = 0x09;
		static int pitch = 100;
		static int volume = 100;
		
		ImGui::Begin("Virtual MIDI port");
		ImGui::InputInt("Channel", &channel);
		ImGui::InputInt("Pitch", &pitch);
		ImGui::InputInt("Volume", &volume);

		if ( ImGui::Button("Send MIDI note") )
			sendMidiNoteOn(channel, pitch, volume);

		ImGui::End();

	}



	// camera not connected or not streaming
	if ( !eye ) {
		// new popup (modal window)

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

			eye->init( cameraParameters.width, cameraParameters.height, (uint16_t) cameraParameters.fps, PS3EYECam::EOutputFormat::RAW8 );

			eye->start(); // start camera thread
			cameraParameters.update( true ); // refresh all camera parameters

			// init and create texture
			//grayTexture.init( eye->getWidth(), eye->getHeight(), GL_LUMINANCE );
			grayTexture.init( 640, 480, GL_LUMINANCE );
			//rgbTexture.init( eye->getWidth(), eye->getHeight(), GL_RGB );
			rgbTexture.init( 640, 480, GL_RGB );

			grayTexture.glCreateTexture();
			rgbTexture.glCreateTexture();

			opencvParams.gauss.blockSize = 3;
			opencvParams.gauss.maxValue = 255;
			opencvParams.gauss.floatC = 1.0f;
			opencvParams.binary.max = 255;
			opencvParams.binary.min = 100;
			opencvParams.contoursArea.min = 20;
			opencvParams.contoursArea.max = 1000;
			// start opencv thread
			startOpenCVThread();

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

			ImGui::Combo("Mode", &cameraParameters.view_mode, "Bayer\0Gray\0Color\0Binary threshold\0Gaussan threshold\0Red Color threshold\0");

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

		// OpenCV options
		ImGui::Begin("OpenCV parameters");
		{    
			ImGui::Combo("Interpolation value", &opencvParams.resizeInterpolation, "INTER_NEAREST\0INTER_LINEAR\0INTER_CUBIC\0INTER_AREA\0INTER_LANCZOS4\0");

			ImGui::DragIntRange2("Contours area Threshold", &opencvParams.contoursArea.min, &opencvParams.contoursArea.max);

			ImGui::DragIntRange2("Binary Threshold", &opencvParams.binary.min, &opencvParams.binary.max);
			ImGui::SliderInt("Gauss maxvalue", &opencvParams.gauss.maxValue, 1, 255);
			if (ImGui::SliderInt("Gauss blockSize", &opencvParams.gauss.blockSize, 1, 255)) {

				if (opencvParams.gauss.blockSize % 2 != 1 ) 
					opencvParams.gauss.blockSize = opencvParams.gauss.blockSize + 1;
				if (opencvParams.gauss.blockSize < 3 ) 
					opencvParams.gauss.blockSize = 3;
			}
			ImGui::SliderFloat("Gauss C", &opencvParams.gauss.floatC, 0.0f, 1.0f);

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

	if ( devices.size() > 0 ) {
		eye = devices.at(0);
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
	double now = 0;
	double lastScreenUpdate = 0;
	double screenUpdateInt = 1.0f/60.0f;
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


		}

		createGUI();
		grayTexture.glUpdate();
		rgbTexture.glUpdate();
		render();
		time = glfwGetTime();
	}

	eye->stop();

	grayTexture.glDestroyTexture();
	rgbTexture.glDestroyTexture();

	terminateOpenGL();

	if ( MIDIport ) 
		virtualMIDIClosePort( MIDIport );

	return 0;
}
