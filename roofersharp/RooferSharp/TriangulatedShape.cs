using System.Numerics;
using System.Runtime.InteropServices;

namespace RooferSharp;

/// <summary>
/// Triangulated mesh with vertices and faces
/// </summary>
public class TriangulatedShape : IDisposable
{
    public Vector3[] Vertices { get; private set; }
    public int[] Faces { get; private set; }

    private IntPtr _vertexPtr;
    private IntPtr _facePtr;
    private bool _disposed = false;

    internal TriangulatedShape(IntPtr vertexPtr, UIntPtr vertexCount, IntPtr facePtr, UIntPtr faceCount)
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

    ~TriangulatedShape()
    {
        Dispose(false);
    }
}