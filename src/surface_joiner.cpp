#include "surface_joiner.h"
#include "bridge_topology.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <tuple>

namespace terrain {
namespace {
constexpr double EPS=1e-8;
Vec3 add(Vec3 a,Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 sub(Vec3 a,Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 mul(Vec3 a,double t) { return {a.x*t,a.y*t,a.z*t}; }
double dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec3 cross(Vec3 a,Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
Vec3 unit(Vec3 v) { double l=std::sqrt(dot(v,v)); return l>EPS?mul(v,1/l):Vec3{}; }
struct Plane { Vec3 n; double d; };
struct Bounds { Vec3 low,high; };
using Polygon=std::vector<Vec3>;
struct Solid { std::vector<Plane> planes; Bounds box; int id; };
struct Face { Polygon vertices; Vec3 normal; Color color; Voxel material=Voxel::Empty; int owner=0; };
Bounds bounds(const Polygon &poly) {
    Bounds b{poly[0],poly[0]};
    for(auto p:poly) {
        b.low={std::min(b.low.x,p.x),std::min(b.low.y,p.y),std::min(b.low.z,p.z)};
        b.high={std::max(b.high.x,p.x),std::max(b.high.y,p.y),std::max(b.high.z,p.z)};
    }
    return b;
}
bool touches(Bounds a,Bounds b) {
    return a.low.x<=b.high.x+EPS && a.high.x>=b.low.x-EPS &&
        a.low.y<=b.high.y+EPS && a.high.y>=b.low.y-EPS &&
        a.low.z<=b.high.z+EPS && a.high.z>=b.low.z-EPS;
}
bool valid(const Polygon &poly) {
    if(poly.size()<3) return false;
    Vec3 area{};
    for(size_t i=1;i+1<poly.size();++i) area=add(area,cross(sub(poly[i],poly[0]),sub(poly[i+1],poly[0])));
    return dot(area,area)>EPS*EPS;
}
Polygon clip(const Polygon &poly,Plane plane,bool inside) {
    Polygon out;
    if(poly.empty()) return out;
    for(size_t i=0;i<poly.size();++i) {
        Vec3 a=poly[i],b=poly[(i+1)%poly.size()];
        double da=dot(plane.n,a)-plane.d,db=dot(plane.n,b)-plane.d;
        bool keep_a=inside?da<=EPS:da>=-EPS;
        bool keep_b=inside?db<=EPS:db>=-EPS;
        if(keep_a) out.push_back(a);
        if(keep_a!=keep_b && std::abs(da-db)>EPS) out.push_back(add(a,mul(sub(b,a),da/(da-db))));
    }
    Polygon clean;
    for(auto p:out) {
        if(clean.empty() || dot(sub(clean.back(),p),sub(clean.back(),p))>EPS*EPS) clean.push_back(p);
    }
    if(clean.size()>1 && dot(sub(clean.front(),clean.back()),sub(clean.front(),clean.back()))<EPS*EPS) clean.pop_back();
    return clean;
}
std::vector<Polygon> subtract(const Polygon &poly,Vec3 normal,int owner,const Solid &cutter) {
    if(!touches(bounds(poly),cutter.box)) return {poly};
    // Keep one exterior copy of co-facing coplanar overlap. Opposing faces
    // are internal to the union and must be removed from both solids.
    for(auto plane:cutter.planes) {
        bool coplanar=true;
        for(auto p:poly) coplanar&=std::abs(dot(plane.n,p)-plane.d)<EPS;
        if(coplanar && dot(normal,plane.n)>EPS && owner<cutter.id) return {poly};
    }
    Polygon remainder=poly;
    std::vector<Polygon> fragments;
    for(auto plane:cutter.planes) {
        bool all_inside=true,all_outside=true;
        for(auto p:remainder) {
            double d=dot(plane.n,p)-plane.d;
            all_inside&=d<=EPS; all_outside&=d>=-EPS;
        }
        if(all_inside) continue;
        if(all_outside) { if(valid(remainder)) fragments.push_back(remainder); return fragments; }
        Polygon outside=clip(remainder,plane,false);
        if(valid(outside)) fragments.push_back(std::move(outside));
        remainder=clip(remainder,plane,true);
        if(!valid(remainder)) return fragments;
    }
    return fragments;
}
void subtract_solid(std::vector<Polygon> &pieces,Vec3 normal,int owner,const Solid &cutter) {
    std::vector<Polygon> next;
    for(const auto &poly:pieces) {
        auto parts=subtract(poly,normal,owner,cutter);
        for(auto &part:parts) next.push_back(std::move(part));
    }
    pieces.swap(next);
}
Plane shifted(Plane p,Vec3 center) { p.d+=dot(p.n,center); return p; }
Solid octahedron(Vec3 center) {
    Solid solid{{},{{center.x-2,center.y-2,center.z-2},{center.x+2,center.y+2,center.z+2}},0};
    for(int sign:{-1,1}) {
        solid.planes.push_back(shifted({{double(sign),0,0},2},center));
        solid.planes.push_back(shifted({{0,double(sign),0},2},center));
        solid.planes.push_back(shifted({{0,0,double(sign)},2},center));
    }
    for(int x:{-1,1}) for(int y:{-1,1}) for(int z:{-1,1})
        solid.planes.push_back(shifted({{double(x),double(y),double(z)},3},center));
    return solid;
}
Color tint(Voxel voxel) {
    if(voxel==Voxel::Grass) return {0.23f,0.48f,0.105f,1};
    if(voxel==Voxel::Dirt) return {0.34f,0.20f,0.10f,1};
    return {0.30f,0.33f,0.37f,1};
}
Vec3 center(int x,int y,int z) {
    int p=OctahedralLattice::parity(y);
    return {double(4*x+2*p-31),double(2*y+2),double(4*z+2*p-31)};
}
Int3 cell_at_center(Vec3 p) {
    int64_t y=std::llround((p.y-2)/2);
    int parity=OctahedralLattice::parity(y);
    return {std::llround((p.x+31-2*parity)/4),y,std::llround((p.z+31-2*parity)/4)};
}
void add_bridge(Vec3 origin,const BridgeFrame &frame,int id,Color color,Voxel material,std::vector<Solid> &bridges,std::vector<Face> &faces) {
    // The same eight inner vertices are rotated/reflected to any of the six
    // axis-aligned surface directions, without moving original cell vertices.
    Polygon transformed;
    for(auto p:BRIDGE_VERTICES) transformed.push_back(add(origin,frame.rotate(p)));
    Solid bridge{{},bounds(transformed),id};
    for(const auto &definition:BRIDGE_PLANES) {
        Plane plane{definition.normal,definition.distance};
        Polygon polygon;
        Vec3 centroid{};
        for(auto p:BRIDGE_VERTICES) if(std::abs(dot(plane.n,p)-plane.d)<EPS) { polygon.push_back(p); centroid=add(centroid,p); }
        centroid=mul(centroid,1.0/polygon.size());
        Vec3 n=unit(plane.n);
        Vec3 u=unit(sub(polygon[0],centroid)),v=cross(n,u);
        std::sort(polygon.begin(),polygon.end(),[&](Vec3 a,Vec3 b) {
            a=sub(a,centroid);b=sub(b,centroid);
            return std::atan2(dot(a,v),dot(a,u))>std::atan2(dot(b,v),dot(b,u));
        });
        for(auto &p:polygon) p=add(origin,frame.rotate(p));
        n=frame.rotate(n);
        // Reflections change winding; keep Godot front faces clockwise.
        if(dot(cross(sub(polygon[2],polygon[0]),sub(polygon[1],polygon[0])),n)<0)
            std::reverse(polygon.begin(),polygon.end());
        faces.push_back({std::move(polygon),n,color,material,id});
        bridge.planes.push_back(shifted({frame.rotate(plane.n),plane.d},origin));
    }
    bridges.push_back(std::move(bridge));
}
// Canonical coordinates and edge subdivision ensure adjacent clipped polygons
// use identical vertices, including intersections on an existing long edge.
Vec3 canonical(Vec3 p) { return {std::round(p.x*1e8)/1e8,std::round(p.y*1e8)/1e8,std::round(p.z*1e8)/1e8}; }
using Key=std::tuple<int64_t,int64_t,int64_t>;
Key key(Vec3 p) { return {std::llround(p.x*1e8),std::llround(p.y*1e8),std::llround(p.z*1e8)}; }
Key bucket(Vec3 p) { return {int64_t(std::floor(p.x)),int64_t(std::floor(p.y)),int64_t(std::floor(p.z))}; }
MeshBuffers triangulate(std::vector<Face> &faces) {
    std::map<Key,Vec3> unique;
    for(auto &face:faces) for(auto &p:face.vertices) { p=canonical(p); unique.emplace(key(p),p); }
    std::map<Key,std::vector<Vec3>> buckets;
    for(const auto &item:unique) buckets[bucket(item.second)].push_back(item.second);
    MeshBuffers out;
    const double scale=OctahedronGeometry::EDGE_LENGTH/std::sqrt(2.0);
    for(const auto &face:faces) {
        Polygon boundary;
        for(size_t i=0;i<face.vertices.size();++i) {
            Vec3 a=face.vertices[i],b=face.vertices[(i+1)%face.vertices.size()],direction=sub(b,a);
            double length2=dot(direction,direction);
            if(length2<EPS*EPS) continue;
            std::vector<std::pair<double,Vec3>> points{{0,a}};
            Bounds box=bounds({a,b});
            for(int64_t x=int64_t(std::floor(box.low.x));x<=int64_t(std::floor(box.high.x));++x)
            for(int64_t y=int64_t(std::floor(box.low.y));y<=int64_t(std::floor(box.high.y));++y)
            for(int64_t z=int64_t(std::floor(box.low.z));z<=int64_t(std::floor(box.high.z));++z) {
                auto found=buckets.find({x,y,z}); if(found==buckets.end()) continue;
                for(auto p:found->second) {
                    double t=dot(sub(p,a),direction)/length2;
                    if(t<=EPS || t>=1-EPS) continue;
                    auto distance=sub(p,add(a,mul(direction,t)));
                    if(dot(distance,distance)<EPS*EPS) points.push_back({t,p});
                }
            }
            std::sort(points.begin(),points.end(),[](auto &a,auto &b){return a.first<b.first;});
            for(auto point:points) boundary.push_back(point.second);
        }
        if(!valid(boundary)) continue;
        const uint32_t base=uint32_t(out.positions.size());
        auto emit=[&](Vec3 p){out.positions.push_back(mul(p,scale));out.normals.push_back(face.normal);out.colors.push_back(face.color);out.materials.push_back(face.material);};
        Vec3 centroid{};
        for(auto p:boundary) centroid=add(centroid,p);
        centroid=mul(centroid,1.0/boundary.size());
        emit(centroid);
        for(auto p:boundary) emit(p);
        for(uint32_t i=0;i<boundary.size();++i) {
            out.indices.insert(out.indices.end(),{base,base+1+i,base+1+(i+1)%uint32_t(boundary.size())});
        }
        ++out.face_count;
    }
    return out;
}
}
MeshBuffers SurfaceJoiner::join(const VoxelChunk &chunk,const MeshBuffers &input) {
    std::vector<Solid> bridges,cells;
    std::vector<Face> bridge_faces;
    // Tangential y steps span TWO logical layers: alternating BCC layers
    // are offset, while y and y+2 have aligned square faces.
    for(const auto &frame:BRIDGE_FRAMES) {
        for(int y=0;y<VoxelChunk::SIZE;++y) for(int z=0;z<VoxelChunk::SIZE;++z) for(int x=0;x<VoxelChunk::SIZE;++x) {
            Int3 anchor{x,y,z};
            Int3 quad[4]={anchor,anchor+frame.du,anchor+frame.dv,anchor+frame.du+frame.dv};
            bool exposed=true;
            for(auto cell:quad) exposed&=chunk.get(cell)!=Voxel::Empty && chunk.get(cell+frame.forward)==Voxel::Empty;
            if(!exposed) continue;
            const Vec3 origin=center(x,y,z);
            Int3 central=cell_at_center(add(origin,frame.rotate({2,2,2})));
            if(chunk.get(central)!=Voxel::Empty) continue;
            Color color{0,0,0,1};
            std::array<int,4> material_counts{};
            for(auto cell:quad) { const auto voxel=chunk.get(cell); auto c=tint(voxel);color.r+=c.r/4;color.g+=c.g/4;color.b+=c.b/4;++material_counts[size_t(voxel)]; }
            Voxel material=Voxel::Empty;
            for(size_t i=1;i<material_counts.size();++i)
                if(material_counts[i]>material_counts[size_t(material)]) material=Voxel(i);
            add_bridge(origin,frame,int(bridges.size())+1,color,material,bridges,bridge_faces);
        }
    }
    if(bridges.empty()) return input;
    for(int y=0;y<VoxelChunk::SIZE;++y) for(int z=0;z<VoxelChunk::SIZE;++z) for(int x=0;x<VoxelChunk::SIZE;++x)
        if(chunk.get({x,y,z})!=Voxel::Empty) cells.push_back(octahedron(center(x,y,z)));
    std::vector<Face> faces;
    const double inverse=std::sqrt(2.0)/OctahedronGeometry::EDGE_LENGTH;
    // Original geometry consists of consecutive square/hexagon triangle fans.
    for(size_t i=0;i<input.indices.size();) {
        uint32_t base=input.indices[i];
        size_t end=i;
        while(end<input.indices.size() && input.indices[end]==base) end+=3;
        size_t count=(end-i)/3+2;
        Face face{{},input.normals[base],input.colors[base],input.materials[base],0};
        for(size_t j=0;j<count;++j) face.vertices.push_back(canonical(mul(input.positions[base+j],inverse)));
        std::vector<Polygon> pieces{face.vertices};
        for(const auto &bridge:bridges) { subtract_solid(pieces,face.normal,0,bridge); if(pieces.empty()) break; }
        for(auto &poly:pieces) faces.push_back({std::move(poly),face.normal,face.color,face.material,0});
        i=end;
    }
    for(auto &face:bridge_faces) {
        std::vector<Polygon> pieces{face.vertices};
        for(const auto &cell:cells) { subtract_solid(pieces,face.normal,face.owner,cell); if(pieces.empty()) break; }
        for(const auto &bridge:bridges) {
            if(bridge.id==face.owner) continue;
            subtract_solid(pieces,face.normal,face.owner,bridge); if(pieces.empty()) break;
        }
        for(auto &poly:pieces) faces.push_back({std::move(poly),face.normal,face.color,face.material,face.owner});
    }
    return triangulate(faces);
}
}
