cmake_minimum_required(VERSION 3.28...3.30)

# The plugin can be built in two ways:
#   1. In-tree: copy this directory to obs-studio/plugins/ and add
#      add_core_module(obs-beauty-filter) to plugins/CMakeLists.txt.
#   2. Standalone: point CMAKE_PREFIX_PATH at an OBS build/install tree that
#      provides libobsConfig.cmake.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(obs-beauty-filter VERSION 1.1.0 LANGUAGES C)
  set(BEAUTY_FILTER_STANDALONE ON)
  find_package(libobs REQUIRED)
else()
  set(BEAUTY_FILTER_STANDALONE OFF)
  if(NOT TARGET OBS::libobs)
    message(FATAL_ERROR "obs-beauty-filter must be built inside OBS or with find_package(libobs).")
  endif()
endif()

# OBS Studio 33 changed the third-party plugin layout on Windows.  The new
# layout is:
#   <plugins-root>/obs-beauty-filter/obs-beauty-filter.dll
#   <plugins-root>/obs-beauty-filter/data/*
# where <plugins-root> is %ProgramData%\obs-studio\plugins for a normal install
# and <OBS-portable-root>\plugins for portable mode.  The old layout remains
# loadable until OBS Studio 34 but is flagged as Legacy.  This option is ON by
# default on Windows because 33.x still accepts both layouts; set it to OFF
# only when packaging for OBS 32 or older.
if(WIN32)
  set(_beauty_obs33_layout_default ON)
else()
  set(_beauty_obs33_layout_default OFF)
endif()
option(BEAUTY_OBS33_PLUGIN_LAYOUT
       "Install plugins using the OBS Studio 33+ Windows plugin layout"
       ${_beauty_obs33_layout_default})
unset(_beauty_obs33_layout_default)

