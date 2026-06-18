#include "cavamonitor.hpp"

#include <algorithm>
#include "cavacore.h"
#include <cmath>
#include <cstddef>
#include <pipewire/pipewire.h>
#include <qdebug.h>
#include <qthread.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/latency-utils.h>
#include <stop_token>
#include <vector>

namespace cavamonitor {
PipeWireWorker::PipeWireWorker(std::stop_token token, CavaMonitor* monitor)
    : m_loop(nullptr)
    , m_stream(nullptr)
    , m_timer(nullptr)
    , m_idle(true)
    , m_token(token)
    , m_monitor(monitor) {
    pw_init(nullptr, nullptr);

    m_loop = pw_main_loop_new(nullptr);
    if (!m_loop) {
        qWarning() << "PipeWireWorker::init: failed to create PipeWire main loop";
        pw_deinit();
        return;
    }

    timespec timeout = { 0, 10 * SPA_NSEC_PER_MSEC };
    m_timer = pw_loop_add_timer(pw_main_loop_get_loop(m_loop), handleTimeout, this);
    pw_loop_update_timer(pw_main_loop_get_loop(m_loop), m_timer, &timeout, &timeout, false);

    auto props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture", PW_KEY_MEDIA_ROLE, "Music", nullptr);
    pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");
    pw_properties_setf(
        props, PW_KEY_NODE_LATENCY, "%u/%u", nextPowerOf2(512 * SAMPLE_RATE / 48000), SAMPLE_RATE);
    pw_properties_set(props, PW_KEY_NODE_PASSIVE, "true");
    pw_properties_set(props, PW_KEY_NODE_VIRTUAL, "true");
    pw_properties_set(props, PW_KEY_STREAM_DONT_REMIX, "false");
    pw_properties_set(props, "channelmix.upmix", "true");

    std::vector<uint8_t> buffer(CHUNK_SIZE);
    spa_pod_builder b;
    spa_pod_builder_init(&b, buffer.data(), static_cast<quint32>(buffer.size()));

    spa_audio_info_raw info{};
    info.format = SPA_AUDIO_FORMAT_S16;
    info.rate = SAMPLE_RATE;
    info.channels = 1;

    const spa_pod* params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

    pw_stream_events events{};
    events.state_changed = [](void* data, pw_stream_state, pw_stream_state state, const char*) {
        auto* self = static_cast<PipeWireWorker*>(data);
        self->streamStateChanged(state);
    };
    events.process = [](void* data) {
        auto* self = static_cast<PipeWireWorker*>(data);
        self->processStream();
    };

    m_stream = pw_stream_new_simple(pw_main_loop_get_loop(m_loop), "cavamonitor", props, &events, this);

    const int success = pw_stream_connect(m_stream, PW_DIRECTION_INPUT, PW_ID_ANY,
        static_cast<pw_stream_flags>(
            PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),
        params, 1);
    if (success < 0) {
        qWarning() << "PipeWireWorker::init: failed to connect stream";
        pw_stream_destroy(m_stream);
        pw_main_loop_destroy(m_loop);
        pw_deinit();
        return;
    }

    pw_main_loop_run(m_loop);

    pw_stream_destroy(m_stream);
    pw_main_loop_destroy(m_loop);
    pw_deinit();
}

void PipeWireWorker::handleTimeout(void* data, uint64_t expirations) {
    auto* self = static_cast<PipeWireWorker*>(data);

    if (self->m_token.stop_requested()) {
        pw_main_loop_quit(self->m_loop);
        return;
    }

    if (!self->m_idle) {
        if (expirations < 10) {
            self->m_monitor->clearBuffer();
        } else {
            self->m_idle = true;
            timespec timeout = { 0, 500 * SPA_NSEC_PER_MSEC };
            pw_loop_update_timer(pw_main_loop_get_loop(self->m_loop), self->m_timer, &timeout, &timeout, false);
        }
    }
}

void PipeWireWorker::streamStateChanged(pw_stream_state state) {
    m_idle = false;
    switch (state) {
    case PW_STREAM_STATE_PAUSED: {
        timespec timeout = { 0, 10 * SPA_NSEC_PER_MSEC };
        pw_loop_update_timer(pw_main_loop_get_loop(m_loop), m_timer, &timeout, &timeout, false);
        break;
    }
    case PW_STREAM_STATE_STREAMING:
        pw_loop_update_timer(pw_main_loop_get_loop(m_loop), m_timer, nullptr, nullptr, false);
        break;
    case PW_STREAM_STATE_ERROR:
        pw_main_loop_quit(m_loop);
        break;
    default:
        break;
    }
}

void PipeWireWorker::processStream() {
    if (m_token.stop_requested()) {
        pw_main_loop_quit(m_loop);
        return;
    }

    pw_buffer* buffer = pw_stream_dequeue_buffer(m_stream);
    if (buffer == nullptr) {
        return;
    }

    const spa_buffer* buf = buffer->buffer;
    const qint16* samples = reinterpret_cast<const qint16*>(buf->datas[0].data);
    if (samples == nullptr) {
        return;
    }

    const quint32 count = buf->datas[0].chunk->size / 2;
    m_monitor->loadChunk(samples, count);

    pw_stream_queue_buffer(m_stream, buffer);
}

unsigned int PipeWireWorker::nextPowerOf2(unsigned int n) {
    if (n == 0) {
        return 1;
    }

    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;

    return n;
}
CavaProcessor::CavaProcessor(QObject* parent)
    : QObject(parent)
    , m_plan(nullptr)
    , m_in(new double[CHUNK_SIZE])
    , m_out(nullptr)
    , m_bars(0)
    , m_timer(nullptr) {}

CavaProcessor::~CavaProcessor() {
    stop();
    cleanup();
    delete[] m_in;
}

void CavaProcessor::init() {
    m_timer = new QTimer(this);
    m_timer->setInterval(static_cast<int>(CHUNK_SIZE * 1000.0 / SAMPLE_RATE));
    connect(m_timer, &QTimer::timeout, this, &CavaProcessor::process);
}

void CavaProcessor::start() {
    if (m_timer) {
        m_timer->start();
    }
}

void CavaProcessor::stop() {
    if (m_timer) {
        m_timer->stop();
    }
}

void CavaProcessor::process() {
    if (!m_plan || m_bars == 0 || !m_out) {
        return;
    }
    CavaMonitor* monitor = qobject_cast<CavaMonitor*>(parent());
    if (!monitor) {
        return;
    }

    const int count = static_cast<int>(monitor->readChunk(m_in));

    cava_execute(m_in, count, m_out, m_plan);

    QVector<double> values(m_bars);

    const double inv = 1.0 / 1.5;
    double carry = 0.0;
    for (int i = 0; i < m_bars; ++i) {
        carry = std::max(m_out[i], carry * inv);
        values[i] = carry;
    }
    carry = 0.0;
    for (int i = m_bars - 1; i >= 0; --i) {
        carry = std::max(m_out[i], carry * inv);
        values[i] = std::max(values[i], carry);
    }
    if (values != m_values) {
        m_values = std::move(values);
        emit valuesChanged(m_values);
    }
}

void CavaProcessor::setBars(int bars) {
    if (bars < 0) {
        qWarning() << "CavaProcessor::setBars: bars must be greater than 0. Setting to 0.";
        bars = 0;
    }

    if (m_bars != bars) {
        m_bars = bars;
        reload();
    }
}

void CavaProcessor::reload() {
    cleanup();
    initCava();
}

void CavaProcessor::cleanup() {
    if (m_plan) {
        cava_destroy(m_plan);
        m_plan = nullptr;
    }

    if (m_out) {
        delete[] m_out;
        m_out = nullptr;
    }
}

void CavaProcessor::initCava() {
    if (m_plan || m_bars == 0) {
        return;
    }

    m_plan = cava_init(m_bars, SAMPLE_RATE, 1, 1, 0.85, 50, 10000);

    if (m_plan->status == -1) {
        qWarning() << "CavaProcessor::initCava: failed to initialise cava plan";
        cleanup();
        return;
    }

    m_out = new double[static_cast<size_t>(m_bars)];
}
CavaMonitor::CavaMonitor(QObject* parent)
    : QObject(parent)
    , m_bars(64)
    , m_values(m_bars, 0.0)
    , m_active(false)
    , m_processor(nullptr)
    , m_processorThread(nullptr)
    , m_buffer1(CHUNK_SIZE)
    , m_buffer2(CHUNK_SIZE)
    , m_readBuffer(&m_buffer1)
    , m_writeBuffer(&m_buffer2) {
    m_processor = new CavaProcessor(this);
    m_processor->init();
    m_processor->setBars(m_bars);
}

CavaMonitor::~CavaMonitor() {
    stopAudio();
}

int CavaMonitor::bars() const {
    return m_bars;
}

void CavaMonitor::setBars(int bars) {
    if (bars < 0) {
        qWarning() << "CavaMonitor::setBars: bars must be greater than 0. Setting to 0.";
        bars = 0;
    }

    if (m_bars == bars) {
        return;
    }

    m_values.resize(bars, 0.0);
    m_bars = bars;
    emit barsChanged();
    emit valuesChanged();

    if (m_processor) {
        QMetaObject::invokeMethod(m_processor, "setBars", Qt::QueuedConnection, Q_ARG(int, bars));
    }
}

QVector<double> CavaMonitor::values() const {
    return m_values;
}

bool CavaMonitor::active() const {
    return m_active;
}

void CavaMonitor::setActive(bool active) {
    if (m_active == active) {
        return;
    }

    m_active = active;
    emit activeChanged();

    if (active) {
        startAudio();
    } else {
        stopAudio();
    }
}

void CavaMonitor::clearBuffer() {
    auto* writeBuffer = m_writeBuffer.load(std::memory_order_relaxed);
    std::fill(writeBuffer->begin(), writeBuffer->end(), 0.0f);

    auto* oldRead = m_readBuffer.exchange(writeBuffer, std::memory_order_acq_rel);
    m_writeBuffer.store(oldRead, std::memory_order_release);
}

void CavaMonitor::loadChunk(const qint16* samples, quint32 count) {
    if (count > CHUNK_SIZE) {
        count = CHUNK_SIZE;
    }

    auto* writeBuffer = m_writeBuffer.load(std::memory_order_relaxed);
    std::transform(samples, samples + count, writeBuffer->begin(), [](qint16 sample) {
        return sample / 32768.0f;
    });

    auto* oldRead = m_readBuffer.exchange(writeBuffer, std::memory_order_acq_rel);
    m_writeBuffer.store(oldRead, std::memory_order_release);
}

quint32 CavaMonitor::readChunk(double* out, quint32 count) {
    if (count == 0 || count > CHUNK_SIZE) {
        count = CHUNK_SIZE;
    }

    auto* readBuffer = m_readBuffer.load(std::memory_order_acquire);
    std::transform(readBuffer->begin(), readBuffer->begin() + count, out, [](float sample) {
        return static_cast<double>(sample);
    });

    return count;
}

void CavaMonitor::updateValues(QVector<double> values) {
    if (values != m_values) {
        m_values = values;
        emit valuesChanged();
    }
}

void CavaMonitor::startAudio() {
    if (m_pipewireThread.joinable()) {
        return;
    }

    clearBuffer();
    connect(m_processor, &CavaProcessor::valuesChanged, this, &CavaMonitor::updateValues);
    if (m_processor) {
        m_processor->start();
    }
    m_pipewireThread = std::jthread([this](std::stop_token token) {
        PipeWireWorker worker(token, this);
    });
}

void CavaMonitor::stopAudio() {
    if (m_processor) {
        m_processor->stop();
        disconnect(m_processor, &CavaProcessor::valuesChanged, this, &CavaMonitor::updateValues);
    }
    if (m_pipewireThread.joinable()) {
        m_pipewireThread.request_stop();
        m_pipewireThread.join();
    }
    m_values.fill(0.0);
    emit valuesChanged();
}

}
