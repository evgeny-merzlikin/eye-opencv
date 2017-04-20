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

#include "imgui.h"
#include "imgui_impl_sdl.h"


void err(const char *fmt, ...);
void log(const char *fmt, ...);
void process_frame(uint8_t* video_framebuf);

void create_texture();
void update_texture();
void render_texture();
void delete_texture();

void createGUI();

void run_camera(int width, int height, int fps);
void load_textureRGB(uint8_t* pixels, GLuint texture_ID);

int main(int argc, char *argv[]);

struct ps3eye_context 
{
    ps3eye_context(int _width, int _height, int _fps) :
          eye(0)
        , devices(ps3eye::PS3EYECam::getDevices())
        , last_ticks(0)
        , last_frames(0)
    {
        width = _width;
		height = _height;
		fps = _fps;

		if (hasDevices()) {
            eye = devices[0];
            eye->init(_width, _height, (uint16_t)_fps);
			
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
    std::vector<ps3eye::PS3EYECam::PS3EYERef> devices;
    ps3eye::PS3EYECam::PS3EYERef eye;

    bool running;
	//std::atomic_bool running;
    Uint32 last_ticks;
    Uint32 last_frames;
	int width;
	int height;
	int fps;
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
GLuint					texture_ID = 0;



void old_run_camera(int width, int height, int fps)
{
	
	double angle = 0;
	SDL_RendererFlip flip = SDL_RendererFlip::SDL_FLIP_NONE;
	bool autocontrols = true;
	size_t frame_size;





	ctx = new ps3eye_context(width, height, fps);
	
	if (!ctx->hasDevices()) 
	{
		err("No PS3 Eye camera connected\n");
		return;
	}
	ctx->eye->setFlip(true); /* mirrored left-right */

	char title[256];
	sprintf_s(title, 256, "%dx%d@%d\n", ctx->eye->getWidth(), ctx->eye->getHeight(), ctx->eye->getFrameRate());

	SDL_Window *window = SDL_CreateWindow(
		title, SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_OPENGL);
	if (window == NULL) 
	{
		err("Failed to create window: %s\n", SDL_GetError());
	}

	SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);


	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
		SDL_RENDERER_ACCELERATED);
	if (renderer == NULL) 
	{
		SDL_DestroyWindow(window);
		err("Failed to create renderer: %s\n", SDL_GetError());
	}
	SDL_RenderSetLogicalSize(renderer, ctx->eye->getWidth(), ctx->eye->getHeight());
	

	SDL_Texture *video_tex = SDL_CreateTexture(
		renderer, SDL_PIXELFORMAT_BGR24, SDL_TEXTUREACCESS_STREAMING,
		ctx->eye->getWidth(), ctx->eye->getHeight());

	if (video_tex == NULL)
	{
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		err("Failed to create video texture: %s\n", SDL_GetError());
	}



	ctx->eye->start();
	ctx->running = true;

	frame_size = ctx->getFrameSize();

	//allocate video framebuffer
	video_framebuf = (uint8_t*)calloc(frame_size, sizeof(uint8_t));
	if (video_framebuf == NULL)
		err("Unable to allocate video_framebuf");

	SDL_Event e;

	Uint32 start_ticks = SDL_GetTicks();
	while (ctx->running) 
	{

		while (SDL_PollEvent(&e)) 
		{
			ImGui_ImplSdl_ProcessEvent(&e);

			switch (e.type) 
			{
				case SDL_QUIT:
					ctx->running = false;
					break;

				case SDL_KEYUP:
					switch (e.key.keysym.scancode) 
					{

						case SDL_SCANCODE_ESCAPE:
							ctx->running = false;
							break;

						case SDL_SCANCODE_RETURN :
							// toggle fullscreen
							SDL_SetWindowFullscreen(window, SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
							break;

						case SDL_SCANCODE_R :
							// rotate 90 degrees
							if ((angle += 90) >= 360)
								angle = 0;
							break;

						case SDL_SCANCODE_A :
							if (autocontrols) 
							{
								ctx->eye->setAutoExposure(false);
								printf("Exposure: %d\n", ctx->eye->getExposure());
						
								ctx->eye->setAutogain(false);
								printf("Gain: %d\n", ctx->eye->getGain());

								//ctx->eye->setAutoWhiteBalance(false);

							} 
							else 
							{
								ctx->eye->setAutoExposure(true);
								ctx->eye->setAutogain(true);
								//ctx->eye->setAutoWhiteBalance(true);
							}
							autocontrols= !autocontrols;

							break;

					

					case SDL_SCANCODE_F :
						// flip horizontally
						switch (flip) 
						{
							case SDL_RendererFlip::SDL_FLIP_NONE :
								flip = SDL_RendererFlip::SDL_FLIP_HORIZONTAL;
								break;

							case SDL_RendererFlip::SDL_FLIP_HORIZONTAL :
								flip = SDL_RendererFlip::SDL_FLIP_VERTICAL;
								break;

							case SDL_RendererFlip::SDL_FLIP_VERTICAL :
								flip = static_cast<SDL_RendererFlip>(SDL_RendererFlip::SDL_FLIP_VERTICAL | SDL_RendererFlip::SDL_FLIP_HORIZONTAL);
								break;

							default:
								flip = SDL_RendererFlip::SDL_FLIP_NONE;
								break;
						}
					}

					break;
				}

			}

		{
			Uint32 now_ticks = SDL_GetTicks();

			ctx->last_frames++;

			if (now_ticks - ctx->last_ticks > 1000)
			{
				//printf("FPS: %.2f\n", 1000 * ctx->last_frames / (float(now_ticks - ctx->last_ticks)));
				ctx->last_ticks = now_ticks;
				ctx->last_frames = 0;
			}
		}

		ctx->eye->getFrame(video_framebuf);
		process_frame(video_framebuf);

		void *video_tex_pixels;
		int pitch;
		SDL_LockTexture(video_tex, NULL, &video_tex_pixels, &pitch);
		memcpy(video_tex_pixels, video_framebuf, frame_size);
		SDL_UnlockTexture(video_tex);

		SDL_RenderCopyEx(renderer, video_tex, NULL, NULL, angle, NULL, flip);
		SDL_RenderPresent(renderer);
	}


	// stop camera thread
	ctx->eye->stop();
	delete(ctx);

	SDL_DestroyTexture(video_tex);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	
	//deallocate video framebuffer
	free(video_framebuf);

}


void run_camera(int width, int height, int fps)
{
    
	double angle = 0;
	bool autocontrols = true;
	size_t frame_size;
	
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0) {
		err("Failed to initialize SDL: %s\n", SDL_GetError());
	}

	// init camera
	ctx = new ps3eye_context(width, height, fps);
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

	frame_size = ctx->getFrameSize();

	video_framebuf = (uint8_t*)calloc(frame_size, sizeof(uint8_t));
	if (video_framebuf == NULL)
		err("Unable to allocate video_framebuf");

	create_texture();

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
		ctx->eye->getFrame(video_framebuf);
		process_frame(video_framebuf);
		update_texture();
		
        ImGui_ImplSdl_NewFrame(window);
		/*

        {
            ImGui::ColorEdit3("clear color", (float*)&clear_color);
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::Image(&texture_ID, ImVec2(128,128));
        }
		*/
		
        //ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);
        //ImGui::ShowTestWindow(&show_test_window);

        // Rendering

		createGUI();

        glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
		


		render_texture();
        ImGui::Render();
		

		//swap windows buffer
        SDL_GL_SwapWindow(window);
    }

