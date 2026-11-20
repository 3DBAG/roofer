using System.Runtime.InteropServices;

// ReSharper disable InconsistentNaming
// naming is based on underlying C++ methods.

namespace RooferSharp;

/// <summary>
/// Native method declarations
/// </summary>
internal static class NativeMethods
{
    private const string LibName = "roofer";

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr roofer_config_create();

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_config_destroy(IntPtr handle);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_config_set_complexity_factor(IntPtr handle, float value);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_config_set_clip_ground(IntPtr handle, int value);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_config_set_lod(IntPtr handle, int value);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_config_set_lod13_step_height(IntPtr handle, float value);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_config_set_floor_elevation(IntPtr handle, float value);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_config_set_override_with_floor_elevation(IntPtr handle, int value);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_config_set_plane_detect_k(IntPtr handle, int value);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_config_set_plane_detect_min_points(IntPtr handle, int value);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_config_set_plane_detect_epsilon(IntPtr handle, float value);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_config_set_plane_detect_normal_angle(IntPtr handle, float value);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_config_set_line_detect_epsilon(IntPtr handle, float value);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_config_set_thres_alpha(IntPtr handle, float value);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_config_set_thres_reg_line_dist(IntPtr handle, float value);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_config_set_thres_reg_line_ext(IntPtr handle, float value);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int roofer_config_is_valid(IntPtr handle);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr roofer_reconstruct_with_ground(
        [In] float[] points_roof, UIntPtr points_roof_count,
        [In] float[] points_ground, UIntPtr points_ground_count,
        [In] float[] footprint_exterior, UIntPtr footprint_exterior_count,
        [In] float[] footprint_interiors, [In] UIntPtr[] interior_counts, UIntPtr interior_ring_count,
        IntPtr config);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr roofer_reconstruct(
        [In] float[] points_roof, UIntPtr points_roof_count,
        [In] float[] footprint_exterior, UIntPtr footprint_exterior_count,
        [In] float[] footprint_interiors, [In] UIntPtr[] interior_counts, UIntPtr interior_ring_count,
        IntPtr config);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern UIntPtr roofer_mesh_array_size(IntPtr handle);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr roofer_mesh_array_get(IntPtr handle, UIntPtr index);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_mesh_array_destroy(IntPtr handle);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern UIntPtr roofer_mesh_polygon_count(IntPtr handle);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern UIntPtr roofer_mesh_get_polygon_exterior_size(IntPtr handle, UIntPtr poly_index);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_mesh_get_polygon_exterior(IntPtr handle, UIntPtr poly_index, [Out] float[] points);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern UIntPtr roofer_mesh_get_polygon_interior_count(IntPtr handle, UIntPtr poly_index);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern UIntPtr roofer_mesh_get_polygon_interior_size(IntPtr handle, UIntPtr poly_index, UIntPtr interior_index);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_mesh_get_polygon_interior(IntPtr handle, UIntPtr poly_index, UIntPtr interior_index, [Out] float[] points);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int roofer_triangulate_mesh(
        IntPtr mesh_handle,
        out IntPtr out_vertices, out UIntPtr out_vertex_count,
        out IntPtr out_faces, out UIntPtr out_face_count);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern void roofer_free_triangulation(IntPtr vertices, IntPtr faces);
}