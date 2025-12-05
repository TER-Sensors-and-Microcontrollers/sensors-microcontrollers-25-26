#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QGridLayout>
#include <QFrame>
#include <QPalette>
#include <QFont>
#include <QString>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // --- 1. Initialize and Configure GUI ---
    setWindowTitle("Electric Racing Telemetry Dashboard");
    
    // Set a dark, high-contrast palette for dashboard look
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(45, 45, 50));
    darkPalette.setColor(QPalette::WindowText, QColor(255, 255, 255));
    darkPalette.setColor(QPalette::Base, QColor(30, 30, 30));
    darkPalette.setColor(QPalette::Text, QColor(255, 255, 255));
    setPalette(darkPalette);
    
    // Set up the central widget and layout
    QWidget *centralWidget = new QWidget(this);
    QGridLayout *layout = new QGridLayout(centralWidget);
    
    // --- 2. Create Engine Temperature Gauge/Display ---
    QFrame *tempFrame = new QFrame(centralWidget);
    tempFrame->setFrameShape(QFrame::Box);
    tempFrame->setFrameShadow(QFrame::Raised);
    tempFrame->setLineWidth(2);
    tempFrame->setPalette(QPalette(QColor(60, 60, 70)));
    tempFrame->setAutoFillBackground(true);
    tempFrame->setMinimumSize(300, 200);

    QGridLayout *frameLayout = new QGridLayout(tempFrame);

    // Title
    QLabel *titleLabel = new QLabel("ENGINE TEMP", tempFrame);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #61afef;"); 
    frameLayout->addWidget(titleLabel, 0, 0, 1, 2);

    // Value Display (The main temperature number)
    m_tempLabel = new QLabel("---", tempFrame);
    QFont valueFont = m_tempLabel->font();
    valueFont.setPointSize(72);
    valueFont.setBold(true);
    m_tempLabel->setFont(valueFont);
    m_tempLabel->setAlignment(Qt::AlignCenter);
    m_tempLabel->setStyleSheet("color: #e5c07b;");
    frameLayout->addWidget(m_tempLabel, 1, 0, 1, 2); 
    
    // Unit Label
    QLabel *unitLabel = new QLabel("°C", tempFrame);
    unitLabel->setFont(QFont("Arial", 24));
    unitLabel->setAlignment(Qt::AlignCenter);
    unitLabel->setStyleSheet("color: #98c379;"); 
    frameLayout->addWidget(unitLabel, 2, 0, 1, 2);

    layout->addWidget(tempFrame, 0, 0, Qt::AlignCenter);
    
    setCentralWidget(centralWidget);
    resize(800, 600); // Set initial window size

    // --- 3. Initialize CAN Data Bridge and Connect Signals ---
    // The bridge starts the polling timer and prepares the data.
    m_canBridge = new CanBridge(this); 
    
    // Connect the signal from the bridge to the slot in the main window
    connect(m_canBridge, &CanBridge::engineTempChanged, 
            this, &MainWindow::updateEngineTempDisplay);

    // Trigger an initial update to set the starting value
    m_canBridge->pollAndUpdate(); 
}

MainWindow::~MainWindow()
{
    // The m_canBridge is parented to 'this', so it should be deleted automatically
    delete ui;
}

// --- SLOT Implementation ---
void MainWindow::updateEngineTempDisplay(float newTemp) {
    if (std::isnan(newTemp)) {
        m_tempLabel->setText("N/A");
    } else {
        // Format the temperature to one decimal place
        m_tempLabel->setText(QString::number(newTemp, 'f', 1));
        
        // Optional: Change color based on warning level (e.g., above 95C is critical)
        if (newTemp > 95.0f) {
             m_tempLabel->setStyleSheet("color: #e06c75;"); // Red (Warning)
        } else if (newTemp > 85.0f) {
             m_tempLabel->setStyleSheet("color: #e5c07b;"); // Yellow (Warm)
        } else {
             m_tempLabel->setStyleSheet("color: #98c379;"); // Green (Normal)
        }
    }
}