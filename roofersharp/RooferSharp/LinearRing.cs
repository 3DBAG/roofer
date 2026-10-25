// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)
// C# P/Invoke wrapper for roofer library

using System.Numerics;

namespace RooferSharp
{

    /// <summary>
    /// Linear ring (exterior or interior ring of a polygon)
    /// </summary>
    public class LinearRing
    {
        public List<Vector3> Points { get; set; } = new List<Vector3>();
    }
}
