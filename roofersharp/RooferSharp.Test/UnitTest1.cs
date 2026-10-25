using System.Numerics;
using RooferSharp.Test.Model;

namespace RooferSharp.Test;

public class Tests
{
    [SetUp]
    public void Setup()
    {
        
        
    }

    [Test]
    public void Test1()
    {
        var config = new ReconstructionConfig();
        config.ClipGround = false;
        // example file containing roof points and ground points
        var file = File.ReadAllText("testdata/slantedroof.json");
        var payload = System.Text.Json.JsonSerializer.Deserialize<GeometryPayload>(file);

        Assert.That(payload != null);
        var result = RooferSharp.Reconstructor.ReconstructWithGround(config, 
            payload!.PointsRoof.ToVector3List(), 
            payload.PointsGround.ToVector3List(), 
            payload.FootprintExterior.ToVector3List());
        var data = result[0];
        var polygons = data.GetPolygons();
        Assert.That(polygons.Count, Is.GreaterThan(0));
    }
}