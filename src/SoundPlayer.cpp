#include "SoundPlayer.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QtEndian>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mmsystem.h>
#include <shlwapi.h>

// Exported by mfplat but missing from some MinGW headers.
extern "C" HRESULT WINAPI MFCreateMFByteStreamOnStream(IStream *stream, IMFByteStream **byteStream);
#elif defined(EYETIMER_QT_AUDIO)
#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioSink>
#include <QBuffer>
#include <QThread>
#include <QUrl>
#endif

namespace {
constexpr int kLeadInMs = 700;
constexpr float kVolume = 0.75f;

#if defined(Q_OS_WIN)

template <typename T>
void release(T *&object)
{
    if (object) {
        object->Release();
        object = nullptr;
    }
}

// Decodes an audio file (a path or a ":/" resource) to 16-bit PCM and wraps
// it in an in-memory WAV image, with the lead-in silence and volume applied,
// that PlaySound can stream.
QByteArray decodeFileToWav(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray encoded = file.readAll();

    const HRESULT comInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) {
        if (SUCCEEDED(comInit))
            CoUninitialize();
        return {};
    }

    QByteArray pcm;
    UINT32 channels = 0;
    UINT32 sampleRate = 0;

    IStream *stream = SHCreateMemStream(reinterpret_cast<const BYTE *>(encoded.constData()),
                                        static_cast<UINT>(encoded.size()));
    IMFByteStream *byteStream = nullptr;
    IMFSourceReader *reader = nullptr;
    IMFMediaType *requested = nullptr;
    IMFMediaType *actual = nullptr;
    HRESULT hr = stream ? MFCreateMFByteStreamOnStream(stream, &byteStream) : E_OUTOFMEMORY;
    if (SUCCEEDED(hr)) {
        // Tell the source resolver what it is reading; a memory stream has no
        // file extension to go by.
        IMFAttributes *attributes = nullptr;
        if (SUCCEEDED(byteStream->QueryInterface(IID_IMFAttributes, reinterpret_cast<void **>(&attributes)))) {
            attributes->SetString(MF_BYTESTREAM_CONTENT_TYPE, L"audio/mpeg");
            release(attributes);
        }
        hr = MFCreateSourceReaderFromByteStream(byteStream, nullptr, &reader);
    }
    if (SUCCEEDED(hr))
        hr = reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    if (SUCCEEDED(hr))
        hr = reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
    if (SUCCEEDED(hr))
        hr = MFCreateMediaType(&requested);
    if (SUCCEEDED(hr))
        hr = requested->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (SUCCEEDED(hr))
        hr = requested->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    if (SUCCEEDED(hr))
        hr = requested->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    if (SUCCEEDED(hr))
        hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, requested);
    if (SUCCEEDED(hr))
        hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &actual);
    if (SUCCEEDED(hr)) {
        channels = MFGetAttributeUINT32(actual, MF_MT_AUDIO_NUM_CHANNELS, 0);
        sampleRate = MFGetAttributeUINT32(actual, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
        if (channels == 0 || sampleRate == 0)
            hr = E_FAIL;
    }

    while (SUCCEEDED(hr)) {
        DWORD flags = 0;
        IMFSample *sample = nullptr;
        hr = reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample);
        if (FAILED(hr))
            break;
        if (sample) {
            IMFMediaBuffer *buffer = nullptr;
            if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buffer))) {
                BYTE *data = nullptr;
                DWORD length = 0;
                if (SUCCEEDED(buffer->Lock(&data, nullptr, &length))) {
                    pcm.append(reinterpret_cast<const char *>(data), static_cast<qsizetype>(length));
                    buffer->Unlock();
                }
                release(buffer);
            }
            release(sample);
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            break;
    }

    release(actual);
    release(requested);
    release(reader);
    release(byteStream);
    release(stream);
    MFShutdown();
    if (SUCCEEDED(comInit))
        CoUninitialize();

    if (FAILED(hr) || pcm.isEmpty())
        return {};

    auto *samples = reinterpret_cast<qint16 *>(pcm.data());
    for (qsizetype i = 0; i < pcm.size() / 2; ++i)
        samples[i] = static_cast<qint16>(samples[i] * kVolume);

    const quint16 blockAlign = static_cast<quint16>(channels * 2);
    const QByteArray silence(static_cast<qsizetype>(sampleRate) * kLeadInMs / 1000 * blockAlign, '\0');
    const quint32 dataSize = static_cast<quint32>(silence.size() + pcm.size());

    QByteArray wav;
    wav.reserve(44 + static_cast<qsizetype>(dataSize));
    auto put16 = [&wav](quint16 value) {
        char bytes[2];
        qToLittleEndian(value, bytes);
        wav.append(bytes, 2);
    };
    auto put32 = [&wav](quint32 value) {
        char bytes[4];
        qToLittleEndian(value, bytes);
        wav.append(bytes, 4);
    };
    wav.append("RIFF", 4);
    put32(36 + dataSize);
    wav.append("WAVEfmt ", 8);
    put32(16);
    put16(1); // PCM
    put16(static_cast<quint16>(channels));
    put32(sampleRate);
    put32(sampleRate * blockAlign);
    put16(blockAlign);
    put16(16);
    wav.append("data", 4);
    put32(dataSize);
    wav.append(silence);
    wav.append(pcm);
    return wav;
}

