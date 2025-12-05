cmake_minimum_required(VERSION 3.19)
project(Frontend LANGUAGES CXX)

# Find necessary Qt components (Core and Widgets are already here)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

# Add SQLite library for the CAN reader logic
find_library(SQLITE_LIB sqlite3)
if(NOT SQLITE_LIB)
    message(FATAL_ERROR "SQLite (libsqlite3) not found. Please ensure it is installed.")
endif()

qt_standard_project_setup()

# Define the source files for the executable
qt_add_executable(Frontend
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    
    # ----------------------------------------------------
    # Include CAN Reader logic from the ../backend directory
    # FIX: Added can_data_reader_qt_bridge.h to enable MOC processing.
    # ----------------------------------------------------
    ../backend/can_data_reader.cpp
    ../backend/can_data_reader.h 
    ../backend/can_data_reader_qt_bridge.h
)

target_link_libraries(Frontend
    PRIVATE
        Qt::Core
        Qt::Widgets
        
        # ----------------------------------------------------
        # Link the SQLite library (REQUIRED by can_data_reader.cpp)
        # ----------------------------------------------------
        ${SQLITE_LIB}
)

# ----------------------------------------------------
# Define include directories so the frontend can find the backend's headers
# ----------------------------------------------------
target_include_directories(Frontend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../backend)

include(GNUInstallDirs)

install(TARGETS Frontend
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Frontend
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})