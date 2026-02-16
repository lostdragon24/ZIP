QT       += core gui sql

greaterThan(QT_MAJOR_VERSION, 5): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    database.cpp

HEADERS += \
    mainwindow.h \
    database.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# Отключаем предупреждения для более чистого вывода
QMAKE_CXXFLAGS += -Wno-deprecated-declarations

# Имя выходного файла
TARGET = ZIPInventory

# Папки для промежуточных файлов
MOC_DIR = .moc
OBJECTS_DIR = .obj
RCC_DIR = .rcc
UI_DIR = .ui

# Папка для выходного файла
DESTDIR = $$OUT_PWD

# Настройки для разных систем
win32 {
    # Для Windows
    # RC_ICONS = app.ico
}

macx {
    # Для macOS
    # ICON = app.icns
}

unix:!macx {
    # Для Linux
    QMAKE_CXXFLAGS += -std=c++17
}

# Включаем отладку
CONFIG += debug
