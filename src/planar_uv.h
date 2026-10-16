#pragma once

#include "voxel_chunk.h"
#include <cmath>

namespace terrain {

struct UV2 { double u=0, v=0; };

// Stable orthonormal triplanar face projection shared by the CPU surface
// payload and the Godot adapter. world_offset is the chunk's physical origin.
class PlanarUVProjection {
public:
    PlanarUVProjection(Vec3 normal,Vec3 world_offset,double period)
        : period_(period) {
        const double length=std::sqrt(dot(normal,normal));
        normal=length>1e-12?multiply(normal,1.0/length):Vec3{0,1,0};
        const Vec3 reference=std::abs(normal.y)<0.9?Vec3{0,1,0}:Vec3{1,0,0};
        tangent_=normalized(cross(reference,normal));
        bitangent_=cross(normal,tangent_);
        phase_u_=std::floor(dot(world_offset,tangent_)/period_);
        phase_v_=std::floor(dot(world_offset,bitangent_)/period_);
        offset_u_=dot(world_offset,tangent_)/period_-phase_u_;
        offset_v_=dot(world_offset,bitangent_)/period_-phase_v_;
    }

    UV2 project(Vec3 local_physical) const {
        return {offset_u_+dot(local_physical,tangent_)/period_,
                offset_v_+dot(local_physical,bitangent_)/period_};
    }

    static double dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
    static Vec3 cross(Vec3 a,Vec3 b) {
        return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
    }
    static Vec3 multiply(Vec3 a,double scale) { return {a.x*scale,a.y*scale,a.z*scale}; }
    static Vec3 normalized(Vec3 value) {
        const double length=std::sqrt(dot(value,value));
        return length>1e-12?multiply(value,1.0/length):Vec3{};
    }

private:
    Vec3 tangent_{},bitangent_{};
    double period_=1,phase_u_=0,phase_v_=0,offset_u_=0,offset_v_=0;
};

} // namespace terrain
