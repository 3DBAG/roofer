using System.Numerics;

namespace RooferSharp.Test.Model;

public static class TestPayloadExtensions
{
    public static List<Vector3> ToVector3List(this List<Vec3> vec3List)
    {
        return vec3List.Select(v => new Vector3(v.X, v.Y, v.Z)).ToList();
    }
}