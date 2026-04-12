/**
 * @file main.cpp
 * @brief 程序入口文件。
 * 负责初始化 Qt、创建主窗口并启动事件循环。
 */

#include "mainwindow.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

namespace {
QString findAppIconPath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath("app_icon.ico"),
        QDir(appDir).filePath("assets/app_icon.ico"),
        QDir(appDir).filePath("../../assets/app_icon.ico"),
        QDir(appDir).filePath("../../../assets/app_icon.ico"),
        QDir(appDir).filePath("app_icon.png"),
        QDir(appDir).filePath("assets/app_icon.png"),
        QDir(appDir).filePath("../../assets/app_icon.png"),
        QDir(appDir).filePath("../../../assets/app_icon.png")
    };
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    return QString();
}
}

int main(int argc, char *argv[]) {
#ifdef Q_OS_WIN
    using SetAppIdFn = HRESULT (WINAPI *)(PCWSTR);
    HMODULE shell32 = GetModuleHandleW(L"shell32.dll");
    if (shell32) {
        auto setAppId = reinterpret_cast<SetAppIdFn>(GetProcAddress(shell32, "SetCurrentProcessExplicitAppUserModelID"));
        if (setAppId) {
            setAppId(L"ORI2333.EEN1071Ass2");
        }
    }
#endif

    // 创建 Qt 应用对象，负责 GUI 生命周期管理。
    QApplication a(argc, argv);

    const QString iconPath = findAppIconPath();
    if (!iconPath.isEmpty()) {
        a.setWindowIcon(QIcon(iconPath));
    }

    // 创建主窗口对象。
    MainWindow w;

    // 显示主窗口。
    w.show();

    // 进入事件循环，直到程序退出。
    return a.exec();
}