# Keep the default Windows install target pointed at the right root.  The OBS
# 33+ layout installs relative to the plugin root (%ProgramData% for a normal
# install), while the legacy layout installs relative to the OBS install root.
# An explicit -DCMAKE_INSTALL_PREFIX or --prefix still wins.
if(WIN32 AND BEAUTY_FILTER_STANDALONE AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if(BEAUTY_OBS33_PLUGIN_LAYOUT)
    set(CMAKE_INSTALL_PREFIX "$ENV{ProgramData}/obs-studio/plugins"
        CACHE PATH "OBS third-party plugin root" FORCE)
  else()
    set(CMAKE_INSTALL_PREFIX "$ENV{ProgramFiles}/obs-studio"
        CACHE PATH "OBS install root" FORCE)
  endif()
endif()

set(TARGET_NAME obs-beauty-filter)

add_library(${TARGET_NAME} MODULE)
target_sources(
  ${TARGET_NAME}
  PRIVATE
    src/beauty-filter.c
    src/beauty-filter.h
    src/face-region.c
    src/face-region.h
    src/plugin-main.c
)

target_link_libraries(${TARGET_NAME} PRIVATE OBS::libobs)
target_compile_features(${TARGET_NAME} PRIVATE c_std_99)

if(MSVC)
  target_compile_options(${TARGET_NAME} PRIVATE /W3 /utf-8)
else()
  target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

set_target_properties(
  ${TARGET_NAME}
  PROPERTIES
    OUTPUT_NAME obs-beauty-filter
    PREFIX ""
    C_VISIBILITY_PRESET default
)

set(BEAUTY_OBS_HELPERS FALSE)
if(COMMAND set_target_properties_obs)
  # This helper already installs the binary and calls
  # target_install_resources() for in-tree builds.
  set_target_properties_obs(${TARGET_NAME} PROPERTIES FOLDER plugins PREFIX "")
  set(BEAUTY_OBS_HELPERS TRUE)
endif()

option(BEAUTY_BUILD_TESTS "Build the standalone tests" OFF)
if(BEAUTY_BUILD_TESTS)
  add_executable(beauty-load-test tests/load-test.c)
  target_link_libraries(beauty-load-test PRIVATE OBS::libobs)
  target_compile_features(beauty-load-test PRIVATE c_std_99)

  add_executable(beauty-face-region-test tests/face-region-test.c src/face-region.c)
  target_include_directories(beauty-face-region-test PRIVATE src)
  target_compile_features(beauty-face-region-test PRIVATE c_std_99)
  if(MSVC)
    target_compile_options(beauty-face-region-test PRIVATE /W3 /utf-8)
  else()
    target_compile_options(beauty-face-region-test PRIVATE -Wall -Wextra)
  endif()
endif()

# In-tree builds use OBS' own layout and helpers (including the core-module
# layout in current OBS). Standalone builds use the conventional OBS
# installation layout. Do not call target_install_resources() separately when
# set_target_properties_obs() was used: that helper already calls it.
if(NOT BEAUTY_FILTER_STANDALONE AND BEAUTY_OBS_HELPERS)
  # Nothing to add; OBS' helpers own the install/staging rules.
elseif(NOT BEAUTY_FILTER_STANDALONE AND COMMAND target_install_resources)
  target_install_resources(${TARGET_NAME})
else()
  include(GNUInstallDirs)

  if(WIN32)
    if(BEAUTY_OBS33_PLUGIN_LAYOUT)
      # New OBS 33+ layout.  The install prefix is the plugin root:
      # %ProgramData%\obs-studio\plugins or <portable-root>\plugins.
      set(_beauty_plugin_dir "${TARGET_NAME}")
      set(_beauty_data_dir "${TARGET_NAME}/data")
      set(_beauty_stage_plugin_dir "plugins/${TARGET_NAME}")
      set(_beauty_stage_data_dir "plugins/${TARGET_NAME}/data")
    else()
      # Legacy layout for OBS 32 and older.  OBS 33 still loads it but flags it
      # as Legacy; OBS 34 will stop loading it.
      set(_beauty_plugin_dir "obs-plugins/64bit")
      set(_beauty_data_dir "data/obs-plugins/${TARGET_NAME}")
      set(_beauty_stage_plugin_dir "${_beauty_plugin_dir}")
      set(_beauty_stage_data_dir "${_beauty_data_dir}")
    endif()
  elseif(APPLE)
    # macOS plugin locations did not change in OBS 33.
    set(_beauty_plugin_dir "obs-plugins")
    set(_beauty_data_dir "data/obs-plugins/${TARGET_NAME}")
    set(_beauty_stage_plugin_dir "${_beauty_plugin_dir}")
    set(_beauty_stage_data_dir "${_beauty_data_dir}")
  else()
    # Linux plugin locations are unchanged for now (OBS 33 documentation says
    # the new Linux layout is still to be announced).
    set(_beauty_plugin_dir "${CMAKE_INSTALL_LIBDIR}/obs-plugins")
    set(_beauty_data_dir "${CMAKE_INSTALL_DATADIR}/obs/obs-plugins/${TARGET_NAME}")
    set(_beauty_stage_plugin_dir "${_beauty_plugin_dir}")
    set(_beauty_stage_data_dir "${_beauty_data_dir}")
  endif()

  install(
    TARGETS ${TARGET_NAME}
    RUNTIME DESTINATION "${_beauty_plugin_dir}"
    LIBRARY DESTINATION "${_beauty_plugin_dir}"
  )
  install(DIRECTORY data/ DESTINATION "${_beauty_data_dir}")

  # Local staging directory for quick runtime testing.  On Windows with the
  # OBS 33 layout this mirrors a portable OBS tree: rundir/plugins/<name>/...
  add_custom_command(
    TARGET ${TARGET_NAME}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_BINARY_DIR}/rundir/${_beauty_stage_plugin_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_BINARY_DIR}/rundir/${_beauty_stage_data_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${TARGET_NAME}>"
            "${CMAKE_BINARY_DIR}/rundir/${_beauty_stage_plugin_dir}/"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/data"
            "${CMAKE_BINARY_DIR}/rundir/${_beauty_stage_data_dir}"
    VERBATIM
  )
endif()
