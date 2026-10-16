#include "ambient_occlusion.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace terrain {
namespace {
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 mul(Vec3 a,double s){return {a.x*s,a.y*s,a.z*s};}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Vec3 unit(Vec3 a){double l=std::sqrt(dot(a,a));return l>1e-12?mul(a,1/l):Vec3{};}
double axis(Vec3 a,int i){return i==0?a.x:i==1?a.y:a.z;}
Vec3 low(Vec3 a,Vec3 b){return {std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z)};}
Vec3 high(Vec3 a,Vec3 b){return {std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z)};}
struct Triangle { Vec3 a,b,c,min,max,center; };
struct Node { Vec3 min,max; int first=0,count=0,left=-1,right=-1; };
class Tree {
    std::vector<Triangle> triangles;
    std::vector<Node> nodes;
    int build(int first,int end) {
        const int id=int(nodes.size()); nodes.push_back({});
        Vec3 min=triangles[first].min,max=triangles[first].max;
        Vec3 cmin=triangles[first].center,cmax=cmin;
        for(int i=first+1;i<end;++i){min=low(min,triangles[i].min);max=high(max,triangles[i].max);cmin=low(cmin,triangles[i].center);cmax=high(cmax,triangles[i].center);}
        nodes[id].min=min;nodes[id].max=max;
        if(end-first<=8){nodes[id].first=first;nodes[id].count=end-first;return id;}
        Vec3 span=sub(cmax,cmin);int split=span.y>span.x?1:0;if(span.z>axis(span,split))split=2;
        int mid=(first+end)/2;
        std::nth_element(triangles.begin()+first,triangles.begin()+mid,triangles.begin()+end,[split](const Triangle&a,const Triangle&b){return axis(a.center,split)<axis(b.center,split);});
        int left=build(first,mid),right=build(mid,end);
        nodes[id].left=left;nodes[id].right=right;return id;
    }
    static bool hit_box(const Node &node,Vec3 origin,Vec3 dir,double far) {
        double near=0;
        for(int i=0;i<3;++i){
            double o=axis(origin,i),d=axis(dir,i),min=axis(node.min,i),max=axis(node.max,i);
            if(std::abs(d)<1e-12){if(o<min-1e-10||o>max+1e-10)return false;continue;}
            double a=(min-o)/d,b=(max-o)/d;if(a>b)std::swap(a,b);
            near=std::max(near,a);far=std::min(far,b);if(near>far+1e-10)return false;
        }
        return true;
    }
    static double hit_triangle(const Triangle&t,Vec3 origin,Vec3 dir,double far) {
        Vec3 e1=sub(t.b,t.a),e2=sub(t.c,t.a),h=cross(dir,e2);
        double det=dot(e1,h);if(std::abs(det)<1e-12)return far;
        double inv=1/det;Vec3 s=sub(origin,t.a);
        double u=dot(s,h)*inv;if(u< -1e-9||u>1+1e-9)return far;
        Vec3 q=cross(s,e1);double v=dot(dir,q)*inv;if(v< -1e-9||u+v>1+1e-9)return far;
        double distance=dot(e2,q)*inv;
        return distance>1e-7 && distance<far?distance:far;
    }
public:
    explicit Tree(const MeshBuffers &mesh) {
        triangles.reserve(mesh.indices.size()/3);
        for(size_t i=0;i+2<mesh.indices.size();i+=3){
            Vec3 a=mesh.positions[mesh.indices[i]],b=mesh.positions[mesh.indices[i+1]],c=mesh.positions[mesh.indices[i+2]];
            if(dot(cross(sub(b,a),sub(c,a)),cross(sub(b,a),sub(c,a)))<1e-20)continue;
            triangles.push_back({a,b,c,low(a,low(b,c)),high(a,high(b,c)),mul(add(a,add(b,c)),1.0/3)});
        }
        if(!triangles.empty())build(0,int(triangles.size()));
    }
    double nearest(Vec3 origin,Vec3 dir,double radius) const {
        if(nodes.empty())return radius;
        std::array<int,128> stack;int size=0;stack[size++]=0;double distance=radius;
        while(size){
            const Node &node=nodes[stack[--size]];
            if(!hit_box(node,origin,dir,distance))continue;
            if(node.count){for(int i=node.first;i<node.first+node.count;++i)distance=hit_triangle(triangles[i],origin,dir,distance);}
            else {stack[size++]=node.left;stack[size++]=node.right;}
        }
        return distance;
    }
};
}
std::vector<float> AmbientOcclusion::calculate(const MeshBuffers &mesh,double radius) {
    std::vector<float> result(mesh.positions.size(),1.0f);
    if(mesh.indices.empty() || !std::isfinite(radius) || radius<=0 || mesh.normals.size()!=mesh.positions.size())return result;
    const Tree tree(mesh);
    // Identical positions on coplanar polygons share a value even when the
    // final mesh splits the vertices for different colors or hard normals.
    std::map<std::array<int64_t,6>,float> cache;
    const double bias=std::min(radius*0.001,OctahedronGeometry::EDGE_LENGTH*0.002);
    for(size_t i=0;i<mesh.positions.size();++i){
        Vec3 p=mesh.positions[i],n=unit(mesh.normals[i]);
        if(dot(n,n)<0.5)continue;
        const std::array<int64_t,6> key={std::llround(p.x*1e7),std::llround(p.y*1e7),std::llround(p.z*1e7),std::llround(n.x*1e7),std::llround(n.y*1e7),std::llround(n.z*1e7)};
        auto found=cache.find(key);if(found!=cache.end()){result[i]=found->second;continue;}
        Vec3 helper=std::abs(n.y)<0.9?Vec3{0,1,0}:Vec3{1,0,0};
        Vec3 tangent=unit(cross(helper,n)),bitangent=cross(n,tangent);
        Vec3 origin=add(p,mul(n,bias));double blocked=0;
        for(int sample=0;sample<SAMPLES;++sample){
            double r=std::sqrt((sample+0.5)/SAMPLES),angle=sample*2.399963229728653;
            Vec3 direction=add(add(mul(tangent,r*std::cos(angle)),mul(bitangent,r*std::sin(angle))),mul(n,std::sqrt(1-r*r)));
            double distance=tree.nearest(origin,direction,radius);
            double weight=1-distance/radius;blocked+=weight*weight;
        }
        result[i]=float(std::clamp(1-blocked/SAMPLES,0.0,1.0));cache.emplace(key,result[i]);
    }
    return result;
}
}
