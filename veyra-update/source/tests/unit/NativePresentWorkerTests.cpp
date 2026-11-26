#include "veyra/engine/NativePresentWorker.h"
#include <cassert>
#include <future>
#include <iostream>
using namespace veyra::engine;
static int64_t now100ns(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()/100;}
int main(){
    NativePresentWorker worker;
    std::atomic<unsigned> calls{0};
    auto cancelled=worker.submit(now100ns()+2000000,[&]{++calls;},[]{return true;});
    assert(cancelled);cancelled->cancel();cancelled->wait();assert(calls==0);
    auto invalid=worker.submit(now100ns(),[&]{++calls;},[]{return false;});
    assert(invalid);invalid->wait();assert(calls==0);
    std::promise<void> entered,release;auto released=release.get_future().share();
    auto running=worker.submit(now100ns(),[&]{entered.set_value();released.wait();++calls;},[]{return true;});
    assert(entered.get_future().wait_for(std::chrono::seconds(2))==std::future_status::ready);
    assert(!worker.submit(now100ns(),[]{},[]{return true;})); // bounded, no accumulating frames
    release.set_value();running->wait();assert(calls==1);
    auto resource=std::make_shared<int>(7);std::weak_ptr<int> weak=resource;
    auto lifetime=worker.submit(now100ns(),[pin=resource]{assert(*pin==7);},[]{return true;});
    resource.reset();lifetime->wait();assert(weak.expired()); // captures released before done
    auto failure=worker.submit(now100ns(),[]{throw 42;},[]{return true;});failure->wait();
    for(unsigned i=0;i<100;++i){auto t=worker.submit(now100ns(),[&]{++calls;},[]{return true;});assert(t);t->wait();}
    assert(calls==101);
    const auto due=now100ns()+100000;std::atomic<int64_t> called{0};
    auto timed=worker.submit(due,[&]{called=now100ns();},[]{return true;});timed->wait();
    assert(called>=due); // no early presentation
    std::cout<<"PASS native presentation: deadline, cancellation, validity, capacity, reuse, lifetime and exception completion\n";
}
