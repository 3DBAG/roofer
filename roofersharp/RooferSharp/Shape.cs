using System.Numerics;

namespace RooferSharp;

/// <summary>
/// Mesh consisting of multiple polygons
/// </summary>
public class Shape : IDisposable
{
    internal IntPtr Handle { get; set; }
    private bool _disposed = false;

    internal Shape(IntPtr handle)
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

    ~Shape()
    {
        Dispose(false);
    }
}