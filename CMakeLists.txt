cmake_minimum_required(VERSION 3.10)
project(hazard-1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 5.14 COMPONENTS Widgets REQUIRED)
find_package(OpenCV REQUIRED)

add_executable(hazard-1
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/DetectionEngine.h
    src/DetectionEngine.cpp
    src/DetectionWorker.h
    src/DetectionWorker.cpp
    src/AlertManager.h
    src/AlertManager.cpp
    src/Detection.h
)

target_include_directories(hazard-1 PRIVATE ${OpenCV_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(hazard-1 PRIVATE Qt5::Widgets ${OpenCV_LIBS})

if(UNIX)
    target_compile_definitions(hazard-1 PRIVATE HAZARD1_PLATFORM_LINUX)
endif()

# Keep a runnable copy of the model next to the binary for convenience during
# local testing (packaging for the Pi is handled separately, see docs/README).
add_custom_command(TARGET hazard-1 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/models"
        "$<TARGET_FILE_DIR:hazard-1>/models"
)

install(TARGETS hazard-1 RUNTIME DESTINATION bin)
install(DIRECTORY models/ DESTINATION bin/models)
