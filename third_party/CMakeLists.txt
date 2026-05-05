set(ARMP_GUI_AVAILABLE FALSE PARENT_SCOPE)

set(_glfw_dir "${CMAKE_CURRENT_SOURCE_DIR}/glfw")
set(_imgui_dir "${CMAKE_CURRENT_SOURCE_DIR}/imgui")

if(EXISTS "${_glfw_dir}/CMakeLists.txt" AND EXISTS "${_imgui_dir}/imgui.h")
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
    add_subdirectory(${_glfw_dir} EXCLUDE_FROM_ALL)

    add_library(armp_imgui STATIC
        ${_imgui_dir}/imgui.cpp
        ${_imgui_dir}/imgui_draw.cpp
        ${_imgui_dir}/imgui_tables.cpp
        ${_imgui_dir}/imgui_widgets.cpp
        ${_imgui_dir}/imgui_demo.cpp
        ${_imgui_dir}/backends/imgui_impl_glfw.cpp
        ${_imgui_dir}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(armp_imgui PUBLIC
        ${_imgui_dir}
        ${_imgui_dir}/backends
    )
    target_link_libraries(armp_imgui PUBLIC glfw)
    target_compile_features(armp_imgui PUBLIC cxx_std_17)

    if(NOT WIN32 AND NOT APPLE)
        find_package(OpenGL QUIET)
        if(OpenGL_FOUND)
            target_link_libraries(armp_imgui PUBLIC OpenGL::GL)
        endif()
    elseif(WIN32)
        target_link_libraries(armp_imgui PUBLIC opengl32)
    elseif(APPLE)
        target_link_libraries(armp_imgui PUBLIC "-framework OpenGL")
    endif()

    set(ARMP_GUI_AVAILABLE TRUE PARENT_SCOPE)
    message(STATUS "[arm_project_simple] GUI зависимости найдены, GUI будет собран.")
else()
    message(WARNING "[arm_project_simple] third_party/glfw или third_party/imgui отсутствуют - GUI отключён, доступен только CLI.")
endif()
