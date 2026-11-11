cmake_minimum_required(VERSION 3.25)

project(
    fooyin-scrobbler
    VERSION 0.15.0
    DESCRIPTION "Scrobbler plugin for fooyin"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Fooyin 0.15 REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Widgets)

set(FOOYIN_VERSION "${PROJECT_VERSION}")

option(BUILD_TESTING "Build the plugin tests" ON)

add_library(
    scrobbler_library OBJECT
        src/customservicedialog.cpp
        src/customservicedialog.h
        src/lovedcache.cpp
        src/lovedcache.h
        src/scrobbler.cpp
        src/scrobbler.h
        src/scrobblerauthsession.cpp
        src/scrobblerauthsession.h
        src/scrobblercache.cpp
        src/scrobblercache.h
        src/scrobblertoggle.cpp
        src/scrobblertoggle.h
        src/services/lastfmservice.cpp
        src/services/lastfmservice.h
        src/services/librefmservice.cpp
        src/services/librefmservice.h
        src/services/listenbrainzservice.cpp
        src/services/listenbrainzservice.h
        src/services/scrobblerservice.cpp
        src/services/scrobblerservice.h
        src/services/servicedetails.cpp
        src/services/servicedetails.h
        src/settings/scrobblerpage.cpp
        src/settings/scrobblerpage.h
        src/settings/scrobblerservicespage.cpp
        src/settings/scrobblerservicespage.h
        src/settings/scrobblersettings.cpp
        src/settings/scrobblersettings.h
)
set_target_properties(
    scrobbler_library
    PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON
)
target_include_directories(scrobbler_library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(scrobbler_library PUBLIC QT_USE_QSTRINGBUILDER)
target_link_libraries(scrobbler_library PUBLIC Fooyin::Core Fooyin::Gui Qt6::Network Qt6::Widgets)

create_fooyin_plugin(
    scrobbler
    DEPENDS scrobbler_library
    SOURCES src/scrobblerplugin.cpp src/scrobblerplugin.h
    JSON_IN src/scrobbler.json.in
)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
