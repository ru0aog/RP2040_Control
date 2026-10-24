#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "spectrumwidget.h"

#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QMessageBox>
#include <QDateTime>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

// ============================================================================
// FFT: radix-2, in-place
// ============================================================================

static void calculateFFT(
    QVector<std::complex<float>>& data)
{
    const int n = data.size();

    if (n < 2 || (n & (n - 1)) != 0)
        return;

    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;

        for (; j & bit; bit >>= 1)
            j ^= bit;

        j ^= bit;

        if (i < j)
            std::swap(data[i], data[j]);
    }

    constexpr float PI = 3.14159265358979323846f;

    for (int len = 2; len <= n; len <<= 1) {
        const float angle = -2.0f * PI / float(len);

        const std::complex<float> wlen(
            std::cos(angle),
            std::sin(angle));

        for (int i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);

            for (int j = 0; j < len / 2; ++j) {
                const auto u = data[i + j];
                const auto v = data[i + j + len / 2] * w;

                data[i + j] = u + v;
                data[i + j + len / 2] = u - v;

                w *= wlen;
            }
        }
    }
}

// ============================================================================
// Виджет графика спектра
// ============================================================================

class SpectrumPlot : public QWidget
{
public:
    explicit SpectrumPlot(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(180);
        setSizePolicy(
            QSizePolicy::Expanding,
            QSizePolicy::Expanding);
    }

    void setSpectrum(
        const QVector<float>& values,
        int rate)
    {
        spectrum = values;
        sampleRate = rate;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        p.fillRect(rect(), QColor("#071017"));

        const int left = 52;
        const int right = width() - 12;
        const int top = 12;
        const int bottom = height() - 30;

        const int graphWidth = right - left;
        const int graphHeight = bottom - top;

        if (graphWidth <= 0 || graphHeight <= 0)
            return;

        // Сетка по уровню.
        p.setFont(QFont("Segoe UI", 8));

        for (int db = 0; db >= -120; db -= 20) {
            const int y = bottom -
                ((db + 120) * graphHeight / 120);

            p.setPen(QColor("#263945"));
            p.drawLine(left, y, right, y);

            p.setPen(QColor("#9BAFBD"));
            p.drawText(
                2, y - 5, left - 8, 14,
                Qt::AlignRight | Qt::AlignVCenter,
                QString::number(db));
        }

        // Частотная сетка 0...8000 Гц.
        for (int hz = 0; hz <= 8000; hz += 1000) {
            const int x = left +
                hz * graphWidth / 8000;

            p.setPen(QColor("#263945"));
            p.drawLine(x, top, x, bottom);

            p.setPen(QColor("#9BAFBD"));
            p.drawText(
                x - 24, bottom + 5, 48, 18,
                Qt::AlignHCenter | Qt::AlignTop,
                QString::number(hz));
        }

        if (spectrum.isEmpty() || sampleRate <= 0)
            return;

        QPainterPath path;
        bool started = false;

        const int bins = spectrum.size();

        for (int x = 0; x < graphWidth; ++x) {
            const float frequency =
                8000.0f * float(x) / float(graphWidth);

            if (frequency > sampleRate / 2.0f)
                break;

            const int bin = std::clamp(
                int(frequency * FFT_SIZE_LOCAL / sampleRate),
                0,
                bins - 1);

            const float db = std::clamp(
                spectrum[bin], -120.0f, 0.0f);

            const int y = bottom -
                int((db + 120.0f) * graphHeight / 120.0f);

            if (!started) {
                path.moveTo(left + x, y);
                started = true;
            }
            else {
                path.lineTo(left + x, y);
            }
        }

        p.setPen(QPen(QColor("#00D7BA"), 1.5));
        p.drawPath(path);

        p.setPen(QColor("#69D8C5"));
        p.drawText(
            left, 1, graphWidth, 12,
            Qt::AlignLeft | Qt::AlignVCenter,
            "Уровень, dBFS");

        p.drawText(
            left, height() - 15, graphWidth, 14,
            Qt::AlignHCenter | Qt::AlignVCenter,
            "Частота, Гц");
    }

private:
    static constexpr int FFT_SIZE_LOCAL = 2048;

    QVector<float> spectrum;
    int sampleRate = 16000;
};

// ============================================================================
// AudioSpectrumWidget
// ============================================================================

