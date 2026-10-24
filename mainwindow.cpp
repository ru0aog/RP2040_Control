
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "mainwindow.h"
#include "spectrumwidget.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDateTime>
#include <QScrollBar>
#include <QMessageBox>
#include <QMetaObject>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QTimer>
#include <QButtonGroup>
#include <QAbstractButton>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFontMetrics>
#include <QSignalBlocker>
#include <QApplication>
#include <QClipboard>
#include <QTextCursor>
#include <QQueue>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <utility>

// ============================================================================
// Индикатор частоты: клик выбирает разряд, колесо мыши изменяет частоту.
// ============================================================================

class FrequencyDisplay : public QWidget
{
public:
    explicit FrequencyDisplay(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(105);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setToolTip(
            "Клик по цифре выбирает шаг; колесо мыши меняет частоту");
    }

    void setFrequency(quint32 hz)
    {
        frequency = hz;
        update();
    }

    quint32 getFrequency() const
    {
        return frequency;
    }

    int getStep() const
    {
        if (selectedDigit < 0 || selectedDigit >= 8)
            return 100;

        return steps[selectedDigit];
    }

    void setFrequencyCallback(std::function<void(quint32)> cb)
    {
        frequencyCallback = std::move(cb);
    }

    void setStepCallback(std::function<void(int)> cb)
    {
        stepCallback = std::move(cb);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#071017"));

        const QString digits =
            QString("%1").arg(frequency, 8, 10, QChar('0'));

        const QFont font("Consolas", 34, QFont::DemiBold);
        p.setFont(font);

        const QFontMetrics fm(font);
        const int charWidth = fm.horizontalAdvance('0');
        const int totalWidth = charWidth * 8;
        const int startX = (width() - totalWidth) / 2;
        const int baseY = height() / 2 + fm.ascent() / 2 - 5;

        for (int i = 0; i < 8; ++i) {
            const QRect r(
                startX + i * charWidth,
                12,
                charWidth,
                height() - 24);

            if (i == selectedDigit) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor("#00BFA5"));
                p.drawRoundedRect(
                    r.adjusted(1, 0, -1, 0), 5, 5);
            }

            p.setPen(i == selectedDigit
                ? QColor("#FFFFFF")
                : QColor("#E6EDF3"));

            p.drawText(r.left(), baseY, QString(digits[i]));
        }

        p.setPen(QColor("#69D8C5"));
        p.setFont(QFont("Consolas", 12));
        p.drawText(startX, height() - 7, "Hz");
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(e);
            return;
        }

        const QFont font("Consolas", 34, QFont::DemiBold);
        const QFontMetrics fm(font);

        const int charWidth = fm.horizontalAdvance('0');
        const int totalWidth = charWidth * 8;
        const int startX = (width() - totalWidth) / 2;
        const int x = static_cast<int>(e->position().x());

        if (x >= startX && x < startX + totalWidth) {
            const int digit = (x - startX) / charWidth;

            if (digit >= 0 && digit < 8) {
                selectedDigit = digit;

                if (stepCallback)
                    stepCallback(getStep());

                update();
            }
        }

        QWidget::mousePressEvent(e);
    }

    void mouseDoubleClickEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) {
            QWidget::mouseDoubleClickEvent(e);
            return;
        }

        const QFont font("Consolas", 34, QFont::DemiBold);
        const QFontMetrics fm(font);

        const int charWidth = fm.horizontalAdvance('0');
        const int totalWidth = charWidth * 8;
        const int startX = (width() - totalWidth) / 2;
        const int x = static_cast<int>(e->position().x());

        if (x >= startX && x < startX + totalWidth) {
            const int digit = (x - startX) / charWidth;

            if (digit >= 0 && digit < 8) {
                selectedDigit = digit;

                // Обнуляем выбранный разряд и все разряды справа.
                const int step = steps[digit];
                frequency = (frequency / (step * 10)) * (step * 10);

                if (frequencyCallback)
                    frequencyCallback(frequency);

                if (stepCallback)
                    stepCallback(getStep());

                update();
            }
        }

        e->accept();
    }

    void wheelEvent(QWheelEvent* e) override
    {
        const int delta = e->angleDelta().y();

        if (delta == 0) {
            e->ignore();
            return;
        }

        const int direction = delta > 0 ? 1 : -1;

        const qint64 next =
            qint64(frequency) + qint64(getStep()) * direction;

        frequency = quint32(
            std::clamp<qint64>(next, 0, 99999999));

        if (frequencyCallback)
            frequencyCallback(frequency);

        update();
        e->accept();
    }

