#include "MultiCam.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MultiCam w;
    w.show();
    return a.exec();
}
