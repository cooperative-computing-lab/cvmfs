
#
# global test configuration
#
set (PROJECT_TEST_NAME "${PROJECT_NAME}_test")

#
# unit test source files
#
set (CVMFS_UNITTEST_SOURCES
  main.cc

  t_atomic.cc
  t_smallhash.cc
)

#
# build googletest framework
#
include (${GOOGLETEST_BUILTIN_LOCATION}/CVMFS-CMakeLists.txt)
set (INCLUDE_DIRECTORIES ${INCLUDE_DIRECTORIES} ${GOOGLETEST_BUILTIN_LOCATION}/include)

#
# Compiler and Linker Flags for unit tests
#
set (CVMFS_UNITTESTS_CFLAGS "${CVMFS_UNITTESTS_CFLAGS} -I ${CMAKE_SOURCE_DIR}/cvmfs")
set (CVMFS_UNITTESTS_LD_FLAGS "${CVMFS_UNITTESTS_LD_FLAGS}")

#
# build CernVM-FS test cases
#
add_executable (${PROJECT_TEST_NAME} ${CVMFS_UNITTEST_SOURCES})
add_dependencies (${PROJECT_TEST_NAME} googletest)

set_target_properties (${PROJECT_TEST_NAME} PROPERTIES COMPILE_FLAGS "${CVMFS_UNITTESTS_CFLAGS}" LINK_FLAGS "${CVMFS_UNITTESTS_LD_FLAGS}")

# link the stuff (*_LIBRARIES are dynamic link libraries)
target_link_libraries (${PROJECT_TEST_NAME} ${GOOGLETEST_ARCHIVE} pthread)

#
# Integrate the test running into CMake
#
add_test (NAME unittests COMMAND ${PROJECT_TEST_NAME})