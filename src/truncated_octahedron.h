#pragma once

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/variant/color.hpp>

namespace godot {
class TruncatedOctahedron : public MeshInstance3D {
    GDCLASS(TruncatedOctahedron, MeshInstance3D)

    double edge_length = 1.4;
    Color color = Color(0.025, 0.36, 0.83);

protected:
    static void _bind_methods();

public:
    void _ready() override;
    void rebuild_mesh();
    void set_edge_length(double p_length);
    double get_edge_length() const;
    void set_color(const Color &p_color);
    Color get_color() const;
};
} // namespace godot
