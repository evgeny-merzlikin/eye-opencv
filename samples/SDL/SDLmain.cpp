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
#include "ps3eye.h"



void err(const char *fmt, ...);
void log(const char *fmt, ...);

void process_frame(uint8_t* video_framebuf);
void run_camera(int width, int height, int fps);


struct ps3eye_context 
{
    ps3eye_context(int _width = 640, int _height = 480, int _fps = 60, ps3eye::PS3EYECam::EOutputFormat _outputFormat = ps3eye::PS3EYECam::EOutputFormat::RGB) :
          eye(0)
        , devices(ps3eye::PS3EYECam::getDevices())
        , last_ticks(0)
        , last_frames(0)
    {
        width = _width;
		height = _height;
		fps = _fps;
		outputFormat = _outputFormat;

		if (hasDevices()) {
            eye = devices[0];
            eye->init(_width, _height, (uint16_t)_fps, _outputFormat);
			
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
	ps3eye::PS3EYECam::EOutputFormat outputFormat;
};



ps3eye_context*			ctx;
uint8_t*				video_framebuf = NULL;


void run_camera(int width, int height, int fps)
{
	
	double angle = 0;
	SDL_RendererFlip flip = SDL_RendererFlip::SDL_FLIP_NONE;
	bool autocontrols = true;
	size_t frame_size;

	ctx = new ps3eye_context(width, height, fps, ps3eye::PS3EYECam::EOutputFormat::RGB);
	
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
		renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
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



int main_0(int argc, char *argv[])
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
	MessageBox(0, message, "Error", MB_OK);
	
}