AudioSpectrumWidget::AudioSpectrumWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    auto* controls = new QHBoxLayout();

    deviceCombo = new QComboBox(this);
    startButton = new QPushButton("Начать захват", this);
    statusLabel = new QLabel("Захват остановлен", this);

    controls->addWidget(new QLabel("Аудиовход:"));
    controls->addWidget(deviceCombo, 1);
    controls->addWidget(startButton);
    controls->addWidget(statusLabel);

    plot = new SpectrumPlot(this);

    layout->addLayout(controls);
    layout->addWidget(plot, 1);

    updateTimer = new QTimer(this);
    updateTimer->setInterval(50);

    connect(updateTimer, &QTimer::timeout,
        this, &AudioSpectrumWidget::updateSpectrum);

    connect(startButton, &QPushButton::clicked,
        this, &AudioSpectrumWidget::toggleCapture);

    connect(deviceCombo,
        &QComboBox::currentIndexChanged,
        this, [this](int) {
            if (capturing)
                stopCapture();
        });

    refreshDevices();

    spectrumDb.fill(-120.0f, FFT_SIZE / 2);
    plot->setSpectrum(spectrumDb, sampleRate);
}

AudioSpectrumWidget::~AudioSpectrumWidget()
{
    stopCapture();
}

// ============================================================================
// Перечисление устройств записи Windows
// ============================================================================

