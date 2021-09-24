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
	//�̳߳����������Ӧ����Сһ�㣩
#define THREADPOOL_MAX_NUM	16
//#define  THREADPOOL_AUTO_GROW

	/*
		�̳߳أ� �����ύ��κ�������ķ����ʽ����������ִ�У����Ի�ȡִ�з���ֵ��
				��ֱ��֧�����Ա������֧���ྲ̬��Ա������ȫ�ֺ�����Operator()������
	*/
	class ThreadPool
	{
	private:
		using Task = std::function<void()>;		// ��������
		std::vector<std::thread> _pool;			// �̳߳�
		std::queue<Task> _tasks;				// �������
		std::mutex _lock;						// ͬ��
		std::condition_variable _task_cv;		// ��������
		std::atomic<bool> _run{ true };			// �̳߳��Ƿ�ִ��
		std::atomic<int> _idleThread{ 0 };		// �����߳�����
	public:
		ThreadPool(unsigned short size = 4)
		{
			AddThread(size);
		}

		~ThreadPool()
		{
			_run = false;
			_task_cv.notify_all(); // ���������߳�ִ��
			for (std::thread& thread : _pool)
			{
				//thread.detach();	// ���߳���������
				if (thread.joinable())
					thread.join();	// ʹ�߳������̷߳��루��detach��ȣ����̻߳�ȴ��߳�ִ����ϣ�
			}
		}
	public:
		ThreadPool(const ThreadPool& that) = delete;
		ThreadPool(ThreadPool&& that) = delete;
		ThreadPool& operator = (const ThreadPool& that) = delete;
	public:
		/*
			�ύһ������
			����.get()��ȡ����ֵ��ȴ�����ִ����ϣ���ȡ����ֵ�����ַ�������ʵ�ֵ������Ա��
			1. ʹ��std::bind:		.Commit(std::bind(&Dog::sayHello, &dog));
			2. ʹ��std::mem_fn:		.Commit(std::mem_fn(&Dog::sayHello), this);
		*/
		template<typename F, typename... Args>
		auto Commit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>
		{
			if (!_run)
				throw std::runtime_error("Commit on ThreadPool is Stopped!");
			// typename std::result_of<F(Args...)>::type, ���� f �ķ���ֵ����
			using RetType = decltype(f(args...));
			auto task = std::make_shared<std::packaged_task<RetType()>>(
				// ��������ת��
				std::bind(std::forward<F>(f), std::forward<Args>(args)...)
				);

			// �Ѻ����˿ڼ�����������󶨣�
			std::future<RetType> future = task->get_future();
			{
				// ������񵽶���
				//lock_guard: �Խ���������lock������unlock��
				std::lock_guard<std::mutex> lock{ _lock };
				// ���뵽���У���ͬ�ڣ�insert(Task{...})
				_tasks.emplace([task]() {(*task)(); });
			}
#ifdef THREADPOOL_AUTO_GROW
			if (_idleThread < 1 && _pool.size() < THREADPOOL_MAX_NUM)
				AddThread(1);
#endif // !THREADPOOL_AUTO_GROW
			// ����һ���߳�ִ��
			_task_cv.notify_one();
			return future;
		}

		// �����߳�����
		int IdleThreadPoolSize()
		{
			return _idleThread;
		}

		// �����߳�����
		int ThreadPoolSize()
		{
			return _pool.size();
		}

#ifndef THREADPOOL_AUTO_GROW
	private:
#endif // !THREADPOOL_AUTO_GROW

		// ���ָ���������߳�
		void AddThread(unsigned short size)
		{
			//�����߳���������������Ԥ������ THREADPOOL_MAX_NUM
			for (; _pool.size() < THREADPOOL_MAX_NUM && size > 0; --size)
			{
				// �����̺߳���
				_pool.emplace_back([this]() {
					while (_run)
					{
						// ��ȡһ����ִ�е�task
						Task task;
						{
							// ��ռ������Դ�������Խ����ô�ʱ��������ʱlock��unlock
							std::unique_lock<std::mutex> lock{ _lock };
							// ������ǰ�̣߳�֪������Ϊ��ű�����
							_task_cv.wait(lock, [this]() {
								return !_run || !_tasks.empty();
								});

							// waitֱ����task
							if (!_run && _tasks.empty())
								return;
							//���Ƚ��ȳ��Ӷ���ȡһ��task
							task = std::move(_tasks.front()); // std::move: ����ֵ����ֵת��Ϊ��ֵ����task
							_tasks.pop();
						}
						_idleThread--;
						//ִ������
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