/**
 * PS3EYEDriver Simple SDL 2 example, using OpenGL where available.
 * Thomas Perl <m@thp.io>; 2014-01-10
 * Joseph Howse <josephhowse@nummist.com>; 2014-12-26
 **/
#include <sstream>
#include <iostream>
#include <stdarg.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <stdio.h>

#include "SDL.h"
#include "SDL_opengl.h"

#include "ps3eye.h"

#define HAVE_OPENCV_IMGPROC
#include "opencv2\opencv.hpp"

#include "imgui.h"
#include "imgui_impl_sdl.h"

using namespace std;
using namespace ps3eye;
using namespace cv;

void err(const char *fmt, ...);
void log(const char *fmt, ...);
void process_frame();

 
void create_texture( GLuint* TextureID, GLint TextureFormat);
void update_texture( GLuint TextureID, GLint TextureFormat, GLvoid* pixels);
void render_texture( GLuint TextureID );
void delete_texture( GLuint* TextureID );

void createGUI();

void run_camera(int width, int height, int fps);
void load_textureRGB(uint8_t* pixels, GLuint BayerTexture_ID);

int main(int argc, char *argv[]);

struct ps3eye_context 
{
    ps3eye_context(int _width, int _height, int _fps, PS3EYECam::EOutputFormat _format = PS3EYECam::EOutputFormat::RGB) :
          eye(0), 
          devices(PS3EYECam::getDevices()),
          last_ticks(0),
          last_frames(0),
		  width(_width),
		  height(_height),
		  fps(_fps),
		  format(_format)
	{
		if (hasDevices()) {
            eye = devices.at(0);
            eye->init(_width, _height, (uint16_t)_fps, format);
        }
    }

    bool hasDevices()
    {
        return (devices.size() > 0);
    }
	size_t getFrameSize() {
		if ((eye != NULL) && (eye->isStreaming())) {
			return eye->getOutputBytesPerPixel() * eye->getWidth() * eye->getHeight();
		}
		return 0;
	}
    std::vector<PS3EYECam::PS3EYERef> devices;
    PS3EYECam::PS3EYERef eye;

    bool running;
	//std::atomic_bool running;
    Uint32 last_ticks;
    Uint32 last_frames;
	int width;
	int height;
	int fps;
	PS3EYECam::EOutputFormat format;
};

struct GUI_context {
	GUI_context()
	{
		show_camera_controls = true;

	}

	bool show_camera_controls;
	bool autogain;
	int gain;


};

GUI_context*			gui;
ps3eye_context*			ctx;
uint8_t*				video_framebuf = NULL;
uint8_t*				sample_buf = NULL;
GLuint					BayerTexture_ID = 0;
Mat*					BayerMat;
Mat*					GrayMat;


