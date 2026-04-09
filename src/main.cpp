/**
 * @file main.cpp
 * @brief Entry point for the EEN1071 Assignment 2 application.
 * * This file handles the initial startup of the Qt framework,
 * instantiates the main window, and starts the event loop.
 */

#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    // QApplication manages GUI application control flow and main settings.
    // It handles initialization and finalization.
    QApplication a(argc, argv);

    // Instantiate the MainWindow object defined in mainwindow.h
    MainWindow w;

    // By default, widgets are hidden. show() makes the window visible to the user.
    w.show();

    // Enters the main event loop and waits until exit() is called.
    // The return value ensures the OS receives the correct exit status.
    return a.exec();
}