private:
    quint32 frequency = 3600000;

    const int steps[8] = {
        10000000, 1000000, 100000, 10000,
        1000, 100, 10, 1
    };

    int selectedDigit = 5;

    std::function<void(quint32)> frequencyCallback;
    std::function<void(int)> stepCallback;
};


// ============================================================================
// Полоса 0...5000 Гц с двумя перетаскиваемыми маркерами.
// ============================================================================

class BandwidthSlider : public QWidget
{
public:
    explicit BandwidthSlider(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(60);
        setMouseTracking(true);
    }

    void setBand(int low, int high)
    {
        lo = std::clamp(low, 0, 5000);
        hi = std::clamp(high, 0, 5000);

        if (lo > hi)
            std::swap(lo, hi);

        update();
    }

    int low() const
    {
        return lo;
    }

    int high() const
    {
        return hi;
    }

    void setChangedCallback(std::function<void(int, int)> cb)
    {
        changedCallback = std::move(cb);
    }

    void setReleasedCallback(std::function<void(int, int)> cb)
    {
        releasedCallback = std::move(cb);
    }

protected:
    int xForValue(int value) const
    {
        const int left = 12;
        const int right = width() - 12;

        return left + (right - left) * value / 5000;
    }

    int valueForX(int x) const
    {
        const int left = 12;
        const int right = width() - 12;

        return std::clamp(
            (x - left) * 5000 / std::max(1, right - left),
            0,
            5000);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const int y = 24;
        const int x0 = xForValue(0);
        const int x1 = xForValue(5000);
        const int xl = xForValue(lo);
        const int xh = xForValue(hi);

        p.setPen(QPen(
            QColor("#344955"), 8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(x0, y, x1, y);

        p.setPen(QPen(
            QColor("#69D8C5"), 8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(xl, y, xh, y);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#E6EDF3"));
        p.drawEllipse(QPoint(xl, y), 8, 8);
        p.drawEllipse(QPoint(xh, y), 8, 8);

        p.setPen(QColor("#9BAFBD"));
        p.setFont(QFont("Segoe UI", 9));
        p.drawText(x0, 52, "0");
        p.drawText(x1 - 42, 52, "5000 Hz");
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(e);
            return;
        }

        const int x = static_cast<int>(e->position().x());
        const int xl = xForValue(lo);
        const int xh = xForValue(hi);

        dragLow = std::abs(x - xl) <= std::abs(x - xh);
        dragging = true;

        moveMarker(x);
        e->accept();
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (dragging && (e->buttons() & Qt::LeftButton)) {
            moveMarker(static_cast<int>(e->position().x()));
            e->accept();
            return;
        }

        QWidget::mouseMoveEvent(e);
    }

    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (!dragging || e->button() != Qt::LeftButton) {
            QWidget::mouseReleaseEvent(e);
            return;
        }

        moveMarker(static_cast<int>(e->position().x()));
        dragging = false;

        if (releasedCallback)
            releasedCallback(lo, hi);

        e->accept();
    }

private:
    void moveMarker(int x)
    {
        const int value = valueForX(x);

        if (dragLow)
            lo = (std::min)(value, hi);
        else
            hi = std::max(value, lo);

        update();

        if (changedCallback)
            changedCallback(lo, hi);
    }

    int lo = 300;
    int hi = 2700;

    bool dragging = false;
    bool dragLow = true;

    std::function<void(int, int)> changedCallback;
    std::function<void(int, int)> releasedCallback;
};


// ============================================================================
// MainWindow
// ============================================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("RP2040 Control");
    resize(1100, 820);

    auto* central = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(12);

    auto* title = new QLabel("RP2040 RECEIVER");
    title->setStyleSheet(
        "font-size: 22px; font-weight: bold; color: #69D8C5;");

    // COM-порт.
    auto* portLayout = new QHBoxLayout();

    portCombo = new QComboBox();
    baudCombo = new QComboBox();
    baudCombo->addItems({
        "9600", "19200", "38400", "57600", "115200"
        });
    baudCombo->setCurrentText("115200");

    refreshButton = new QPushButton("Обновить");
    connectButton = new QPushButton("Подключить");
    statusLabel = new QLabel("Отключено");

    portLayout->addWidget(new QLabel("COM-порт:"));
    portLayout->addWidget(portCombo, 1);
    portLayout->addWidget(new QLabel("Скорость:"));
    portLayout->addWidget(baudCombo);
    portLayout->addWidget(refreshButton);
    portLayout->addWidget(connectButton);
    portLayout->addWidget(statusLabel);

    // Частота.
    auto* freqGroup = new QGroupBox("Частота приёма · VFO A");
    auto* freqLayout = new QVBoxLayout(freqGroup);

    frequencyDisplay = new FrequencyDisplay();

    stepLabel = new QLabel("Шаг настройки: 100 Гц");
    stepLabel->setAlignment(Qt::AlignCenter);

    auto* directLayout = new QHBoxLayout();
    auto* directLabel = new QLabel("Частота, Гц:");

    auto* frequencyEdit = new QLineEdit("3600000");
    frequencyEdit->setObjectName("frequencyEdit");
    frequencyEdit->setPlaceholderText("Например, 3600000");

    auto* setFrequencyButton = new QPushButton("Установить");

    directLayout->addWidget(directLabel);
    directLayout->addWidget(frequencyEdit, 1);
    directLayout->addWidget(setFrequencyButton);

    freqLayout->addWidget(frequencyDisplay);
    freqLayout->addWidget(stepLabel);
    freqLayout->addLayout(directLayout);

    // Ручка настройки полностью удалена.

    // Режимы демодуляции.
    auto* modeGroupBox = new QGroupBox("Режим демодуляции");
    auto* modeLayout = new QVBoxLayout(modeGroupBox);

    modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);

