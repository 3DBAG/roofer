add_subdirectory(core)
if(RF_BUILD_APPS OR RF_BUILD_TESTING)
  add_subdirectory(extra)
endif()
