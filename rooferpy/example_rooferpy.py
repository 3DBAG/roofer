import rooferpy
import laspy
import numpy as np
from shapely import wkt
import rerun as rr

def apply_offset(points, x_offset, y_offset):
    for point in points:
        point[0] += x_offset
        point[1] += y_offset
    return points

def read_las_from_file(file_path, x_offset=0, y_offset=0):
    with laspy.open(file_path) as las:
        # Read the points
        points = las.read()

        # Access point data
        x = points['x']
        y = points['y']
        z = points['z']
        classification = points['classification']

    # Separate building and ground points based on classification
    building_mask = classification == 6
    ground_mask = classification == 2

    building_points = np.column_stack((x[building_mask], y[building_mask], z[building_mask]))
    ground_points = np.column_stack((x[ground_mask], y[ground_mask], z[ground_mask]))

    # Apply offset
    building_points = apply_offset(building_points, x_offset, y_offset)
    ground_points = apply_offset(ground_points, x_offset, y_offset)

    return building_points, ground_points

def read_wkt_from_file(file_path):
    with open(file_path, 'r') as file:
        wkt_str = file.read().strip()
    return wkt_str

def wkt_polygon_to_rings(wkt_str, x_offset=0, y_offset=0):
    # Parse WKT string to a shapely geometry object
    geom = wkt.loads(wkt_str)
    
    # Ensure the geometry is a Polygon
    if geom.geom_type != 'Polygon':
        raise ValueError("The WKT geometry is not a Polygon, it is a {0}".format(geom.geom_type))
    
    # Function to convert coordinates to list of lists of 3-element arrays
    def coords_to_ring(coords):
        return [[float(coord[0]) + x_offset, float(coord[1]) + y_offset, float(coord[2]) if len(coord) == 3 else 0.0] for coord in coords]
    
    # Extract exterior ring
    exterior_ring = coords_to_ring(geom.exterior.coords)
    
    # Extract interior rings (if any)
    interior_rings = [coords_to_ring(interior.coords) for interior in geom.interiors]
    
    # Combine exterior and interior rings into a single list
    all_rings = [exterior_ring] + interior_rings
    
    return all_rings

# Define offsets to avoid truncation errors
x_offset = -85205.20
y_offset = -446846

# Load building points and ground points
print("Reading .LAZ...")
building_pts, ground_pts = read_las_from_file('./example_data/input.laz', x_offset, y_offset)

print("Reading the WKT polygon...")
# Load polygon points
footprint_str = read_wkt_from_file('./example_data/input.txt')
footprint = wkt_polygon_to_rings(footprint_str, x_offset, y_offset)

# Set the reconstruction configuration
roofer_config = rooferpy.ReconstructionConfig()
roofer_config.complexity_factor = 0.7

# Reconstruct
print("Reconstructing building...")
roofer_meshes = rooferpy.reconstruct_single_instance(building_pts, ground_pts, footprint, roofer_config)

print("Triangulating mesh")
vertices, faces = rooferpy.triangulate_mesh(roofer_meshes[0])

# Show the results in rerun
rr.init("Reconstruction results", spawn=True)
rr.connect()  # Connect to a remote viewer
rr.log("mesh faces", rr.Mesh3D(vertex_positions=vertices,
                               triangle_indices=faces))