    const struct {
        const char* text;
        int mode;
    } modes[] = {
        { "LSB", 1 },
        { "USB", 2 },
        { "CW",  3 },
        { "FM",  4 },
        { "AM",  5 }
    };

    for (const auto& item : modes) {
        auto* button = new QPushButton(item.text);
        button->setCheckable(true);
        button->setMinimumHeight(38);
        button->setProperty("radioMode", item.mode);

        modeGroup->addButton(button, item.mode);

        auto* row = new QHBoxLayout();
        row->addStretch(15);
        row->addWidget(button, 70);
        row->addStretch(15);
        modeLayout->addLayout(row);
    }

    // Фильтр.
    auto* filterGroup = new QGroupBox(
        "Полоса пропускания · 0–5000 Гц");
    auto* filterLayout = new QVBoxLayout(filterGroup);

    bandwidthLabel = new QLabel();
    bandwidthLabel->setAlignment(Qt::AlignCenter);

    bandwidthSlider = new BandwidthSlider();

    auto* filterEditLayout = new QHBoxLayout();

    auto* lowEdit = new QLineEdit("300");
    lowEdit->setObjectName("lowEdit");
    lowEdit->setMaximumWidth(100);

    auto* highEdit = new QLineEdit("2700");
    highEdit->setObjectName("highEdit");
    highEdit->setMaximumWidth(100);

