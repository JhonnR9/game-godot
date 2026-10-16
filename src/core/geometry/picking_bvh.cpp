#include "picking_bvh.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace terrain::core {
namespace {
Vec3 add(Vec3 a,Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 sub(Vec3 a,Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 mul(Vec3 a,double s) { return {a.x*s,a.y*s,a.z*s}; }
double dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec3 cross(Vec3 a,Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
ClipBounds point_bounds(Vec3 a,Vec3 b,Vec3 c) {
    return {{std::min({a.x,b.x,c.x}),std::min({a.y,b.y,c.y}),std::min({a.z,b.z,c.z})},
            {std::max({a.x,b.x,c.x}),std::max({a.y,b.y,c.y}),std::max({a.z,b.z,c.z})}};
}
ClipBounds combine(ClipBounds a,ClipBounds b) {
    return {{std::min(a.low.x,b.low.x),std::min(a.low.y,b.low.y),std::min(a.low.z,b.low.z)},
            {std::max(a.high.x,b.high.x),std::max(a.high.y,b.high.y),std::max(a.high.z,b.high.z)}};
}

uint32_t build_node(PickingBvh &bvh,uint32_t first,uint32_t count,uint32_t leaf_capacity) {
    const uint32_t node_index=static_cast<uint32_t>(bvh.nodes.size());
    ClipBounds bounds=bvh.triangles[bvh.order[first]].bounds;
    for(uint32_t i=1;i<count;++i) bounds=combine(bounds,bvh.triangles[bvh.order[first+i]].bounds);
    bvh.nodes.push_back({bounds,first,count,0,0,false});
    if(count<=leaf_capacity) { bvh.nodes[node_index].leaf=true; return node_index; }

    const Vec3 extent=sub(bounds.high,bounds.low);
    int axis=extent.y>extent.x ? 1:0;
    if((axis==0 ? extent.x:extent.y)<extent.z) axis=2;
    auto coordinate=[axis](Vec3 p) { return axis==0?p.x:(axis==1?p.y:p.z); };
    std::sort(bvh.order.begin()+first,bvh.order.begin()+first+count,[&](uint32_t a,uint32_t b) {
        const double ca=coordinate(bvh.triangles[a].centroid),cb=coordinate(bvh.triangles[b].centroid);
        return ca==cb ? a<b : ca<cb;
    });
    const uint32_t left_count=count/2;
    const uint32_t left=build_node(bvh,first,left_count,leaf_capacity);
    const uint32_t right=build_node(bvh,first+left_count,count-left_count,leaf_capacity);
    bvh.nodes[node_index].left=left;
    bvh.nodes[node_index].right=right;
    bvh.nodes[node_index].leaf=false;
    return node_index;
}

bool ray_box(Vec3 origin,Vec3 direction,const ClipBounds &box,double limit) {
    double low=0,high=limit;
    const double o[3]={origin.x,origin.y,origin.z};
    const double d[3]={direction.x,direction.y,direction.z};
    const double mn[3]={box.low.x,box.low.y,box.low.z};
    const double mx[3]={box.high.x,box.high.y,box.high.z};
    for(int axis=0;axis<3;++axis) {
        if(std::abs(d[axis])<1e-15) { if(o[axis]<mn[axis] || o[axis]>mx[axis]) return false; continue; }
        double a=(mn[axis]-o[axis])/d[axis],b=(mx[axis]-o[axis])/d[axis];
        if(a>b) std::swap(a,b);
        low=std::max(low,a); high=std::min(high,b);
        if(low>high) return false;
    }
    return true;
}

bool ray_triangle(Vec3 origin,Vec3 direction,Vec3 a,Vec3 b,Vec3 c,double &distance) {
    constexpr double eps=1e-10;
    const Vec3 e1=sub(b,a),e2=sub(c,a),p=cross(direction,e2);
    const double det=dot(e1,p);
    if(std::abs(det)<eps) return false;
    const double inverse=1.0/det;
    const Vec3 tvec=sub(origin,a);
    const double u=dot(tvec,p)*inverse;
    if(u < -eps || u > 1+eps) return false;
    const Vec3 q=cross(tvec,e1);
    const double v=dot(direction,q)*inverse;
    if(v < -eps || u+v > 1+eps) return false;
    const double t=dot(e2,q)*inverse;
    if(t<0) return false;
    distance=t;
    return true;
}
}

PickingBvh PickingBvh::build(const SurfaceMeshPayload &mesh,uint32_t leaf_capacity) {
    PickingBvh result;
    if(leaf_capacity==0) leaf_capacity=1;
    for(uint32_t surface_index=0;surface_index<mesh.surfaces.size();++surface_index) {
        const auto &surface=mesh.surfaces[surface_index];
        for(uint32_t offset=0;offset+2<surface.indices.size();offset+=3) {
            if(offset/3>=surface.triangle_primitives.size()) continue;
            const uint32_t ia=surface.indices[offset],ib=surface.indices[offset+1],ic=surface.indices[offset+2];
            if(ia>=surface.positions.size() || ib>=surface.positions.size() || ic>=surface.positions.size()) continue;
            const Vec3 a=surface.positions[ia],b=surface.positions[ib],c=surface.positions[ic];
            const auto bounds=point_bounds(a,b,c);
            result.triangles.push_back({surface_index,offset,surface.triangle_primitives[offset/3],surface.material,
                bounds,mul(add(add(a,b),c),1.0/3.0)});
        }
    }
    result.order.resize(result.triangles.size());
    for(uint32_t i=0;i<result.order.size();++i) result.order[i]=i;
    if(!result.order.empty()) build_node(result,0,static_cast<uint32_t>(result.order.size()),leaf_capacity);
    return result;
}

RayHit raycast(const SurfaceMeshPayload &mesh,const PickingBvh &bvh,Vec3 origin,Vec3 direction,double max_distance) {
    RayHit hit;
    hit.distance=max_distance;
    if(bvh.nodes.empty() || !(max_distance>=0)) return hit;
    std::vector<uint32_t> stack{0};
    while(!stack.empty()) {
        const uint32_t node_index=stack.back(); stack.pop_back();
        const auto &node=bvh.nodes[node_index];
        if(!ray_box(origin,direction,node.bounds,hit.distance)) continue;
        if(!node.leaf) { stack.push_back(node.left); stack.push_back(node.right); continue; }
        for(uint32_t i=0;i<node.count;++i) {
            const auto &triangle=bvh.triangles[bvh.order[node.first+i]];
            const auto &surface=mesh.surfaces[triangle.surface];
            const size_t offset=triangle.index_offset;
            const Vec3 a=surface.positions[surface.indices[offset]];
            const Vec3 b=surface.positions[surface.indices[offset+1]];
            const Vec3 c=surface.positions[surface.indices[offset+2]];
            double distance;
            if(ray_triangle(origin,direction,a,b,c,distance) && distance<=hit.distance) {
                hit.hit=true;
                hit.distance=distance;
                hit.position=add(origin,mul(direction,distance));
                hit.primitive=triangle.primitive;
                hit.material=triangle.material;
            }
        }
    }
    return hit;
}

} // namespace terrain::core
