#include "surface_job_queue.h"
#include <limits>

namespace terrain::core {

SurfaceJobQueue::SurfaceJobQueue(size_t workers,size_t output_credits,size_t max_bytes_per_output,
                                 std::array<size_t,size_t(JobClass::Count)> queue_capacities)
    : scheduler_(workers,queue_capacities),output_capacity_(output_credits),byte_capacity_(
          max_bytes_per_output && output_credits>std::numeric_limits<size_t>::max()/max_bytes_per_output
              ? std::numeric_limits<size_t>::max() : output_credits*max_bytes_per_output),
      max_bytes_per_output_(max_bytes_per_output) {}

SurfaceJobQueue::~SurfaceJobQueue() { shutdown(); }

bool SurfaceJobQueue::submit(ChunkKey key,uint64_t revision,JobClass priority,NeighborhoodSnapshot snapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(stopping_) return false;
    auto it=reservations_.find(key);
    const bool fresh=it==reservations_.end();
    if(fresh && reservations_.size()>=output_capacity_) return false;
    if(fresh && max_bytes_per_output_>byte_capacity_-reserved_bytes_) return false;
    if(!fresh && revision<=it->second.revision) return false;
    uint64_t id=fresh?next_id_++:it->second.id;
    if(id==0) id=next_id_++;
    const size_t per_credit=max_bytes_per_output_;
    JobTag tag{id,revision};
    const bool accepted=scheduler_.submit(priority,tag,[this,key,id,revision,snapshot=std::move(snapshot)]() mutable {
        build_and_publish(key,id,revision,std::move(snapshot));
    });
    if(!accepted) return false;
    if(fresh) {
        reservations_.emplace(key,Reservation{id,revision,per_credit});
        reserved_bytes_+=per_credit;
    } else {
        completed_.erase(key);
        it->second.revision=revision;
    }
    return true;
}

void SurfaceJobQueue::cancel(ChunkKey key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it=reservations_.find(key);
    if(it==reservations_.end()) return;
    scheduler_.cancel_key(it->second.id);
    reserved_bytes_-=it->second.bytes;
    reservations_.erase(it);
    completed_.erase(key);
}

std::optional<CompletedSurface> SurfaceJobQueue::take(ChunkKey key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it=completed_.find(key);
    if(it==completed_.end()) return std::nullopt;
    CompletedSurface result=std::move(it->second);
    completed_.erase(it);
    const auto reservation=reservations_.find(key);
    if(reservation!=reservations_.end()) {
        reserved_bytes_-=reservation->second.bytes;
        reservations_.erase(reservation);
    }
    return result;
}

std::optional<CompletedSurface> SurfaceJobQueue::wait_take(ChunkKey key,std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    ready_.wait_for(lock,timeout,[&]{return stopping_ || completed_.find(key)!=completed_.end();});
    const auto it=completed_.find(key);
    if(it==completed_.end()) return std::nullopt;
    CompletedSurface result=std::move(it->second);
    completed_.erase(it);
    const auto reservation=reservations_.find(key);
    if(reservation!=reservations_.end()) {
        reserved_bytes_-=reservation->second.bytes;
        reservations_.erase(reservation);
    }
    return result;
}

SurfaceJobQueue::Metrics SurfaceJobQueue::metrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {reservations_.size(),completed_.size(),reserved_bytes_,oversized_results_,failed_results_,scheduler_.metrics()};
}

void SurfaceJobQueue::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(stopping_) return;
        stopping_=true;
    }
    scheduler_.shutdown();
    ready_.notify_all();
}

void SurfaceJobQueue::build_and_publish(ChunkKey key,uint64_t id,uint64_t revision,
                                        NeighborhoodSnapshot snapshot) {
    const JobTag tag{id,revision};
    if(!scheduler_.is_current(tag)) return;
    SurfaceMeshPayload payload;
    try {
        payload=SurfaceBuildService::build_mesh_payload(snapshot,key);
    } catch(...) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto reservation=reservations_.find(key);
        if(reservation!=reservations_.end() && reservation->second.id==id && reservation->second.revision==revision) {
            reserved_bytes_-=reservation->second.bytes;
            reservations_.erase(reservation);
            ++failed_results_;
        }
        ready_.notify_all();
        return;
    }
    if(!scheduler_.is_current(tag)) return;
    const size_t bytes=payload_bytes(payload);
    std::lock_guard<std::mutex> lock(mutex_);
    const auto reservation=reservations_.find(key);
    if(reservation==reservations_.end() || reservation->second.id!=id || reservation->second.revision!=revision) return;
    if(bytes>reservation->second.bytes) {
        ++oversized_results_;
        reserved_bytes_-=reservation->second.bytes;
        reservations_.erase(reservation);
        return;
    }
    completed_[key]=CompletedSurface{key,revision,std::move(payload)};
    ready_.notify_all();
}

size_t SurfaceJobQueue::payload_bytes(const SurfaceMeshPayload &payload) {
    size_t bytes=payload.missing_chunks.size()*sizeof(ChunkKey);
    for(const auto &surface:payload.surfaces) {
        bytes+=surface.positions.size()*sizeof(Vec3)+surface.normals.size()*sizeof(Vec3)+
               surface.uvs.size()*sizeof(Vec3)+surface.indices.size()*sizeof(uint32_t)+
               surface.triangle_primitives.size()*sizeof(PrimitiveId);
    }
    return bytes;
}

} // namespace terrain::core
