#pragma once

#include <array>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace terrain::core {

enum class JobClass : uint8_t { Critical = 0, Visible = 1, Background = 2, Count = 3 };

struct JobTag {
    uint64_t key = 0;
    uint64_t revision = 0;
    bool operator==(const JobTag &b) const { return key==b.key && revision==b.revision; }
};

// Persistent workers, bounded per-class queues, coalescing by key and
// cancellation by key/revision. Functions execute without scheduler locks.
class JobScheduler {
public:
    using Task = std::function<void()>;
    struct Metrics {
        std::array<size_t,size_t(JobClass::Count)> queued{};
        size_t running = 0;
        uint64_t submitted = 0, coalesced = 0, rejected = 0, cancelled = 0, completed = 0;
    };

    JobScheduler(size_t workers, std::array<size_t,size_t(JobClass::Count)> capacities);
    ~JobScheduler();
    JobScheduler(const JobScheduler &) = delete;
    JobScheduler &operator=(const JobScheduler &) = delete;

    bool submit(JobClass priority, JobTag tag, Task task);
    size_t cancel_key(uint64_t key);
    bool is_current(JobTag tag) const;
    void shutdown();
    Metrics metrics() const;

private:
    struct Entry { JobTag tag; Task task; uint64_t sequence=0; };
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::array<std::deque<Entry>,size_t(JobClass::Count)> queues_;
    std::array<size_t,size_t(JobClass::Count)> capacities_{};
    std::vector<std::thread> workers_;
    Metrics metrics_;
    std::unordered_map<uint64_t,uint64_t> latest_revision_;
    uint64_t next_sequence_ = 0;
    bool stopping_ = false;
    void worker_loop();
};

} // namespace terrain::core
