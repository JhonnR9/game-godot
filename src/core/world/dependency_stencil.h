#pragma once

#include "world_repository.h"
#include <set>

namespace terrain::core {

struct BridgeAnchorOffset { Int3 cell_offset; uint8_t direction = 0; };

// Conservative cell dependencies derived from the six bridge frames and the
// actual bridge/cell bounds. Parity is explicit because odd BCC layers shift.
class DependencyStencil {
public:
    static std::vector<Int3> cell_offsets(int y_parity);
    static std::vector<BridgeAnchorOffset> bridge_anchor_offsets(int y_parity);
    static std::vector<ChunkKey> chunk_keys_for(ChunkKey target);
};

} // namespace terrain::core
