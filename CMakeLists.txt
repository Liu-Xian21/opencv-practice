cmake_minimum_required(VERSION 3.16)
project(opencv_demo)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 寻找OpenCV包（Windows和Ubuntu都能用！这就是CMake的好处）
find_package(OpenCV REQUIRED)

# ========== 新增的拷贝代码在这里 ==========
file(COPY ${CMAKE_SOURCE_DIR}/assets/images/ DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/assets/videos/ DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/assets/models/ DESTINATION ${CMAKE_BINARY_DIR})

# 把头文件路径加入
include_directories(${OpenCV_INCLUDE_DIRS})

# 生成可执行文件：名字叫demo，源码是main.cpp
add_executable(demo src/OpenCV练习.cpp)

# 链接OpenCV库
target_link_libraries(demo ${OpenCV_LIBS})