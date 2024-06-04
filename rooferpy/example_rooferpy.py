import rooferpy
import laspy
import numpy as np
from shapely import wkt

def read_las_from_file(file_path):
    with laspy.open(file_path) as las:
        # Read the points
        points = las.read()
 
        # Access point data
        x = points['X']
        y = points['Y']
        z = points['Z']

    # Combine x, y, and z coordinates into a numpy array of shape (N, 3)
    point_cloud = np.column_stack((x, y, z))

    # Convert the numpy array to a list of arrays
    point_cloud_list = point_cloud.tolist()

    return point_cloud_list


def read_wkt_from_file(file_path):
    with open(file_path, 'r') as file:
        wkt_str = file.read().strip()
    return wkt_str

def wkt_polygon_to_rings(wkt_str):
    # Parse WKT string to a shapely geometry object
    geom = wkt.loads(wkt_str)
    
    # Ensure the geometry is a Polygon
    if geom.geom_type != 'Polygon':
        raise ValueError(f"The WKT geometry is not a Polygon, it is a {geom.geom_type}")
    
    # Function to convert coordinates to list of lists
    def coords_to_ring(coords):
        return [[float(x), float(y), float(z)] if len(coord) == 3 else [float(x), float(y), 0.0] for coord in coords]
    
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

# Load building points
print("Reading .LAZ...")
pts = read_las_from_file('./example_data/input.laz')

print("Reading the WKT polygon...")
# Load polygon points
footprint = read_wkt_from_file('./example_data/input.txt')

# Reconstruct
print("Reconstructing building...")
roofer_meshes = rooferpy.reconstruct_single_instance(pts, footprint, roofer_config)

# Output reconstruct todo 
#print(roofer_meshes)
