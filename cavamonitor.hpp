#pragma once

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QVector>
#include <atomic>
#include "cavacore.h"
#include <pipewire/pipewire.h>
#include <qqmlintegration.h>
#include <spa/param/audio/format-utils.h>
#include <stop_token>
#include <thread>
#include <vector>

namespace cavamonitor {

constexpr quint32 SAMPLE_RATE = 44100;
constexpr quint32 CHUNK_SIZE = 512;

class CavaMonitor;
class PipeWireWorker {
public:
    explicit PipeWireWorker(std::stop_token token, CavaMonitor* monitor);
    void run();

private:
    pw_main_loop* m_loop;
    pw_stream* m_stream;
    spa_source* m_timer;
    bool m_idle;

    std::stop_token m_token;
    CavaMonitor* m_monitor;

    static void handleTimeout(void* data, uint64_t expirations);
    void streamStateChanged(pw_stream_state state);
    void processStream();

    [[nodiscard]] unsigned int nextPowerOf2(unsigned int n);
};

class CavaProcessor : public QObject {
    Q_OBJECT

public:
    explicit CavaProcessor(QObject* parent = nullptr);
    ~CavaProcessor();

    void setBars(int bars);

signals:
    void valuesChanged(QVector<double> values);

public slots:
    void init();
    void start();
    void stop();
    void process();

private:
    struct cava_plan* m_plan;
    double* m_in;
    double* m_out;

    int m_bars;
    QVector<double> m_values;
    QTimer* m_timer;

    void reload();
    void initCava();
    void cleanup();
};

class CavaMonitor : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int bars READ bars WRITE setBars NOTIFY barsChanged)
    Q_PROPERTY(QVector<double> values READ values NOTIFY valuesChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
    explicit CavaMonitor(QObject* parent = nullptr);
    ~CavaMonitor();

    [[nodiscard]] int bars() const;
    void setBars(int bars);

    [[nodiscard]] QVector<double> values() const;

    [[nodiscard]] bool active() const;
    void setActive(bool active);

    void clearBuffer();
    void loadChunk(const qint16* samples, quint32 count);
    quint32 readChunk(double* out, quint32 count = 0);

signals:
    void barsChanged();
    void valuesChanged();
    void activeChanged();

private:
    int m_bars;
    QVector<double> m_values;
    bool m_active;

    CavaProcessor* m_processor;
    QThread* m_processorThread;

    std::jthread m_pipewireThread;
    std::vector<float> m_buffer1;
    std::vector<float> m_buffer2;
    std::atomic<std::vector<float>*> m_readBuffer;
    std::atomic<std::vector<float>*> m_writeBuffer;

    void updateValues(QVector<double> values);
    void startAudio();
    void stopAudio();
};

}
