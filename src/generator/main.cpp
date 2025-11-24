/**
 * App 1: Image Generator (simulating a high-speed camera streaming data to the backend system)
 * - Reads image files from the '../images/' directory.
 * - Dynamically rescans the directory on each loop iteration (to handle file addition and removal).
 * - Encodes the image into a compressed buffer (e.g., JPEG bytes).
 * - Publishes a two-part message (filename, image_buffer) to a ZMQ PUB socket.
 */

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <thread>
#include <stdexcept>

#include "opencv2/opencv.hpp"
#include "zmq.hpp"
#include "Constants.hpp"

namespace fs = std::filesystem;

// Helper function to find images dynamically ---
std::vector<std::string> find_available_images(const fs::path& dir_path) {
	std::vector<std::string> image_paths;
	if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
		// The application cannot run without the directory.
		throw std::runtime_error("Demo directory not found: " + dir_path.string());
	}

	// Iterate over the directory contents
	for (const auto& entry : fs::directory_iterator(dir_path)) {
		if (entry.is_regular_file()) {
			std::string path_str = entry.path().string();
			// Check for valid extensions (case-insensitive for robustness)
			std::string extension = entry.path().extension().string();
			std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

			if (extension == ".jpg" || extension == ".jpeg" || extension == ".png") {
				// Store the full path for reading later
				image_paths.push_back(path_str);
			}
		}
	}
	return image_paths;
}

int main(int argc, char* argv[]) {

	fs::path demo_dir;

	if (argc >= 2) {
		demo_dir = fs::path(argv[1]);
	} else {
		// default image directory path
		demo_dir = fs::current_path() / "../images";
	}
	
	// Check for directory existence
	try {
		if (!fs::exists(demo_dir)) {
			std::cerr << "Fatal Error: Demo directory not found: " << demo_dir << std::endl;
			return -1;
		}
	} catch (const std::exception& e) {
		std::cerr << "Filesystem Error: " << e.what() << std::endl;
		return -1;
	}

	// ZMQ Setup
	zmq::context_t context(1);
	zmq::socket_t publisher(context, zmq::socket_type::pub);
	
	try {
		publisher.bind(constants::GENERATOR_ENDPOINT);
		std::cout << "Generator started, publishing on " << constants::GENERATOR_ENDPOINT << std::endl;
	} catch (const zmq::error_t& e) {
		std::cerr << "Error binding ZMQ publisher: " << e.what() << std::endl;
		return -1;
	}

	// Loop over contents of the image directory forever (as per instructions) 
	int frame_count = 0;
	while (true) {
		// Update the directory contents list every time (to handle image addition or removal)
		std::vector<std::string> image_paths;
		try {
			image_paths = find_available_images(demo_dir);
		} catch (const std::runtime_error& e) {
			std::cerr << "Scan Error: " << e.what() << std::endl;
			// Sleep and try again in the next loop iteration
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
		
		if (image_paths.empty()) {
			std::cout << "Waiting for images to appear in directory..." << std::endl;
			// Sleep briefly to avoid busy waiting
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue; // Start the loop over to re-check
		}

		// Process the current list of images
		for (const std::string& full_path : image_paths) {
			frame_count++;
			
			// Read image from disk
			cv::Mat image = cv::imread(full_path, cv::IMREAD_COLOR);
			
			if (image.empty()) {
				std::cerr << "Warning: Could not read image " << full_path 
				<< ". Skipping and removing from current path scan." << std::endl;
				// If a file is suddenly corrupted/deleted during a loop, we skip it.
				continue; 
			}

			// Encode image to memory buffer
			std::vector<uchar> img_buffer;
			std::string extension = fs::path(full_path).extension().string(); 
			cv::imencode(extension, image, img_buffer);

			// Create ZMQ message parts
			std::string filename_only = fs::path(full_path).filename().string();
			
			// Part 1: Filename
			zmq::message_t name_msg(filename_only.begin(), filename_only.end());

			// Part 2: Image buffer
			zmq::message_t img_msg(img_buffer.data(), img_buffer.size());

			// Publish multi-part message
			publisher.send(name_msg, zmq::send_flags::sndmore);
			publisher.send(img_msg, zmq::send_flags::none);

			std::cout << "Sent image: " << filename_only << " (Frame " << frame_count 
			<< ", " << (img_buffer.size() / 1024) << " KB)" << std::endl;

			// Optionally sleep to simulate a slower frame rate (e.g., 50ms = 20 FPS)
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		
		// After sending the full batch, we pause slightly before re-scanning and starting the next batch.
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	return 0;
}
