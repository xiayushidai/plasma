#include "kwin_wl_backend.h"
#include <QApplication>
#include "kwin_wl_backend.h"
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    KWinWaylandBackend *backend=new KWinWaylandBackend();
    qDebug()<<"hello";
   // Widget w;
   //w.show();

    return a.exec();
}
