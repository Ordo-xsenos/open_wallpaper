cmake_minimum_required(VERSION 3.28)
project(AetherWall VERSION 0.5.0 LANGUAGES CXX)
configure_file(src/system/BuildVersion.h.in generated/aetherwall_version.h @ONLY)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)
option(AETHERWALL_BUILD_CORE "Build the Windows desktop prototype" ${WIN32})
option(AETHERWALL_BUILD_MANAGER "Build the dynamically linked Qt Manager" OFF)

find_package(nlohmann_json CONFIG QUIET)
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_path(AETHERWALL_JSON_INCLUDE_DIR nlohmann/json.hpp REQUIRED)
    add_library(aetherwall_json INTERFACE)
    target_include_directories(aetherwall_json SYSTEM INTERFACE "${AETHERWALL_JSON_INCLUDE_DIR}")
    add_library(nlohmann_json::nlohmann_json ALIAS aetherwall_json)
endif()
find_package(LibArchive REQUIRED)
find_package(ZLIB REQUIRED)
find_package(WebP CONFIG QUIET)
if(TARGET WebP::webp)
    add_library(aetherwall_webp INTERFACE)
    target_link_libraries(aetherwall_webp INTERFACE WebP::webp)
else()
    find_path(WEBP_INCLUDE_DIR webp/decode.h REQUIRED)
    find_library(WEBP_LIBRARY NAMES webp libwebp REQUIRED)
    add_library(aetherwall_webp INTERFACE)
    target_include_directories(aetherwall_webp SYSTEM INTERFACE "${WEBP_INCLUDE_DIR}")
    target_link_libraries(aetherwall_webp INTERFACE "${WEBP_LIBRARY}")
endif()
add_library(aetherwall_scenes STATIC src/scenes/Scene.cpp src/scenes/SceneParser.cpp src/scenes/ScenePackage.cpp)
target_include_directories(aetherwall_scenes PUBLIC src)
target_link_libraries(aetherwall_scenes PUBLIC LibArchive::LibArchive ZLIB::ZLIB aetherwall_webp PRIVATE nlohmann_json::nlohmann_json)
if(WIN32 AND VCPKG_TARGET_TRIPLET MATCHES "static")
    target_compile_definitions(aetherwall_scenes PUBLIC LIBARCHIVE_STATIC)
endif()
if(WIN32)
    target_link_libraries(aetherwall_scenes PUBLIC bcrypt)
