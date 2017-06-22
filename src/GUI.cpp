#include "GLFWMain.h"

void createMenu()
{

    // Menu
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
					ImGui::MenuItem("(dummy menu)", NULL, false, false);
					if (ImGui::MenuItem("New")) {}
					if (ImGui::MenuItem("Open", "Ctrl+O")) {}
					if (ImGui::BeginMenu("Open Recent"))
					{
							ImGui::MenuItem("fish_hat.c");
							ImGui::MenuItem("fish_hat.inl");
							ImGui::MenuItem("fish_hat.h");
							if (ImGui::BeginMenu("More.."))
							{
									ImGui::MenuItem("Hello");
									ImGui::MenuItem("Sailor");
									if (ImGui::BeginMenu("Recurse.."))
									{
											ShowExampleMenuFile();
											ImGui::EndMenu();
									}
									ImGui::EndMenu();
							}
							ImGui::EndMenu();
					}
					if (ImGui::MenuItem("Save", "Ctrl+S")) {}
					if (ImGui::MenuItem("Save As..")) {}
					ImGui::Separator();
					if (ImGui::BeginMenu("Options"))
					{
							static bool enabled = true;
							ImGui::MenuItem("Enabled", "", &enabled);
							ImGui::BeginChild("child", ImVec2(0, 60), true);
							for (int i = 0; i < 10; i++)
									ImGui::Text("Scrolling Text %d", i);
							ImGui::EndChild();
							static float f = 0.5f;
							static int n = 0;
							ImGui::SliderFloat("Value", &f, 0.0f, 1.0f);
							ImGui::InputFloat("Input", &f, 0.1f);
							ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
							ImGui::EndMenu();
					}
					if (ImGui::BeginMenu("Help"))
					{
							for (int i = 0; i < ImGuiCol_COUNT; i++)
									ImGui::MenuItem(ImGui::GetStyleColName((ImGuiCol)i));
							ImGui::EndMenu();
					}
					if (ImGui::BeginMenu("Disabled", false)) // Disabled
					{
							IM_ASSERT(0);
					}
					if (ImGui::MenuItem("Checked", NULL, true)) {}
					if (ImGui::MenuItem("Quit", "Alt+F4")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Examples"))
        {
            ImGui::MenuItem("Main menu bar", NULL, &show_app_main_menu_bar);
            ImGui::MenuItem("Console", NULL, &show_app_console);
            ImGui::MenuItem("Log", NULL, &show_app_log);
            ImGui::MenuItem("Simple layout", NULL, &show_app_layout);
            ImGui::MenuItem("Property editor", NULL, &show_app_property_editor);
            ImGui::MenuItem("Long text display", NULL, &show_app_long_text);
            ImGui::MenuItem("Auto-resizing window", NULL, &show_app_auto_resize);
            ImGui::MenuItem("Constrained-resizing window", NULL, &show_app_constrained_resize);
            ImGui::MenuItem("Simple overlay", NULL, &show_app_fixed_overlay);
            ImGui::MenuItem("Manipulating window title", NULL, &show_app_manipulating_window_title);
            ImGui::MenuItem("Custom rendering", NULL, &show_app_custom_rendering);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("Metrics", NULL, &show_app_metrics);
            ImGui::MenuItem("Style Editor", NULL, &show_app_style_editor);
            ImGui::MenuItem("About ImGui", NULL, &show_app_about);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

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
		ImGui::Begin("Ready to start tracking");
		ImGui::Text("PS3 EYE camera is connected to PC. Press button below to setup camera position and start tracking. MIDI and other options can be adjusted from menu\n\n");
		if ( ImGui::Button("Setup camera position") ) {
			startCamera();
			opencvParams.mode = OpenCVParameters::CurrentMode::SETUP_POSITION;
		}
		ImGui::End();
	}



	if ( guiParams.showCameraOptions ) {

		ImGui::Begin("Select PS3 EYE camera options");

		ImGui::Text("Please select camera parameters:\n\n");

		const char* vga_rates[] = { "2", "3", "5", "8", "10", "15", "20", "25", "30", "40", "50", "60", "75", "83" };    

		const char* qvga_rates[] = { "2", "3", "5", "7", "10", "12", "15", "17", "30", "37", "40", "50", "60", "75", "90", "100", "125", "137", "150", "187", "205", "290" };

		// camera resolution
		int camera_res = cameraParameters.width == 640 ? 0 : 1;
		int camera_fps = 0;

		if ( camera_res == 0 ) {
			while( camera_fps < IM_ARRAYSIZE(vga_rates) ) {
				if ( atoi(vga_rates[camera_fps++]) == cameraParameters.fps ) 
					break;
			}
		} else {
			while( camera_fps < IM_ARRAYSIZE(qvga_rates) ) {
				if ( atoi(qvga_rates[camera_fps++]) == cameraParameters.fps ) 
					break;
			}
		}


		if ( ImGui::Combo("Camera resolution", &camera_res, "VGA\0QVGA\0") )

		if ( camera_res == 0 ) // VGA
			ImGui::Combo("Camera fps: ", &camera_fps, vga_rates, IM_ARRAYSIZE(vga_rates));

		if ( camera_res == 1 ) // QVGA
			ImGui::Combo("Camera fps: ", &camera_fps, qvga_rates, IM_ARRAYSIZE(qvga_rates));

		// start streaming button
		if ( ImGui::Button("Apply") ) {

			bool wasStreaming = eye && eye->isStreaming();
			stopCamera();

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

			findCamera();
			if ( wasStreaming )
				startCamera();


		} // ImGui::Button("Apply")

		if ( ImGui::Button("Cancell") ) {
		}

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