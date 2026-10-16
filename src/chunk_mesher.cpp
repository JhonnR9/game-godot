#include "chunk_mesher.h"
#include "surface_joiner.h"

namespace terrain {
MeshBuffers ChunkMesher::build(const VoxelChunk &chunk, bool connect_aligned_surfaces) const {
    MeshBuffers output;
    for (int y=0;y<VoxelChunk::SIZE;++y) {
        for (int z=0;z<VoxelChunk::SIZE;++z) {
            for (int x=0;x<VoxelChunk::SIZE;++x) {
                const Int3 cell{x,y,z};
                const Voxel value=chunk.get(cell);
                if (value==Voxel::Empty) { continue; }
                Color tint;
                switch (value) {
                    case Voxel::Grass: tint={0.23f,0.48f,0.105f,1}; break;
                    case Voxel::Dirt: tint={0.34f,0.20f,0.10f,1}; break;
                    default: tint={0.30f,0.33f,0.37f,1}; break;
                }
                // Axis-aligned square caps and their planar bridges must use
                // the same material color. Per-cell brightness here draws the
                // old voxel diamonds back onto the connected flat surface.
                const Int3 global=chunk.world_cell(cell);
                const int parity=OctahedralLattice::parity(global.y);
                // Render the selected chunk near the origin. World coordinates
                // select the data, never enter float positions (large-world safe).
                const Int3 center{4*x+2*parity-31, 2*y+2, 4*z+2*parity-31};
                for (int face=0;face<14;++face) {
                    // Only one chunk is displayed: preserve outer cut faces.
                    // Adjacent selected regions sample the same continuous world.
                    if (chunk.get(OctahedralLattice::neighbor(cell,face))!=Voxel::Empty) { continue; }
                    OctahedronGeometry::append_face(face,center,tint,value,output);
                }
            }
        }
    }
    return connect_aligned_surfaces ? SurfaceJoiner::join(chunk, output) : output;
}
} // namespace terrain
