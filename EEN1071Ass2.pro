#-------------------------------------------------------------------------
# EEN1071 项目配置文件
# 该文件定义 qmake 使用的模块、源码与链接库。
#-------------------------------------------------------------------------

# 添加核心 Qt 模块（逻辑与界面）
QT       += core gui widgets printsupport network

# 显式添加 widgets，兼容较新 Qt 版本
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 使用 C++17 标准
CONFIG += c++17
CONFIG += utf8_source

# MinGW 下统一使用 UTF-8，避免中文乱码
win32-g++ {
    QMAKE_CFLAGS   += -finput-charset=UTF-8 -fexec-charset=UTF-8
    QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
}

# 如需限制过时 Qt API，可启用下一行
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000

# 需要编译的 C++ 源文件
SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/qcustomplot.cpp

# 头文件列表
HEADERS += \
    include/mainwindow.h \
    include/qcustomplot.h

# Qt Designer 生成的 UI 文件
FORMS += \
    ui/mainwindow.ui

INCLUDEPATH += \
    include \
    src \
    ui

win32 {
    RC_ICONS = assets/app_icon.ico
}

# 外部库链接：Paho MQTT C 客户端库
LIBS  += -lpaho-mqtt3c

# 部署规则：定义不同系统下的安装路径
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    README.md \
    assets/app_icon.ico \
    assets/README.md
