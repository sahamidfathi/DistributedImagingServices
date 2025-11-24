/**
 * App 2: Feature Extractor
 *
 * - Main thread:
 *   - Subscribes to the Image Generator's ZMQ PUB socket.
 *   - Receives multi-part messages (filename, image_buffer).
 *   - Wraps them as ImageTask and pushes into a SafeQueue<ImageTask>.
 *
 * - Worker threads:
 *   - Pop ImageTask from the work queue.
 *   - Decode the image with OpenCV.
 *   - Run SIFT to extract keypoints.
 *   - Serialize keypoints into a binary buffer.
 *   - Wrap as ProcessedTask and push into a SafeQueue<ProcessedTask>.
 *
 * - Sender thread:
 *   - Owns a ZMQ PUB socket bound at constants::EXTRACTOR_ENDPOINT.
 *   - Pops ProcessedTask and publishes multipart message:
 *	   [0] filename
 *	   [1] image_buffer (same compressed buffer from generator)
 *	   [2] keypoints_buffer
 */

#include <iostream>
#include <string>
#include <vector>
#include <thread>

#include "opencv2/opencv.hpp"
#include "opencv2/features2d.hpp"
#include "zmq.hpp"

#include "Constants.hpp"
#include "Serialization.hpp"
#include "SafeQueue.hpp"

// Work item received from generator
struct ImageTask {
	std::string filename;
	std::vector<uchar> img_buffer; // compressed image bytes
};

// Result item to send to logger
struct ProcessedTask {
	std::string filename;
	std::vector<uchar> img_buffer;	 // same compressed image
	std::vector<char> keypoints_buffer;
};

// Worker thread: pop ImageTask -> process -> push ProcessedTask
void worker_thread(int id,
				   SafeQueue<ImageTask>& work_queue,
				   SafeQueue<ProcessedTask>& result_queue)
{
	try {
		cv::Ptr<cv::SIFT> sift = cv::SIFT::create();

		ImageTask task;
		while (work_queue.pop(task)) {
			// Decode image
			cv::Mat image = cv::imdecode(task.img_buffer, cv::IMREAD_COLOR);
			if (image.empty()) {
				std::cerr << "[Worker " << id << "] Failed to decode " << task.filename << "\n";
				continue;
			}

			// Convert to grayscale (standard for SIFT)
			cv::Mat gray;
			cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

			// Extract keypoints
			std::vector<cv::KeyPoint> keypoints;
			sift->detect(gray, keypoints);

			// Serialize keypoints
			std::vector<char> kps_buffer = serialize_keypoints(keypoints);

			// Pack result
			ProcessedTask result;
			result.filename = std::move(task.filename);
			result.img_buffer = std::move(task.img_buffer);
			result.keypoints_buffer = std::move(kps_buffer);

			result_queue.push(std::move(result));

			std::cout << "[Worker " << id << "] Processed "
					  << result.filename << " (" << keypoints.size()
					  << " keypoints)\n";
		}
	} catch (const std::exception& e) {
		std::cerr << "[Worker " << id << "] Error: " << e.what() << "\n";
	}
}

void sender_thread(zmq::context_t& context,
				   SafeQueue<ProcessedTask>& result_queue)
{
	// Sender thread: pop ProcessedTask -> send via ZMQ PUB	
	zmq::socket_t publisher(context, zmq::socket_type::pub);
	try {
		publisher.bind(constants::EXTRACTOR_ENDPOINT);
		std::cout << "[Sender] Extractor publishing on "
				  << constants::EXTRACTOR_ENDPOINT << std::endl;
	} catch (const zmq::error_t& e) {
		std::cerr << "[Sender] Error binding ZMQ publisher: "
				  << e.what() << std::endl;
		return;
	}

	try {
		ProcessedTask result;
		while (result_queue.pop(result)) {
			// Build multipart message to logger
			// Part 1: filename
			zmq::message_t name_msg(result.filename.begin(), result.filename.end());

			// Part 2: image buffer
			zmq::message_t img_msg(result.img_buffer.data(),
								   result.img_buffer.size());

			// Part 3: keypoints buffer
			zmq::message_t kps_msg(result.keypoints_buffer.data(),
								   result.keypoints_buffer.size());

			publisher.send(name_msg, zmq::send_flags::sndmore);
			publisher.send(img_msg,  zmq::send_flags::sndmore);
			publisher.send(kps_msg,  zmq::send_flags::none);
		}
	} catch (const zmq::error_t& e) {
		std::cerr << "[Sender] ZMQ error: " << e.what() << std::endl;
	} catch (const std::exception& e) {
		std::cerr << "[Sender] Error: " << e.what() << std::endl;
	}
}

int main(int argc, char* argv[]) {
	// ZMQ context for the whole extractor process
	zmq::context_t context(1);

	// Socket to receive from image generator (App 1)
	zmq::socket_t subscriber(context, zmq::socket_type::sub);
	try {
		subscriber.connect(constants::GENERATOR_CONNECT_TO);
		subscriber.set(zmq::sockopt::subscribe, ""); 
		std::cout << "[Extractor] Subscribing to "
				  << constants::GENERATOR_CONNECT_TO << std::endl;
	} catch (const zmq::error_t& e) {
		std::cerr << "[Extractor] Error connecting ZMQ subscriber: "
				  << e.what() << std::endl;
		return -1;
	}

	// Shared queues
	SafeQueue<ImageTask>	work_queue;
	SafeQueue<ProcessedTask> result_queue;

	// Start worker threads
	unsigned int num_workers = std::thread::hardware_concurrency();
	if (num_workers == 0) num_workers = 2; // fallback

	std::cout << "[Extractor] Launching " << num_workers
			  << " worker threads...\n";

	std::vector<std::thread> workers;
	workers.reserve(num_workers);
	for (unsigned int i = 0; i < num_workers; ++i) {
		workers.emplace_back(worker_thread,
							 static_cast<int>(i),
							 std::ref(work_queue),
							 std::ref(result_queue));
	}

	// Start sender thread (owns PUB socket)
	std::thread sender(sender_thread, std::ref(context), std::ref(result_queue));

	// Main loop: receive from generator & push tasks
	while (true) {
		zmq::message_t name_msg;
		zmq::message_t img_msg;
		
		// Receive Part 1: Filename
		auto recv_name = subscriber.recv(name_msg);
		if (!recv_name.has_value()) { continue; }

		if (!subscriber.get(zmq::sockopt::rcvmore)) {
			std::cerr << "[Extractor] Warning: expected 2 parts, got 1. Skipping.\n";
			continue;
		}
		
		// Receive Part 2: Image buffer
		auto recv_img = subscriber.recv(img_msg);
		if (!recv_img.has_value()) { continue; }

		// Check that this is the last part
		if (subscriber.get(zmq::sockopt::rcvmore)) {
			std::cerr << "[Extractor] Warning: received >2 parts. Flushing extras.\n";
			zmq::message_t temp;
			while (subscriber.get(zmq::sockopt::rcvmore)) {
				subscriber.recv(temp);
			}
			continue;
		}

		ImageTask task;
		task.filename = name_msg.to_string();

		task.img_buffer.assign(
			static_cast<uchar*>(img_msg.data()),
			static_cast<uchar*>(img_msg.data()) + img_msg.size()
		);

		work_queue.push(std::move(task));
	}

	sender.join();
	for (auto& w : workers) {
		if (w.joinable()) w.join();
	}

	return 0;
}

