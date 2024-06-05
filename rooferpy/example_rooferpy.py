import rooferpy
import laspy
import numpy as np
from shapely import wkt

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

    # Convert the numpy arrays to lists of 3-element arrays
    building_points_list = [point.tolist() for point in building_points]
    ground_points_list = [point.tolist() for point in ground_points]

    # Apply offset
    building_points_list = apply_offset(building_points_list, x_offset, y_offset)
    ground_points_list = apply_offset(ground_points_list, x_offset, y_offset)

    return building_points_list, ground_points_list

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

# Set the reconstruction configuration
roofer_config = rooferpy.ReconstructionConfig()
roofer_config.complexity_factor = 0.7

# Define offsets to avoid truncation errors
x_offset = 0
y_offset = 0

# Load building points and ground points
print("Reading .LAZ...")
building_pts, ground_pts = read_las_from_file('./example_data/input.laz', x_offset, y_offset)
"""
building_pts = [
       [0., 0., 1.], [1., 0., 1.], [1., 1., 1.], [0., 1., 1.], [0., 0., 1.], [0.1, 0.1, 1.], [0.2, 0.2, 1.], [0.1, 0.2, 1.], [0.2, 0.1, 1.], [0.3, 0.3, 1.], [0.4, 0.4, 1.], [0.5, 0.5, 1.], [0.6, 0.6, 1.],
       [0.7, 0.7, 1.], [0.8, 0.8, 1.], [0.9, 0.9, 1.], [1., 1., 1.],
       [0., 0., 1.], [1., 0., 1.], [1., 1., 1.], [0., 1., 1.], [0., 0., 1.], [0.1, 0.1, 1.], [0.2, 0.2, 1.], [0.1, 0.2, 1.], [0.2, 0.1, 1.], [0.3, 0.3, 1.], [0.4, 0.4, 1.], [0.5, 0.5, 1.], [0.6, 0.6, 1.],
       [0.7, 0.7, 1.], [0.8, 0.8, 1.], [0.9, 0.9, 1.], [1., 1., 1.],
       [0., 0., 1.], [1., 0., 1.], [1., 1., 1.], [0., 1., 1.], [0., 0., 1.], [0.1, 0.1, 1.], [0.2, 0.2, 1.], [0.1, 0.2, 1.], [0.2, 0.1, 1.], [0.3, 0.3, 1.], [0.4, 0.4, 1.], [0.5, 0.5, 1.], [0.6, 0.6, 1.],
       [0.7, 0.7, 1.], [0.8, 0.8, 1.], [0.9, 0.9, 1.], [1., 1., 1.],
       [0., 0., 1.], [1., 0., 1.], [1., 1., 1.], [0., 1., 1.], [0., 0., 1.], [0.1, 0.1, 1.], [0.2, 0.2, 1.], [0.1, 0.2, 1.], [0.2, 0.1, 1.], [0.3, 0.3, 1.], [0.4, 0.4, 1.], [0.5, 0.5, 1.], [0.6, 0.6, 1.],
       [0.7, 0.7, 1.], [0.8, 0.8, 1.], [0.9, 0.9, 1.], [1., 1., 1.]
]

ground_pts = [
    [0., 0., 0.], [1., 0., 0.], [1., 1., 0.], [0., 1., 0.]
]
"""

print("Reading the WKT polygon...")
# Load polygon points
footprint_str = read_wkt_from_file('./example_data/input.txt')
footprint = wkt_polygon_to_rings(footprint_str, x_offset, y_offset)
"""
footprint = [
    [[0., 0., 0.], [1., 0., 0.], [1., 1., 0.], [0., 1., 0.]]
]
"""

# Reconstruct
print("Reconstructing building...")
roofer_meshes = rooferpy.reconstruct_single_instance(building_pts, ground_pts, footprint, roofer_config)

# Output reconstruct todo 
#print(roofer_meshes)
