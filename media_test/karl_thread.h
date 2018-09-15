#ifndef _KARL_QUEUE_H_
#define _KARL_QUEUE_H_

#include <iostream>
#include <queue>
#include <atomic>
#include <future>
#include <condition_variable>

namespace karl
{

using std::cout;
using std::endl;
using std::queue;
using std::mutex;
using std::condition_variable;
using std::atomic;
using std::unique_lock;
using std::future;
/*==============================================================================*/
template<class T>
class karl_queue{
public:
    mutex _lock;
    condition_variable _cv;
    queue<T> _queue;
    uint32_t _limit;
    karl_queue(unsigned int limit = 1):_limit(limit){
    }
    ~karl_queue(){
    }
    unsigned int size(){
        return _queue.size();
    }
    bool is_full(){
        return (_queue.size() >= _limit);
    }
    bool is_empty(){
        return (_queue.size() == 0);
    }
};
/*==============================================================================*/
template<class T>
class karl_protected{
public:
    mutex _lock;
    condition_variable _cv;
    T _data;
    karl_protected(){
    }
    template<typename T1>
    karl_protected(T1& data):_data(data){
    }
    ~karl_protected(){
    }
};
/*==============================================================================*/
}

#endif
