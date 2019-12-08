#include <iostream>
#include <string>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <cassert>
#include <memory>
#include <functional>


class Queue
{
public:
	virtual void push(uint8_t val) = 0;
	virtual bool pop(uint8_t& val) = 0;
	virtual bool empty() const = 0;
	virtual std::string to_string() const = 0;
	virtual ~Queue() {}

protected:
	std::chrono::milliseconds wait_time_ = std::chrono::milliseconds(1);
};


class MutexDynamicQueue : public Queue
{
public:
	void push(uint8_t val) override
	{
		std::lock_guard<std::mutex> guard(mutex_);
		queue_.push(val);
	}

	bool pop(uint8_t& val) override
	{
		std::unique_lock<std::mutex> unq_lock(mutex_);
		if (queue_.empty())
		{
			unq_lock.unlock();
			std::this_thread::sleep_for(wait_time_);

			unq_lock.lock();
			if (queue_.empty())
				return false;
		}

		val = queue_.front();
		queue_.pop();
		return true;
	}

	bool empty() const override
	{
		return queue_.empty();
	}

	std::string to_string() const override
	{
		return "динамическая очередь с использованием std::mutex";
	}

	~MutexDynamicQueue() override {}

private:
	std::queue<uint8_t> queue_;
	std::mutex mutex_;
};


class MutexFixedQueue : public Queue
{
public:
	MutexFixedQueue(size_t size) : size_(size) {}

	void push(uint8_t val) override
	{
		std::unique_lock<std::mutex> unq_lock(mutex_);
		push_condition_variable_.wait(unq_lock, std::bind(&MutexFixedQueue::is_not_full, this));
		queue_.push(val);
		pop_condition_variable_.notify_one();
	}

	bool pop(uint8_t& val) override
	{
		std::unique_lock<std::mutex> unq_lock(mutex_);
		if (pop_condition_variable_.wait_for(unq_lock, wait_time_, std::bind(&MutexFixedQueue::is_not_empty, this)))
		{
			val = queue_.front();
			queue_.pop();
			push_condition_variable_.notify_one();
			return true;
		}
		else
			return false;
	}

	bool empty() const override
	{
		return queue_.empty();
	}

	std::string to_string() const override
	{
		return "очередь размера " + std::to_string(size_) + " с использованием std::mutex";
	}

	~MutexFixedQueue() override {}

private:
	std::queue<uint8_t> queue_;
	std::size_t size_;
	std::mutex mutex_;
	std::condition_variable push_condition_variable_;
	std::condition_variable pop_condition_variable_;

	bool is_not_full()
	{
		return queue_.size() < size_;
	}

	bool is_not_empty()
	{
		return !queue_.empty();
	}
};



class AtomicQueue : public Queue
{
public:
	AtomicQueue()
	{
		head_ = NodePtr(new Node());
		tail_ = head_;
	}

	void push(uint8_t val) override
	{
		NodePtr node(new Node(val));
		while (true)
		{
			NodePtr tail = std::atomic_load(&tail_);
			NodePtr null_ptr(nullptr);
			if (std::atomic_compare_exchange_strong(&(tail->next), &null_ptr, node))
			{
				std::atomic_compare_exchange_strong(&tail_, &tail, node);
				return;
			}
			else
				std::atomic_compare_exchange_strong(&tail_, &tail, tail->next);
		}
	}

	bool pop(uint8_t& val) override
	{
		bool is_wait = false;
		while (true)
		{
			NodePtr head = std::atomic_load(&head_);
			NodePtr tail = std::atomic_load(&tail_);
			NodePtr after_head = std::atomic_load(&(head->next));
			if (head == tail)
			{
				if (after_head == nullptr)
				{
					if (is_wait)
						return false;
					else
					{
						std::this_thread::sleep_for(wait_time_);
						is_wait = true;
					}
				}
			}
			else
			{
				uint8_t temp_val = after_head->value;
				if (std::atomic_compare_exchange_strong(&head_, &head, after_head))
				{
					val = temp_val;
					return true;
				}
			}
		}
	}

	bool empty() const override
	{
		return head_ == tail_ && head_->next == nullptr;
	}

	std::string to_string() const override
	{
		return "очередь с использованием std::atomic";
	}

	~AtomicQueue() override {}

private:
	struct Node
	{
		uint8_t value;
		std::shared_ptr<Node> next;

		Node(const uint8_t& val = 0) : value(val), next(nullptr) {}
	};

	using NodePtr = std::shared_ptr<Node>;

	NodePtr head_;
	NodePtr tail_;
};


void put_thread_function(Queue& queue, const int& task_num)
{
	for (int i = 0; i < task_num; i++)
		queue.push(1);
}


void pop_thread_function(Queue& queue, const int& all_tasks, std::atomic_int& sum)
{
	uint8_t val;
	while (sum.load() < all_tasks)
		sum += queue.pop(val);
}


void execute_task(Queue& queue, const int& producer_num, const int& consumer_num, const int& task_num)
{
	int all_tasks = producer_num * task_num;
	std::atomic_int sum(0);
	std::vector<std::thread> producers(producer_num);
	std::vector<std::thread> consumers(consumer_num);
	std::chrono::time_point<std::chrono::high_resolution_clock> start, end;

	start = std::chrono::high_resolution_clock::now();

	for (auto& thread : producers)
		thread = std::thread(put_thread_function, std::ref(queue), std::ref(task_num));

	for (auto& thread : consumers)
		thread = std::thread(pop_thread_function, std::ref(queue), std::ref(all_tasks), std::ref(sum));

	for (auto& thread : producers)
		if (thread.joinable())
			thread.join();

	for (auto& thread : consumers)
		if (thread.joinable())
			thread.join();

	end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> time = end - start;

	std::cout << "Время работы " << (queue.to_string()) << " при " << producer_num << " производителей, " << consumer_num << " потребителей и " << task_num
		<< " количестве задач равно " << time.count() << " с" << std::endl;

	assert(queue.empty());
}


int main()
{
	setlocale(LC_ALL, "russian");

	const std::vector<int> producer_nums = { 1, 2, 4 };
	const std::vector<int> consumer_nums = { 1, 2, 4 };
	const std::vector<size_t> queue_size = { 1, 4, 16 };
	const int task_num = 100;

	for (const int& producer_num : producer_nums)
	{
		for (const int& consumer_num : consumer_nums)
		{
			MutexDynamicQueue mutex_dynamic_queue;
			execute_task(mutex_dynamic_queue, producer_num, consumer_num, task_num);

			AtomicQueue atomic_queue;
			execute_task(atomic_queue, producer_num, consumer_num, task_num);

			for (const size_t& size : queue_size)
			{
				MutexFixedQueue mutex_fixed_queue(size);
				execute_task(mutex_fixed_queue, producer_num, consumer_num, task_num);
			}
		}
	}

	return 0;
}
