#pragma once
#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

#include <vector>
#include <queue>
#include <atomic>
#include <future>
#include <stdexcept>

namespace mystd
{
	//线程池最大容量（应尽量小一点）
#define THREADPOOL_MAX_NUM	16
//#define  THREADPOOL_AUTO_GROW

	/*
		线程池： 可以提交变参函数或拉姆达表达式的匿名函数执行；可以获取执行返回值；
				不直接支持类成员函数，支持类静态成员函数或全局函数，Operator()函数等
	*/
	class ThreadPool
	{
	private:
		using Task = std::function<void()>;		// 任务类型
		std::vector<std::thread> _pool;			// 线程池
		std::queue<Task> _tasks;				// 任务队列
		std::mutex _lock;						// 同步
		std::condition_variable _task_cv;		// 条件阻塞
		std::atomic<bool> _run{ true };			// 线程池是否执行
		std::atomic<int> _idleThread{ 0 };		// 空闲线程数量
	public:
		ThreadPool(unsigned short size = 4)
		{
			AddThread(size);
		}

		~ThreadPool()
		{
			_run = false;
			_task_cv.notify_all(); // 唤醒所有线程执行
			for (std::thread& thread : _pool)
			{
				//thread.detach();	// 让线程自生自灭
				if (thread.joinable())
					thread.join();	// 使线程与主线程分离（与detach相比：主线程会等待线程执行完毕）
			}
		}
	public:
		ThreadPool(const ThreadPool& that) = delete;
		ThreadPool(ThreadPool&& that) = delete;
		ThreadPool& operator = (const ThreadPool& that) = delete;
	public:
		/*
			提交一个任务
			调用.get()获取返回值会等待任务执行完毕，获取返回值有两种方法可以实现调用类成员：
			1. 使用std::bind:		.Commit(std::bind(&Dog::sayHello, &dog));
			2. 使用std::mem_fn:		.Commit(std::mem_fn(&Dog::sayHello), this);
		*/
		template<typename F, typename... Args>
		auto Commit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>
		{
			if (!_run)
				throw std::runtime_error("Commit on ThreadPool is Stopped!");
			// typename std::result_of<F(Args...)>::type, 函数 f 的返回值类型
			using RetType = decltype(f(args...));
			auto task = std::make_shared<std::packaged_task<RetType()>>(
				// 参数完美转发
				std::bind(std::forward<F>(f), std::forward<Args>(args)...)
				);

			// 把函数人口及参数打包（绑定）
			std::future<RetType> future = task->get_future();
			{
				// 添加任务到队列
				//lock_guard: 自解锁（构造lock、析构unlock）
				std::lock_guard<std::mutex> lock{ _lock };
				// 插入到队列（等同于：insert(Task{...})
				_tasks.emplace([task]() {(*task)(); });
			}
#ifdef THREADPOOL_AUTO_GROW
			if (_idleThread < 1 && _pool.size() < THREADPOOL_MAX_NUM)
				AddThread(1);
#endif // !THREADPOOL_AUTO_GROW
			// 唤醒一个线程执行
			_task_cv.notify_one();
			return future;
		}

		// 空闲线程数量
		int IdleThreadPoolSize()
		{
			return _idleThread;
		}

		// 池中线程数量
		int ThreadPoolSize()
		{
			return _pool.size();
		}

#ifndef THREADPOOL_AUTO_GROW
	private:
#endif // !THREADPOOL_AUTO_GROW

		// 添加指定数量的线程
		void AddThread(unsigned short size)
		{
			//增加线程数量，但不超过预定数量 THREADPOOL_MAX_NUM
			for (; _pool.size() < THREADPOOL_MAX_NUM && size > 0; --size)
			{
				// 工作线程函数
				_pool.emplace_back([this]() {
					while (_run)
					{
						// 获取一个待执行的task
						Task task;
						{
							// 独占锁（资源独享）与自解锁好处时：可以随时lock、unlock
							std::unique_lock<std::mutex> lock{ _lock };
							// 阻塞当前线程，知道条件为真才被唤醒
							_task_cv.wait(lock, [this]() {
								return !_run || !_tasks.empty();
								});

							// wait直到有task
							if (!_run && _tasks.empty())
								return;
							//按先进先出从队列取一个task
							task = std::move(_tasks.front()); // std::move: 将左值或将亡值转换为右值赋给task
							_tasks.pop();
						}
						_idleThread--;
						//执行任务
						task();
						_idleThread++;
					}
				});
				_idleThread++;
			}
		}
	};

}

#endif