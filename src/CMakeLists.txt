cmake_minimum_required(VERSION 3.18)
project(DFIGPluginRoot)

# Since test_runner automatically includes dfigPlugin.cmake and builds it as a dependency,
# we only need to add the test_runner subdirectory. This prevents duplicate target definitions.
add_subdirectory(test_runner)
