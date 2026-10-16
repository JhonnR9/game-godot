#include "world_types.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <set>
#include <tuple>

using namespace terrain;

static void coordinates() {
    assert(floor_div(-1, 16) == -1 && floor_mod(-1, 16) == 15);
    assert(floor_div(-16, 16) == -1 && floor_mod(-16, 16) == 0);
    assert(floor_div(-17, 16) == -2 && floor_mod(-17, 16) == 15);
    assert(floor_div(17, 16) == 1 && floor_mod(17, 16) == 1);

    for (int64_t x = -257; x <= 257; ++x) {
        for (int64_t y = -33; y <= 33; ++y) {
            const Int3 global{x, y, -x};
            const auto address = split_global_cell(global);
            assert(VoxelChunk::contains(address.local));
            assert(join_global_cell(address.chunk, address.local) == global);
        }
    }
    const auto neg = split_global_cell({-1, -1, -1});
    assert((neg.chunk == Int3{-1, -1, -1}));
    assert((neg.local == Int3{15, 15, 15}));

    const double s = OctahedralLattice::SCALE;
    const Vec3 origin = OctahedralLattice::center({0, 0, 0});
    const Vec3 chunk_x = OctahedralLattice::center({16, 0, 0});
    const Vec3 chunk_y = OctahedralLattice::center({0, 16, 0});
    const Vec3 chunk_z = OctahedralLattice::center({0, 0, 16});
    assert(std::abs(chunk_x.x - origin.x - 64*s) < 1e-12);
    assert(std::abs(chunk_y.y - origin.y - 32*s) < 1e-12);
    assert(std::abs(chunk_z.z - origin.z - 64*s) < 1e-12);
}

static void topology() {
    for (int64_t y = -32; y <= 32; ++y) {
        for (int64_t x = -2; x <= 2; ++x) {
            for (int64_t z = -2; z <= 2; ++z) {
                const Int3 source{x, y, z};
                std::set<std::tuple<int64_t,int64_t,int64_t>> unique;
                for (int face = 0; face < 14; ++face) {
                    const Int3 target = OctahedralLattice::neighbor(source, face);
                    assert(unique.emplace(target.x,target.y,target.z).second);
                    bool reverse = false;
                    for (int candidate = 0; candidate < 14; ++candidate)
                        reverse |= OctahedralLattice::neighbor(target,candidate) == source;
                    assert(reverse);
                    const Vec3 a = OctahedralLattice::noise_position(source);
                    const Vec3 b = OctahedralLattice::noise_position(target);
                    const double dx=b.x-a.x, dy=b.y-a.y, dz=b.z-a.z;
                    const double d2=dx*dx+dy*dy+dz*dz;
                    assert(std::abs(d2-(face<6 ? 4.0 : 3.0)) < 1e-12);
                }
                assert(unique.size() == 14);
            }
        }
    }
    assert(OctahedralLattice::parity(-1) == 1);
    assert(OctahedralLattice::parity(-2) == 0);
}

static void catalog_and_domain() {
    static_assert(static_cast<uint16_t>(BlockId::Air) == 0);
    static_assert(static_cast<uint16_t>(BlockId::Bedrock) == 8);
    for (uint16_t id=0; id<=8; ++id) {
        const auto *block = BlockRegistry::find(static_cast<BlockId>(id));
        assert(block && static_cast<uint16_t>(block->id)==id);
    }
    assert(BlockRegistry::find(static_cast<BlockId>(9)) == nullptr);
    assert(!BlockRegistry::find(BlockId::Air)->solid);
    assert(BlockRegistry::find(BlockId::Bedrock)->solid);
    assert(!BlockRegistry::find(BlockId::Bedrock)->removable);

    using Kind = WorldDescriptor::BoundaryKind;
    const auto floor_a=WorldDescriptor::classify_boundary({0,-128,0});
    const auto floor_b=WorldDescriptor::classify_boundary({0,-127,0});
    const auto below=WorldDescriptor::classify_boundary({0,-129,0});
    const auto above=WorldDescriptor::classify_boundary({0,128,0});
    const auto edge=WorldDescriptor::classify_boundary({(int64_t{1}<<30)+1,0,0});
    assert(floor_a.kind==Kind::Stored && floor_a.block==BlockId::Bedrock && !floor_a.editable);
    assert(floor_b.kind==Kind::Stored && floor_b.block==BlockId::Bedrock && !floor_b.editable);
    assert(below.kind==Kind::VirtualBedrock && below.known && below.block==BlockId::Bedrock);
    assert(above.kind==Kind::AboveWorld && above.known && above.block==BlockId::Air && !above.editable);
    assert(edge.kind==Kind::OutsideOperationalRange && edge.known && !edge.editable);
    assert(WorldDescriptor::classify_boundary({0,-126,0}).kind==Kind::Stored);
}

int main() {
    coordinates();
    topology();
    catalog_and_domain();
    std::cout << "PASS: T01 coordinates, BCC topology, registry, and world bounds\n";
}