void run_camera(int width, int height, int fps)
{
    
	double angle = 0;
	bool autocontrols = true;
	size_t frame_size;
	
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0) {
		err("Failed to initialize SDL: %s\n", SDL_GetError());
	}

	// init camera
	ctx = new ps3eye_context(width, height, fps, PS3EYECam::EOutputFormat::Bayer);

	gui = new GUI_context();
	
	if (!ctx->hasDevices()) {
		err("No PS3 Eye camera connected\n");
		return;
	}
	ctx->eye->setFlip(true); /* mirrored left-right */

	// Setup window
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(0, &current);


    SDL_Window *window = SDL_CreateWindow("PS3 camera", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL);

	if (window == NULL) {
		err("Failed to create window: %s\n", SDL_GetError());
	}

	SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);

    SDL_GLContext glcontext = SDL_GL_CreateContext(window);


	glViewport( 0.f, 0.f, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y ); //Initialize Projection Matrix 
	glMatrixMode( GL_PROJECTION ); 
	glLoadIdentity(); 
	glOrtho( 0.0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y, 0.0, 1.0, -1.0 ); 
	//Initialize Modelview Matrix 
	glMatrixMode( GL_MODELVIEW ); 
	glLoadIdentity(); 
	//Initialize clear color 
	glClearColor( 0.f, 0.f, 0.f, 1.f ); //Enable texturing 
	glEnable( GL_TEXTURE_2D );

    // Setup ImGui binding
    ImGui_ImplSdl_Init(window);

    // Load Fonts
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 25.0f);

    bool show_test_window = false;
    bool show_another_window = false;
    ImVec4 clear_color = ImColor(114, 144, 154);

	// start camera thread
	ctx->eye->start();
	ctx->running = true;

	BayerMat = new Mat( height, width, CV_8UC1);  
	GrayMat = new Mat( height, width, CV_8UC1); 
	//BayerMat->setTo(Scalar(5));
	//frame_size = width * height * 3;
	
	//video_framebuf = (uint8_t*)calloc(frame_size, sizeof(uint8_t));
	//if (video_framebuf == NULL)
	//	err("Unable to allocate video_framebuf");
	//memset(video_framebuf, 5, sizeof(uint8_t));
	create_texture( &BayerTexture_ID, GL_LUMINANCE);

    // Main loop
    while (ctx->running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSdl_ProcessEvent(&event);

            if (event.type == SDL_QUIT)
                ctx->running = false;

			if (event.type == SDL_KEYUP) 
			{
				switch (event.key.keysym.scancode) 
				{

					case SDL_SCANCODE_ESCAPE:
						ctx->running = false;
						break;

				} // switch
			}
		}
        

		// process frame
		ctx->eye->getFrame( (uint8_t*)BayerMat->data );
		//ctx->eye->getFrame( video_framebuf );
		cvtColor(*BayerMat, *GrayMat, CV_BayerGB2GRAY);
		
		update_texture( BayerTexture_ID, GL_LUMINANCE, (GLvoid*)GrayMat->data );

		//update_texture( BayerTexture_ID, GL_LUMINANCE, (GLvoid*)video_framebuf );

        ImGui_ImplSdl_NewFrame(window);
		/*

        {
            ImGui::ColorEdit3("clear color", (float*)&clear_color);
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::Image(&BayerTexture_ID, ImVec2(128,128));
        }
		*/
		
        //ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);
        //ImGui::ShowTestWindow(&show_test_window);

        // Rendering

		createGUI();

        glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
		


		render_texture(BayerTexture_ID);
        ImGui::Render();
		

		//swap windows buffer
        SDL_GL_SwapWindow(window);
    }

	// stop camera thread
	ctx->eye->stop();
	delete(ctx);
	delete(gui);

	delete(BayerMat);
	delete_texture(&BayerTexture_ID);

	//deallocate video framebuffer
	//free(video_framebuf);

    // Cleanup
    ImGui_ImplSdl_Shutdown();
    SDL_GL_DeleteContext(glcontext);
    SDL_DestroyWindow(window);



}

void createGUI()
{


	ImGui::Begin("PS3 Camera controls", &gui->show_camera_controls, 0);
	ImGui::Text("Camera: %d x %d, fps %d.", ctx->width, ctx->height, ctx->fps);
	ImGui::End();



}

void create_texture( GLuint* TextureID, GLint TextureFormat )
{

	glGenTextures( 1, TextureID ); 
	glBindTexture( GL_TEXTURE_2D, *TextureID ); 
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ); 

	int textureWidth = ctx->width;
	int textureHeight = ctx->height;
	auto pow2 = [](unsigned v) { int p = 2; while (v >>= 1) p <<= 1; return p; };
	textureWidth = std::max(textureWidth, pow2(textureWidth));
	textureHeight = std::max(textureHeight, pow2(textureHeight));

	int channels = 3;
	if ( TextureFormat == GL_LUMINANCE ) 
		channels = 1;

	std::vector<GLubyte> pixels(textureWidth * textureHeight * channels);

	glTexImage2D( GL_TEXTURE_2D, 0, TextureFormat, textureWidth, textureHeight, 0, TextureFormat, GL_UNSIGNED_BYTE, &pixels[0] ); 

	glBindTexture( GL_TEXTURE_2D, NULL );

}

