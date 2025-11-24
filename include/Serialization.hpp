#ifndef SERIALIZATION_HPP
#define SERIALIZATION_HPP

#include <vector>
#include <opencv2/core.hpp> // For cv::KeyPoint

/**
 * @brief Serializes a vector of cv::KeyPoint into a flat binary buffer.
 *
 * cv::KeyPoint is a struct containing multiple fields. This function
 * packs them into a single byte vector for transport.
 *
 * Format per KeyPoint:
 * - pt.x (float)
 * - pt.y (float)
 * - size (float)
 * - angle (float)
 * - response (float)
 * - octave (int)
 * - class_id (int)
 *
 * @param keypoints The vector of keypoints to serialize.
 * @return A std::vector<char> containing the serialized data.
 */
std::vector<char> serialize_keypoints(const std::vector<cv::KeyPoint>& keypoints);

/**
 * @brief Deserializes a flat binary buffer back into a vector of cv::KeyPoint.
 *
 * @param data The binary data buffer.
 * @return A std::vector<cv::KeyPoint> reconstructed from the buffer.
 * @throws std::runtime_error if the buffer size is invalid.
 */
std::vector<cv::KeyPoint> deserialize_keypoints(const std::vector<char>& data);

#endif // SERIALIZATION_HPP
