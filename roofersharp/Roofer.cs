// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)
// C# P/Invoke wrapper for roofer library

using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Numerics;

namespace Roofer
{

    /// <summary>
    /// Linear ring (exterior or interior ring of a polygon)
    /// </summary>
    public class LinearRing
    {
        public List<Vector3> Points { get; set; } = new List<Vector3>();
    }

    /// <summary>
    /// Polygon with optional interior rings (holes)
    /// </summary>
    public class Polygon
    {
        public LinearRing Exterior { get; set; } = new LinearRing();
        public List<LinearRing> Interiors { get; set; } = new List<LinearRing>();
    }

    /// <summary>
    /// Mesh consisting of multiple polygons
    /// </summary>
    public class Mesh : IDisposable
    {
        internal IntPtr Handle { get; set; }
        private bool _disposed = false;

        internal Mesh(IntPtr handle)
        {
            Handle = handle;
        }

        public List<Polygon> GetPolygons()
        {
            var polygons = new List<Polygon>();
            var count = (int)NativeMethods.roofer_mesh_polygon_count(Handle);

            for (int i = 0; i < count; i++)
            {
                var polygon = new Polygon();

                // Get exterior ring
                var exteriorSize = (int)NativeMethods.roofer_mesh_get_polygon_exterior_size(Handle, (UIntPtr)i);
                var exteriorPoints = new float[exteriorSize * 3];
                NativeMethods.roofer_mesh_get_polygon_exterior(Handle, (UIntPtr)i, exteriorPoints);

                polygon.Exterior.Points.Capacity = exteriorSize;
                for (int j = 0; j < exteriorSize; j++)
                {
                    polygon.Exterior.Points.Add(new Vector3(
                        exteriorPoints[j * 3],
                        exteriorPoints[j * 3 + 1],
                        exteriorPoints[j * 3 + 2]
                    ));
                }

                // Get interior rings
                var interiorCount = (int)NativeMethods.roofer_mesh_get_polygon_interior_count(Handle, (UIntPtr)i);
                for (int k = 0; k < interiorCount; k++)
                {
                    var interior = new LinearRing();
                    var interiorSize = (int)NativeMethods.roofer_mesh_get_polygon_interior_size(Handle, (UIntPtr)i, (UIntPtr)k);
                    var interiorPoints = new float[interiorSize * 3];
                    NativeMethods.roofer_mesh_get_polygon_interior(Handle, (UIntPtr)i, (UIntPtr)k, interiorPoints);

                    interior.Points.Capacity = interiorSize;
                    for (int j = 0; j < interiorSize; j++)
                    {
                        interior.Points.Add(new Vector3(
                            interiorPoints[j * 3],
                            interiorPoints[j * 3 + 1],
                            interiorPoints[j * 3 + 2]
                        ));
                    }
                    polygon.Interiors.Add(interior);
                }

                polygons.Add(polygon);
            }

            return polygons;
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposed)
            {
                // Note: Mesh handles from array are not separately freed
                _disposed = true;
            }
        }

