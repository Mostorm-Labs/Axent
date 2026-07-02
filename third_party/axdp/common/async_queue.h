//
// Created by Staney on 25/7/2022.
//

#ifndef AUX_ASYNC_QUEUE_H
#define AUX_ASYNC_QUEUE_H

#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include "log_utils.h"
#include "string_buffer.h"

namespace axdp {

    template<class T>
    class AsyncQueue {
    public:
        AsyncQueue() {
            LOGV("AsyncQueue init\n");
            worker_thread_ = new std::thread(&AsyncQueue::run, this);
        }

        virtual ~AsyncQueue() {
            LOGV("AsyncQueue de-init\n");
            state_ = THREAD_STATE::STATE_STOP;
            cond_var_.notify_all();
            if (worker_thread_->joinable()) {
                LOGV("thread wait...\n");
                worker_thread_->join();
            }
            LOGV("thread done\n");
            delete worker_thread_;
        }

        virtual void onData(std::shared_ptr<T> t) = 0;

        size_t push(std::shared_ptr<T> t, bool async = true) {
            if (async) {
                std::unique_lock<std::mutex> lock(mutex_data_);
                queue_.emplace(t);
                lock.unlock();
                cond_var_.notify_one();
            } else {
                onData(t);
            }
            return queue_.size();
        }

        void clear() {
            std::lock_guard<std::mutex> lock(mutex_data_);
            while (!queue_.empty()) {
                queue_.pop();
            }
        }

    protected:
        std::shared_ptr<T> pop() {
            std::unique_lock<std::mutex> lock(mutex_data_);
            std::shared_ptr<T> t = queue_.front();
            queue_.pop();
            lock.unlock();
            //return std::move(t);
            return t;
        }

    private:
        void run() {
            state_ = THREAD_STATE::STATE_RUNNING;
            std::unique_lock<std::mutex> lock(mutex_);

            while (state_ != THREAD_STATE::STATE_STOP) {
                queue_.empty() ? cond_var_.wait(lock) : onData(pop());
            }
        }

        enum class THREAD_STATE : uint8_t {
            STATE_IDLE,
            STATE_RUNNING,
            STATE_STOP
        };

        std::queue<std::shared_ptr<T>> queue_;
        std::thread *worker_thread_{nullptr};
        std::mutex mutex_;
        std::mutex mutex_data_;
        std::condition_variable cond_var_;
        THREAD_STATE state_{THREAD_STATE::STATE_IDLE};
    };

    template<class T>
    class BaseQueue : public AsyncQueue<T> {
    public:
        BaseQueue() {
            LOGV("BaseQueue init\n");
        }

        ~BaseQueue() override {
            LOGV("BaseQueue de-init\n");
        }

        void onData(std::shared_ptr<T> t) override {

        }
    };

    class RawBytes {
    public:
        RawBytes(const uint8_t *buffer, uint32_t len) : buffer_(nullptr), len_(0) {
            if (buffer != nullptr && len > 0) {
                buffer_ = new uint8_t[len];
                memcpy(buffer_, buffer, len);
                len_ = len;
            }
        }

        ~RawBytes() {
            if (buffer_) { delete[]buffer_; }
            len_ = 0;
        }

        uint8_t *buf() { return buffer_; }

        uint16_t len() const { return len_; }

    private:
        uint8_t *buffer_;
        uint16_t len_;
    };

    class TaskQueue : public BaseQueue<RawBytes> {
    public:
        TaskQueue() {
            LOGV("TaskQueue init\n");
        }

        ~TaskQueue() override {
            LOGV("TaskQueue de-init\n");
        }

        void onData(std::shared_ptr<RawBytes> t) override {
            //LOGV("on data get...[%d]%s\n", t.get()->len_, aux::HEX::dump(t.get()->buffer_, t.get()->len_).get());
        }

        void push(uint8_t *buffer, uint32_t len, uint32_t offset = 0) {
            //        auto *data = new uint8_t[len];
            //        memcpy(data, buffer + offset, len);
            //        std::shared_ptr<uint8_t> hex(data, std::default_delete<uint8_t[]>());
            //        AsyncQueue::push(std::move(hex));

            //  AsyncQueue::push(std::move(std::make_shared<RawBytes>(buffer + offset, len)));
            AsyncQueue::push(std::make_shared<RawBytes>(buffer + offset, len));
        }
    };

}


#endif //AUX_ASYNC_QUEUE_H
