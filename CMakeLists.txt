cmake_minimum_required(VERSION 3.14)

project(fitness-tracker-userver CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(userver COMPONENTS core postgresql REQUIRED)

add_executable(${PROJECT_NAME}
    src/api_utils.cpp
    src/fitness_storage.cpp
    src/handlers.cpp
    src/main.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE userver::postgresql)
