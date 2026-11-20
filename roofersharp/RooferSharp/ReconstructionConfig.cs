namespace RooferSharp;

/// <summary>
/// Configuration for reconstruction
/// </summary>
public class ReconstructionConfig
{
    /**
    * Complexity factor for building model geometry.
     *
     * A number between `0.0` and `1.0`. Higher values lead to
     * more detailed building models, lower values to simpler models.
     */
    public float ComplexityFactor { get; set; } = 0.888f;

    /**
     * Set to true to activate the procedure that
     * clips parts from the input footprint wherever patches of ground points
     * are detected. May cause irregular outlines in reconstruction result.
     */
    public bool ClipGround { get; set; } = true;

    /**
     * - 12: LoD 1.2
     * - 13: LoD 1.3
     * - 22: LoD 2.2
     */
    public LevelOfDetail Lod { get; set; } = LevelOfDetail.Lod22;

    /**
     * Step height used for LoD13 generalisation, i.e. roofparts with a
     * height discontinuity that is smaller than this value are merged. Only
     * affects LoD 1.3 reconstructions. Unit: meters.
     */
    public float Lod13StepHeight { get; set; } = 3f;

    public float FloorElevation { get; set; } = 0f;

    /**
     * @brief Force flat floor instead of using the
     * elevation of the footprint (API only).
     */
    public bool OverrideWithFloorElevation { get; set; } = false;

    /**
     * @brief Number of points used in nearest neighbour queries
     * during plane detection.
     */
    public int PlaneDetectK { get; set; } = 15;

    /**
     * Minimum number of points required to
     * detect a plane.
     */
    public int PlaneDetectMinPoints { get; set; } = 15;

    /**
     * @brief Maximum allowed angle between points inside the same
     * detected plane. This value is compared to the dot product between two
     * unit normals. Eg. 0 means 90 degrees (orthogonal normals), and 1.0 means
     * 0 degrees (parallel normals)
     */
    public float PlaneDetectEpsilon { get; set; } = 0.3f;
    /**
     * @brief Maximum distance from candidate points to line during line
     * fitting procedure. Higher values offer more robustness against irregular
     * lines, lower values give more accurate lines (ie. more closely wrapping
     * around point cloud). Unit: meters.
     */
    public float PlaneDetectNormalAngle { get; set; } =0.75f;

    /**
     * @brief Distance used in computing alpha-shape of detected plane
     * segments prior to line detection. Higher values offer more robustness
     * against irregular lines, lower values give more accurate lines (ie. more
     * closely wrapping around point cloud). Unit: meters.
     */
    public float LineDetectEpsilon { get; set; } = 1.0f;
    /**
     * @brief Distance used in computing alpha-shape of detected plane
     * segments prior to line detection. Higher values offer more robustness
     * against irregular lines, lower values give more accurate lines (ie. more
     * closely wrapping around point cloud). Unit: meters.
     */
    public float ThresAlpha { get; set; } = 0.25f;

    /**
     * @brief Maximum distance to merge lines during line regularisation
     * (after line detection). Approximately parallel lines that are closer to
     * each other than this threshold are merged. Higher values yield more
     * regularisation, lower values preserve more finer details. Unit: meters.
     */
    public float ThresRegLineDist { get; set; } = 3.0f;

    /**
     * @brief Extension of regularised lines prior to optimisation. Used
     * to compensate for undetected parts in the roofpart boundaries. Use higher
     * values when the input pointcloud is less dense. However, setting this too
     * high can lead to unrealistic reconstruction results. Unit: meters.
     */
    public float ThresRegLineExt { get; set; } = 3.0f;

    public bool IsValid => (ComplexityFactor >= 0 && ComplexityFactor <= 1.0) && Lod13StepHeight > 0;
}

public enum LevelOfDetail
{
    Lod12 = 12,
    Lod13 = 13,
    Lod22 = 22
}