    auto* narrowButton = new QPushButton("Узкая");
    auto* normalButton = new QPushButton("Норм");
    auto* wideButton = new QPushButton("Широкая");

    filterEditLayout->addWidget(new QLabel("ФВЧ, Гц:"));
    filterEditLayout->addWidget(lowEdit);
    filterEditLayout->addSpacing(20);
    filterEditLayout->addWidget(new QLabel("ФНЧ, Гц:"));
    filterEditLayout->addWidget(highEdit);
    filterEditLayout->addStretch();
    filterEditLayout->addWidget(narrowButton);
    filterEditLayout->addWidget(normalButton);
    filterEditLayout->addWidget(wideButton);

    filterLayout->addWidget(bandwidthLabel);
    filterLayout->addWidget(bandwidthSlider);

    // Аудиовход и спектр 0–8000 Гц.
    audioSpectrumWidget = new AudioSpectrumWidget(filterGroup);
    filterLayout->addWidget(audioSpectrumWidget);

    filterLayout->addLayout(filterEditLayout);

    // Журнал CAT.
    auto* logLabel = new QLabel("Журнал обмена TX / RX");

    auto* copyLogButton = new QPushButton("Копировать лог");
    auto* clearLogButton = new QPushButton("Очистить лог");

    auto* logHeaderLayout = new QHBoxLayout();
    logHeaderLayout->addWidget(logLabel);
    logHeaderLayout->addStretch();
    logHeaderLayout->addWidget(copyLogButton);
    logHeaderLayout->addWidget(clearLogButton);

    logView = new QPlainTextEdit();
    logView->setReadOnly(true);
    logView->setMaximumBlockCount(5000);
    logView->setStyleSheet(
        "background: #0B1116; color: #D6E2EA;"
        "font-family: Consolas; font-size: 12px;");

    auto* cmdLayout = new QHBoxLayout();

    commandEdit = new QLineEdit();
    commandEdit->setPlaceholderText("CAT-команда, например IF");
    sendButton = new QPushButton("Отправить");

    cmdLayout->addWidget(commandEdit, 1);
    cmdLayout->addWidget(sendButton);

    auto* topRow = new QHBoxLayout();
    topRow->addWidget(freqGroup, 5);
    topRow->addWidget(modeGroupBox, 2);

    mainLayout->addWidget(title);
    mainLayout->addLayout(portLayout);
    mainLayout->addLayout(topRow);
    mainLayout->addWidget(filterGroup);
    mainLayout->addLayout(logHeaderLayout);
    mainLayout->addWidget(logView, 1);
    mainLayout->addLayout(cmdLayout);

    setCentralWidget(central);

    setStyleSheet(
        "QMainWindow, QWidget {"
        " background: #101820; color: #E6EDF3;"
        "}"
        "QGroupBox {"
        " border: 1px solid #344955;"
        " border-radius: 8px;"
        " margin-top: 10px;"
        " padding-top: 12px;"
        " font-weight: bold;"
        "}"
        "QGroupBox::title {"
        " subcontrol-origin: margin;"
        " left: 10px;"
        " padding: 0 5px;"
        " color: #69D8C5;"
        "}"
        "QComboBox, QLineEdit {"
        " background: #1D2A34;"
        " padding: 7px;"
        " border: 1px solid #344955;"
        " border-radius: 4px;"
        "}"
        "QPushButton {"
        " background: #176B5C;"
        " padding: 8px 12px;"
        " border: none;"
        " border-radius: 5px;"
        "}"
        "QPushButton:hover {"
        " background: #218776;"
        "}"
        "QPushButton:checked {"
        " background: #00BFA5;"
        " color: #FFFFFF;"
        " border: 2px solid #FFFFFF;"
        " font-weight: bold;"
        "}"
        "QPushButton:checked:hover {"
        " background: #00D7BA;"
        "}"
    );

    bandwidthSlider->setBand(lowCutHz, highCutHz);
    updateBand(lowCutHz, highCutHz);
    frequencyDisplay->setFrequency(currentFrequency);

