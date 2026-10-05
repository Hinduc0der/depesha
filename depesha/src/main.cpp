#include <QApplication>
#include <QPixmapCache>

#include "kglog.h"
#include "mainwindow.h"

// VERSION приходит из .pro: на symbian-abld без кавычек, поэтому строится
// здесь -- как и в Kutegram.
#define KG_STR2(x) #x
#define KG_STR(x) KG_STR2(x)

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QApplication::setApplicationName("Depesha");
    QApplication::setOrganizationName("Depesha");
    QApplication::setOrganizationDomain("depesha.local");
    QApplication::setApplicationVersion(QString(KG_STR(VERSION)).replace("\"", ""));

#ifdef Q_OS_SYMBIAN
    // Ровно наоборот к Kutegram: там клавиатурная навигация Qt мешала, потому
    // что весь интерфейс был одной сценой QML. Здесь интерфейс -- обычные
    // виджеты, и эта навигация и есть то, ради чего всё переписано.
    QApplication::setNavigationMode(Qt::NavigationModeKeypadDirectional);
#endif

    // Кэш уменьшенных картинок: аватарки (тридцатка -- 4 КБ) и миниатюры
    // сообщений (девяносто шесть точек -- 36 КБ). При 256 КБ миниатюры
    // вытесняли друг друга на каждой прокрутке, и фотография разворачивалась
    // заново на каждую перерисовку -- это и была медленная прокрутка по чату с
    // фотографиями. Мегабайта хватает примерно на два экрана.
    QPixmapCache::setCacheLimit(1024);

    kgLog("Depesha " + QApplication::applicationVersion() + " starting");

    MainWindow window;

#ifdef Q_OS_SYMBIAN
    // Не showFullScreen: во весь экран Symbian убирает и полосу софт-клавиш,
    // а правая софт-клавиша здесь -- единственный выход назад.
    window.showMaximized();
#else
    window.resize(240, 320);
    window.show();
#endif

    return app.exec();
}
