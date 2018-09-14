#ifndef _KARL_QUEUE_H_
#define _KARL_QUEUE_H_

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

templat<class T>
class karl_queue{
public:
	mutex _lock;
	condition_variable _cv;
	queue<T> _queue;
	uint32_t _limit;
	msgQueue(unsigned int limit = 1):_limit(limit){
	}
	~msgQueue(){
	}
        void push(T inst){
                unique_lock<mutex> lock(_lock);
                if(_queue.size() >= _limit && _connected){
                        _cv_i.wait(lock, [this]{ return _queue.size()<_limit || !_connected; });
                }
                if(_connected){
                        _queue.push(inst);
                }
                _cv_o.notify_all();
        }
	T pop(){
		unique_lock<mutex> lock(_lock);
		if(_queue.empty() && _connected){
			_cv_o.wait(lock, [this]{ return !_queue.empty() || !_connected; });
		}
		if(_connected){
			T inst = _queue.front();
			_queue.pop();
			_cv_i.notify_all();
			return inst;
		}else{
			T tmp;
			_cv_i.notify_all();
			return tmp;
		}
	}
	int size(){
		return _queue.size();
	}
	bool is_full(){
		return (_queue.size() >= _limit);
	}
	bool is_empty(){
		return (_queue.size() == 0);
	}
};

}

#endif