    frequencyDisplay->setFrequencyCallback(
        [this](quint32 hz) {
            currentFrequency = hz;

            auto* edit = findChild<QLineEdit*>("frequencyEdit");
            if (edit && !edit->hasFocus())
                edit->setText(QString::number(hz));

            sendFrequency();
        });

    frequencyDisplay->setStepCallback(
        [this](int step) {
            tuningStep = step;
            stepLabel->setText(
                "Шаг настройки: " + QString::number(step) + " Гц");
        });

    bandwidthSlider->setChangedCallback(
        [this](int lo, int hi) {
            lowCutHz = lo;
            highCutHz = hi;
            updateBand(lo, hi);
        });

    bandwidthSlider->setReleasedCallback(
        [this](int lo, int hi) {
            sendLowCut(lo);
            sendHighCut(hi);
        });

    connect(refreshButton, &QPushButton::clicked,
        this, &MainWindow::refreshPorts);

    connect(connectButton, &QPushButton::clicked,
        this, &MainWindow::toggleConnection);

    connect(sendButton, &QPushButton::clicked,
        this, &MainWindow::sendCommand);

    connect(commandEdit, &QLineEdit::returnPressed,
        this, &MainWindow::sendCommand);

    // Копировать весь журнал в буфер обмена.
    connect(copyLogButton, &QPushButton::clicked,
        this, [this]() {
            QApplication::clipboard()->setText(logView->toPlainText());
        });

    // Очистить журнал.
    connect(clearLogButton, &QPushButton::clicked,
        this, [this]() {
            logView->clear();
        });

    auto setFrequency = [this, frequencyEdit]() {
        bool ok = false;
        const quint32 hz =
            frequencyEdit->text().trimmed().toUInt(&ok);

        if (!ok || hz == 0 || hz > 99999999) {
            QMessageBox::warning(
                this,
                "Частота",
                "Введи корректную частоту от 1 до 99999999 Гц.");

            frequencyEdit->setFocus();
            frequencyEdit->selectAll();
            return;
        }

        currentFrequency = hz;
        frequencyDisplay->setFrequency(hz);
        sendFrequency();

        frequencyEdit->setText(QString::number(hz));
        frequencyEdit->setModified(false);
        };

    connect(setFrequencyButton, &QPushButton::clicked,
        this, setFrequency);

    connect(frequencyEdit, &QLineEdit::returnPressed,
        this, setFrequency);

    // Обработчики QDial удалены вместе с самой ручкой.

    connect(modeGroup, &QButtonGroup::idClicked,
        this, &MainWindow::changeMode);

    auto setBand = [this, lowEdit, highEdit]() {
        bool okLow = false;
        bool okHigh = false;

        const int lo = lowEdit->text().trimmed().toInt(&okLow);
        const int hi = highEdit->text().trimmed().toInt(&okHigh);

        if (!okLow || !okHigh ||
            lo < 0 || hi > 5000 || lo >= hi) {
            QMessageBox::warning(
                this,
                "Полоса фильтра",
                "Нужны значения 0–5000 Гц, "
                "нижняя граница меньше верхней.");

            return;
        }

        lowCutHz = lo;
        highCutHz = hi;

        bandwidthSlider->setBand(lo, hi);
        updateBand(lo, hi);
        sendLowCut(lo);
        sendHighCut(hi);

        lowEdit->setText(QString::number(lo));
        highEdit->setText(QString::number(hi));
        };

    auto applyBand = [this, lowEdit, highEdit](int lo, int hi) {
        lowCutHz = lo;
        highCutHz = hi;

        bandwidthSlider->setBand(lo, hi);
        updateBand(lo, hi);
        sendLowCut(lo);
        sendHighCut(hi);

        lowEdit->setText(QString::number(lo));
        highEdit->setText(QString::number(hi));
        };

    connect(narrowButton, &QPushButton::clicked,
        this, [applyBand]() { applyBand(300, 2200); });

