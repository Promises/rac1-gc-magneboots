# Going Commando Magneboots: an OpenRAC native mod for Ratchet & Clank (from the SDK's template).
#
#   python3 tools/fetch_sdk.py                     # the SDK sdk.lock pins, into .sdk/
#   cmake -S . -B build -G Ninja
#   cmake --build build && ctest --test-dir build
#   python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/gc_magneboots
#
# A tag v<version> (the manifest's) makes the workflow publish the release the launcher installs.
cmake_minimum_required(VERSION 3.20)
project(gc_magneboots LANGUAGES C CXX)

# The SDK: the release sdk.lock pins (tools/fetch_sdk.py puts it in .sdk/); OPENRAC_SDK=<dir> (an
# unpacked SDK, or sdk/ in a checkout of OpenRAC) or OpenRACMod_DIR=<dir>/cmake for another.
if(NOT OpenRACMod_DIR)
  if(DEFINED OPENRAC_SDK)
    set(OpenRACMod_DIR "${OPENRAC_SDK}/cmake")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock" lock)
    string(JSON sdk_version GET "${lock}" version)
    set(OpenRACMod_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.sdk/openrac-mod-sdk-${sdk_version}/cmake")
    if(NOT EXISTS "${OpenRACMod_DIR}/OpenRACModConfig.cmake")
      message(FATAL_ERROR "No OpenRAC mod SDK ${sdk_version}: run python3 tools/fetch_sdk.py first "
                          "(or pass -DOPENRAC_SDK=<an unpacked SDK>)")
    endif()
  endif()
endif()
find_package(OpenRACMod 1.23 REQUIRED)

openrac_add_mod(gc_magneboots MANIFEST openrac-mod.json SOURCES src/mod.cpp)

# The Gravity Boots rules (ctest; the workflow runs it on every platform).
enable_testing()
add_executable(gc_magneboots_test src/gravity_boots_test.cpp)
target_compile_features(gc_magneboots_test PRIVATE cxx_std_17)
add_test(NAME gravity_boots COMMAND gc_magneboots_test)
