#include "Serialization.hpp"
#include <stdexcept>
#include <cstring> // memcpy

// Define the size of a single serialized KeyPoint
// 5 floats + 2 ints
const size_t SIZEOF_SERIALIZED_KEYPOINT = (5 * sizeof(float)) + (2 * sizeof(int));

std::vector<char> serialize_keypoints(const std::vector<cv::KeyPoint>& keypoints) {
	// Pre-allocate the buffer
	std::vector<char> buffer(keypoints.size() * SIZEOF_SERIALIZED_KEYPOINT);
	
	char* ptr = buffer.data();
	
	for (const auto& kp : keypoints) {
		// Copy each field into the buffer
		std::memcpy(ptr, &kp.pt.x, sizeof(float));
		ptr += sizeof(float);
		
		std::memcpy(ptr, &kp.pt.y, sizeof(float));
		ptr += sizeof(float);
		
		std::memcpy(ptr, &kp.size, sizeof(float));
		ptr += sizeof(float);
		
		std::memcpy(ptr, &kp.angle, sizeof(float));
		ptr += sizeof(float);
		
		std::memcpy(ptr, &kp.response, sizeof(float));
		ptr += sizeof(float);
		
		std::memcpy(ptr, &kp.octave, sizeof(int));
		ptr += sizeof(int);
		
		std::memcpy(ptr, &kp.class_id, sizeof(int));
		ptr += sizeof(int);
	}
	
	return buffer;
}

std::vector<cv::KeyPoint> deserialize_keypoints(const std::vector<char>& data) {
	if (data.size() % SIZEOF_SERIALIZED_KEYPOINT != 0) {
		throw std::runtime_error("Invalid data size for keypoint deserialization.");
	}

	size_t num_keypoints = data.size() / SIZEOF_SERIALIZED_KEYPOINT;
	std::vector<cv::KeyPoint> keypoints(num_keypoints);
	
	const char* ptr = data.data();
	
	for (size_t i = 0; i < num_keypoints; ++i) {
		std::memcpy(&keypoints[i].pt.x, ptr, sizeof(float));
		ptr += sizeof(float);
		
		std::memcpy(&keypoints[i].pt.y, ptr, sizeof(float));
		ptr += sizeof(float);
		
		std::memcpy(&keypoints[i].size, ptr, sizeof(float));
		ptr += sizeof(float);
		
		std::memcpy(&keypoints[i].angle, ptr, sizeof(float));
		ptr += sizeof(float);
		
		std::memcpy(&keypoints[i].response, ptr, sizeof(float));
		ptr += sizeof(float);
		
		std::memcpy(&keypoints[i].octave, ptr, sizeof(int));
		ptr += sizeof(int);
		
		std::memcpy(&keypoints[i].class_id, ptr, sizeof(int));
		ptr += sizeof(int);
	}
	
	return keypoints;
}
