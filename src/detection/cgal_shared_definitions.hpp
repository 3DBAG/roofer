#pragma once

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>

typedef CGAL::Exact_predicates_inexact_constructions_kernel EPICK;
typedef EPICK::Point_3 Point;
typedef EPICK::Vector_3 Vector;
typedef EPICK::Vector_2 Vector_2;
typedef EPICK::Plane_3 Plane;
typedef EPICK::Line_3 Line;

typedef std::unordered_map<int, std::pair<Plane, std::vector<Point>>> IndexedPlanesWithPoints;