#endif
} // namespace

#if defined(Q_OS_WIN)
QByteArray SoundPlayer::decodeToWav(const QString &filePath)
{
    return decodeFileToWav(filePath);
}
#endif

SoundPlayer::SoundPlayer(QObject *parent)
    : QObject(parent)
{
#if defined(EYETIMER_QT_AUDIO)
    m_audioThread = new QThread(this);
    m_audioThread->setObjectName(QStringLiteral("EyeTimerAudio"));
    m_audioContext = new QObject;
    m_audioContext->moveToThread(m_audioThread);
    m_audioThread->start(QThread::HighPriority);
#endif
}

SoundPlayer::~SoundPlayer()
{
    stopAll();
#if defined(EYETIMER_QT_AUDIO)
    m_audioThread->quit();
    m_audioThread->wait();
    delete m_audioContext; // the thread has finished, so this is safe here
#endif
}

void SoundPlayer::play(const QString &filePath)
{
    if (!QFileInfo::exists(filePath)) {
        QApplication::beep();
        return;
    }

#if defined(Q_OS_WIN)
    auto it = m_wavCache.find(filePath);
    if (it == m_wavCache.end()) {
        const QByteArray wav = decodeToWav(filePath);
        if (wav.isEmpty()) {
            QApplication::beep();
            return;
        }
        it = m_wavCache.insert(filePath, wav);
    }
    // The cached image outlives the playback; starting a new tone stops the
    // previous one, so only one buffer is ever being read.
    if (!PlaySoundW(reinterpret_cast<LPCWSTR>(it->constData()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT))
        QApplication::beep();
#elif defined(EYETIMER_QT_AUDIO)
    auto *decode = new Decode;
    decode->decoder = new QAudioDecoder(this);
    m_decodes.append(decode);

    connect(decode->decoder, &QAudioDecoder::bufferReady, this, [decode] {
        const QAudioBuffer buffer = decode->decoder->read();
        if (!buffer.isValid())
            return;
        if (!decode->format.isValid())
            decode->format = buffer.format();
        decode->pcm.append(buffer.constData<char>(), buffer.byteCount());
    });
    connect(decode->decoder, &QAudioDecoder::finished, this, [this, decode] { finishDecode(decode); });
    connect(decode->decoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error), this,
            [this, decode](QAudioDecoder::Error) { finishDecode(decode); });

    // Reading through a QFile works for ":/" resources as well as files.
    auto *file = new QFile(filePath, decode->decoder);
    if (!file->open(QIODevice::ReadOnly)) {
        finishDecode(decode);
        return;
    }
    decode->decoder->setSourceDevice(file);
    decode->decoder->start();
#else
    playWithPlatformPlayer(filePath);
#endif
}

