#include "convex_clipper.h"
#include <algorithm>
#include <cmath>

namespace terrain::core {
namespace {
constexpr double EPS=1e-8;
Vec3 add(Vec3 a,Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 sub(Vec3 a,Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 mul(Vec3 a,double k) { return {a.x*k,a.y*k,a.z*k}; }
double dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec3 cross(Vec3 a,Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
ConvexPolygon clip(const ConvexPolygon &poly,ClipPlane plane,bool inside) {
    ConvexPolygon out;
    if(poly.empty()) return out;
    for(size_t i=0;i<poly.size();++i) {
        const Vec3 a=poly[i],b=poly[(i+1)%poly.size()];
        const double da=dot(plane.normal,a)-plane.distance,db=dot(plane.normal,b)-plane.distance;
        const bool keep_a=inside?da<=EPS:da>=-EPS,keep_b=inside?db<=EPS:db>=-EPS;
        if(keep_a) out.push_back(a);
        if(keep_a!=keep_b && std::abs(da-db)>EPS) out.push_back(add(a,mul(sub(b,a),da/(da-db))));
    }
    ConvexPolygon clean;
    for(const auto p:out) {
        const auto d=sub(clean.empty()?p:clean.back(),p);
        if(clean.empty() || dot(d,d)>EPS*EPS) clean.push_back(p);
    }
    if(clean.size()>1) { const auto d=sub(clean.front(),clean.back()); if(dot(d,d)<EPS*EPS) clean.pop_back(); }
    return clean;
}
}

ClipBounds ConvexClipper::bounds(const ConvexPolygon &polygon) {
    ClipBounds b{polygon.front(),polygon.front()};
    for(const auto p:polygon) {
        b.low={std::min(b.low.x,p.x),std::min(b.low.y,p.y),std::min(b.low.z,p.z)};
        b.high={std::max(b.high.x,p.x),std::max(b.high.y,p.y),std::max(b.high.z,p.z)};
    }
    return b;
}

bool ConvexClipper::overlaps(const ClipBounds &a,const ClipBounds &b) {
    return a.low.x<=b.high.x+EPS && a.high.x>=b.low.x-EPS &&
           a.low.y<=b.high.y+EPS && a.high.y>=b.low.y-EPS &&
           a.low.z<=b.high.z+EPS && a.high.z>=b.low.z-EPS;
}

bool ConvexClipper::valid(const ConvexPolygon &poly) {
    if(poly.size()<3) return false;
    Vec3 area{};
    for(size_t i=1;i+1<poly.size();++i) {
        const auto c=cross(sub(poly[i],poly[0]),sub(poly[i+1],poly[0]));
        area=add(area,c);
    }
    return dot(area,area)>EPS*EPS;
}

std::vector<ConvexPolygon> ConvexClipper::subtract(const ConvexPolygon &poly,Vec3 normal,
                                                    const PrimitiveId &owner,const ClipSolid &cutter) {
    if(!overlaps(bounds(poly),cutter.bounds)) return {poly};
    for(const auto plane:cutter.planes) {
        bool coplanar=true;
        for(const auto p:poly) coplanar &= std::abs(dot(plane.normal,p)-plane.distance)<EPS;
        if(coplanar && dot(normal,plane.normal)>EPS && owner<cutter.id) return {poly};
    }
    ConvexPolygon remainder=poly;
    std::vector<ConvexPolygon> fragments;
    for(const auto plane:cutter.planes) {
        bool all_inside=true,all_outside=true;
        for(const auto p:remainder) {
            const double d=dot(plane.normal,p)-plane.distance;
            all_inside &= d<=EPS; all_outside &= d>=-EPS;
        }
        if(all_inside) continue;
        if(all_outside) { if(valid(remainder)) fragments.push_back(remainder); return fragments; }
        auto outside=clip(remainder,plane,false);
        if(valid(outside)) fragments.push_back(std::move(outside));
        remainder=clip(remainder,plane,true);
        if(!valid(remainder)) return fragments;
    }
    return fragments;
}

} // namespace terrain::core
