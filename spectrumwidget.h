#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <QWidget>
#include <QVector>
#include <QByteArray>
#include <QTimer>

#include <windows.h>
#include <mmsystem.h>

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

    QComboBox* deviceCombo = nullptr;
    QPushButton* startButton = nullptr;
    QPushButton* playbackButton = nullptr;
    QLabel* statusLabel = nullptr;
    SpectrumPlot* plot = nullptr;
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
    std::mutex sampleMutex;
    std::atomic_bool playbackEnabled{ false };
    std::mutex playbackMutex;

    QVector<qint16> pendingSamples;
    QVector<float> spectrumDb;

    int sampleRate = 16000;

    static constexpr int FFT_SIZE = 2048;
};