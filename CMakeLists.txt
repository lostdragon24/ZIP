cmake_minimum_required(VERSION 3.16)
project(ZIPInventory VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Настройка Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql)
set(QT_LIBRARIES Qt6::Core Qt6::Widgets Qt6::Sql)

# Исходные файлы
set(SOURCES
    main.cpp
    mainwindow.cpp
    database.cpp
    materialmodel.cpp
)

# Заголовочные файлы
set(HEADERS
    mainwindow.h
    database.h
    materialmodel.h
)

# Файлы форм
set(FORMS
    mainwindow.ui
)

# Ресурсы
set(RESOURCES
    resources.qrc
)

# Автоматическая обработка MOC, UIC и RCC
qt_add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${FORMS} ${RESOURCES})
qt_generate_deploy_app_script(${PROJECT_NAME})

# Подключение библиотек Qt
target_link_libraries(${PROJECT_NAME} PRIVATE ${QT_LIBRARIES})
