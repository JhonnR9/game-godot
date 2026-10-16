#include "ambient_occlusion.h"
#include "chunk_mesher.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace terrain;
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 mul(Vec3 a,double t){return {a.x*t,a.y*t,a.z*t};}
void ceiling(MeshBuffers &mesh,Vec3 normal,double distance){
 Vec3 u=std::abs(normal.x)>.5?Vec3{0,1,0}:Vec3{1,0,0};
 Vec3 v=std::abs(normal.z)>.5?Vec3{0,1,0}:Vec3{0,0,1};
 Vec3 center=mul(normal,distance);
 uint32_t base=uint32_t(mesh.positions.size());
 for(auto p:{add(mul(u,-10),mul(v,-10)),add(mul(u,10),mul(v,-10)),add(mul(u,10),mul(v,10)),add(mul(u,-10),mul(v,10))}){
  mesh.positions.push_back(add(center,p));mesh.normals.push_back(normal);mesh.colors.push_back({1,1,1,1});
 }
 mesh.indices.insert(mesh.indices.end(),{base,base+1,base+2,base,base+2,base+3});
}
int main(){
 MeshBuffers empty;assert(AmbientOcclusion::calculate(empty,1).empty());
 for(auto normal:{Vec3{1,0,0},Vec3{-1,0,0},Vec3{0,1,0},Vec3{0,-1,0},Vec3{0,0,1},Vec3{0,0,-1}}){
  MeshBuffers mesh;mesh.positions.push_back({0,0,0});mesh.normals.push_back(normal);mesh.colors.push_back({.5,.5,.5,1});
  assert(AmbientOcclusion::calculate(mesh,1)[0]==1);
  ceiling(mesh,normal,.1);
  auto positions=mesh.positions;auto indices=mesh.indices;auto colors=mesh.colors;
  auto ao=AmbientOcclusion::calculate(mesh,1.25);
  assert(ao[0]<.45 && ao[0]>0);
  assert(ao==AmbientOcclusion::calculate(mesh,1.25));
  assert(mesh.indices==indices && mesh.positions.size()==positions.size() && mesh.colors[0].r==colors[0].r);
  assert(AmbientOcclusion::calculate(mesh,.05)[0]==1);
  assert(AmbientOcclusion::calculate(mesh,0)[0]==1);
  MeshBuffers behind;behind.positions.push_back({0,0,0});behind.normals.push_back(normal);behind.colors.push_back({1,1,1,1});
  ceiling(behind,normal,-.1);assert(AmbientOcclusion::calculate(behind,1.25)[0]==1);
 }
 VoxelChunk chunk;for(auto p:{Int3{5,4,5},Int3{6,4,5},Int3{5,4,6},Int3{6,4,6}})chunk.set(p,Voxel::Stone);
 auto joined=ChunkMesher().build(chunk,true);
 auto ao=AmbientOcclusion::calculate(joined,1.25);
 assert(ao.size()==joined.positions.size());
 int exposed_caps=0,shaded=0;
 for(size_t i=0;i<ao.size();++i){
  assert(std::isfinite(ao[i]) && ao[i]>=0 && ao[i]<=1);
  double scale=OctahedronGeometry::EDGE_LENGTH/std::sqrt(2.0);
  bool top=joined.normals[i].y>.99 && std::abs(joined.positions[i].y-12*scale)<1e-8;
  bool bottom=joined.normals[i].y<-.99 && std::abs(joined.positions[i].y-8*scale)<1e-8;
  if(top || bottom){assert(ao[i]>.9999f);++exposed_caps;}
  if(ao[i]<.99)++shaded;
 }
 assert(exposed_caps>0 && shaded>0);
 std::cout<<"PASS geometry AO: six directions, finite radius, back hemisphere, deterministic samples, unshadowed new flat caps, unchanged mesh\n";
}
