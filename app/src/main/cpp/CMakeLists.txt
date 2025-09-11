# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project("cameraxtest")

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
#
# In this top level CMakeLists.txt, ${CMAKE_PROJECT_NAME} is used to define
# the target library name; in the sub-module's CMakeLists.txt, ${PROJECT_NAME}
# is preferred for the same purpose.
#
# In order to load a library into your app from Java/Kotlin, you must call
# System.loadLibrary() and pass the name of the library defined here;
# for GameActivity/NativeActivity derived applications, the same library name must be
# used in the AndroidManifest.xml file.
add_library(${CMAKE_PROJECT_NAME} SHARED
        # List C/C++ source files with relative paths to this CMakeLists.txt.
        native-lib.cpp
        file_utils.c
        postprocess.cc
        yolov8.cc)

include_directories(${CMAKE_SOURCE_DIR}/mylibs/include)
include_directories(${CMAKE_SOURCE_DIR}/)

target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/mylibs/lib)

#声明 "rga" 是预编译的共享库
add_library(
        rga                    # 目标库名称：rga（与后续链接时的名称一致）
        SHARED                 # 库类型：共享库
        IMPORTED               # 标记为“预编译导入库”
)

#给 "rga" 目标设置属性：指定 librga.so 的实际路径
set_target_properties(
        rga                    # 目标库名称
        PROPERTIES IMPORTED_LOCATION  # 配置“导入库路径”属性
        ${CMAKE_SOURCE_DIR}/myLibs/lib/librga.so
)

#声明 "rknnrt" 是预编译的共享库
add_library(
        rknnrt                    # 目标库名称：rga（与后续链接时的名称一致）
        SHARED                 # 库类型：共享库
        IMPORTED               # 标记为“预编译导入库”
)

#给 "rknnrt" 目标设置属性：指定 librknnrt.so 的实际路径
set_target_properties(
        rknnrt
        PROPERTIES IMPORTED_LOCATION  # 配置“导入库路径”属性
        ${CMAKE_SOURCE_DIR}/myLibs/lib/librknnrt.so
)

# Specifies libraries CMake should link to your target library. You
# can link libraries from various origins, such as libraries defined in this
# build script, prebuilt third-party libraries, or Android system libraries.
target_link_libraries(${CMAKE_PROJECT_NAME}
        # List libraries link to the target library
        android
        log
        rga
        rknnrt)
