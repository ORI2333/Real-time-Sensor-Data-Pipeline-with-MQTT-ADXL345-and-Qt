/**
 * @file main.cpp
 * @brief Entry point for the EEN1071 Assignment 2 application.
 * * This file handles the initial startup of the Qt framework,
 * instantiates the main window, and starts the event loop.
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

    // QApplication manages GUI application control flow and main settings.
    // It handles initialization and finalization.
    QApplication a(argc, argv);

    const QString iconPath = findAppIconPath();
    if (!iconPath.isEmpty()) {
        a.setWindowIcon(QIcon(iconPath));
    }

    // Instantiate the MainWindow object defined in mainwindow.h
    MainWindow w;

    // By default, widgets are hidden. show() makes the window visible to the user.
    w.show();

    // Enters the main event loop and waits until exit() is called.
    // The return value ensures the OS receives the correct exit status.
    return a.exec();
}
