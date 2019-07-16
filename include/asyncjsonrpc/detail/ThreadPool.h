/**
This library was written by Samer Afach, find this code in the repository
git.afach.de, 2017

Terms of use:

-You're free to do anything you wish with this simple library, including any
embedding or modifications you wish, with static or dynamic linking (you can't
dynamically link it... it's header only! :-) ).
-The creator of this library is not to be held liable for anything you do with
it. Using this library is your own responsibility
*/

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

class ThreadPool
{
    long                              numOfThreads;
    std::deque<std::function<void()>> _tasks;
    std::mutex                        _queueLock;
    bool                              conclude_work   = false;
    bool                              started_already = false;
    std::condition_variable           _queueCond;
    std::condition_variable           _threadFinishedCond;
    int                               num_of_threads_running = 0;
    std::vector<std::thread>          _threads;

protected:
    inline void thread_worker();
    inline void join_all();

public:
    inline ThreadPool();
    inline ~ThreadPool();
    inline void push(const std::function<void()>& task);
    inline void push(std::function<void()>&& task);
    inline void start(const long NumOfThreads = std::thread::hardware_concurrency());
    inline void finish();
};

void ThreadPool::thread_worker()
{
    while (true) {
        std::unique_lock<decltype(_queueLock)> lg(_queueLock);
        while (_tasks.empty() && !conclude_work) {
            _queueCond.wait(lg);
        }
        if (!_tasks.empty()) {
            auto theTask = _tasks[0];
            _tasks.pop_front();
            lg.unlock();
            theTask();
            lg.lock();
        }
        if (_tasks.empty() && conclude_work) {
            num_of_threads_running--;
            _threadFinishedCond.notify_one();
            break;
        }
    }
}

void ThreadPool::join_all()
{
    for (long i = 0; i < static_cast<long>(_threads.size()); i++) {
        if (_threads[i].joinable())
            _threads[i].join();
    }
}

ThreadPool::ThreadPool() {}

ThreadPool::~ThreadPool() { this->finish(); }

void ThreadPool::push(const std::function<void()>& task)
{
    std::unique_lock<decltype(_queueLock)> lg(_queueLock);
    _tasks.push_back(task);
    _queueCond.notify_one();
}

void ThreadPool::push(std::function<void()>&& task)
{
    std::unique_lock<decltype(_queueLock)> lg(_queueLock);
    _tasks.push_back(std::move(task));
    _queueCond.notify_one();
}

void ThreadPool::start(const long NumOfThreads)
{
    if (!started_already)
        started_already = true;
    else
        throw std::logic_error("You cannot start the thread pool multiple times.");
    numOfThreads = NumOfThreads;
    _threads.reserve(numOfThreads);
    for (long i = 0; i < numOfThreads; i++) {
        _threads.push_back(std::thread(std::bind(&ThreadPool::thread_worker, this)));
        num_of_threads_running++;
    }
}

void ThreadPool::finish()
{
    std::unique_lock<decltype(_queueLock)> lg(_queueLock);
    conclude_work = true;
    _queueCond.notify_all();
    while (num_of_threads_running > 0) {
        _threadFinishedCond.wait(lg);
    }
    lg.unlock();
    join_all();
}

#endif // THREADPOOL_H
