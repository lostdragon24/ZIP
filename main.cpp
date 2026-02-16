#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Настройка кодировки для поддержки русского языка
    QApplication::setApplicationName("Учет ЗИП");
    QApplication::setOrganizationName("MyCompany");

    // Принудительно устанавливаем Qt стиль
    // Попробуйте разные стили:
    // - "Fusion" - кросс-платформенный стиль Qt
    // - "Windows" - стиль Windows
    // - "WindowsVista" - стиль Windows Vista
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // Опционально: установка палитры для единообразия
    /*
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(53,53,53));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Base, QColor(25,25,25));
    palette.setColor(QPalette::AlternateBase, QColor(53,53,53));
    palette.setColor(QPalette::ToolTipBase, Qt::white);
    palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Button, QColor(53,53,53));
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, QColor(42, 130, 218));
    palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    palette.setColor(QPalette::HighlightedText, Qt::black);
    a.setPalette(palette);
    */

    // Или просто используйте системный стиль, но с настройками
    // a.setStyleSheet("QComboBox { min-width: 200px; }");

    MainWindow w;
    w.show();
    return a.exec();
}
