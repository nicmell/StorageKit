#include <QApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    // StorageKit's pickers are QFileDialog-based, so a QApplication is required.
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("StorageKitExample");
    QCoreApplication::setApplicationName("StorageKitExample");

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("StorageKitExample", "Main");

    return QApplication::exec();
}