    connect(normalButton, &QPushButton::clicked,
        this, [applyBand]() { applyBand(100, 2700); });

    connect(wideButton, &QPushButton::clicked,
        this, [applyBand]() { applyBand(0, 3100); });

    connect(lowEdit, &QLineEdit::returnPressed,
        this, setBand);

    connect(highEdit, &QLineEdit::returnPressed,
        this, setBand);

    // Периодический опрос отключён.
    pollTimer = nullptr;

    // Таймер последовательной отправки CAT-команд с паузой,  
    // чтобы приёмник успевал ответить на каждый запрос.  
    txTimer = new QTimer(this);  
    txTimer->setInterval(200);  
    connect(txTimer, &QTimer::timeout,  
    this, &MainWindow::sendNextQueued);

    refreshPorts();

    // Автоподключение при запуске: подключаемся к выбранному  
    // COM-порту, если это не COM1, и сразу запускаем захват звука.  
    const QString autoPort = portCombo->currentText();

    if (!autoPort.isEmpty() && autoPort != "COM1") {
        toggleConnection();

        if (serialHandle != INVALID_HANDLE_VALUE && audioSpectrumWidget)
            audioSpectrumWidget->startCaptureIfIdle();
    }
}

MainWindow::~MainWindow()
{
    closePort();
}

void MainWindow::refreshPorts()
{
    const QString previous = portCombo->currentText();
    portCombo->clear();

    for (int i = 1; i <= 256; ++i) {
        const QString name = QString("COM%1").arg(i);
        const std::wstring deviceName = name.toStdWString();
        wchar_t target[1024] = {};

        if (QueryDosDeviceW(
            deviceName.c_str(),
            target,
            static_cast<DWORD>(std::size(target))) != 0) {
            portCombo->addItem(name);
        }
    }

    int index = portCombo->findText(previous);

    if (index < 0)
        index = portCombo->findText("COM23");

    if (index >= 0)
        portCombo->setCurrentIndex(index);

    appendLog("[INFO] Найдено COM-портов: " +
        QString::number(portCombo->count()));
}

