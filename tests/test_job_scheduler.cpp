#include "core/runtime/job_scheduler.h"
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <vector>

using namespace terrain::core;

int main() {
    JobScheduler scheduler(1,{1,2,1});
    std::mutex mutex;
    std::condition_variable cv;
    bool started=false,release=false;
    std::vector<int> order;
    assert(scheduler.submit(JobClass::Critical,{1,1},[&]{
        std::unique_lock<std::mutex> lock(mutex);
        started=true; cv.notify_all(); cv.wait(lock,[&]{return release;});
        order.push_back(0);
    }));
    {
        std::unique_lock<std::mutex> lock(mutex);
        assert(cv.wait_for(lock,std::chrono::seconds(2),[&]{return started;}));
    }
    assert(scheduler.submit(JobClass::Background,{2,1},[&]{order.push_back(2);}));
    assert(scheduler.submit(JobClass::Background,{2,2},[&]{order.push_back(3);})); // coalesced revision
    assert(!scheduler.is_current({2,1})); // replaced by the newer requested revision
    assert(scheduler.is_current({2,2}));
    assert(!scheduler.submit(JobClass::Critical,{2,1},[]{ })); // stale revision cannot re-enter
    assert(!scheduler.submit(JobClass::Background,{3,1},[&]{order.push_back(9);})); // full bounded queue
    assert(scheduler.submit(JobClass::Critical,{4,1},[&]{order.push_back(1);}));
    assert(scheduler.submit(JobClass::Visible,{5,1},[&]{order.push_back(4);}));
    assert(scheduler.cancel_key(5)==1);
    assert(!scheduler.is_current({5,1}));
    const auto before=scheduler.metrics();
    assert(before.queued[0]==1 && before.queued[1]==0 && before.queued[2]==1);
    assert(before.coalesced==1 && before.rejected==2 && before.cancelled==1);
    {
        std::lock_guard<std::mutex> lock(mutex); release=true;
    }
    cv.notify_all();
    scheduler.shutdown();
    assert((order==std::vector<int>{0,1,3}));
    const auto after=scheduler.metrics();
    assert(after.running==0 && after.queued[0]+after.queued[1]+after.queued[2]==0 && after.completed==3);
    assert(!scheduler.submit(JobClass::Critical,{6,1},[]{ }));
    JobScheduler parallel(4,{64,64,64});
    std::vector<int> results(100,-1);
    for(int i=0;i<100;++i) {
        assert(parallel.submit(JobClass(i%3),{uint64_t(i+100),1},[&,i]{results[size_t(i)]=i*i+17;}));
    }
    parallel.shutdown();
    for(int i=0;i<100;++i) assert(results[size_t(i)]==i*i+17);
    std::cout << "PASS: bounded scheduler coalesces revisions, prioritizes, cancels stale queued work, and joins cleanly\n";
}