void update_texture( GLuint TextureID, GLint TextureFormat, GLvoid* pixels )
{
	glBindTexture( GL_TEXTURE_2D, TextureID ); 
	
	glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, ctx->width, ctx->height, TextureFormat, GL_UNSIGNED_BYTE, pixels ); 

	glBindTexture( GL_TEXTURE_2D, NULL );
}

void render_texture( GLuint TextureID ) {
	 //Set the viewport 
	glViewport( 0.f, 0.f, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y ); //Initialize Projection Matrix 
	glMatrixMode( GL_PROJECTION ); 
	glLoadIdentity(); 
	glOrtho( 0.0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y, 0.0, 1.0, -1.0 ); 
	//Initialize Modelview Matrix 
	glMatrixMode( GL_MODELVIEW ); 
	glLoadIdentity(); 

	glTranslatef( 0, 0, 0.f ); 
	//Set texture ID 
	glBindTexture( GL_TEXTURE_2D, TextureID );

	
	int textureWidth = ctx->width;
	int textureHeight = ctx->height;
	auto pow2 = [](unsigned v) { int p = 2; while (v >>= 1) p <<= 1; return p; };
	textureWidth = std::max(textureWidth, pow2(textureWidth));
	textureHeight = std::max(textureHeight, pow2(textureHeight));

	GLfloat u = 1.0f; GLfloat v = 1.0f;
	u = (float) ctx->width / (float) textureWidth;
	v = (float) ctx->height / (float) textureHeight;
	GLfloat w = ImGui::GetIO().DisplaySize.x; 
	GLfloat h = ImGui::GetIO().DisplaySize.y;
	//Render textured quad 
	glBegin( GL_QUADS ); 
		glTexCoord2f( 0.f, 0.f ); glVertex2f( 0.f, 0.f ); 
		glTexCoord2f( u, 0.f ); glVertex2f( w, 0.f ); 
		glTexCoord2f( u, v ); glVertex2f( w, h); 
		glTexCoord2f( 0.f, v ); glVertex2f( 0.f, h ); 
	glEnd();
	glBindTexture( GL_TEXTURE_2D, NULL );
}

void delete_texture( GLuint* TextureID )
{
	glDeleteTextures( 1, TextureID );
}

int main(int argc, char *argv[])
{

    int width = 640;
    int height = 480;
    int fps = 60;
    if (argc > 1)
    {
        bool good_arg = false;
        for (int arg_ix = 1; arg_ix < argc; ++arg_ix)
        {
            if (std::string(argv[arg_ix]) == "--qvga")
            {
                width = 320;
                height = 240;
                good_arg = true;
            }

            if ((std::string(argv[arg_ix]) == "--fps") && argc > arg_ix)
            {
                std::istringstream new_fps_ss( argv[arg_ix+1] );
                if (new_fps_ss >> fps)
                {
                    good_arg = true;
                }
            }

        }
        if (!good_arg)
        {
            err("Usage: %s [--fps XX] [--qvga]", argv[0]);
        }
    }
	
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0) {
		err("Failed to initialize SDL: %s\n", SDL_GetError());
	}


	run_camera(width, height, fps);
	//old_run_camera(width, height, fps);

	SDL_Quit();

    return EXIT_SUCCESS;
}


void process_frame()
{


}

void err(const char *fmt, ...) 
{
	va_list ap;
	char message[1024];
	
	va_start(ap, fmt);
	if (fmt != NULL) {
		sprintf_s(message, 1024, fmt, ap);
	}
	va_end(ap);

	MessageBox(0, message, "Error", MB_OK);
	exit(0);
}

void log(const char *fmt, ...) 
{
	va_list ap;
	char message[1024];
	
	va_start(ap, fmt);
	if (fmt != NULL) {
		
		sprintf_s(message, 1024, fmt, ap);
	}
	va_end(ap);
	ImGui::Text(message);
	
}