        ~Mesh()
        {
            Dispose(false);
        }
    }

    /// <summary>
    /// Triangulated mesh with vertices and faces
    /// </summary>
    public class TriangulatedMesh : IDisposable
    {
        public Vector3[] Vertices { get; private set; }
        public int[] Faces { get; private set; }

        private IntPtr _vertexPtr;
        private IntPtr _facePtr;
        private bool _disposed = false;

        internal TriangulatedMesh(IntPtr vertexPtr, UIntPtr vertexCount, IntPtr facePtr, UIntPtr faceCount)
        {
            _vertexPtr = vertexPtr;
            _facePtr = facePtr;

            var vCount = (int)vertexCount;
            var fCount = (int)faceCount;

            // Copy vertices
            Vertices = new Vector3[vCount];
            var vertexArray = new float[vCount * 3];
            Marshal.Copy(_vertexPtr, vertexArray, 0, vCount * 3);

            for (int i = 0; i < vCount; i++)
            {
                Vertices[i] = new Vector3(
                    vertexArray[i * 3],
                    vertexArray[i * 3 + 1],
                    vertexArray[i * 3 + 2]
                );
            }

            // Copy faces (convert size_t to int)
            Faces = new int[fCount * 3];
            if (UIntPtr.Size == 8) // 64-bit
            {
                var faceArray = new long[fCount * 3];
                Marshal.Copy(_facePtr, faceArray, 0, fCount * 3);
                for (int i = 0; i < faceArray.Length; i++)
                {
                    Faces[i] = (int)faceArray[i];
                }
            }
            else // 32-bit
            {
                Marshal.Copy(_facePtr, Faces, 0, fCount * 3);
            }
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposed)
            {
                NativeMethods.roofer_free_triangulation(_vertexPtr, _facePtr);
                _disposed = true;
            }
        }

        ~TriangulatedMesh()
        {
            Dispose(false);
        }
    }

    /// <summary>
    /// Configuration for reconstruction
    /// </summary>
    public class ReconstructionConfig
    {
        public float ComplexityFactor { get; set; }
        public bool ClipGround { get; set; }
        public int Lod { get; set; }
        public float Lod13StepHeight { get; set; }
        public float FloorElevation { get; set; }
        public bool OverrideWithFloorElevation { get; set; }
        public int PlaneDetectK { get; set; }
        public int PlaneDetectMinPoints { get; set; }
        public float PlaneDetectEpsilon { get; set; }
        public float PlaneDetectNormalAngle { get; set; }
        public float LineDetectEpsilon { get; set; }
        public float ThresAlpha { get; set; }
        public float ThresRegLineDist { get; set; }
        public float ThresRegLineExt { get; set; }
    }

    /// <summary>
    /// Main Roofer API
    /// </summary>
    public static class Reconstructor
    {
        /// <summary>
        /// Reconstruct building from point cloud with ground points
        /// </summary>
        public static List<Mesh> Reconstruct(
            List<Vector3> pointsRoof,
            List<Vector3> pointsGround,
            LinearRing footprintExterior,
            List<LinearRing> footprintInteriors = null,
            ReconstructionConfig config = null)
        {
            if (pointsRoof == null || pointsRoof.Count == 0)
                throw new ArgumentException("Roof points cannot be null or empty", nameof(pointsRoof));
            if (pointsGround == null || pointsGround.Count == 0)
                throw new ArgumentException("Ground points cannot be null or empty", nameof(pointsGround));
            if (footprintExterior == null || footprintExterior.Points.Count < 3)
                throw new ArgumentException("Footprint exterior must have at least 3 points", nameof(footprintExterior));

            // Flatten points
            var roofArray = FlattenPoints(pointsRoof);
            var groundArray = FlattenPoints(pointsGround);
            var footprintArray = FlattenPoints(footprintExterior.Points);

            // Handle interior rings - pre-calculate total size
            float[] interiorsArray = null;
            UIntPtr[] interiorCounts = null;
            UIntPtr interiorRingCount = UIntPtr.Zero;

            if (footprintInteriors != null && footprintInteriors.Count > 0)
            {
                // Pre-calculate total points
                int totalPoints = 0;
                foreach (var interior in footprintInteriors)
                {
                    totalPoints += interior.Points.Count;
                }

                interiorsArray = new float[totalPoints * 3];
                interiorCounts = new UIntPtr[footprintInteriors.Count];
                
                int offset = 0;
                for (int i = 0; i < footprintInteriors.Count; i++)
                {
                    var interior = footprintInteriors[i];
                    interiorCounts[i] = (UIntPtr)interior.Points.Count;
                    
                    for (int j = 0; j < interior.Points.Count; j++)
                    {
                        var pt = interior.Points[j];
                        interiorsArray[offset++] = pt.X;
                        interiorsArray[offset++] = pt.Y;
                        interiorsArray[offset++] = pt.Z;
                    }
                }
                
                interiorRingCount = (UIntPtr)footprintInteriors.Count;
            }

            // Create and populate native config
            var nativeConfig = CreateAndValidateNativeConfig(config);
            
            try
            {
                var meshArrayHandle = NativeMethods.roofer_reconstruct_with_ground(
                    roofArray, (UIntPtr)pointsRoof.Count,
                    groundArray, (UIntPtr)pointsGround.Count,
                    footprintArray, (UIntPtr)footprintExterior.Points.Count,
                    interiorsArray, interiorCounts, interiorRingCount,
                    nativeConfig);

                return ExtractMeshes(meshArrayHandle);
            }
            finally
            {
                if (nativeConfig != IntPtr.Zero)
                {
                    NativeMethods.roofer_config_destroy(nativeConfig);
                }
            }
        }

        /// <summary>
        /// Reconstruct building from point cloud without ground points
        /// </summary>
        public static List<Mesh> Reconstruct(
            List<Vector3> pointsRoof,
            LinearRing footprintExterior,
            List<LinearRing> footprintInteriors = null,
            ReconstructionConfig config = null)
        {
            if (pointsRoof == null || pointsRoof.Count == 0)
                throw new ArgumentException("Roof points cannot be null or empty", nameof(pointsRoof));
            if (footprintExterior == null || footprintExterior.Points.Count < 3)
                throw new ArgumentException("Footprint exterior must have at least 3 points", nameof(footprintExterior));

            var roofArray = FlattenPoints(pointsRoof);
            var footprintArray = FlattenPoints(footprintExterior.Points);

            // Handle interior rings - pre-calculate total size
            float[] interiorsArray = null;
            UIntPtr[] interiorCounts = null;
            UIntPtr interiorRingCount = UIntPtr.Zero;

            if (footprintInteriors != null && footprintInteriors.Count > 0)
            {
                // Pre-calculate total points
                int totalPoints = 0;
                foreach (var interior in footprintInteriors)
                {
                    totalPoints += interior.Points.Count;
                }

                interiorsArray = new float[totalPoints * 3];
                interiorCounts = new UIntPtr[footprintInteriors.Count];
                
                int offset = 0;
                for (int i = 0; i < footprintInteriors.Count; i++)
                {
                    var interior = footprintInteriors[i];
                    interiorCounts[i] = (UIntPtr)interior.Points.Count;
                    
                    for (int j = 0; j < interior.Points.Count; j++)
                    {
                        var pt = interior.Points[j];
                        interiorsArray[offset++] = pt.X;
                        interiorsArray[offset++] = pt.Y;
                        interiorsArray[offset++] = pt.Z;
                    }
                }
                
                interiorRingCount = (UIntPtr)footprintInteriors.Count;
            }

            // Create and populate native config
            var nativeConfig = CreateAndValidateNativeConfig(config);
            
            try
            {
                var meshArrayHandle = NativeMethods.roofer_reconstruct(
                    roofArray, (UIntPtr)pointsRoof.Count,
                    footprintArray, (UIntPtr)footprintExterior.Points.Count,
                    interiorsArray, interiorCounts, interiorRingCount,
                    nativeConfig);

                return ExtractMeshes(meshArrayHandle);
            }
            finally
            {
                if (nativeConfig != IntPtr.Zero)
                {
                    NativeMethods.roofer_config_destroy(nativeConfig);
                }
            }
        }

        /// <summary>
        /// Triangulate a mesh
        /// </summary>
        public static TriangulatedMesh TriangulateMesh(Mesh mesh)
        {
            if (mesh == null)
                throw new ArgumentNullException(nameof(mesh));

            IntPtr vertices;
            UIntPtr vertexCount;
            IntPtr faces;
            UIntPtr faceCount;

            var result = NativeMethods.roofer_triangulate_mesh(
                mesh.Handle,
                out vertices, out vertexCount,
                out faces, out faceCount);

            if (result == 0)
            {
                throw new InvalidOperationException("Triangulation failed");
            }

            return new TriangulatedMesh(vertices, vertexCount, faces, faceCount);
        }

        private static IntPtr CreateAndValidateNativeConfig(ReconstructionConfig config)
        {
            if (config == null)
            {
                return IntPtr.Zero; // Use default config
            }

            var nativeConfig = NativeMethods.roofer_config_create();
            if (nativeConfig == IntPtr.Zero)
            {
                throw new InvalidOperationException("Failed to create native configuration");
            }

            try
            {
                // Set all config values
                NativeMethods.roofer_config_set_complexity_factor(nativeConfig, config.ComplexityFactor);
                NativeMethods.roofer_config_set_clip_ground(nativeConfig, config.ClipGround ? 1 : 0);
                NativeMethods.roofer_config_set_lod(nativeConfig, config.Lod);
                NativeMethods.roofer_config_set_lod13_step_height(nativeConfig, config.Lod13StepHeight);
                NativeMethods.roofer_config_set_floor_elevation(nativeConfig, config.FloorElevation);
                NativeMethods.roofer_config_set_override_with_floor_elevation(nativeConfig, config.OverrideWithFloorElevation ? 1 : 0);
                NativeMethods.roofer_config_set_plane_detect_k(nativeConfig, config.PlaneDetectK);
                NativeMethods.roofer_config_set_plane_detect_min_points(nativeConfig, config.PlaneDetectMinPoints);
                NativeMethods.roofer_config_set_plane_detect_epsilon(nativeConfig, config.PlaneDetectEpsilon);
                NativeMethods.roofer_config_set_plane_detect_normal_angle(nativeConfig, config.PlaneDetectNormalAngle);
                NativeMethods.roofer_config_set_line_detect_epsilon(nativeConfig, config.LineDetectEpsilon);
                NativeMethods.roofer_config_set_thres_alpha(nativeConfig, config.ThresAlpha);
                NativeMethods.roofer_config_set_thres_reg_line_dist(nativeConfig, config.ThresRegLineDist);
                NativeMethods.roofer_config_set_thres_reg_line_ext(nativeConfig, config.ThresRegLineExt);

                // Validate the config
                if (NativeMethods.roofer_config_is_valid(nativeConfig) == 0)
                {
                    NativeMethods.roofer_config_destroy(nativeConfig);
                    throw new ArgumentException("Invalid reconstruction configuration. Please check parameter values.", nameof(config));
                }

                return nativeConfig;
            }
            catch
            {
                // Cleanup on error
                if (nativeConfig != IntPtr.Zero)
                {
                    NativeMethods.roofer_config_destroy(nativeConfig);
                }
                throw;
            }
        }

        private static float[] FlattenPoints(List<Vector3> points)
        {
            var array = new float[points.Count * 3];
            for (int i = 0; i < points.Count; i++)
            {
                var p = points[i];
                array[i * 3] = p.X;
                array[i * 3 + 1] = p.Y;
                array[i * 3 + 2] = p.Z;
            }
            return array;
        }

        private static List<Mesh> ExtractMeshes(IntPtr meshArrayHandle)
        {
            var count = (int)NativeMethods.roofer_mesh_array_size(meshArrayHandle);
            var meshes = new List<Mesh>(count);

            for (int i = 0; i < count; i++)
            {
                var meshHandle = NativeMethods.roofer_mesh_array_get(meshArrayHandle, (UIntPtr)i);
                meshes.Add(new Mesh(meshHandle));
            }

            NativeMethods.roofer_mesh_array_destroy(meshArrayHandle);
            return meshes;
        }
    }

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
}
