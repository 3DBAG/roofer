// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)
// C wrapper for roofer library to enable C# P/Invoke interop

#ifndef ROOFER_WRAPPER_H
#define ROOFER_WRAPPER_H

#ifdef _WIN32
#define ROOFER_EXPORT __declspec(dllexport)
#else
#define ROOFER_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle types
typedef void* RooferConfigHandle;
typedef void* RooferMeshHandle;
typedef void* RooferMeshArrayHandle;

// Configuration management
ROOFER_EXPORT RooferConfigHandle roofer_config_create();
ROOFER_EXPORT void roofer_config_destroy(RooferConfigHandle handle);
ROOFER_EXPORT void roofer_config_set_complexity_factor(RooferConfigHandle handle, float value);
ROOFER_EXPORT void roofer_config_set_clip_ground(RooferConfigHandle handle, int value);
ROOFER_EXPORT void roofer_config_set_lod(RooferConfigHandle handle, int value);
ROOFER_EXPORT void roofer_config_set_lod13_step_height(RooferConfigHandle handle, float value);
ROOFER_EXPORT void roofer_config_set_floor_elevation(RooferConfigHandle handle, float value);
ROOFER_EXPORT void roofer_config_set_override_with_floor_elevation(RooferConfigHandle handle, int value);
ROOFER_EXPORT void roofer_config_set_plane_detect_k(RooferConfigHandle handle, int value);
ROOFER_EXPORT void roofer_config_set_plane_detect_min_points(RooferConfigHandle handle, int value);
ROOFER_EXPORT void roofer_config_set_plane_detect_epsilon(RooferConfigHandle handle, float value);
ROOFER_EXPORT void roofer_config_set_plane_detect_normal_angle(RooferConfigHandle handle, float value);
ROOFER_EXPORT void roofer_config_set_line_detect_epsilon(RooferConfigHandle handle, float value);
ROOFER_EXPORT void roofer_config_set_thres_alpha(RooferConfigHandle handle, float value);
ROOFER_EXPORT void roofer_config_set_thres_reg_line_dist(RooferConfigHandle handle, float value);
ROOFER_EXPORT void roofer_config_set_thres_reg_line_ext(RooferConfigHandle handle, float value);
ROOFER_EXPORT int roofer_config_is_valid(RooferConfigHandle handle);

// Reconstruction functions
ROOFER_EXPORT RooferMeshArrayHandle roofer_reconstruct_with_ground(
    const float* points_roof, size_t points_roof_count,
    const float* points_ground, size_t points_ground_count,
    const float* footprint_exterior, size_t footprint_exterior_count,
    const float* footprint_interiors, const size_t* interior_counts, size_t interior_ring_count,
    RooferConfigHandle config);

ROOFER_EXPORT RooferMeshArrayHandle roofer_reconstruct(
    const float* points_roof, size_t points_roof_count,
    const float* footprint_exterior, size_t footprint_exterior_count,
    const float* footprint_interiors, const size_t* interior_counts, size_t interior_ring_count,
    RooferConfigHandle config);

// Mesh array functions
ROOFER_EXPORT size_t roofer_mesh_array_size(RooferMeshArrayHandle handle);
ROOFER_EXPORT RooferMeshHandle roofer_mesh_array_get(RooferMeshArrayHandle handle, size_t index);
ROOFER_EXPORT void roofer_mesh_array_destroy(RooferMeshArrayHandle handle);

// Mesh functions
ROOFER_EXPORT size_t roofer_mesh_polygon_count(RooferMeshHandle handle);
ROOFER_EXPORT size_t roofer_mesh_get_polygon_exterior_size(RooferMeshHandle handle, size_t poly_index);
ROOFER_EXPORT void roofer_mesh_get_polygon_exterior(RooferMeshHandle handle, size_t poly_index, float* points);
ROOFER_EXPORT size_t roofer_mesh_get_polygon_interior_count(RooferMeshHandle handle, size_t poly_index);
ROOFER_EXPORT size_t roofer_mesh_get_polygon_interior_size(RooferMeshHandle handle, size_t poly_index, size_t interior_index);
ROOFER_EXPORT void roofer_mesh_get_polygon_interior(RooferMeshHandle handle, size_t poly_index, size_t interior_index, float* points);

// Triangulation
ROOFER_EXPORT int roofer_triangulate_mesh(
    RooferMeshHandle mesh_handle,
    float** out_vertices, size_t* out_vertex_count,
    size_t** out_faces, size_t* out_face_count);
ROOFER_EXPORT void roofer_free_triangulation(float* vertices, size_t* faces);

#ifdef __cplusplus
}
#endif

#endif // ROOFER_WRAPPER_H
