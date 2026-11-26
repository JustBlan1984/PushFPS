#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace veyra::engine {
// One leased native frame at a time. The worker never owns the enhancement
// graph or its command ring. Cancellation joins the callback before the owner
// may release a lease, resize, or replace a graph.
class NativePresentWorker {
public:
    struct Ticket {
        std::atomic<bool> cancelled{false},done{false},executed{false};
        std::mutex mutex;
        std::condition_variable cv;
        void cancel(){cancelled.store(true);cv.notify_all();}
        void wait(){std::unique_lock lock(mutex);cv.wait(lock,[&]{return done.load(std::memory_order_acquire);});}
    };
    using Handle=std::shared_ptr<Ticket>;
    using Waiter=std::function<void(int64_t,const Handle&)>;
    ~NativePresentWorker(){cancelAndWait();{std::lock_guard lock(mutex_);stopping_=true;}cv_.notify_all();if(thread_.joinable())thread_.join();}
    Handle submit(int64_t deadline100ns,std::function<void()> call,std::function<bool()> valid,Handle ticket={},Waiter waiter={}){
        std::lock_guard lock(mutex_);
        if(stopping_||(active_&&!active_->done.load()))return {};
        if(!ticket)ticket=std::make_shared<Ticket>();
        if(!thread_.joinable())thread_=std::thread([this]{run();});
        active_=ticket;job_={ticket,deadline100ns,std::move(call),std::move(valid),std::move(waiter)};
        cv_.notify_one();return ticket;
    }
    void cancelAndWait(){
        Handle ticket;{std::lock_guard lock(mutex_);ticket=active_;}
        if(ticket){ticket->cancel();ticket->wait();}
    }
private:
    struct Job {Handle ticket;int64_t deadline=0;std::function<void()> call;std::function<bool()> valid;Waiter waiter;};
    std::mutex mutex_;std::condition_variable cv_;std::thread thread_;
    Handle active_;Job job_;bool stopping_=false;
    void run(){
        for(;;){
            Job job;
            {std::unique_lock lock(mutex_);cv_.wait(lock,[&]{return stopping_||bool(job_.ticket);});
             if(stopping_)return;job=std::move(job_);job_={};}
            try{
            if(job.waiter){job.waiter(job.deadline,job.ticket);}else{
            const auto due=std::chrono::steady_clock::time_point(std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::nanoseconds(job.deadline*100)));
            // Only a GPU-ready frame is queued. Bound precise waiting to the
            // last 0.3 ms; never spin for GPU completion or display capacity.
            const auto coarse=due-std::chrono::microseconds(300);
            {std::unique_lock lock(job.ticket->mutex);
             job.ticket->cv.wait_until(lock,coarse,[&]{return job.ticket->cancelled.load();});}
            while(!job.ticket->cancelled.load()&&std::chrono::steady_clock::now()<due)std::this_thread::yield();
            }
            if(!job.ticket->cancelled.load()&&(!job.valid||job.valid())){
                job.ticket->executed.store(true);
                job.call();
            }
            }catch(...){/* caller's result remains unsuccessful */}
            // Do not retain resource captures after publishing completion.
            job.call={};job.valid={};job.waiter={};
            {std::lock_guard lock(job.ticket->mutex);job.ticket->done.store(true,std::memory_order_release);}
            job.ticket->cv.notify_all();
        }
    }
};
}
