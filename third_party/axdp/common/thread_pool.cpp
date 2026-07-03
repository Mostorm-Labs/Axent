#include "thread_pool.h"
#include <iostream>
namespace astro {

    ThreadPool::ThreadPool(int reserve_threads):
        run_mode_(RunMode::Static),
        exit_mode_(ExitMode::Lazy),
        shutdown_(false),
        reserve_threads_(reserve_threads),
        nthreads_(0),
        threads_waiting_(0)
    {
        for (int i = 0; i < reserve_threads_; i++)
        {
            std::lock_guard<std::mutex> lock(mu_);
            nthreads_++;
            new DynamicThread(this);
        }
    }

    ThreadPool::~ThreadPool()
    {
        std::unique_lock<std::mutex> lock_(mu_);
        shutdown_ = true;
        cv_.notify_all();
        //std::cout << "start delete pool" << callbacks_.size() << std::endl;
        while (nthreads_ != 0)
        {
            shutdown_cv_.wait(lock_);
        }

        reapThreads(&dead_threads_);
        //std::cout << "cb nums" << callbacks_.size() << std::endl;
    }

    void ThreadPool::add(const std::function<void()>& callback)
    {
        std::lock_guard<std::mutex> lock(mu_);

        // Add works to the callbacks list
        callbacks_.push(callback);
        //std::cout << "cb nums" << callbacks_.size() << std::endl;
        //std::cout << "waiting nums" << threads_waiting_ << std::endl;
        // Increase pool size or notify as needed
        if (threads_waiting_ == 0)
        {
            // Kick off a new thread
            if (run_mode_ == RunMode::Dynamic)
            {
                nthreads_++;
                new DynamicThread(this);
                //std::cout << "thread num" << nthreads_ << std::endl;
            }
        }
        else
        {
            cv_.notify_one();
        }

        // Also use this chance to harvest dead threads
        if (!dead_threads_.empty())
        {
            reapThreads(&dead_threads_);
        }
    }

    void ThreadPool::reapThreads(std::list<DynamicThread*>* tlist)
    {
        for (auto t = tlist->begin(); t != tlist->end(); t = tlist->erase(t))
        {
            delete* t;
        }
    }

    void ThreadPool::threadFunc()
    {
        for (;;)
        {
            std::unique_lock<std::mutex> lock(mu_);

            // Wait until work is available or we are shutting down.
            if (!shutdown_ && callbacks_.empty())
            {
                // If there are too many threads waiting, then quit this thread
                if (threads_waiting_ >= reserve_threads_)
                {
                    break;
                }

                threads_waiting_++;
                cv_.wait(lock);
                threads_waiting_--;
            }

            // Drain callbacks before considering shutdown to ensure all work gets completed.
            if (exit_mode_ == ExitMode::Lazy && shutdown_)
            {
                break;
            }
            else
            {
                if (!callbacks_.empty())
                {
                    auto cb = callbacks_.front();
                    callbacks_.pop();
                    lock.unlock();
                    cb();
                    //std::cout << "cb execute " << callbacks_.size() << std::endl;
                }
                else if (shutdown_)
                {
                    break;
                }
            }
        }
    }

    ThreadPool::DynamicThread::DynamicThread(ThreadPool* pool)
        : pool_(pool),
        thd_(new std::thread(&ThreadPool::DynamicThread::threadFunc, this))
    {
    }

    ThreadPool::DynamicThread::~DynamicThread()
    {
        thd_->join();
        thd_.reset();
    }

    void ThreadPool::DynamicThread::threadFunc()
    {
        pool_->threadFunc();

        // Now that we have killed ourselves, we should reduce the thread count
        std::unique_lock<std::mutex> lock(pool_->mu_);
        pool_->nthreads_--;

        // Move ourselves to dead list
        pool_->dead_threads_.push_back(this);

        if ((pool_->shutdown_) && (pool_->nthreads_ == 0))
        {
            pool_->shutdown_cv_.notify_one();
        }
    }

}  // namespace astro
