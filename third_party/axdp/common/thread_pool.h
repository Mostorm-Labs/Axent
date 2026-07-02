#ifndef ASTRO_INTERNAL_CPP_DYNAMIC_THREAD_POOL_H
#define ASTRO_INTERNAL_CPP_DYNAMIC_THREAD_POOL_H

#include <list>
#include <memory>
#include <queue>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace astro {
    enum class ExitMode
    {
        Lazy,
        Diligent
    };
    enum class RunMode {
        Dynamic,
        Static
    };
    class ThreadPool
    {
    public:
        explicit ThreadPool(int reserve_threads);
        ~ThreadPool();

        void add(const std::function<void()>& callback);

        //make threads num fixed and tasks blocked if it is true
        void setRunMode(RunMode mode) { run_mode_ = mode; }
        RunMode runMode() { return run_mode_; }

        //delete thread pool without process callbacks if it is lazy
        void setExitMode(ExitMode mode) { exit_mode_ = mode; }
        ExitMode exitMode() { return exit_mode_; }

    private:
        class DynamicThread
        {
        public:
            explicit DynamicThread(ThreadPool* pool);
            ~DynamicThread();

        private:
            ThreadPool* pool_;
            std::unique_ptr<std::thread> thd_;
            void threadFunc();
        };

        RunMode run_mode_;
        ExitMode exit_mode_;
        std::mutex mu_;
        std::condition_variable cv_;
        std::condition_variable shutdown_cv_;
        bool shutdown_;
        std::queue<std::function<void()>> callbacks_;
        int reserve_threads_;
        int nthreads_;
        int threads_waiting_;
        std::list<DynamicThread*> dead_threads_;

        void threadFunc();
        static void reapThreads(std::list<DynamicThread*>* tlist);
    };
   
}  // namespace astro

#endif 