void AudioSpectrumWidget::refreshDevices()
{
    if (capturing)
        stopCapture();

    deviceCombo->clear();

    const UINT count = waveInGetNumDevs();

    for (UINT i = 0; i < count; ++i) {
        WAVEINCAPSW caps{};

        if (waveInGetDevCapsW(
            i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {

            deviceCombo->addItem(
                QString::fromWCharArray(caps.szPname),
                QVariant::fromValue<quint32>(i));
        }
    }

    if (deviceCombo->count() == 0) {
        deviceCombo->addItem("Нет доступных аудиовходов");
        startButton->setEnabled(false);
        statusLabel->setText("Нет устройств");
    }
    else {
        startButton->setEnabled(true);
        statusLabel->setText(
            QString("Найдено устройств: %1").arg(count));
    }
}

// ============================================================================
// Включение / выключение
// ============================================================================

void AudioSpectrumWidget::toggleCapture()
{
    if (capturing) {
        stopCapture();
        return;
    }

    if (!startCapture()) {
        QMessageBox::warning(
            this,
            "Захват аудио",
            "Не удалось открыть устройство записи.\n"
            "Проверь настройки Windows и подключение устройства.");
    }
}

bool AudioSpectrumWidget::startCapture()
{
    if (deviceCombo->currentIndex() < 0)
        return false;

    const UINT deviceId =
        deviceCombo->currentData().toUInt();

    // Пробуем распространённые PCM-форматы.
    const int rates[] = {
        16000, 22050, 44100, 48000
    };

    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 2;
    format.cbSize = 0;

    bool opened = false;

    for (const int rate : rates) {
        format.nSamplesPerSec = DWORD(rate);
        format.nAvgBytesPerSec = DWORD(rate) * 2;

        const MMRESULT query = waveInOpen(
            nullptr,
            deviceId,
            &format,
            0,
            0,
            WAVE_FORMAT_QUERY);

        if (query != MMSYSERR_NOERROR)
            continue;

        const MMRESULT result = waveInOpen(
            &waveIn,
            deviceId,
            &format,
            reinterpret_cast<DWORD_PTR>(
                &AudioSpectrumWidget::waveInCallback),
            reinterpret_cast<DWORD_PTR>(this),
            CALLBACK_FUNCTION);

        if (result == MMSYSERR_NOERROR) {
            sampleRate = rate;
            opened = true;
            break;
        }

        waveIn = nullptr;
    }

    if (!opened)
        return false;

    {
        std::lock_guard<std::mutex> lock(sampleMutex);
        pendingSamples.clear();
    }

    // Подготавливаем буферы и передаём их драйверу.
    for (auto& buffer : buffers) {
        buffer.header = {};
        buffer.header.lpData = buffer.data.data();
        buffer.header.dwBufferLength =
            DWORD(buffer.data.size());

        MMRESULT result = waveInPrepareHeader(
            waveIn,
            &buffer.header,
            sizeof(WAVEHDR));

        if (result != MMSYSERR_NOERROR) {
            stopCapture();
            return false;
        }

        buffer.prepared = true;

        result = waveInAddBuffer(
            waveIn,
            &buffer.header,
            sizeof(WAVEHDR));

        if (result != MMSYSERR_NOERROR) {
            stopCapture();
            return false;
        }
    }

    capturing = true;

    const MMRESULT startResult = waveInStart(waveIn);

    if (startResult != MMSYSERR_NOERROR) {
        stopCapture();
        return false;
    }

    startButton->setText("Остановить");
    statusLabel->setText(
        QString("Захват: %1 Гц, mono, PCM16")
        .arg(sampleRate));

    updateTimer->start();

    return true;
}

// ============================================================================
// Остановка и освобождение ресурсов
// ============================================================================

void AudioSpectrumWidget::stopCapture()
{
    capturing = false;
    updateTimer->stop();

    if (waveIn) {
        waveInStop(waveIn);
        waveInReset(waveIn);

        for (auto& buffer : buffers) {
            if (buffer.prepared) {
                waveInUnprepareHeader(
                    waveIn,
                    &buffer.header,
                    sizeof(WAVEHDR));

                buffer.prepared = false;
            }
        }

        waveInClose(waveIn);
        waveIn = nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(sampleMutex);
        pendingSamples.clear();
    }

    startButton->setText("Начать захват");
    statusLabel->setText("Захват остановлен");
}

// ============================================================================
// Callback Windows
// ============================================================================

void CALLBACK AudioSpectrumWidget::waveInCallback(
    HWAVEIN,
    UINT message,
    DWORD_PTR instance,
    DWORD_PTR param1,
    DWORD_PTR)
{
    if (message != WIM_DATA || instance == 0)
        return;

    auto* self =
        reinterpret_cast<AudioSpectrumWidget*>(instance);

    auto* header =
        reinterpret_cast<WAVEHDR*>(param1);

    if (self && header)
        self->handleWaveBuffer(header);
}

void AudioSpectrumWidget::handleWaveBuffer(WAVEHDR* header)
{
    if (!header)
        return;

    if (capturing && header->dwBytesRecorded > 0) {
        const auto* samples =
            reinterpret_cast<const qint16*>(header->lpData);

        const int count =
            int(header->dwBytesRecorded / sizeof(qint16));

        std::lock_guard<std::mutex> lock(sampleMutex);

        for (int i = 0; i < count; ++i)
            pendingSamples.append(samples[i]);

        // Не позволяем очереди расти бесконечно.
        constexpr int MAX_PENDING = 32768;

        if (pendingSamples.size() > MAX_PENDING) {
            const int excess =
                pendingSamples.size() - MAX_PENDING;

            pendingSamples.remove(0, excess);
        }
    }

    if (capturing && waveIn) {
        header->dwBytesRecorded = 0;

        waveInAddBuffer(
            waveIn,
            header,
            sizeof(WAVEHDR));
    }
}

// ============================================================================
// FFT и обновление графика
// ============================================================================

void AudioSpectrumWidget::updateSpectrum()
{
    QVector<qint16> input;

    {
        std::lock_guard<std::mutex> lock(sampleMutex);

        if (pendingSamples.size() < FFT_SIZE)
            return;

        const int start =
            pendingSamples.size() - FFT_SIZE;

        input.reserve(FFT_SIZE);

        for (int i = start; i < pendingSamples.size(); ++i)
            input.append(pendingSamples[i]);

        // Сохраняем часть буфера для перекрытия окон.
        constexpr int KEEP = FFT_SIZE / 2;

        if (pendingSamples.size() > KEEP)
            pendingSamples.remove(
                0, pendingSamples.size() - KEEP);
    }

    QVector<std::complex<float>> data(FFT_SIZE);

    // Убираем постоянную составляющую.
    double mean = 0.0;

    for (qint16 sample : input)
        mean += sample;

    mean /= double(FFT_SIZE);

    // Окно Ханна уменьшает спектральные утечки.
    constexpr float PI = 3.14159265358979323846f;

    for (int i = 0; i < FFT_SIZE; ++i) {
        const float window =
            0.5f - 0.5f * std::cos(
                2.0f * PI * float(i) /
                float(FFT_SIZE - 1));

        const float sample =
            (float(input[i]) - float(mean)) / 32768.0f;

        data[i] = std::complex<float>(
            sample * window, 0.0f);
    }

    calculateFFT(data);

    spectrumDb.resize(FFT_SIZE / 2);

    for (int i = 0; i < FFT_SIZE / 2; ++i) {
        const float magnitude =
            std::abs(data[i]) * (2.0f / float(FFT_SIZE));

        const float db =
            20.0f * std::log10(
                std::max(magnitude, 1.0e-6f));

        spectrumDb[i] = std::clamp(db, -120.0f, 0.0f);
    }

    plot->setSpectrum(spectrumDb, sampleRate);
}