#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel> // We'll use QLabel for the display
#include "../backend/can_data_reader_qt_bridge.h" // Include the data bridge

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Slot to receive updates from the CanBridge
    void updateEngineTempDisplay(float newTemp);

private:
    Ui::MainWindow *ui;
    
    // Pointer to the CAN data bridge
    CanBridge *m_canBridge; 
    
    // UI element to display the temperature
    QLabel *m_tempLabel;
};
#endif // MAINWINDOW_H