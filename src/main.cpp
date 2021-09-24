#include <iostream>
#include "threadpool.h"

void C_Function(int num)
{
	std::this_thread::sleep_for(std::chrono::microseconds(10));
	std::cout << "C_Function num = " << num << "\t" << "ThreadID: " << std::this_thread::get_id() << std::endl;
	std::this_thread::sleep_for(std::chrono::microseconds(5));
}

struct StructWithFun
{
	int operator()(int n)
	{
		std::cout << "StructWithFun(" << n << ")" << "\t" << "ThreadID: " << std::this_thread::get_id() << std::endl;
		return 42;
	}
};

class ClassWithStaticFun
{
public:
	static int StaticMemberFun1(int n = 0)
	{
		std::cout << "StaticMemberFun1 = " << n << "\t" << "ThreadID: " << std::this_thread::get_id() << std::endl;
		return n;
	}

	static std::string StaticMemberFun2(int n, std::string str, char c)
	{
		std::cout << "StaticMemberFun2 = " << n << "\t" << str << "\t" << c << "ThreadID: " << std::this_thread::get_id() << std::endl;
		return str;
	}
};

#define TEST_TASK_COUNT 100
void AddTaskNoWaitFinish()
{
	std::cout << std::endl << __FUNCTION__ << std::endl;
	mystd::ThreadPool threadpool{ 50 };
	for (int i = 1; i <= TEST_TASK_COUNT; i++)
	{
		threadpool.Commit(C_Function, i);
	}
	std::cout << "Commit all ThreadID idleSize = " << threadpool.IdleThreadPoolSize() << std::endl;
}

void AddTaskWaitFinish()
{
	std::cout << std::endl << __FUNCTION__ << std::endl;
	mystd::ThreadPool threadpool(10);
	std::vector<std::future<int>> results;
	for (int i = 1; i <= TEST_TASK_COUNT; i++)
	{
		results.emplace_back(threadpool.Commit([i]() {
				//模拟任务时间
				std::this_thread::sleep_for(std::chrono::milliseconds(30));
				std::cout << "Num = " << i << "ThreadID = " << std::this_thread::get_id() << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(30));
				return i * i;
			}));
	}

	for (auto&& result : results)
	{
		result.get();
	}
}

void GetTaskReturnValueTest()
{
	std::cout << std::endl << __FUNCTION__ << std::endl;
	mystd::ThreadPool threadpool{ 50 };
	std::future<void> cFunFuture = threadpool.Commit(C_Function, 100);
	std::future<int> structFuture = threadpool.Commit(StructWithFun{}, 1111);
	ClassWithStaticFun aStaticClass;
	std::future<int> StaticFun1Future = threadpool.Commit(aStaticClass.StaticMemberFun1, 9999); //IDE提示错误,但可以编译运行
	std::future<std::string> StaticFun2Future = threadpool.Commit(ClassWithStaticFun::StaticMemberFun2, 9998, "mult args", 123);
	std::future<std::string> LambdaFuture = threadpool.Commit([]()->std::string {
		std::cout << "Lambda ThreadID = " << std::this_thread::get_id() << std::endl;
		return "Lambda return \n"; 
		});

	//调用.get()获取返回值会等待线程执行完,获取返回值
	cFunFuture.get();
	structFuture.get();
	StaticFun1Future.get();
	StaticFun2Future.get();
	printf("%s \n", LambdaFuture.get().c_str());
}

int main()
{
	try
	{
		AddTaskNoWaitFinish();
		AddTaskWaitFinish();
		GetTaskReturnValueTest();
	}
	catch (std::exception& e)
	{
		std::cout << "Some unhappy happened... ThreadID = " << std::this_thread::get_id() << "Error: " << e.what() << std::endl;
	}

	return 0;
}