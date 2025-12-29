cmake_minimum_required(VERSION 3.20)
project(us.elections)

set(CMAKE_CXX_STANDARD 17)

if(WIN32 AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
  set(CMAKE_CONFIGURATION_TYPES "RelWithDebInfo" CACHE STRING "" FORCE)
endif()

#//////////////////////////
# DuckDB (pre-built)
#//////////////////////////

set(DUCKDB_ROOT ${CMAKE_SOURCE_DIR}/ext/duckdb-1.4.3)
set(DUCKDB_BUILD ${CMAKE_SOURCE_DIR}/build/duckdb-1.4.3)

if(WIN32)
  set(LIB_DIR ${DUCKDB_BUILD}/src/RelWithDebInfo)
  set(THIRD_PARTY_DIR ${DUCKDB_BUILD}/third_party)
  set(EXT_DIR ${DUCKDB_BUILD}/extension)
  set(LIB_SUFFIX .lib)
  set(LIB_PREFIX )
  set(CFG /RelWithDebInfo)
else()
  set(LIB_DIR ${DUCKDB_BUILD}/src)
  set(THIRD_PARTY_DIR ${DUCKDB_BUILD}/third_party)
  set(EXT_DIR ${DUCKDB_BUILD}/extension)
  set(LIB_SUFFIX .a)
  set(LIB_PREFIX lib)
  set(CFG )
endif()

add_library(duckdb_static STATIC IMPORTED)
set_target_properties(duckdb_static PROPERTIES IMPORTED_LOCATION ${LIB_DIR}/${LIB_PREFIX}duckdb_static${LIB_SUFFIX})

set(DUCKDB_THIRD_PARTY_LIBS
  utf8proc
  fmt
  fastpforlib
  miniz
  re2
  hyperloglog
  fsst
  mbedtls
  yyjson
  zstd
)

foreach(lib ${DUCKDB_THIRD_PARTY_LIBS})
  add_library(duckdb_${lib} STATIC IMPORTED)
  set_target_properties(duckdb_${lib} PROPERTIES IMPORTED_LOCATION ${THIRD_PARTY_DIR}/${lib}${CFG}/${LIB_PREFIX}duckdb_${lib}${LIB_SUFFIX})
endforeach()

add_library(duckdb_skiplistlib STATIC IMPORTED)
set_target_properties(duckdb_skiplistlib PROPERTIES IMPORTED_LOCATION ${THIRD_PARTY_DIR}/skiplist${CFG}/${LIB_PREFIX}duckdb_skiplistlib${LIB_SUFFIX})

add_library(duckdb_pg_query STATIC IMPORTED)
set_target_properties(duckdb_pg_query PROPERTIES IMPORTED_LOCATION ${THIRD_PARTY_DIR}/libpg_query${CFG}/${LIB_PREFIX}duckdb_pg_query${LIB_SUFFIX})

add_library(duckdb_parquet_extension STATIC IMPORTED)
set_target_properties(duckdb_parquet_extension PROPERTIES IMPORTED_LOCATION ${EXT_DIR}/parquet${CFG}/${LIB_PREFIX}parquet_extension${LIB_SUFFIX})

add_library(duckdb_core_functions STATIC IMPORTED)
set_target_properties(duckdb_core_functions PROPERTIES IMPORTED_LOCATION ${EXT_DIR}/core_functions${CFG}/${LIB_PREFIX}core_functions_extension${LIB_SUFFIX})

if(NOT WIN32)
  add_library(duckdb_jemalloc_extension STATIC IMPORTED)
  set_target_properties(duckdb_jemalloc_extension PROPERTIES IMPORTED_LOCATION ${EXT_DIR}/jemalloc${CFG}/${LIB_PREFIX}jemalloc_extension${LIB_SUFFIX})
endif()

set(DUCKDB_LIBS
  duckdb_static
  duckdb_utf8proc
  duckdb_fmt
  duckdb_fastpforlib
  duckdb_miniz
  duckdb_re2
  duckdb_hyperloglog
  duckdb_fsst
  duckdb_mbedtls
  duckdb_yyjson
  duckdb_pg_query
  duckdb_zstd
  duckdb_parquet_extension
  duckdb_core_functions
  duckdb_skiplistlib
)

if(NOT WIN32)
  list(APPEND DUCKDB_LIBS duckdb_jemalloc_extension)
endif()

#//////////////////////////
# DuckDB spatial library
#//////////////////////////

add_library(lib_spatial STATIC src/spatial.cc src/spatial.hh)
target_link_libraries(lib_spatial PUBLIC ${DUCKDB_LIBS})
target_include_directories(lib_spatial PUBLIC ${CMAKE_SOURCE_DIR} ${DUCKDB_ROOT}/src/include)

#//////////////////////////
# DuckDB client; load from data from CSV and generate database
#//////////////////////////

add_executable(loader src/loader.cc src/data.cc src/data.hh)
target_link_libraries(loader PRIVATE lib_spatial)
target_compile_definitions(lib_spatial PUBLIC DUCKDB_STATIC_BUILD DUCKDB_BUILD_LIBRARY)
target_compile_definitions(loader PRIVATE DUCKDB_STATIC_BUILD DUCKDB_BUILD_LIBRARY)

if(WIN32)
  target_link_libraries(loader PRIVATE ws2_32 crypt32 rstrtmgr)
endif()

#//////////////////////////
# Wt 
#//////////////////////////

message(STATUS "Source directory is " ${CMAKE_SOURCE_DIR})
message(STATUS "Build directory is " ${CMAKE_CURRENT_BINARY_DIR})
message(STATUS "WT_INCLUDE: " ${WT_INCLUDE})
message(STATUS "BOOST_INCLUDE_DIR: " ${BOOST_INCLUDE_DIR})
message(STATUS "BOOST_LIB_DIRS: " ${BOOST_LIB_DIRS})

include_directories(${BOOST_INCLUDE_DIR})
include_directories(${WT_INCLUDE})
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)

