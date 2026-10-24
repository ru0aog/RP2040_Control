
#include <QApplication>
#include <QScreen>
#include "mainwindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.resize(630, window.minimumSizeHint().height());

    // Центрируем окно на экране.  
    const QRect screen = window.screen()->availableGeometry();
    QRect frame = window.frameGeometry();
    frame.moveCenter(screen.center());
    window.move(frame.topLeft());

    window.show();
    return app.exec();
}
