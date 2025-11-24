/**
 * App 3: Data Logger
 * - Subscribes to the Feature Extractor's ZMQ PUB socket.
 * - Receives multi-part messages (filename, image_buffer, keypoints_buffer).
 * - Stores them into SQLite as BLOBs.
 */

#include <iostream>
#include <string>
#include <vector>

#include "zmq.hpp"
#include "sqlite3.h"
#include "Constants.hpp"
#include "Serialization.hpp" 

// Helper function to initialize the database
void setup_database(sqlite3** db) {
	if (sqlite3_open("processed_data.db", db) != SQLITE_OK) {
		std::cerr << "Error opening database: "
				  << sqlite3_errmsg(*db) << std::endl;
		*db = nullptr;
		return;
	}

	const char* create_table_sql = R"(
	CREATE TABLE IF NOT EXISTS processed_images (
		id INTEGER PRIMARY KEY AUTOINCREMENT,
		filename TEXT NOT NULL,
		timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
		image_blob BLOB,
		keypoints_blob BLOB
	);
	)";

	char* err_msg = nullptr;
	if (sqlite3_exec(*db, create_table_sql, 0, 0, &err_msg) != SQLITE_OK) {
		std::cerr << "Error creating table: " << err_msg << std::endl;
		sqlite3_free(err_msg);
	}
}

int main(int argc, char* argv[]) {
	// SQLite Setup
	sqlite3* db = nullptr;
	setup_database(&db);
	if (!db) {
		return -1;
	}

	// Prepare the INSERT statement
	sqlite3_stmt* stmt = nullptr;
	const char* insert_sql = 
		"INSERT INTO processed_images (filename, image_blob, keypoints_blob) "
		"VALUES (?, ?, ?);";
	if (sqlite3_prepare_v2(db, insert_sql, -1, &stmt, 0) != SQLITE_OK) {
		std::cerr << "Error preparing statement: "
				  << sqlite3_errmsg(db) << std::endl;
		sqlite3_close(db);
		return -1;
	}

	// ZMQ Setup
	zmq::context_t context(1);
	zmq::socket_t subscriber(context, zmq::socket_type::sub);
	try {
		subscriber.connect(constants::EXTRACTOR_CONNECT_TO);
		subscriber.set(zmq::sockopt::subscribe, ""); // Subscribe to all
		std::cout << "Logger started, subscribing to "
				  << constants::EXTRACTOR_CONNECT_TO << std::endl;
	} catch (const zmq::error_t& e) {
		std::cerr << "Error connecting ZMQ subscriber: "
				  << e.what() << std::endl;
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return -1;
	}

	while (true) {
		zmq::message_t name_msg;
		zmq::message_t img_msg;
		zmq::message_t kps_msg;

		// Part 1: Filename
		auto recv_name = subscriber.recv(name_msg);
		if (!recv_name.has_value()) { continue; }

		if (!subscriber.get(zmq::sockopt::rcvmore)) {
			std::cerr << "Warning: expected 3 parts, got 1." << std::endl;
			continue;
		}

		// Part 2: Image buffer
		auto recv_img = subscriber.recv(img_msg);
		if (!recv_img.has_value()) { continue; }

		if (!subscriber.get(zmq::sockopt::rcvmore)) {
			std::cerr << "Warning: expected 3 parts, got 2." << std::endl;
			continue;
		}

		// Part 3: Keypoints
		auto recv_kps = subscriber.recv(kps_msg);
		if (!recv_kps.has_value()) { continue; }

		if (subscriber.get(zmq::sockopt::rcvmore)) {
			std::cerr << "Warning: received >3 parts. Flushing extras." << std::endl;
			zmq::message_t temp;
			while (subscriber.get(zmq::sockopt::rcvmore)) {
				subscriber.recv(temp);
			}
			continue;
		}

		std::string filename = name_msg.to_string();

		// Bind data to prepared stmt
		sqlite3_reset(stmt);

		if (sqlite3_bind_text(stmt, 1, filename.c_str(), -1,
							  SQLITE_TRANSIENT) != SQLITE_OK) {
			std::cerr << "SQLite bind error (filename)." << std::endl;
			continue;
		}

		if (sqlite3_bind_blob(stmt, 2, img_msg.data(), img_msg.size(),
							  SQLITE_TRANSIENT) != SQLITE_OK) {
			std::cerr << "SQLite bind error (image_blob)." << std::endl;
			continue;
		}

		if (sqlite3_bind_blob(stmt, 3, kps_msg.data(), kps_msg.size(),
							  SQLITE_TRANSIENT) != SQLITE_OK) {
			std::cerr << "SQLite bind error (keypoints_blob)." << std::endl;
			continue;
		}

		int rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE) {
			std::cerr << "Error inserting data: "
					  << sqlite3_errmsg(db) << std::endl;
		} else {
			std::vector<char> kps_vec(
				static_cast<char*>(kps_msg.data()),
				static_cast<char*>(kps_msg.data()) + kps_msg.size()
			);
			size_t keypoint_count = 0;
			try {
				keypoint_count = deserialize_keypoints(kps_vec).size();
			} catch (...) {
				// ignore
			}

			std::cout << "Logged image: " << filename
					  << " (" << (img_msg.size() / 1024)
					  << " KB, " << keypoint_count
					  << " keypoints)" << std::endl;
		}
	}

	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return 0;
}

