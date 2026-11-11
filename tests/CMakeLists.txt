find_package(GTest CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_remoteplayback remoteplaybacktest.cpp testmain.cpp)
target_link_libraries(test_remoteplayback PRIVATE scrobbler_library GTest::gtest Qt6::Test)

include(GoogleTest)
gtest_discover_tests(test_remoteplayback DISCOVERY_TIMEOUT 30)
