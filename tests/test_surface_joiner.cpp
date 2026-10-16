#include "chunk_mesher.h"
#include "terrain_generator.h"
#include <cassert>
#include <cmath>
#include <map>
#include <tuple>
#include <set>
#include <iostream>
using namespace terrain;
Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
double verify(const MeshBuffers &mesh,const char *name) {
 std::map<std::tuple<double,double,double>,int> unique;
 std::map<std::pair<int,int>,std::pair<int,int>> edges;
 std::set<std::array<int,3>> tris;
 double volume=0;
 for(size_t i=0;i<mesh.indices.size();i+=3) {
  Vec3 p[3];int ids[3];
  for(int j=0;j<3;++j) {
   auto index=mesh.indices[i+j]; assert(index<mesh.positions.size()); p[j]=mesh.positions[index];
   auto key=std::make_tuple(p[j].x,p[j].y,p[j].z);
   auto found=unique.find(key);if(found==unique.end())found=unique.emplace(key,int(unique.size())).first;ids[j]=found->second;
  }
  auto n=cross(sub(p[2],p[0]),sub(p[1],p[0]));
  assert(dot(n,n)>1e-16);
  assert(dot(n,mesh.normals[mesh.indices[i]])>0);
  for(int j=1;j<3;++j) assert(dot(sub(mesh.normals[mesh.indices[i]],mesh.normals[mesh.indices[i+j]]),sub(mesh.normals[mesh.indices[i]],mesh.normals[mesh.indices[i+j]]))<1e-16);
  volume-=dot(p[0],cross(p[1],p[2]))/6;
  std::array<int,3> tri={ids[0],ids[1],ids[2]};std::sort(tri.begin(),tri.end());assert(tris.insert(tri).second);
  for(int j=0;j<3;++j){int a=ids[j],b=ids[(j+1)%3];auto &e=edges[{std::min(a,b),std::max(a,b)}];++e.first;e.second+=a<b?1:-1;}
 }
 int bad=0;
 for(auto e:edges) if(e.second.first!=2||e.second.second!=0) {if(bad<8)std::cerr<<"bad edge "<<e.first.first<<","<<e.first.second<<" count="<<e.second.first<<" winding="<<e.second.second<<'\n';++bad;}
 if(bad)std::cerr<<name<<" has "<<bad<<" unmatched edges\n";
 assert(bad==0);
 std::cout<<"PASS "<<name<<": triangles="<<mesh.indices.size()/3<<" volume="<<volume<<'\n';return volume;
}
bool identical(const MeshBuffers&a,const MeshBuffers&b){
 if(a.indices!=b.indices||a.positions.size()!=b.positions.size())return false;
 for(size_t i=0;i<a.positions.size();++i) {
  if(dot(sub(a.positions[i],b.positions[i]),sub(a.positions[i],b.positions[i]))>1e-20) return false;
 }
 return true;
}
int main(){
 VoxelChunk chunk;
 chunk.set({5,4,5},Voxel::Grass);
 auto raw=ChunkMesher().build(chunk,false);
 assert(identical(raw,ChunkMesher().build(chunk,true)));
 chunk.set({6,4,5},Voxel::Grass);chunk.set({5,4,6},Voxel::Grass);
 assert(identical(ChunkMesher().build(chunk,false),ChunkMesher().build(chunk,true)));
 chunk.set({6,4,6},Voxel::Grass);
 const auto before=chunk.cells;
 auto joined=ChunkMesher().build(chunk,true);
 assert(!identical(ChunkMesher().build(chunk,false),joined)); assert(before==chunk.cells);
 double scale=OctahedronGeometry::EDGE_LENGTH/std::sqrt(2.0);
 double rawvolume=verify(ChunkMesher().build(chunk,false),"four original cells");
 double volume=verify(joined,"four aligned cells + eight-vertex plane");
 assert(std::abs(volume-rawvolume-2*(34.0/3)*scale*scale*scale)<1e-8);
 auto covers_center=[&](const MeshBuffers &mesh) {
  Vec3 target{-9*scale,12*scale,-9*scale};
  for(size_t i=0;i<mesh.indices.size();i+=3) {
   auto n=mesh.normals[mesh.indices[i]];
   auto a=mesh.positions[mesh.indices[i]],b=mesh.positions[mesh.indices[i+1]],c=mesh.positions[mesh.indices[i+2]];
   if(n.y<.99 || std::abs(a.y-target.y)>1e-8) continue;
   auto orient=[](Vec3 a,Vec3 b,Vec3 p){return (b.x-a.x)*(p.z-a.z)-(b.z-a.z)*(p.x-a.x);};
   double x=orient(a,b,target),y=orient(b,c,target),z=orient(c,a,target);
   if((x>=-1e-8&&y>=-1e-8&&z>=-1e-8)||(x<=1e-8&&y<=1e-8&&z<=1e-8)) return true;
  }
  return false;
 };
 assert(!covers_center(ChunkMesher().build(chunk,false)));
 assert(covers_center(joined));
 // The former square caps and new bridges must have one continuous base
 // color on the flat top; lighting/AO can then supply actual shading.
 int planar_vertices=0;
 for(size_t i=0;i<joined.positions.size();++i) {
  if(joined.normals[i].y>.99 && std::abs(joined.positions[i].y-12*scale)<1e-8) {
   assert(std::abs(joined.colors[i].r-.23f)<1e-6);
   assert(std::abs(joined.colors[i].g-.48f)<1e-6);
   assert(std::abs(joined.colors[i].b-.105f)<1e-6);
   ++planar_vertices;
  }
 }
 assert(planar_vertices>0);
 // Four adjacent bridges must cancel their shared vertical triangles.
 for(int x=5;x<=7;++x)for(int z=5;z<=7;++z)chunk.set({x,4,z},Voxel::Grass);
 joined=ChunkMesher().build(chunk,true);
 rawvolume=verify(ChunkMesher().build(chunk,false),"3x3 original cells");
 volume=verify(joined,"3x3 aligned cells with adjacent bridges");
 assert(std::abs(volume-rawvolume-8*(34.0/3)*scale*scale*scale)<1e-8);
 // Occupied upper central cell prevents the local patch.
 VoxelChunk blocked;for(auto p:{Int3{5,4,5},Int3{6,4,5},Int3{5,4,6},Int3{6,4,6},Int3{5,5,5},Int3{5,3,5}})blocked.set(p,Voxel::Stone);
 assert(identical(ChunkMesher().build(blocked,false),ChunkMesher().build(blocked,true)));
 // Rotate the identical four-cell construction onto all three principal
 // planes. Both opposing directions must receive the same planar bridge.
 auto covers=[](const MeshBuffers &mesh,Vec3 point,Vec3 normal) {
  for(size_t i=0;i<mesh.indices.size();i+=3) {
   auto a=mesh.positions[mesh.indices[i]],b=mesh.positions[mesh.indices[i+1]],c=mesh.positions[mesh.indices[i+2]];
   if(dot(mesh.normals[mesh.indices[i]],normal)<.99 || std::abs(dot(sub(point,a),normal))>1e-8) continue;
   double x=dot(cross(sub(b,a),sub(point,a)),normal);
   double y=dot(cross(sub(c,b),sub(point,b)),normal);
   double z=dot(cross(sub(a,c),sub(point,c)),normal);
   if((x>=-1e-8&&y>=-1e-8&&z>=-1e-8)||(x<=1e-8&&y<=1e-8&&z<=1e-8)) return true;
  }
  return false;
 };
 for(int axis=0;axis<3;++axis) {
  Int3 anchor{5,4,5};
  Int3 du=axis==0?Int3{0,2,0}:Int3{1,0,0};
  Int3 dv=axis==2?Int3{0,2,0}:Int3{0,0,1};
  VoxelChunk plane;
  for(auto q:{anchor,anchor+du,anchor+dv,anchor+du+dv}) plane.set(q,Voxel::Stone);
  auto before=plane.cells;
  auto original=ChunkMesher().build(plane,false),connected=ChunkMesher().build(plane,true);
  assert(before==plane.cells);
  const double raw_volume=verify(original,"rotated four cells original");
  const double joined_volume=verify(connected,"rotated four cells both directions");
  assert(std::abs(joined_volume-raw_volume-2*(34.0/3)*scale*scale*scale)<1e-8);
  Vec3 center{-11,10,-11};
  if(axis==0){center.y+=2;center.z+=2;}
  if(axis==1){center.x+=2;center.z+=2;}
  if(axis==2){center.x+=2;center.y+=2;}
  for(int sign:{-1,1}) {
   Vec3 normal{axis==0?double(sign):0,axis==1?double(sign):0,axis==2?double(sign):0};
   Vec3 target{(center.x+2*normal.x)*scale,(center.y+2*normal.y)*scale,(center.z+2*normal.z)*scale};
   assert(!covers(original,target,normal));assert(covers(connected,target,normal));
  }
 }
 // A cell just outside the group partially touches a new side triangle.
 VoxelChunk side;for(auto p:{Int3{5,4,5},Int3{6,4,5},Int3{5,4,6},Int3{6,4,6},Int3{5,5,4}})side.set(p,Voxel::Grass);
 verify(ChunkMesher().build(side,true),"bridge against outer upper cell");
 verify(ChunkMesher().build(TerrainGenerator(1337).generate({0,0,0}),true),"generated surface and caves");
 verify(ChunkMesher().build(TerrainGenerator(1360).generate({3,0,11}),true),"current editor terrain");
 verify(ChunkMesher().build(TerrainGenerator(-419).generate({-2,0,-3}),true),"negative region");
 std::cout<<"PASS selective joining: unchanged isolated/rough cells, flat octagon, solid union, no internal faces or T junctions\n";
}
