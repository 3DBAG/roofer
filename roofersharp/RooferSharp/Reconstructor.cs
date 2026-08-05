using System.Drawing;
using System.Numerics;

namespace RooferSharp;

/// <summary>
/// Main Roofer API
/// </summary>
public static class Reconstructor
{
    /// <summary>
    /// Reconstructs a single instance of a building from a point cloud
    /// </summary>
    /// <param name="config">Reconstruction configuration</param>
    /// <param name="pointsRoof">List of points for the roof</param>
    /// <param name="pointsGround">List of points for the ground, also for interior curves</param>
    /// <param name="footprintExterior">Polyline representing the ground (should be ordered)</param>
    /// <param name="footprintInteriors">Polylines representing interior curves</param>
    /// <returns></returns>
    /// <exception cref="ArgumentException"></exception>
    
    public static List<Shape> ReconstructWithGround(
        ReconstructionConfig config,
        List<Vector3> pointsRoof,
        List<Vector3> pointsGround,
        List<Vector3> footprintExterior,
        List<List<Vector3>>? footprintInteriors = null
        )
    {
        if (pointsRoof == null || pointsRoof.Count == 0)
            throw new ArgumentException("Roof points cannot be null or empty", nameof(pointsRoof));
        if (pointsGround == null || pointsGround.Count == 0)
            throw new ArgumentException("Ground points cannot be null or empty", nameof(pointsGround));
        if (footprintExterior == null || footprintExterior.Count < 3)
            throw new ArgumentException("Footprint exterior must have at least 3 points", nameof(footprintExterior));

        // Flatten points
        var roofArray = FlattenPoints(pointsRoof);
        var groundArray = FlattenPoints(pointsGround);
        var footprintArray = FlattenPoints(footprintExterior);

        // Handle interior rings - pre-calculate total size
        float[]? interiorsArray = null;
        UIntPtr[]? interiorCounts = null;
        var interiorRingCount = UIntPtr.Zero;

        if (footprintInteriors != null && footprintInteriors.Count > 0)
        {
            // Pre-calculate total points
            var totalPoints = footprintInteriors.Sum(interior => interior.Count);

            interiorsArray = new float[totalPoints * 3];
            interiorCounts = new UIntPtr[footprintInteriors.Count];
                
            var offset = 0;
            for (var i = 0; i < footprintInteriors.Count; i++)
            {
                var interior = footprintInteriors[i];
                interiorCounts[i] = (UIntPtr)interior.Count;
                    
                foreach (var pt in interior)
                {
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
                footprintArray, (UIntPtr)footprintExterior.Count,
                interiorsArray ?? [], interiorCounts ?? [], interiorRingCount,
                nativeConfig);

            return ExtractShapes(meshArrayHandle);
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
    /// <param name="config">Reconstruction configuration</param>
    /// <param name="pointsRoof">List of points for the roof</param>
    /// <param name="footprintExterior">Polyline representing the ground (should be ordered)</param>
    /// <param name="footprintInteriors">Polylines representing interior curves (optional)</param>
    /// <returns></returns>
    /// <exception cref="ArgumentException"></exception>
    public static List<Shape> ReconstructWithoutGround(
        ReconstructionConfig config,
        List<Vector3> pointsRoof,
        List<Vector3> footprintExterior,
        List<List<Vector3>>? footprintInteriors = null)
    {
        if (pointsRoof == null || pointsRoof.Count == 0)
            throw new ArgumentException("Roof points cannot be null or empty", nameof(pointsRoof));
        if (footprintExterior == null || footprintExterior.Count < 3)
            throw new ArgumentException("Footprint exterior must have at least 3 points", nameof(footprintExterior));

        var roofArray = FlattenPoints(pointsRoof);
        var footprintArray = FlattenPoints(footprintExterior);

        // Handle interior rings - pre-calculate total size
        float[]? interiorsArray = null;
        UIntPtr[]? interiorCounts = null;
        var interiorRingCount = UIntPtr.Zero;

        if (footprintInteriors != null && footprintInteriors.Count > 0)
        {
            // Pre-calculate total points
            var totalPoints = footprintInteriors.Sum(interior => interior.Count);

            interiorsArray = new float[totalPoints * 3];
            interiorCounts = new UIntPtr[footprintInteriors.Count];
                
            var offset = 0;
            for (var i = 0; i < footprintInteriors.Count; i++)
            {
                var interior = footprintInteriors[i];
                interiorCounts[i] = (UIntPtr)interior.Count;
                    
                foreach (var pt in interior)
                {
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
                footprintArray, (UIntPtr)footprintExterior.Count,
                interiorsArray ?? [], interiorCounts ?? [], interiorRingCount,
                nativeConfig);

            return ExtractShapes(meshArrayHandle);
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
    public static TriangulatedShape TriangulateMesh(Shape shape)
    {
        if (shape == null)
            throw new ArgumentNullException(nameof(shape));

        var result = NativeMethods.roofer_triangulate_mesh(
            shape.Handle,
            out var vertices, out var vertexCount,
            out var faces, out var faceCount);

        return result == 0 ? throw new InvalidOperationException("Triangulation failed") 
            : new TriangulatedShape(vertices, vertexCount, faces, faceCount);
    }

    private static IntPtr CreateAndValidateNativeConfig(ReconstructionConfig config)
    {
        if (!config.IsValid)
        {
            throw new ArgumentException("Invalid reconstruction configuration. Please check parameter values.", nameof(config));
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
            NativeMethods.roofer_config_set_lod(nativeConfig, (int)config.Lod);
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
            if (NativeMethods.roofer_config_is_valid(nativeConfig) != 0) return nativeConfig;
            
            NativeMethods.roofer_config_destroy(nativeConfig);
            throw new ArgumentException("Invalid reconstruction configuration. Please check parameter values.", nameof(config));
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
        for (var i = 0; i < points.Count; i++)
        {
            var p = points[i];
            array[i * 3] = p.X;
            array[i * 3 + 1] = p.Y;
            array[i * 3 + 2] = p.Z;
        }
        return array;
    }

    private static List<Shape> ExtractShapes(IntPtr meshArrayHandle)
    {
        var count = (int)NativeMethods.roofer_mesh_array_size(meshArrayHandle);
        var meshes = new List<Shape>(count);

        for (var i = 0; i < count; i++)
        {
            var meshHandle = NativeMethods.roofer_mesh_array_get(meshArrayHandle, (UIntPtr)i);
            meshes.Add(new Shape(meshHandle));
        }

        NativeMethods.roofer_mesh_array_destroy(meshArrayHandle);
        return meshes;
    }
}