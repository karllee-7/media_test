#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <iostream>
#include <vector>
#include <queue>
#include <atomic>
#include <future>
#include <condition_variable>
#include <thread>
#include <functional>
#include <stdexcept>
#include <memory>

namespace karl
{
#define  THREADPOOL_MAX_NUM 16
//#define  THREADPOOL_AUTO_GROW

using std::cout;
using std::endl;
using std::function;
using std::vector;
using std::queue;
using std::mutex;
using std::condition_variable;
using std::atomic;
using std::unique_lock;
using std::future;
using std::runtime_error;
using std::make_shared;
using std::packaged_task;
using std::forward;
using std::bind;
using std::thread;
using std::shared_ptr;
using std::make_shared;

#if 0
template<class T>
class msgQueue{
private:
	atomic<bool> _run{true};
	queue<T> _queue;
	unsigned int _limit;
	mutex _lock;
	condition_variable _cv_i;
	condition_variable _cv_o;
public:
	msgQueue(unsigned int limit = 1):_limit(limit){
	}
	~msgQueue(){
	}
	
        void push(T inst){
                unique_lock<mutex> lock(_lock);
                if(_queue.size() >= _limit && _run){
                        _cv_i.wait(lock, [this]{ return _queue.size()<_limit || !_run; });
                }
                if(_run){
                        _queue.push(inst);
                }
                _cv_o.notify_all();
        }

	
	T pop(){
		unique_lock<mutex> lock(_lock);
		if(_queue.empty() && _run){
			_cv_o.wait(lock, [this]{ return !_queue.empty() || !_run; });
		}
		if(_run){
			T inst = _queue.back();
			_queue.pop();
			_cv_i.notify_all();
			return inst;
		}else{
			T tmp;
			_cv_i.notify_all();
			return tmp;
		}
	}

	unsigned int size(){
		unique_lock<mutex> lock(_lock);
		return _queue.size();
	}

	bool is_full(){
		return (_queue.size() >= _limit);
	}

	bool is_empty(){
		return (_queue.size() == 0);
	}

	void discon(){
		unique_lock<mutex> lock(_lock);
		while(!_queue.empty()){
			T inst = _queue.back();
			_queue.pop();
		}
		_run.store(false);
		_cv_i.notify_all();
		_cv_o.notify_all();
	}
};
#endif

class threadPool
{
	using Task = function<void()>;	
	vector<thread> _pool;     
	queue<Task> _tasks;            
	mutex _lock;                   
	condition_variable _pool_cv;   
	condition_variable _query_cv;   
	atomic<bool> _run{ true };     
	atomic<int>  _idlThrNum{ 0 };  
public:
	inline threadPool(unsigned short size = 4) { 
		for (; _pool.size() < THREADPOOL_MAX_NUM && size > 0; --size){   
			_pool.emplace_back( [this]{ 
				while (_run){
					Task task; 
					{	
						unique_lock<mutex> lock(_lock);
						if(_tasks.empty()){
							_idlThrNum++;
							// _query_cv.notify_all();
							_pool_cv.wait(lock, [this]{return !_run || !_tasks.empty();}); 
							_idlThrNum--;
						}
						if (!_run)
							return;
						task = move(_tasks.front()); 
						_tasks.pop();
						_query_cv.notify_all();
					}
					task();
				}
			});
		}
	}
	inline ~threadPool(){
		{
			unique_lock<mutex> lock(_lock);
			_run=false;
		}
		_pool_cv.notify_all(); 
		_query_cv.notify_all(); 
		for (thread& thread : _pool) {
			if(thread.joinable())
				thread.join(); 
		}
	}

	template<class F, class... Args>
	auto commit(F&& f, Args&&... args) ->future<decltype(f(args...))>{
		if (!_run)    
			throw runtime_error("commit on ThreadPool is stopped.");

		using RetType = decltype(f(args...)); 
		auto task = make_shared<packaged_task<RetType()>>(
			bind(forward<F>(f), forward<Args>(args)...)
		); 
		future<RetType> future = task->get_future();
		{    
			unique_lock<mutex> lock(_lock);
			_tasks.emplace([task](){ (*task)(); });
		}
		_pool_cv.notify_all(); 
		return future;
	}
	template<class F, class... Args>
	auto commitWait(F&& f, Args&&... args) ->future<decltype(f(args...))>{
		if (!_run)    
			throw runtime_error("commit on ThreadPool is stopped.");

		using RetType = decltype(f(args...)); 
		auto task = make_shared<packaged_task<RetType()>>(
			bind(forward<F>(f), forward<Args>(args)...)
		); 
		future<RetType> future = task->get_future();
		{    
			unique_lock<mutex> lock(_lock);
			// cout << "_idlThrNum: " << _idlThrNum << " _tasks.empty: " << _tasks.empty() << endl;
			if(!_tasks.empty()){
				// cout << "commit wait in." << endl;
				_query_cv.wait(lock, [this]{ return !_run || _tasks.empty(); });
				// cout << "commit wait out." << endl;
			}else{
				// cout << "commit no wait." << endl;
			}
			
			if(!_run)
				return future;
			_tasks.emplace([task](){ (*task)(); });
			_pool_cv.notify_all(); 
		}
		return future;
	}
	int idlCount() { 
		unique_lock<mutex> lock(_lock);
		return _idlThrNum; 
	}
	/*int taskWait() { 
		unique_lock<mutex> lock(_lock);
		return _pool.size(); 
	}*/
};




}

#endif
