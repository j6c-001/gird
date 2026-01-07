cmake_minimum_required(VERSION 3.20)
project(gird LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(GIRD_WEB "Build gird for WebAssembly (Emscripten)" OFF)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)

add_library(imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Always needed (native + web)
target_include_directories(imgui PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

add_executable(gird
        src/main.cpp
        src/GridViewImGui.cpp
        src/GridFramework.cpp
        src/GridPersistence.cpp
)

target_link_libraries(gird PRIVATE imgui)

if (GIRD_WEB)
    # Build an .html launcher
    set_target_properties(gird PROPERTIES SUFFIX ".html")

    # Emscripten provides GLFW + WebGL; put these on the final link.
    target_link_options(gird PRIVATE
            -sUSE_GLFW=3
            -sFULL_ES3=1
            -sWASM=1
            -sINITIAL_MEMORY=67108864       # 64MB initial
            -sALLOW_MEMORY_GROWTH=1         # Dynamic growth
            -sMAXIMUM_MEMORY=268435456      # 256MB max
            -sASSERTIONS=0                  # Faster
    )

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
else()
    find_package(OpenGL REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_search_module(GLFW3 REQUIRED glfw3)

    target_include_directories(imgui PUBLIC ${GLFW3_INCLUDE_DIRS})
    target_link_libraries(imgui PUBLIC ${GLFW3_LIBRARIES} OpenGL::GL)
    target_link_options(imgui PUBLIC ${GLFW3_LDFLAGS})
endif()
