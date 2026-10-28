cmake_minimum_required(VERSION 3.22)
project(CitronGraphicPacksChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTORCC_OPTIONS --compress-algo zlib)
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent Test)
set(PACK_DIR "${CMAKE_CURRENT_LIST_DIR}/../../src/citron/graphic_packs")
add_library(graphic_packs STATIC
    ${PACK_DIR}/pack.cpp ${PACK_DIR}/pack.h
    ${PACK_DIR}/catalog.cpp ${PACK_DIR}/catalog.h
    ${PACK_DIR}/installer.cpp ${PACK_DIR}/installer.h
    ${PACK_DIR}/dialog.cpp ${PACK_DIR}/dialog.h
    ${PACK_DIR}/nx_optimizer.cpp ${PACK_DIR}/nx_optimizer.h)
target_include_directories(graphic_packs PUBLIC ${PACK_DIR})
target_compile_definitions(graphic_packs PUBLIC QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII QT_USE_QSTRINGBUILDER)
target_link_libraries(graphic_packs PUBLIC Qt6::Widgets Qt6::Network Qt6::Concurrent)
add_executable(graphic_packs_tests tests.cpp ${PACK_DIR}/resources.qrc)
target_link_libraries(graphic_packs_tests PRIVATE graphic_packs Qt6::Test)
add_executable(graphic_packs_preview preview.cpp ${PACK_DIR}/resources.qrc)
target_link_libraries(graphic_packs_preview PRIVATE graphic_packs)
enable_testing()
add_test(NAME graphic_packs COMMAND graphic_packs_tests -platform offscreen)