void MainWindow::toggleConnection()
{
    if (serialHandle != INVALID_HANDLE_VALUE) {
        closePort();
        return;
    }

    if (portCombo->currentText().isEmpty()) {
        QMessageBox::warning(
            this, "COM-порт", "Выбери COM-порт.");
        return;
    }

    // Правильный путь Windows: \\.\COM3
    const QString device =
        "\\\\.\\" + portCombo->currentText();

    HANDLE handle = CreateFileW(
        reinterpret_cast<LPCWSTR>(device.utf16()),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        appendLog("[ERR] Не удалось открыть порт. Код: " +
            QString::number(GetLastError()));
        return;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(handle, &dcb)) {
        appendLog("[ERR] GetCommState: " +
            QString::number(GetLastError()));
        CloseHandle(handle);
        return;
    }

    dcb.BaudRate = baudCombo->currentText().toUInt();
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;

    if (!SetCommState(handle, &dcb)) {
        appendLog("[ERR] SetCommState: " +
            QString::number(GetLastError()));
        CloseHandle(handle);
        return;
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 100;
    SetCommTimeouts(handle, &timeouts);

    serialHandle = handle;
    rxPending.clear();

    connectButton->setText("Отключить");
    statusLabel->setText("Подключено");

    appendLog("[INFO] Подключено к " + portCombo->currentText());

    readerRunning = true;
    readerThread = std::thread(&MainWindow::readerLoop, this, handle);

    requestRadioState();
}

void MainWindow::sendCommand()
{
    QString command = commandEdit->text().trimmed();

    if (command.isEmpty())
        return;

    if (!command.endsWith(';'))
        command += ';';

    sendRawCommand(command);
    commandEdit->clear();
}

void MainWindow::sendRawCommand(const QString& command)
{
    if (serialHandle == INVALID_HANDLE_VALUE) {
        appendLog("[ERR] Сначала подключись к приёмнику.");
        return;
    }

    const QByteArray data = command.toLatin1();
    DWORD written = 0;

    if (!WriteFile(
        serialHandle,
        data.constData(),
        DWORD(data.size()),
        &written,
        nullptr)) {
        appendLog("[ERR] WriteFile: " +
            QString::number(GetLastError()));
        return;
    }

    appendLog("[TX] " + command);
}

void MainWindow::readerLoop(HANDLE handle)
{
    char buffer[256];

    while (readerRunning) {
        DWORD received = 0;

        if (!ReadFile(
            handle, buffer, sizeof(buffer), &received, nullptr)) {
            if (readerRunning) {
                const DWORD error = GetLastError();

                QMetaObject::invokeMethod(
                    this,
                    [this, error]() {
                        appendLog("[ERR] ReadFile: " +
                            QString::number(error));
                    },
                    Qt::QueuedConnection);
            }

            break;
        }

        if (received > 0) {
            const QByteArray data(buffer, int(received));

            QMetaObject::invokeMethod(
                this,
                [this, data]() {
                    processReceived(data);

                    // Отдельно декодируем принятые данные для журнала.
                    appendLog("[RX] " + QString::fromUtf8(data));
                },
                Qt::QueuedConnection);
        }
    }
}

void MainWindow::closePort()  
{  
    readerRunning = false;  
  
    if (txTimer)  
        txTimer->stop();  
  
    commandQueue.clear();  
  
    if (serialHandle != INVALID_HANDLE_VALUE)  
        CancelIoEx(serialHandle, nullptr);

    if (readerThread.joinable())
        readerThread.join();

    if (serialHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(serialHandle);
        serialHandle = INVALID_HANDLE_VALUE;
    }

    if (connectButton)
        connectButton->setText("Подключить");

    if (statusLabel)
        statusLabel->setText("Отключено");

    if (logView)
        appendLog("[INFO] COM-порт закрыт");
}

void MainWindow::appendLog(const QString& text)
{
    const QString time =
        QDateTime::currentDateTime().toString("HH:mm:ss.zzz");

    logView->appendPlainText("[" + time + "] " + text);

    logView->verticalScrollBar()->setValue(
        logView->verticalScrollBar()->maximum());
}

void MainWindow::requestRadioState()  
{  
    enqueueCommand("FA;");
    enqueueCommand("FA;");
    enqueueCommand("MD;");
    enqueueCommand("MD;");
    enqueueCommand("SL;");  
    enqueueCommand("SH;");
}  
  
void MainWindow::enqueueCommand(const QString& command)  
{  
    if (serialHandle == INVALID_HANDLE_VALUE) {  
        appendLog("[ERR] Сначала подключись к приёмнику.");  
        return;  
    }  
  
    commandQueue.enqueue(command.toLatin1());  
  
    // Если очередь только что ожила — отправляем первую команду  
    // сразу, остальные уйдут по тикам таймера.  
    if (!txTimer->isActive()) {  
        sendNextQueued();  
        txTimer->start();  
    }  
}  
  
void MainWindow::sendNextQueued()  
{  
    if (commandQueue.isEmpty() ||  
        serialHandle == INVALID_HANDLE_VALUE) {  
        txTimer->stop();  
        commandQueue.clear();  
        return;  
    }  
  
    sendRawCommand(QString::fromLatin1(commandQueue.dequeue()));  
}

void MainWindow::pollRadio()
{
    if (serialHandle == INVALID_HANDLE_VALUE)
        return;

    requestRadioState();
}

void MainWindow::processReceived(const QByteArray& data)
{
    rxPending += data;

    while (true) {
        const int pos = rxPending.indexOf(';');

        if (pos < 0)
            break;

        const QByteArray packet = rxPending.left(pos).trimmed();
        rxPending.remove(0, pos + 1);

        if (packet.startsWith("FA") && packet.size() >= 3) {
            bool ok = false;
            const quint32 hz = packet.mid(2).toUInt(&ok);

            if (ok)
                updateFrequency(hz);
        }
        else if (packet.startsWith("MD") && packet.size() >= 3) {
            const int mode = packet.mid(2, 1).toInt();

            if (mode >= 1 && mode <= 5)
                updateMode(mode);
        }
        else if (packet.startsWith("SL") && packet.size() >= 3) {
            bool ok = false;
            const int lo = packet.mid(2).toInt(&ok);

            if (ok && lo >= 0 && lo <= 5000) {
                lowCutHz = lo;
                bandwidthSlider->setBand(lowCutHz, highCutHz);
                updateBand(lowCutHz, highCutHz);
            }
        }
        else if (packet.startsWith("SH") && packet.size() >= 3) {
            bool ok = false;
            const int hi = packet.mid(2).toInt(&ok);

            if (ok && hi >= 0 && hi <= 5000) {
                highCutHz = hi;
                bandwidthSlider->setBand(lowCutHz, highCutHz);
                updateBand(lowCutHz, highCutHz);
            }
        }
        else if (packet == "?") {
            appendLog("[WARN] Приёмник вернул ошибку CAT.");
        }
    }
}

void MainWindow::updateFrequency(quint32 hz)
{
    currentFrequency = hz;
    frequencyDisplay->setFrequency(hz);

    auto* edit = findChild<QLineEdit*>("frequencyEdit");

    if (edit && !edit->hasFocus())
        edit->setText(QString::number(hz));
}

void MainWindow::updateMode(int mode)
{
    currentMode = mode;

    QAbstractButton* button = modeGroup->button(mode);

    if (button) {
        const QSignalBlocker blocker(modeGroup);
        button->setChecked(true);
    }
}

void MainWindow::updateBand(int low, int high)
{
    bandwidthLabel->setText(
        QString("ФВЧ: %1 Гц     ФНЧ: %2 Гц     Ширина: %3 Гц")
        .arg(low)
        .arg(high)
        .arg(high - low));

    auto* lowEdit = findChild<QLineEdit*>("lowEdit");
    auto* highEdit = findChild<QLineEdit*>("highEdit");

    if (lowEdit && !lowEdit->hasFocus())
        lowEdit->setText(QString::number(low));

    if (highEdit && !highEdit->hasFocus())
        highEdit->setText(QString::number(high));
}

void MainWindow::sendFrequency()
{
    sendRawCommand(
        QString("FA%1;").arg(currentFrequency, 11, 10, QChar('0')));
}

// Оставлено для совместимости с объявлением в mainwindow.h.
// Круглая ручка настройки больше не создаётся и этот метод не вызывает.
void MainWindow::tuneFrequency(int direction)
{
    const qint64 hz =
        qint64(currentFrequency) + qint64(tuningStep) * direction;

    currentFrequency = quint32(
        std::clamp<qint64>(hz, 1, 99999999));

    frequencyDisplay->setFrequency(currentFrequency);

    auto* edit = findChild<QLineEdit*>("frequencyEdit");
    if (edit && !edit->hasFocus())
        edit->setText(QString::number(currentFrequency));

    sendFrequency();
}

void MainWindow::changeMode(int mode)
{
    if (mode < 1 || mode > 5)
        return;

    currentMode = mode;

    sendRawCommand(QString("MD%1;").arg(mode));
}

void MainWindow::sendLowCut(int hz)
{
    if (hz < 0 || hz > 5000 || hz >= highCutHz)
        return;

    lowCutHz = hz;

    sendRawCommand(
        QString("SL%1;").arg(hz, 4, 10, QChar('0')));
}

void MainWindow::sendHighCut(int hz)
{
    if (hz < 0 || hz > 5000 || hz <= lowCutHz)
        return;

    highCutHz = hz;

    sendRawCommand(
        QString("SH%1;").arg(hz, 4, 10, QChar('0')));
}
