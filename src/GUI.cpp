#include "GLFWMain.h"
;;


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

			eye->init( cameraParameters.width, cameraParameters.height, (uint16_t) cameraParameters.fps, ps3eye::PS3EYECam::EOutputFormat::RAW8 );

			eye->start(); // start camera thread
			cameraParameters.update( true ); // refresh all camera parameters

			// init and create texture
			//grayTexture.init( eye->getWidth(), eye->getHeight(), GL_LUMINANCE );
			grayTexture.init( 640, 480, GL_LUMINANCE );
			//rgbTexture.init( eye->getWidth(), eye->getHeight(), GL_RGB );
			rgbTexture.init( 640, 480, GL_RGB );

			grayTexture.glCreateTexture();
			rgbTexture.glCreateTexture();

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