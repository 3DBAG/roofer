using System.Text.Json.Serialization;

namespace RooferSharp.Test.Model;

public sealed class GeometryPayload
{
    [JsonPropertyName("pointsRoof")]
    public List<Vec3> PointsRoof { get; set; }

    [JsonPropertyName("pointsGround")]
    public List<Vec3> PointsGround { get; set; }

    [JsonPropertyName("footprintExterior")]
    public List<Vec3> FootprintExterior { get; set; }

    [JsonPropertyName("footprintInteriors")]
    public List<List<Vec3>> FootprintInteriors { get; set; } // null => omitted if you want

    public GeometryPayload()
    {
        PointsRoof = new List<Vec3>();
        PointsGround = new List<Vec3>();
        FootprintExterior = new List<Vec3>();
        FootprintInteriors = null; // keep optional
    }
}