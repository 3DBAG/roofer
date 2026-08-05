using System.Numerics;
using System.Text.Json.Serialization;

namespace RooferSharp.Test.Model;

public sealed class Vec3
{
    [JsonPropertyName("x")]
    public float X { get; set; }

    [JsonPropertyName("y")]
    public float Y { get; set; }

    [JsonPropertyName("z")]
    public float Z { get; set; }

    public Vec3() { } // for deserialization

    public Vec3(float x, float y, float z)
    {
        X = x; Y = y; Z = z;
    }
    
    public Vector3 ToVector3()
    {
        return new Vector3(X, Y, Z);
    }
}