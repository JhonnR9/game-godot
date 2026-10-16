#pragma once

#include "job_scheduler.h"
#include "../geometry/surface_build_service.h"
#include <chrono>
#include <condition_variable>
#include <optional>
#include <unordered_map>

namespace terrain::core {

struct CompletedSurface {
    ChunkKey key;
    uint64_t revision = 0;
    SurfaceMeshPayload payload;
};

// Bridges immutable world snapshots to worker-built geometry. Each accepted
// request reserves one bounded output credit until its result is consumed.
class SurfaceJobQueue {
public:
    struct Metrics {
        size_t reserved_outputs = 0;
        size_t ready_outputs = 0;
        size_t reserved_bytes = 0;
        uint64_t oversized_results = 0, failed_results = 0;
        JobScheduler::Metrics jobs;
    };

    SurfaceJobQueue(size_t workers, size_t output_credits, size_t max_bytes_per_output,
                    std::array<size_t,size_t(JobClass::Count)> queue_capacities);
    ~SurfaceJobQueue();
    SurfaceJobQueue(const SurfaceJobQueue &) = delete;
    SurfaceJobQueue &operator=(const SurfaceJobQueue &) = delete;

    bool submit(ChunkKey key, uint64_t revision, JobClass priority, NeighborhoodSnapshot snapshot);
    void cancel(ChunkKey key);
    std::optional<CompletedSurface> take(ChunkKey key);
    std::optional<CompletedSurface> wait_take(ChunkKey key, std::chrono::milliseconds timeout);
    Metrics metrics() const;
    void shutdown();

private:
    struct Reservation { uint64_t id=0, revision=0; size_t bytes=0; };
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    JobScheduler scheduler_;
    size_t output_capacity_;
    size_t byte_capacity_;
    size_t max_bytes_per_output_;
    size_t reserved_bytes_=0;
    uint64_t next_id_=1;
    uint64_t oversized_results_=0;
    uint64_t failed_results_=0;
    bool stopping_=false;
    std::unordered_map<ChunkKey,Reservation,ChunkKeyHash> reservations_;
    std::unordered_map<ChunkKey,CompletedSurface,ChunkKeyHash> completed_;

    void build_and_publish(ChunkKey key, uint64_t id, uint64_t revision,
                           NeighborhoodSnapshot snapshot);
    static size_t payload_bytes(const SurfaceMeshPayload &payload);
};

} // namespace terrain::core
