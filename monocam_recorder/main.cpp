#include "MutiCam.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MutiCam w;
    w.show();
    return a.exec();
}