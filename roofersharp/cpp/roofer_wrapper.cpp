// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)
// C wrapper implementation for roofer library

#include "roofer_wrapper.h"
#include <roofer/roofer.h>
#include <vector>
#include <map>

// Internal wrapper structures
struct RooferConfig {
    roofer::ReconstructionConfig config;
};

struct RooferMesh {
    roofer::Mesh mesh;
};

struct RooferMeshArray {
    std::vector<roofer::Mesh> meshes;
};

// Helper function to convert flat array to LinearRing
static void convert_to_linear_ring(const float* points, size_t point_count, roofer::LinearRing& ring) {
    for (size_t i = 0; i < point_count; ++i) {
        ring.push_back({points[i * 3], points[i * 3 + 1], points[i * 3 + 2]});
    }
}

// Configuration management
extern "C" {

RooferConfigHandle roofer_config_create() {
    return new RooferConfig();
}

void roofer_config_destroy(RooferConfigHandle handle) {
    delete static_cast<RooferConfig*>(handle);
}

void roofer_config_set_complexity_factor(RooferConfigHandle handle, float value) {
    static_cast<RooferConfig*>(handle)->config.complexity_factor = value;
}

void roofer_config_set_clip_ground(RooferConfigHandle handle, int value) {
    static_cast<RooferConfig*>(handle)->config.clip_ground = value != 0;
}

void roofer_config_set_lod(RooferConfigHandle handle, int value) {
    static_cast<RooferConfig*>(handle)->config.lod = value;
}

void roofer_config_set_lod13_step_height(RooferConfigHandle handle, float value) {
    static_cast<RooferConfig*>(handle)->config.lod13_step_height = value;
}

void roofer_config_set_floor_elevation(RooferConfigHandle handle, float value) {
    static_cast<RooferConfig*>(handle)->config.floor_elevation = value;
}

void roofer_config_set_override_with_floor_elevation(RooferConfigHandle handle, int value) {
    static_cast<RooferConfig*>(handle)->config.override_with_floor_elevation = value != 0;
}

void roofer_config_set_plane_detect_k(RooferConfigHandle handle, int value) {
    static_cast<RooferConfig*>(handle)->config.plane_detect_k = value;
}

void roofer_config_set_plane_detect_min_points(RooferConfigHandle handle, int value) {
    static_cast<RooferConfig*>(handle)->config.plane_detect_min_points = value;
}

void roofer_config_set_plane_detect_epsilon(RooferConfigHandle handle, float value) {
    static_cast<RooferConfig*>(handle)->config.plane_detect_epsilon = value;
}

void roofer_config_set_plane_detect_normal_angle(RooferConfigHandle handle, float value) {
    static_cast<RooferConfig*>(handle)->config.plane_detect_normal_angle = value;
}

void roofer_config_set_line_detect_epsilon(RooferConfigHandle handle, float value) {
    static_cast<RooferConfig*>(handle)->config.line_detect_epsilon = value;
}

void roofer_config_set_thres_alpha(RooferConfigHandle handle, float value) {
    static_cast<RooferConfig*>(handle)->config.thres_alpha = value;
}

void roofer_config_set_thres_reg_line_dist(RooferConfigHandle handle, float value) {
    static_cast<RooferConfig*>(handle)->config.thres_reg_line_dist = value;
}

void roofer_config_set_thres_reg_line_ext(RooferConfigHandle handle, float value) {
    static_cast<RooferConfig*>(handle)->config.thres_reg_line_ext = value;
}

int roofer_config_is_valid(RooferConfigHandle handle) {
    return static_cast<RooferConfig*>(handle)->config.is_valid() ? 1 : 0;
}

// Reconstruction functions
RooferMeshArrayHandle roofer_reconstruct_with_ground(
    const float* points_roof, size_t points_roof_count,
    const float* points_ground, size_t points_ground_count,
    const float* footprint_exterior, size_t footprint_exterior_count,
    const float* footprint_interiors, const size_t* interior_counts, size_t interior_ring_count,
    RooferConfigHandle config) {
    
    // Convert roof points
    roofer::PointCollection points_roof_pc;
    for (size_t i = 0; i < points_roof_count; ++i) {
        points_roof_pc.push_back({points_roof[i * 3], points_roof[i * 3 + 1], points_roof[i * 3 + 2]});
    }
    
    // Convert ground points
    roofer::PointCollection points_ground_pc;
    for (size_t i = 0; i < points_ground_count; ++i) {
        points_ground_pc.push_back({points_ground[i * 3], points_ground[i * 3 + 1], points_ground[i * 3 + 2]});
    }
    
    // Convert footprint
    roofer::LinearRing linear_ring;
    convert_to_linear_ring(footprint_exterior, footprint_exterior_count, linear_ring);
    
    // Add interior rings
    size_t offset = 0;
    for (size_t i = 0; i < interior_ring_count; ++i) {
        roofer::LinearRing interior_ring;
        convert_to_linear_ring(footprint_interiors + offset * 3, interior_counts[i], interior_ring);
        linear_ring.interior_rings().push_back(interior_ring);
        offset += interior_counts[i];
    }
    
    // Get config
    roofer::ReconstructionConfig cfg;
    if (config) {
        cfg = static_cast<RooferConfig*>(config)->config;
    }
    
    // Reconstruct
    auto meshes = roofer::reconstruct(points_roof_pc, points_ground_pc, linear_ring, cfg);
    
    // Create result
    auto* result = new RooferMeshArray();
    result->meshes = std::move(meshes);
    return result;
}

RooferMeshArrayHandle roofer_reconstruct(
    const float* points_roof, size_t points_roof_count,
    const float* footprint_exterior, size_t footprint_exterior_count,
    const float* footprint_interiors, const size_t* interior_counts, size_t interior_ring_count,
    RooferConfigHandle config) {
    
    // Convert roof points
    roofer::PointCollection points_roof_pc;
    for (size_t i = 0; i < points_roof_count; ++i) {
        points_roof_pc.push_back({points_roof[i * 3], points_roof[i * 3 + 1], points_roof[i * 3 + 2]});
    }
    
    // Convert footprint
    roofer::LinearRing linear_ring;
    convert_to_linear_ring(footprint_exterior, footprint_exterior_count, linear_ring);
    
    // Add interior rings
    size_t offset = 0;
    for (size_t i = 0; i < interior_ring_count; ++i) {
        roofer::LinearRing interior_ring;
        convert_to_linear_ring(footprint_interiors + offset * 3, interior_counts[i], interior_ring);
        linear_ring.interior_rings().push_back(interior_ring);
        offset += interior_counts[i];
    }
    
    // Get config
    roofer::ReconstructionConfig cfg;
    if (config) {
        cfg = static_cast<RooferConfig*>(config)->config;
    }
    
    // Reconstruct
    auto meshes = roofer::reconstruct(points_roof_pc, linear_ring, cfg);
    
    // Create result
    auto* result = new RooferMeshArray();
    result->meshes = std::move(meshes);
    return result;
}

// Mesh array functions
size_t roofer_mesh_array_size(RooferMeshArrayHandle handle) {
    return static_cast<RooferMeshArray*>(handle)->meshes.size();
}

RooferMeshHandle roofer_mesh_array_get(RooferMeshArrayHandle handle, size_t index) {
    auto* array = static_cast<RooferMeshArray*>(handle);
    if (index >= array->meshes.size()) return nullptr;
    
    auto* mesh = new RooferMesh();
    mesh->mesh = array->meshes[index];
    return mesh;
}

void roofer_mesh_array_destroy(RooferMeshArrayHandle handle) {
    delete static_cast<RooferMeshArray*>(handle);
}

// Mesh functions
size_t roofer_mesh_polygon_count(RooferMeshHandle handle) {
    return static_cast<RooferMesh*>(handle)->mesh.get_polygons().size();
}

size_t roofer_mesh_get_polygon_exterior_size(RooferMeshHandle handle, size_t poly_index) {
    auto& polygons = static_cast<RooferMesh*>(handle)->mesh.get_polygons();
    if (poly_index >= polygons.size()) return 0;
    return polygons[poly_index].size();
}

void roofer_mesh_get_polygon_exterior(RooferMeshHandle handle, size_t poly_index, float* points) {
    auto& polygons = static_cast<RooferMesh*>(handle)->mesh.get_polygons();
    if (poly_index >= polygons.size()) return;
    
    const auto& poly = polygons[poly_index];
    for (size_t i = 0; i < poly.size(); ++i) {
        points[i * 3] = poly[i][0];
        points[i * 3 + 1] = poly[i][1];
        points[i * 3 + 2] = poly[i][2];
    }
}

size_t roofer_mesh_get_polygon_interior_count(RooferMeshHandle handle, size_t poly_index) {
    auto& polygons = static_cast<RooferMesh*>(handle)->mesh.get_polygons();
    if (poly_index >= polygons.size()) return 0;
    return polygons[poly_index].interior_rings().size();
}

size_t roofer_mesh_get_polygon_interior_size(RooferMeshHandle handle, size_t poly_index, size_t interior_index) {
    auto& polygons = static_cast<RooferMesh*>(handle)->mesh.get_polygons();
    if (poly_index >= polygons.size()) return 0;
    
    const auto& interiors = polygons[poly_index].interior_rings();
    if (interior_index >= interiors.size()) return 0;
    return interiors[interior_index].size();
}

void roofer_mesh_get_polygon_interior(RooferMeshHandle handle, size_t poly_index, size_t interior_index, float* points) {
    auto& polygons = static_cast<RooferMesh*>(handle)->mesh.get_polygons();
    if (poly_index >= polygons.size()) return;
    
    const auto& interiors = polygons[poly_index].interior_rings();
    if (interior_index >= interiors.size()) return;
    
    const auto& interior = interiors[interior_index];
    for (size_t i = 0; i < interior.size(); ++i) {
        points[i * 3] = interior[i][0];
        points[i * 3 + 1] = interior[i][1];
        points[i * 3 + 2] = interior[i][2];
    }
}

// Triangulation
int roofer_triangulate_mesh(
    RooferMeshHandle mesh_handle,
    float** out_vertices, size_t* out_vertex_count,
    size_t** out_faces, size_t* out_face_count) {
    
    auto* mesh_wrapper = static_cast<RooferMesh*>(mesh_handle);
    auto tri_mesh = roofer::triangulate_mesh(mesh_wrapper->mesh);
    
    // Build vertex map
    std::map<roofer::arr3f, size_t> vertex_map;
    std::vector<roofer::arr3f> vertices;
    std::vector<std::array<size_t, 3>> faces;
    
    for (const auto& triangle : tri_mesh) {
        for (const auto& vertex : triangle) {
            if (vertex_map.find(vertex) == vertex_map.end()) {
                vertex_map[vertex] = vertex_map.size();
                vertices.push_back(vertex);
            }
        }
        faces.push_back({vertex_map[triangle[0]], vertex_map[triangle[1]], vertex_map[triangle[2]]});
    }
    
    // Allocate output arrays
    *out_vertex_count = vertices.size();
    *out_vertices = new float[vertices.size() * 3];
    for (size_t i = 0; i < vertices.size(); ++i) {
        (*out_vertices)[i * 3] = vertices[i][0];
        (*out_vertices)[i * 3 + 1] = vertices[i][1];
        (*out_vertices)[i * 3 + 2] = vertices[i][2];
    }
    
    *out_face_count = faces.size();
    *out_faces = new size_t[faces.size() * 3];
    for (size_t i = 0; i < faces.size(); ++i) {
        (*out_faces)[i * 3] = faces[i][0];
        (*out_faces)[i * 3 + 1] = faces[i][1];
        (*out_faces)[i * 3 + 2] = faces[i][2];
    }
    
    return 1;
}

void roofer_free_triangulation(float* vertices, size_t* faces) {
    delete[] vertices;
    delete[] faces;
}

} // extern "C"
