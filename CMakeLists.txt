cmake_minimum_required(VERSION 3.14)
project(MapEditor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    add_compile_options(/utf-8)
     # Отключаем предупреждения об устаревших функциях Си (для bzip2 и stb)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)

    # Ensure Debug builds generate full PDBs and no optimizations when using MSVC + Ninja
    # Add /Zi for debug info, /Od to disable optimizations and set linker /DEBUG
    foreach(_flag_var CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG)
        if(NOT ${_flag_var} MATCHES "/Zi")
            set(${_flag_var} "${${_flag_var}} /Zi /Od")
        endif()
    endforeach()

    # Instruct linker to produce debug information for executables and shared libs
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /DEBUG")

    # Put generated PDBs into a dedicated directory inside the build tree
    set(CMAKE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pdb")
endif()

# For single-config generators (like Ninja) default to Debug to make it easier
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

# 1. Автоскачивание stb_image.h
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/stb_image.h")
    message(STATUS "Downloading stb_image.h...")
    file(DOWNLOAD "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h" 
                  "${CMAKE_CURRENT_SOURCE_DIR}/stb_image.h")
endif()

include(FetchContent)

# 2. GLFW
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.3.8)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# 3. ImGui
# //FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.90.4) 
FetchContent_Declare(imgui 
    GIT_REPOSITORY https://github.com/ocornut/imgui.git 
    GIT_TAG docking
)
FetchContent_Populate(imgui)

# 4. ImGui File Browser
FetchContent_Declare(filebrowser GIT_REPOSITORY https://github.com/AirGuanZ/imgui-filebrowser.git GIT_TAG master)
FetchContent_GetProperties(filebrowser)
if(NOT filebrowser_POPULATED)
    FetchContent_Populate(filebrowser)
endif()

find_package(OpenGL REQUIRED)

# 5. BZip2 (ищем в системе, если не найдена — скачиваем и собираем автоматически)
find_package(BZip2 QUIET)

if(NOT BZip2_FOUND AND NOT BZIP2_FOUND)
    message(STATUS "BZip2 not found on system. Fetching and building automatically via FetchContent...")
    
    set(ENABLE_LIB_ONLY ON CACHE BOOL "" FORCE)
    set(ENABLE_STATIC_LIB ON CACHE BOOL "" FORCE)
    set(ENABLE_SHARED_LIB OFF CACHE BOOL "" FORCE)
    set(ENABLE_APP OFF CACHE BOOL "" FORCE)
    set(ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ENABLE_DOCS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        bzip2
        GIT_REPOSITORY https://github.com/libarchive/bzip2.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(bzip2)

    # Создаем псевдоним BZip2::BZip2 для совместимости с find_package
    if(TARGET bz2_static)
        add_library(BZip2::BZip2 ALIAS bz2_static)
    elseif(TARGET bz2)
        add_library(BZip2::BZip2 ALIAS bz2)
    endif()

    set(BZIP2_INCLUDE_DIRS ${bzip2_SOURCE_DIR} ${bzip2_BINARY_DIR})
endif()
# Подключение иконки EXE для Windows
set(APP_RESOURCES "")
if(WIN32)
    set(APP_RESOURCES resource.rc)
endif()

add_executable(MapEditor WIN32
    main.cpp
    MapParser.cpp
    TextureManager.cpp
    ObjectManager.cpp
    ResourceManager.cpp
    EditorWorkspace.cpp
    NamedUnitsSettings.cpp
    ArmySettings.cpp
    LanternSettings.cpp
    ArtifactSettings.cpp
    UnitSettings.cpp
    ScenarioSettings.cpp
    BuildingSettings.cpp
    EventSettings.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${APP_RESOURCES}
)

target_include_directories(MapEditor PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${filebrowser_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BZIP2_INCLUDE_DIRS}
)

target_link_libraries(MapEditor PRIVATE glfw OpenGL::GL BZip2::BZip2)

if(WIN32)
    target_link_libraries(MapEditor PRIVATE comdlg32)
endif()
if(MSVC)
    set_target_properties(MapEditor PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
endif()