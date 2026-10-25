namespace RooferSharp;

/// <summary>
/// Polygon with optional interior rings (holes)
/// </summary>
public class Polygon
{
    public LinearRing Exterior { get; set; } = new LinearRing();
    public List<LinearRing> Interiors { get; set; } = new List<LinearRing>();
}