void SoundPlayer::stopAll()
{
#if defined(Q_OS_WIN)
    PlaySoundW(nullptr, nullptr, 0);
#elif defined(EYETIMER_QT_AUDIO)
    for (Decode *decode : std::as_const(m_decodes)) {
        decode->decoder->disconnect(this);
        decode->decoder->stop();
        decode->decoder->deleteLater();
        delete decode;
    }
    m_decodes.clear();

    QObject *context = m_audioContext;
    QMetaObject::invokeMethod(
        context,
        [context] {
            const QList<QAudioSink *> sinks = context->findChildren<QAudioSink *>(Qt::FindDirectChildrenOnly);
            for (QAudioSink *sink : sinks) {
                sink->disconnect();
                sink->stop();
                delete sink;
            }
            qDeleteAll(context->findChildren<QBuffer *>(Qt::FindDirectChildrenOnly));
        },
        Qt::BlockingQueuedConnection);
#endif
}

#if defined(EYETIMER_QT_AUDIO)

void SoundPlayer::finishDecode(Decode *decode)
{
    if (!m_decodes.removeOne(decode))
        return;

    decode->decoder->disconnect(this);
    decode->decoder->deleteLater();
    const QAudioFormat format = decode->format;
    const QByteArray pcm = decode->pcm;
    delete decode;

    if (!format.isValid() || pcm.isEmpty()) {
        QApplication::beep();
        return;
    }
    startPlayback(format, pcm);
}

void SoundPlayer::startPlayback(const QAudioFormat &format, const QByteArray &pcm)
{
    const int frames = qMax(0, qRound(format.sampleRate() * kLeadInMs / 1000.0));
    const qint32 silenceBytes = format.bytesForFrames(frames);
    const char silenceValue = format.sampleFormat() == QAudioFormat::UInt8 ? char(0x80) : char(0);

    const QByteArray data = QByteArray(silenceBytes, silenceValue) + pcm;

    // Create the sink on the audio thread so its refills never wait on the UI.
    QObject *context = m_audioContext;
    QMetaObject::invokeMethod(context, [context, format, data] {
        auto *buffer = new QBuffer(context);
        buffer->setData(data);
        buffer->open(QIODevice::ReadOnly);

        auto *sink = new QAudioSink(format, context);
        sink->setVolume(kVolume);
        sink->setBufferSize(format.bytesForDuration(250'000));
        QObject::connect(sink, &QAudioSink::stateChanged, context, [sink, buffer](QAudio::State state) {
            if (state == QAudio::IdleState) {
                sink->stop();
            } else if (state == QAudio::StoppedState) {
                sink->deleteLater();
                buffer->deleteLater();
            }
        });
        sink->start(buffer);
    });
}

#elif !defined(Q_OS_WIN)

void SoundPlayer::playWithPlatformPlayer(const QString &filePath)
{
    // External players need a real file; copy embedded tones to the cache once.
    QString localPath = filePath;
    if (filePath.startsWith(QLatin1String(":/"))) {
        const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        localPath = QDir(cacheDir).filePath(QStringLiteral("tones/") + QFileInfo(filePath).fileName());
        if (!QFileInfo::exists(localPath)) {
            QDir().mkpath(QFileInfo(localPath).absolutePath());
            if (!QFile::copy(filePath, localPath)) {
                QApplication::beep();
                return;
            }
            QFile::setPermissions(localPath, QFile::ReadOwner | QFile::WriteOwner);
        }
    }
    const QString nativePath = QDir::toNativeSeparators(QFileInfo(localPath).absoluteFilePath());

#if defined(Q_OS_MACOS)
    if (!QProcess::startDetached(QStringLiteral("afplay"),
                                 {QStringLiteral("-v"), QString::number(kVolume), nativePath}))
        QApplication::beep();
#else
    const QList<QPair<QString, QStringList>> players = {
        {QStringLiteral("mpg123"), {QStringLiteral("-q"), nativePath}},
        {QStringLiteral("pw-play"), {nativePath}},
        {QStringLiteral("ffplay"),
         {QStringLiteral("-nodisp"), QStringLiteral("-autoexit"), QStringLiteral("-loglevel"), QStringLiteral("quiet"),
          nativePath}},
        {QStringLiteral("gst-play-1.0"), {QStringLiteral("-q"), nativePath}},
        {QStringLiteral("paplay"), {nativePath}},
    };
    for (const auto &[program, arguments] : players) {
        if (!QStandardPaths::findExecutable(program).isEmpty() && QProcess::startDetached(program, arguments))
            return;
    }
    QApplication::beep();
#endif
}

#endif
