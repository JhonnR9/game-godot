#include "terrain_generator.h"
#include "chunk_mesher.h"
#include "truncated_octahedron_geometry.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <tuple>
#include <limits>
#include <set>
#include <type_traits>

using namespace terrain;
static_assert(VoxelChunk::SIZE==16 && VoxelChunk::CAPACITY==4096);
static_assert(!std::is_polymorphic<VoxelChunk>::value, "Chunk must be plain logical data");
Vec3 sub(Vec3 a,Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 cross(Vec3 a,Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
double dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
uint64_t digest(const VoxelChunk &chunk) {
    uint64_t h=14695981039346656037ull;
    for (auto v:chunk.cells) { h=(h^uint8_t(v))*1099511628211ull; }
    return h;
}
void verify_mesh(const VoxelChunk &chunk,const char *label) {
    const auto mesh=ChunkMesher().build(chunk,0.0);
    assert(mesh.positions.size()==mesh.normals.size() && mesh.colors.size()==mesh.positions.size());
    assert(mesh.materials.size()==mesh.positions.size());
    assert(mesh.indices.size()%3==0);
    std::map<std::tuple<double,double,double>,int> ids;
    std::map<std::pair<int,int>,std::pair<int,int>> edges;
    std::set<std::array<int,3>> triangles;
    double volume=0;
    for (size_t i=0;i<mesh.indices.size();i+=3) {
        int vi[3];
        Vec3 p[3];
        for (int j=0;j<3;++j) {
            assert(mesh.indices[i+j]<mesh.positions.size());
            p[j]=mesh.positions[mesh.indices[i+j]];
            const auto key=std::make_tuple(p[j].x,p[j].y,p[j].z);
            auto found=ids.find(key);
            if (found==ids.end()) { found=ids.emplace(key,int(ids.size())).first; }
            vi[j]=found->second;
        }
        auto normal=cross(sub(p[1],p[0]),sub(p[2],p[0]));
        assert(dot(normal,mesh.normals[mesh.indices[i]])<0);
        volume-=dot(p[0],cross(p[1],p[2]))/6;
        std::array<int,3> tri={vi[0],vi[1],vi[2]};
        std::sort(tri.begin(),tri.end());
        assert(triangles.insert(tri).second);
        for (int j=0;j<3;++j) {
            const int a=vi[j],b=vi[(j+1)%3];
            auto &incidence=edges[{std::min(a,b),std::max(a,b)}];
            ++incidence.first;
            incidence.second+=(a<b ? 1:-1);
        }
    }
    for (const auto &entry:edges) { assert(entry.second.first==2 && entry.second.second==0); }
    const double expected=chunk.solid_count()*8*std::sqrt(2.0)*std::pow(OctahedronGeometry::EDGE_LENGTH,3);
    assert(std::abs(volume-expected)<std::max(1.0,expected)*1e-9);
    std::cout<<"PASS mesh "<<label<<": voxels="<<chunk.solid_count()<<" faces="<<mesh.face_count<<" triangles="<<mesh.indices.size()/3<<" volume="<<volume<<'\n';
}

int main() {
    const TerrainGenerator generator(1337);
    const auto base=generator.generate({0,0,0});
    assert(base.cells==generator.generate({0,0,0}).cells);
    assert(base.cells!=TerrainGenerator(1338).generate({0,0,0}).cells);
    assert(base.cells!=generator.generate({1,0,0}).cells);
    assert(base.cells!=generator.generate({0,0,1}).cells);
    assert(base.solid_count()>0 && base.solid_count()<4096);
    int caves=0,air=0,grass=0,soil=0,rock=0;
    double lowest=1e9,highest=-1e9;
    for (int y=0;y<16;++y) for(int z=0;z<16;++z) for(int x=0;x<16;++x) {
        const Int3 local{x,y,z};
        const auto p=OctahedralLattice::noise_position(base.world_cell(local));
        const double h=generator.surface_height(p.x,p.z);
        lowest=std::min(lowest,h); highest=std::max(highest,h);
        const auto v=base.get(local);
        if (p.y>h) { assert(v==Voxel::Empty); ++air; }
        else if (v==Voxel::Empty) { assert(h-p.y>=3 && generator.is_cave(p,h)); ++caves; }
        else if(h-p.y<3) { assert(!generator.is_cave(p,h)); }
        grass+=v==Voxel::Grass; soil+=v==Voxel::Dirt; rock+=v==Voxel::Stone;
    }
    assert(caves>20 && air>0 && grass>0 && soil>0 && rock>0 && highest-lowest>1);
    std::cout<<"PASS noise: caves="<<caves<<" above_surface_air="<<air<<" height_range="<<lowest<<".."<<highest<<" hash="<<digest(base)<<'\n';
    verify_mesh(base,"seed 1337 surface and caves");
    verify_mesh(TerrainGenerator(-419).generate({-2,0,-3}),"negative seed and coordinates");
    verify_mesh(generator.generate({0,-1,0}),"underground");
    const auto sky=generator.generate({0,1,0});
    assert(sky.solid_count()==0);
    verify_mesh(sky,"empty sky");
    // Global world sampling does not reset at chunk boundaries, including -1 -> 0.
    for(const Int3 coordinates: {Int3{-1,0,0},Int3{0,0,0},Int3{1,-1,2},Int3{-2,-1,-1}}) {
        const auto chunk=generator.generate(coordinates);
        for(int y=0;y<16;++y) for(int z=0;z<16;++z) for(int x=0;x<16;++x) {
            const Int3 local{x,y,z};
            assert(chunk.get(local)==generator.sample(chunk.world_cell(local)));
        }
        const auto right=generator.generate(coordinates+Int3{1,0,0});
        for(int y=0;y<16;++y) for(int z=0;z<16;++z) {
            assert(chunk.world_cell({16,y,z})==right.world_cell({0,y,z}));
            assert(generator.sample(chunk.world_cell({16,y,z}))==right.get({0,y,z}));
        }
    }
    // Noise is continuous across lattice integer boundaries, unlike hashed occupancy.
    GradientNoise noise(1337);
    for(int i=-5;i<=5;++i) {
        assert(std::abs(noise.noise2(i-1e-7,0.37)-noise.noise2(i+1e-7,0.37))<1e-5);
        assert(std::abs(noise.noise3(.31,i-1e-7,-.18)-noise.noise3(.31,i+1e-7,-.18))<1e-5);
    }
    // Every face neighbor shares exactly the same polygon in global lattice coordinates.
    for(int y=-3;y<4;++y) for(int face=0;face<14;++face) {
        const Int3 cell{-1,y,2};
        const auto neighbor=OctahedralLattice::neighbor(cell,face);
        const auto a=OctahedralLattice::noise_position(cell),b=OctahedralLattice::noise_position(neighbor);
        const double d2=dot(sub(b,a),sub(b,a));
        assert(std::abs(d2-(face<6?4.0:3.0))<1e-12);
        bool reverse=false;
        for(int other=0;other<14;++other) { reverse|=OctahedralLattice::neighbor(neighbor,other)==cell; }
        assert(reverse);
    }
    for(int64_t seed:{std::numeric_limits<int64_t>::min(),std::numeric_limits<int64_t>::max()}) {
        TerrainGenerator extreme(seed);
        for(Int3 p:{Int3{INT32_MAX,INT32_MAX,INT32_MAX},Int3{INT32_MIN,INT32_MIN,INT32_MIN}}) {
            const auto chunk=extreme.generate(p);
            assert(chunk.cells==extreme.generate(p).cells);
        }
    }
    VoxelChunk empty;
    verify_mesh(empty,"data can exist without geometry or engine");
    empty.set({2,3,4},Voxel::Stone);
    const auto single=ChunkMesher().build(empty,false);
    assert(single.face_count==14 && single.indices.size()==132);
    verify_mesh(empty,"single-cell geometry");
    std::cout<<"PASS: pure C++ data, 2D terrain, 3D caves, determinism, global coordinates, watertight meshing, extremes\n";
}
