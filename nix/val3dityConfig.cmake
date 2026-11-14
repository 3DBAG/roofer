include(CMakeFindDependencyMacro)
find_dependency(CGAL)
find_dependency(GEOS CONFIG)
# CGAL Core can request static Boost globally, whereas the upstream Nix
# package builds against shared Boost. Keep that choice local to this lookup.
block(SCOPE_FOR VARIABLES)
  set(Boost_USE_STATIC_LIBS OFF)
  find_dependency(Boost COMPONENTS filesystem)
endblock()
find_dependency(nlohmann_json CONFIG)
find_dependency(spdlog CONFIG)
find_dependency(pugixml CONFIG)

if(NOT TARGET val3dity)
  get_filename_component(_val3dity_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
  add_library(val3dity STATIC IMPORTED)
  set_target_properties(val3dity PROPERTIES
    IMPORTED_LOCATION "${_val3dity_prefix}/lib/libval3dity.a"
    INTERFACE_INCLUDE_DIRECTORIES "${_val3dity_prefix}/include"
    INTERFACE_LINK_LIBRARIES "CGAL::CGAL;GEOS::geos_c;Boost::filesystem;nlohmann_json::nlohmann_json;spdlog::spdlog;pugixml::pugixml")
  unset(_val3dity_prefix)
endif()
