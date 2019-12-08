#include <iostream>
#include <vector>
#include <array>
#include <mutex>
#include <atomic>
#include <chrono>
#include <assert.h>
#include <string>


class Array
{
public:
	Array(const size_t& size)
	{
		size_ = size;
		arr_.assign(size, 0);
	}

	void increment(const int& num_thread, const int sleep_time = 0)
	{
		std::vector<std::thread> threads(num_thread);

		for (auto& thread : threads)
			thread = std::thread(&Array::thread_function, this, sleep_time);

		for (auto& thread : threads)
			if (thread.joinable())
				thread.join();

		for (const int& el : arr_)
			assert(el == 1);
	}

	virtual std::string to_string() = 0;

	virtual ~Array() {}

protected:
	std::vector<int> arr_;
	size_t size_;

	virtual void thread_function(const int& sleep_time = 0) = 0;
};


class MutexArray : public Array
{
public:
	MutexArray(const size_t& size) : Array(size) {}

	std::string to_string() override
	{
		return "std::mutex";
	}

	virtual ~MutexArray() {}

private:
	std::mutex mutex_;
	size_t index_ = -1;

	void thread_function(const int& sleep_time = 0) override
	{
		while (true)
		{
			mutex_.lock();
			index_++;
			if (index_ >= size_)
				break;
			arr_[index_]++;
			if (sleep_time)
				std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_time));
			mutex_.unlock();
		}
		mutex_.unlock();
	}
};


class AtomicArray : public Array
{
public:
	AtomicArray(const size_t& size) : Array(size) {}

	std::string to_string() override
	{
		return "std::atomic";
	}

	virtual ~AtomicArray() {}

private:
	std::atomic_size_t index_ = 0;

	void thread_function(const int& sleep_time = 0) override
	{
		while (true)
		{
			size_t temp_index = index_.fetch_add(1);
			if (temp_index >= size_)
				break;
			arr_.at(temp_index)++;

			if (sleep_time)
				std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_time));
		}
	}
};


void execute_task(Array& array, const int& num_thread, const int& sleep_time)
{
	std::chrono::time_point<std::chrono::high_resolution_clock> start, end;

	start = std::chrono::high_resolution_clock::now();
	array.increment(num_thread, sleep_time);
	end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> time = end - start;

	std::cout << "Время работы " << num_thread << " потоков ";
	if (sleep_time)
		std::cout << "c усыплением потока на " << sleep_time << " нс ";
	std::cout << "с использованием " << array.to_string() << " равно " << time.count() << " с" << std::endl;
}


int main()
{
	setlocale(LC_ALL, "russian");

	const size_t size = 4 * 1024;
	const std::vector<int> num_threads = { 4, 8, 16, 32 };
	const std::vector<int> sleep_times = { 0, 10 };

	for (const int& num_thread : num_threads)
	{
		for (const int& sleep_time : sleep_times)
		{
			MutexArray mutex_array(size);
			execute_task(mutex_array, num_thread, sleep_time);

			AtomicArray atomic_array(size);
			execute_task(atomic_array, num_thread, sleep_time);

		}
	}
	return 0;
}