endif()
add_executable(AetherWall.SceneCheck apps/scenecheck/main.cpp)
target_link_libraries(AetherWall.SceneCheck PRIVATE aetherwall_scenes)
if(MSVC)
    target_compile_options(aetherwall_scenes PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
    target_compile_options(AetherWall.SceneCheck PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
else()
    target_compile_options(aetherwall_scenes PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(aetherwall_policy STATIC
    src/system/Options.cpp
    src/system/FrameClock.cpp
    src/system/MonitorLayout.cpp
    src/system/PausePolicy.cpp
)
target_include_directories(aetherwall_policy PUBLIC src)
if(MSVC)
    target_compile_options(aetherwall_policy PRIVATE /W4 /permissive- /utf-8 /EHsc)
else()
    target_compile_options(aetherwall_policy PRIVATE -Wall -Wextra -Wpedantic)
endif()

find_package(unofficial-sqlite3 CONFIG QUIET)
if(TARGET unofficial::sqlite3::sqlite3)
    set(aetherwall_sqlite unofficial::sqlite3::sqlite3)
else()
    find_package(SQLite3 REQUIRED)
    if(TARGET SQLite3::SQLite3)
        set(aetherwall_sqlite SQLite3::SQLite3)
    else()
        set(aetherwall_sqlite SQLite::SQLite3)
    endif()
endif()
add_library(aetherwall_control STATIC src/ipc/Protocol.cpp src/storage/SettingsStore.cpp
    src/storage/AtomicFile.cpp src/storage/LibraryStore.cpp src/storage/PackageHash.cpp src/control/ControlService.cpp)
target_include_directories(aetherwall_control PUBLIC src)
target_include_directories(aetherwall_control PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(aetherwall_control PUBLIC aetherwall_scenes nlohmann_json::nlohmann_json ${aetherwall_sqlite})
if(MSVC)
    target_compile_options(aetherwall_control PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
    target_compile_definitions(aetherwall_control PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_compile_options(aetherwall_control PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
if(WIN32)
    target_link_libraries(aetherwall_control PUBLIC bcrypt)
else()
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
    target_link_libraries(aetherwall_control PUBLIC OpenSSL::Crypto)
endif()
if(WIN32)
    add_library(aetherwall_pipe STATIC src/ipc/NamedPipeServer.cpp src/ipc/NamedPipeClient.cpp)
    target_include_directories(aetherwall_pipe PUBLIC src)
    target_link_libraries(aetherwall_pipe PUBLIC aetherwall_control advapi32)
    target_compile_definitions(aetherwall_pipe PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00 WINVER=0x0A00)
    target_compile_options(aetherwall_pipe PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
endif()
if(WIN32 AND (AETHERWALL_BUILD_CORE OR AETHERWALL_BUILD_MANAGER))
    enable_language(RC)
endif()
if(AETHERWALL_BUILD_MANAGER)
    add_subdirectory(apps/manager)
endif()

if(AETHERWALL_BUILD_CORE)
    if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "Core requires Windows x64, MSVC, and Windows SDK. Use portable-tests on Linux.")
    endif()

    # fxc comes from Windows SDK; Developer PowerShell normally exposes it on PATH.
    file(GLOB sdk_fxc_candidates
        "$ENV{WindowsSdkDir}/bin/*/x64/fxc.exe"
        "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/fxc.exe")
    list(SORT sdk_fxc_candidates COMPARE NATURAL ORDER DESCENDING)
    set(sdk_fxc_paths "")
    foreach(candidate IN LISTS sdk_fxc_candidates)
        get_filename_component(candidate_dir "${candidate}" DIRECTORY)
        list(APPEND sdk_fxc_paths "${candidate_dir}")
    endforeach()
    find_program(FXC_EXECUTABLE NAMES fxc.exe fxc HINTS ${sdk_fxc_paths} REQUIRED)

    set(shader_headers "")
    foreach(shader_spec IN ITEMS "fullscreen_vs:vs_5_0" "gradient_ps:ps_5_0" "scene_vs:vs_5_0" "scene_ps:ps_5_0")
        string(REPLACE ":" ";" shader_spec "${shader_spec}")
        list(GET shader_spec 0 shader_name)
        list(GET shader_spec 1 shader_profile)
        set(shader_header "${CMAKE_CURRENT_BINARY_DIR}/generated/${shader_name}.h")
        add_custom_command(OUTPUT "${shader_header}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
            COMMAND "${FXC_EXECUTABLE}" /nologo /WX /Ges /O3
                /T "${shader_profile}" /E main /Vn "g_${shader_name}"
                /Fh "${shader_header}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader_name}.hlsl"
            DEPENDS "shaders/${shader_name}.hlsl" "shaders/scene_constants.hlsli"
            VERBATIM)
        list(APPEND shader_headers "${shader_header}")
    endforeach()

    add_executable(AetherWall.Core
        apps/core/main.cpp
        apps/core/CoreApplication.cpp
        apps/core/CoreControl.cpp
        apps/core/resources.rc
        src/system/UserConfiguration.cpp
        src/system/TrayController.cpp
        src/desktop/Win11DesktopHost.cpp
        src/graphics/D3D11Renderer.cpp
        src/graphics/ResourceManager.cpp
        src/graphics/DevicePool.cpp
        src/graphics/GpuTimer.cpp
        src/system/MonitorService.cpp
        src/system/PowerAndForegroundService.cpp
        src/diagnostics/ProcessMetrics.cpp
        src/diagnostics/Logger.cpp
        src/system/WindowsEnvironment.cpp
        ${shader_headers}
    )
    target_include_directories(AetherWall.Core PRIVATE src apps/core "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(AetherWall.Core PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
        _WIN32_WINNT=0x0A00 WINVER=0x0A00)
    target_compile_options(AetherWall.Core PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
    # DPI awareness is set before any HWND is created, rather than relying on an implicit manifest.
    target_link_options(AetherWall.Core PRIVATE "/MANIFESTUAC:level='asInvoker' uiAccess='false'")
    target_link_libraries(AetherWall.Core PRIVATE aetherwall_policy aetherwall_scenes aetherwall_control aetherwall_pipe
        d3d11 dxgi user32 shell32 ole32 uuid wtsapi32 windowscodecs dwmapi psapi)
    add_custom_command(TARGET AetherWall.Core POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/scenes/bundled" "$<TARGET_FILE_DIR:AetherWall.Core>/scenes/bundled")
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        file(GLOB dependency_notices "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/*/copyright")
        foreach(notice IN LISTS dependency_notices)
            get_filename_component(notice_dir "${notice}" DIRECTORY)
            get_filename_component(notice_package "${notice_dir}" NAME)
            add_custom_command(TARGET AetherWall.Core POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:AetherWall.Core>/licenses/${notice_package}"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${notice}" "$<TARGET_FILE_DIR:AetherWall.Core>/licenses/${notice_package}/copyright"
                VERBATIM)
        endforeach()
    endif()
endif()

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(aetherwall_policy_tests tests/unit/PolicyTests.cpp tests/unit/SceneTests.cpp tests/unit/MultiMonitorTests.cpp tests/unit/ControlTests.cpp)
    target_link_libraries(aetherwall_policy_tests PRIVATE aetherwall_policy aetherwall_scenes nlohmann_json::nlohmann_json GTest::gtest_main aetherwall_control)
    if(WIN32)
        target_compile_definitions(aetherwall_policy_tests PRIVATE "AETHERWALL_BUNDLED_DIR=L\"${CMAKE_CURRENT_SOURCE_DIR}/scenes/bundled\"")
    else()
        target_compile_definitions(aetherwall_policy_tests PRIVATE AETHERWALL_BUNDLED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/scenes/bundled")
    endif()
    if(MSVC)
        target_compile_options(aetherwall_policy_tests PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
    else()
        target_compile_options(aetherwall_policy_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    include(GoogleTest)
    gtest_discover_tests(aetherwall_policy_tests)
endif()
