cmake_minimum_required(VERSION 3.16)
project(samp-linux VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_standard_project_setup()

qt_add_executable(samp-linux
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/serverinfo.h
    src/sampquery.h
    src/sampquery.cpp
    src/serverlistmodel.h
    src/serverlistmodel.cpp
    src/favoritesmanager.h
    src/favoritesmanager.cpp
    src/settingsdialog.h
    src/settingsdialog.cpp
    src/directconnectdialog.h
    src/directconnectdialog.cpp
    src/serverpropertiesdialog.h
    src/serverpropertiesdialog.cpp
    src/launcher.h
    src/launcher.cpp
    src/qutils.cpp
    resources/resources.qrc
)

target_link_libraries(samp-linux PRIVATE
    Qt6::Widgets
    Qt6::Network
)

set_target_properties(samp-linux PROPERTIES
    WIN32_EXECUTABLE FALSE
    MACOSX_BUNDLE FALSE
)

install(TARGETS samp-linux RUNTIME DESTINATION bin)
install(FILES packaging/samp-linux.desktop DESTINATION share/applications OPTIONAL)

# Install App desktop icons
install(FILES resource/icons/samp-linux.png 
        DESTINATION share/icons/hicolor/256x256/apps 
        OPTIONAL)