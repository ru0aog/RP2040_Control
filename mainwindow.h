#pragma once

#include <QMainWindow>
#include <QString>
#include <QByteArray>
#include <windows.h>
#include <atomic>
#include <thread>

class QComboBox;
class QPushButton;
class QLineEdit;
class QPlainTextEdit;
class QLabel;
class QTimer;
class QButtonGroup;
class FrequencyDisplay;
class BandwidthSlider;
class AudioSpectrumWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void refreshPorts();
    void toggleConnection();
    void sendCommand();
    void tuneFrequency(int direction);
    void changeMode(int mode);
    void sendFrequency();
    void sendLowCut(int hz);
    void sendHighCut(int hz);
    void pollRadio();

private:
    void appendLog(const QString& text);
    void readerLoop(HANDLE handle);
    void closePort();
    void sendRawCommand(const QString& command);
    void processReceived(const QByteArray& data);
    void requestRadioState();
    void updateFrequency(quint32 hz);
    void updateMode(int mode);
    void updateBand(int low, int high);

    QComboBox* portCombo = nullptr;
    QComboBox* baudCombo = nullptr;
    QPushButton* connectButton = nullptr;
    QPushButton* refreshButton = nullptr;
    QLineEdit* commandEdit = nullptr;
    QPushButton* sendButton = nullptr;
    QPlainTextEdit* logView = nullptr;
    QLabel* statusLabel = nullptr;
    QLabel* bandwidthLabel = nullptr;
    QLabel* stepLabel = nullptr;

    FrequencyDisplay* frequencyDisplay = nullptr;
    BandwidthSlider* bandwidthSlider = nullptr;
    AudioSpectrumWidget* audioSpectrumWidget = nullptr;

    QButtonGroup* modeGroup = nullptr;
    QTimer* pollTimer = nullptr;

    HANDLE serialHandle = INVALID_HANDLE_VALUE;
    std::atomic<bool> readerRunning{ false };
    std::thread readerThread;

    QByteArray rxPending;
    quint32 currentFrequency = 3600000;
    int currentMode = 2;
    int lowCutHz = 300;
    int highCutHz = 2700;
    int tuningStep = 100;
    bool updatingUi = false;
};