	// stop camera thread
	ctx->eye->stop();
	delete(ctx);
	delete(gui);
	delete_texture();

	//deallocate video framebuffer
	free(video_framebuf);

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

void create_texture()
{

	glGenTextures( 1, &texture_ID ); 
	glBindTexture( GL_TEXTURE_2D, texture_ID ); 
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR ); 

	int textureWidth = ctx->width;
	int textureHeight = ctx->height;
	auto pow2 = [](unsigned v) { int p = 2; while (v >>= 1) p <<= 1; return p; };
	textureWidth = std::max(textureWidth, pow2(textureWidth));
	textureHeight = std::max(textureHeight, pow2(textureHeight));

	std::vector<GLubyte> pixels(textureWidth * textureHeight * 3);
	
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, textureWidth, textureHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, &pixels[0] ); 

	glBindTexture( GL_TEXTURE_2D, NULL );

}

void update_texture()
{
	glBindTexture( GL_TEXTURE_2D, texture_ID ); 
	
	glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, ctx->width, ctx->height, GL_RGB, GL_UNSIGNED_BYTE, video_framebuf ); 

	glBindTexture( GL_TEXTURE_2D, NULL );
}

void render_texture() {
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
	glBindTexture( GL_TEXTURE_2D, texture_ID );

	
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

void render_video_framebuf()
{
    GLuint TextureID;

	glGenTextures(1, &TextureID);
	glBindTexture(GL_TEXTURE_2D, TextureID);
 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Black/white checkerboard
	float pixels[] = {
		0.0f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,   0.0f, 0.0f, 0.0f
	};
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_FLOAT, pixels);

	GLenum error = glGetError();
    if( error != GL_NO_ERROR )
    {
        //printf( "Error loading texture" );
		ImGui::Text("Error loading texture: %#06x", error);
    }

	//int Mode = GL_RGB;
 
	/*
	if(Surface->format->BytesPerPixel == 4) {
		Mode = GL_RGBA;
	}*/
 
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, ctx->eye->getWidth(), ctx->eye->getHeight(), 0, GL_RGB, GL_UNSIGNED_BYTE, (GLvoid*) video_framebuf);

	// select modulate to mix texture with color for shading
     //glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
     
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_DECAL);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_DECAL);
     
     // when texture area is small, bilinear filter the closest mipmap
    // glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
     // when texture area is large, bilinear filter the first mipmap
    // glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
     
     // texture should tile



	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);



	float tricoords[12] = { 0.0, 0.0, 
							1.0, 0.0, 
							1.0, 1.0, 
							0.0, 0.0, 
							1.0, 1.0, 
							0.0, 1.0 };
	float texcoords[12] = { 0.0, 0.0, 
							1.0, 0.0, 
							1.0, 1.0, 
							0.0, 0.0, 
							1.0, 1.0,
							0.0, 1.0 };

