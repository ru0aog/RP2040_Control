#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <QWidget>
#include <QVector>
#include <QByteArray>
#include <QTimer>

class QSlider;

#include <windows.h>
#include <mmsystem.h>

#include <fstream>
#include <array>
#include <atomic>
#include <mutex>

class QComboBox;
class QPushButton;
class QLabel;
class QPaintEvent;
class QResizeEvent;
class QMouseEvent;
class QWheelEvent;

class SpectrumPlot;

class AudioSpectrumWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AudioSpectrumWidget(QWidget* parent = nullptr);
    ~AudioSpectrumWidget() override;

    void startCaptureIfIdle();
    void stopCaptureIfActive();
    void startPlaybackIfIdle();

private:
    static void CALLBACK waveInCallback(
        HWAVEIN hwi,
        UINT message,
        DWORD_PTR instance,
        DWORD_PTR param1,
        DWORD_PTR param2);

    static void CALLBACK waveOutCallback(
        HWAVEOUT hwo,
        UINT message,
        DWORD_PTR instance,
        DWORD_PTR param1,
        DWORD_PTR param2);

    void refreshDevices();
    void toggleCapture();
    bool startCapture();
    void stopCapture();

    void handleWaveBuffer(WAVEHDR* header);
    void updateSpectrum();

    void togglePlayback();
    bool startPlayback();
    void stopPlayback();
    void requestStop();   // безопасный запрос остановки из любого потока
    void applyVolume();

    void toggleRecording();
    void startRecording();
    void stopRecording();
    void promptSaveRecording();

    QPushButton* recordButton = nullptr;

    std::ofstream recordFile;   // требует #include <fstream>  
    std::mutex recordMutex;
    std::atomic_bool recording{ false };
    quint32 recordedDataBytes = 0;
    QString recordTempPath;

    QComboBox* deviceCombo = nullptr;
    QPushButton* startButton = nullptr;
    QPushButton* playbackButton = nullptr;
    QLabel* statusLabel = nullptr;
    SpectrumPlot* plot = nullptr;
    QSlider* volumeSlider = nullptr;
    QTimer* updateTimer = nullptr;

    HWAVEIN waveIn = nullptr;
    HWAVEOUT waveOut = nullptr;

    struct AudioBuffer
    {
        WAVEHDR header{};
        std::array<char, 4096> data{};
        bool prepared = false;
    };

    struct PlaybackBuffer
    {
        WAVEHDR header{};
        std::array<char, 4096> data{};
        std::atomic_bool busy{ false };
        bool prepared = false;
    };

    static constexpr int BUFFER_COUNT = 4;
    static constexpr int PLAYBACK_BUFFER_COUNT = 4;
    std::array<AudioBuffer, BUFFER_COUNT> buffers{};
    std::array<PlaybackBuffer, PLAYBACK_BUFFER_COUNT>
        playbackBuffers{};

    std::atomic_bool capturing{ false };
    std::atomic_bool stopRequested{ false };     // защита от повторной остановки  
    std::atomic<qint64> lastBufferMs{ 0 };       // время последнего WIM_DATA  
    std::mutex sampleMutex;
    std::atomic_bool playbackEnabled{ false };
    std::mutex playbackMutex;

    QVector<qint16> pendingSamples;
    QVector<float> spectrumDb;
    QVector<float> spectrumAvg;      // накопленное усреднённое значение, дБ  

    int sampleRate = 16000;

    static constexpr int FFT_SIZE = 8192;

    // Коэффициент сглаживания EMA: 0..1.  
    // Меньше — сильнее усреднение (плавнее, но инертнее).  
    static constexpr float AVG_ALPHA = 0.3f;
};