#//////////////////////////
# source files 
#//////////////////////////

set(src ${src})
set(src ${src} src/data.cc)
set(src ${src} src/data.hh)
set(src ${src} src/map.hh)
set(src ${src} src/map.cc)
set(src ${src} src/elections.cc)

add_executable(elections  ${src})
target_link_libraries(elections PRIVATE lib_spatial)
target_compile_definitions(elections PRIVATE DUCKDB_STATIC_BUILD DUCKDB_BUILD_LIBRARY)

if(WIN32)
  target_link_libraries(elections PRIVATE ws2_32 crypt32 rstrtmgr)
endif()

if (MSVC)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT elections)
  set(wt_options "--http-address=0.0.0.0 --http-port=8080  --docroot=.")
  set_property(TARGET elections PROPERTY VS_DEBUGGER_COMMAND_ARGUMENTS ${wt_options})
endif()

#//////////////////////////
# link with libraries
# lib_dep contains a cascade definition of all the libraries needed to link
#//////////////////////////

set(lib_dep ${lib_dep})

#//////////////////////////
# Linux/Mac
#//////////////////////////

if(UNIX AND NOT APPLE)
  set(LINUX TRUE)
endif()

if(UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated -Wno-deprecated-declarations")
endif()

if(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations -Wno-deprecated-copy -Wno-deprecated-copy-dtor")
endif()

message(STATUS "Compiler flags: ${CMAKE_CXX_FLAGS}")

#//////////////////////////
# LINUX
# order of the link libraries matters; pthread dl
#//////////////////////////

if(LINUX)
  set(lib_dep ${lib_dep} pthread dl)
  set(lib_dep ${lib_dep} stdc++fs) 
  find_program(LSB_RELEASE_EXEC lsb_release)
  execute_process(COMMAND ${LSB_RELEASE_EXEC} -is OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT OUTPUT_STRIP_TRAILING_WHITESPACE)
  message(STATUS "Building in " ${LSB_RELEASE_ID_SHORT})
endif()

if (MSVC)
  set(lib_dep ${lib_dep} ${CMAKE_CURRENT_SOURCE_DIR}/install/wt/lib/wt.lib)
  set(lib_dep ${lib_dep} ${CMAKE_CURRENT_SOURCE_DIR}/install/wt/lib/wthttp.lib)
  set(lib_dep ${lib_dep} ${CMAKE_CURRENT_SOURCE_DIR}/install/wt/lib/wtdbo.lib)
  set(lib_dep ${lib_dep} ${CMAKE_CURRENT_SOURCE_DIR}/install/wt/lib/wtdbosqlite3.lib)
  set(lib_dep ${lib_dep} ${BOOST_LIB_DIRS}/libboost_filesystem-vc143-mt-x64-1_88.lib)
  set(lib_dep ${lib_dep} ${BOOST_LIB_DIRS}/libboost_thread-vc143-mt-x64-1_88.lib)
  set(lib_dep ${lib_dep} ${BOOST_LIB_DIRS}/libboost_program_options-vc143-mt-x64-1_88.lib)
  set(lib_dep ${lib_dep} ${BOOST_LIB_DIRS}/libboost_chrono-vc143-mt-x64-1_88.lib)
endif()

if(APPLE)
  set(lib_dep ${lib_dep} ${CMAKE_CURRENT_SOURCE_DIR}/install/wt/lib/libwt.dylib)
  set(lib_dep ${lib_dep} ${CMAKE_CURRENT_SOURCE_DIR}/install/wt/lib/libwthttp.dylib)
  set(lib_dep ${lib_dep} ${CMAKE_CURRENT_SOURCE_DIR}/install/wt/lib/libwtdbo.dylib)
  set(lib_dep ${lib_dep} ${CMAKE_CURRENT_SOURCE_DIR}/install/wt/lib/libwtdbosqlite3.dylib)
  set(lib_dep ${lib_dep} ${BOOST_LIB_DIRS}/libboost_filesystem-clang-darwin17-mt-x64-1_88.a)
  set(lib_dep ${lib_dep} ${BOOST_LIB_DIRS}/libboost_thread-clang-darwin17-mt-x64-1_88.a)
  set(lib_dep ${lib_dep} ${BOOST_LIB_DIRS}/libboost_program_options-clang-darwin17-mt-x64-1_88.a)
  set(lib_dep ${lib_dep} ${BOOST_LIB_DIRS}/libboost_chrono-clang-darwin17-mt-x64-1_88.a)
endif()

if (LINUX)
  find_package(Boost REQUIRED COMPONENTS filesystem thread program_options)
  set(lib_dep ${lib_dep} ${CMAKE_CURRENT_SOURCE_DIR}/install/wt/lib/libwt.so)
  set(lib_dep ${lib_dep} ${CMAKE_CURRENT_SOURCE_DIR}/install/wt/lib/libwthttp.so)
  set(lib_dep ${lib_dep} ${CMAKE_CURRENT_SOURCE_DIR}/install/wt/lib/libwtdbo.so)
  set(lib_dep ${lib_dep} ${CMAKE_CURRENT_SOURCE_DIR}/install/wt/lib/libwtdbosqlite3.so)
  set(lib_dep ${lib_dep} Boost::filesystem Boost::thread Boost::program_options)
endif()

#//////////////////////////
#  link
#//////////////////////////

message(STATUS "lib_dep: " ${lib_dep})
target_link_libraries(elections PRIVATE ${lib_dep})

#//////////////////////////
# data files 
#//////////////////////////

set(DATA_FILES
  "data/2024_US_County_Level_Presidential_Results.csv"
  "data/counties-10m.json"
  "data/states-10m.json"
)

message(STATUS "Copying data files to: ${CMAKE_BINARY_DIR}")
foreach(DATA_FILE ${DATA_FILES})
  file(COPY "${CMAKE_SOURCE_DIR}/${DATA_FILE}" DESTINATION ${CMAKE_BINARY_DIR})
endforeach()
