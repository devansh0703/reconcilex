cmake_minimum_required(VERSION 3.20)
project(ReconcileX VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SQLite3 REQUIRED)

add_library(recon_core STATIC
    src/trade_store.cpp
    src/matching_engine.cpp
    src/exception_workflow.cpp
    src/settlement.cpp
    src/fix_handler.cpp
    src/reporting.cpp
)

target_include_directories(recon_core PUBLIC include)
target_link_libraries(recon_core PUBLIC SQLite::SQLite3)

add_executable(reconcilex src/main.cpp)
target_link_libraries(reconcilex PRIVATE recon_core)

enable_testing()
add_subdirectory(test)
