#ifndef SAFE_QUEUE_HPP
#define SAFE_QUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class SafeQueue {
private:
	std::queue<T> queue_;
	std::mutex mutex_;
	std::condition_variable cond_;

public:
	void push(T value) {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			queue_.push(std::move(value));
		}
		cond_.notify_one(); // wake a waiting thread
	}

	// Blocks until an item is available
	bool pop(T& value) {
		std::unique_lock<std::mutex> lock(mutex_);
		cond_.wait(lock, [this]{ return !queue_.empty(); });

		value = std::move(queue_.front());
		queue_.pop();
		return true;
	}
};

#endif // SAFE_QUEUE_HPP

