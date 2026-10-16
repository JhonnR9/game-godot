#include "job_scheduler.h"
#include <algorithm>
#include <utility>

namespace terrain::core {

JobScheduler::JobScheduler(size_t workers,std::array<size_t,size_t(JobClass::Count)> capacities)
    : capacities_(capacities) {
    workers_.reserve(workers);
    for(size_t i=0;i<std::max<size_t>(1,workers);++i) workers_.emplace_back([this]{worker_loop();});
}

JobScheduler::~JobScheduler() { shutdown(); }

bool JobScheduler::submit(JobClass priority,JobTag tag,Task task) {
    if(!task) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if(stopping_) { ++metrics_.rejected; return false; }
    const size_t cls=size_t(priority);
    if(cls>=queues_.size()) { ++metrics_.rejected; return false; }
    const auto current=latest_revision_.find(tag.key);
    if(current!=latest_revision_.end() && tag.revision<current->second) {
        ++metrics_.rejected;
        return false;
    }
    auto &target_queue=queues_[cls];
    const bool already_queued=std::any_of(target_queue.begin(),target_queue.end(),[&](const Entry &entry){return entry.tag.key==tag.key;});
    if(!already_queued && target_queue.size()>=capacities_[cls]) {
        ++metrics_.rejected;
        return false;
    }
    bool replaced=false;
    if(current==latest_revision_.end() || tag.revision>current->second) {
        latest_revision_[tag.key]=tag.revision;
        for(size_t q=0;q<queues_.size();++q) {
            auto &pending=queues_[q];
            const auto old=pending.size();
            pending.erase(std::remove_if(pending.begin(),pending.end(),[&](const Entry &entry){return entry.tag.key==tag.key;}),pending.end());
            replaced|=pending.size()!=old;
            metrics_.queued[q]=pending.size();
        }
    }
    auto &queue=queues_[cls];
    const auto existing=std::find_if(queue.begin(),queue.end(),[&](const Entry &entry){return entry.tag.key==tag.key;});
    if(existing!=queue.end()) {
        *existing={tag,std::move(task),next_sequence_++};
        ++metrics_.coalesced;
        wake_.notify_one();
        return true;
    }
    queue.push_back({tag,std::move(task),next_sequence_++});
    if(replaced) ++metrics_.coalesced;
    else ++metrics_.submitted;
    metrics_.queued[cls]=queue.size();
    wake_.notify_one();
    return true;
}

size_t JobScheduler::cancel_key(uint64_t key) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t removed=0;
    for(size_t cls=0;cls<queues_.size();++cls) {
        auto &queue=queues_[cls];
        const auto old=queue.size();
        queue.erase(std::remove_if(queue.begin(),queue.end(),[&](const Entry &entry){return entry.tag.key==key;}),queue.end());
        removed+=old-queue.size();
        metrics_.queued[cls]=queue.size();
    }
    metrics_.cancelled+=removed;
    latest_revision_.erase(key);
    return removed;
}

bool JobScheduler::is_current(JobTag tag) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it=latest_revision_.find(tag.key);
    return !stopping_ && it!=latest_revision_.end() && it->second==tag.revision;
}

void JobScheduler::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(stopping_ && workers_.empty()) return;
        stopping_=true;
    }
    wake_.notify_all();
    for(auto &worker:workers_) if(worker.joinable()) worker.join();
    workers_.clear();
}

JobScheduler::Metrics JobScheduler::metrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return metrics_;
}

void JobScheduler::worker_loop() {
    for(;;) {
        Entry entry;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock,[&]{
                return stopping_ || std::any_of(queues_.begin(),queues_.end(),[](const auto &queue){return !queue.empty();});
            });
            size_t cls=0;
            while(cls<queues_.size() && queues_[cls].empty()) ++cls;
            if(cls==queues_.size()) { if(stopping_) return; else continue; }
            entry=std::move(queues_[cls].front());
            queues_[cls].pop_front();
            metrics_.queued[cls]=queues_[cls].size();
            ++metrics_.running;
        }
        try { entry.task(); } catch(...) { /* worker survives task failures */ }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            --metrics_.running;
            ++metrics_.completed;
            const auto latest=latest_revision_.find(entry.tag.key);
            if(latest!=latest_revision_.end() && latest->second==entry.tag.revision)
                latest_revision_.erase(latest);
        }
    }
}

} // namespace terrain::core