float colors[24] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0 };


	glBindTexture(GL_TEXTURE_2D, TextureID);
	glEnableClientState(GL_VERTEX_ARRAY); //enable gl vertex pointer to be used in draw fct
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(2, GL_FLOAT, 0, tricoords);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoords);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	
	


	//ImGui::Image(pixels, ImVec2(20,20));

	//glLoadIdentity();

        //Move to rendering point
    //glTranslatef( 0, 0, 0.f );

	//glBindTexture(GL_TEXTURE_2D, TextureID);


	//int mTextureWidth = ctx->eye->getWidth();
	//int mTextureHeight = ctx->eye->getHeight();
	
	   //    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
     //  glColor3f(0.0f, 0.0f, 0.0f); // sets color to black.
     /*  glBegin(GL_QUADS);
               glVertex2f(-0.25f, 0.25f); // vertex 1
               glVertex2f(-0.5f, -0.25f); // vertex 2
               glVertex2f(0.5f, -0.25f); // vertex 3
               glVertex2f(0.25f, 0.25f); // vertex 4
       glEnd();
	*/


	/*
	glBegin(GL_QUADS);
		glTexCoord2f( 0.f, 0.f ); glVertex2f(           0.f,            0.f );
		glTexCoord2f( 1.f, 0.f ); glVertex2f( mTextureWidth,            0.f );
		glTexCoord2f( 1.f, 1.f ); glVertex2f( mTextureWidth, mTextureHeight );
		glTexCoord2f( 0.f, 1.f ); glVertex2f(           0.f, mTextureHeight );
	glEnd();
	*/
	//glDrawElements(GL_QUADS, 4, GL_FLOAT, 0);

	//glBindTexture(GL_TEXTURE_2D, NULL);

	

}

void delete_texture()
{
	glDeleteTextures( 1, &texture_ID );
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


void process_frame(uint8_t* video_framebuf)
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



