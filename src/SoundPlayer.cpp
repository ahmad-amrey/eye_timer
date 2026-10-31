#include "SoundPlayer.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#ifdef EYETIMER_HAS_MULTIMEDIA
#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioSink>
#include <QBuffer>
#include <QUrl>
#endif

#if defined(Q_OS_WIN) && !defined(EYETIMER_HAS_MULTIMEDIA)
#include <windows.h>
#include <mmsystem.h>
#endif

namespace {
constexpr int kLeadInMs = 700;
constexpr float kVolume = 0.75f;
} // namespace

SoundPlayer::SoundPlayer(QObject *parent)
    : QObject(parent)
{
}

SoundPlayer::~SoundPlayer()
{
    stopAll();
}

void SoundPlayer::play(const QString &filePath)
{
    if (!QFileInfo::exists(filePath)) {
        QApplication::beep();
        return;
    }

#ifdef EYETIMER_HAS_MULTIMEDIA
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

    decode->decoder->setSource(QUrl::fromLocalFile(filePath));
    decode->decoder->start();
#else
    playWithPlatformPlayer(filePath);
#endif
}

void SoundPlayer::stopAll()
{
#ifdef EYETIMER_HAS_MULTIMEDIA
    for (Decode *decode : std::as_const(m_decodes)) {
        decode->decoder->disconnect(this);
        decode->decoder->stop();
        decode->decoder->deleteLater();
        delete decode;
    }
    m_decodes.clear();
    for (const Playback &playback : std::as_const(m_playbacks)) {
        playback.sink->disconnect(this);
        playback.sink->stop();
        playback.sink->deleteLater();
        playback.buffer->deleteLater();
    }
    m_playbacks.clear();
#elif defined(Q_OS_WIN)
    for (const QString &alias : std::as_const(m_mciAliases))
        mciSendStringW(QStringLiteral("close %1").arg(alias).toStdWString().c_str(), nullptr, 0, nullptr);
    m_mciAliases.clear();
#endif
}

#ifdef EYETIMER_HAS_MULTIMEDIA

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

    auto *buffer = new QBuffer(this);
    buffer->setData(QByteArray(silenceBytes, silenceValue) + pcm);
    buffer->open(QIODevice::ReadOnly);

    auto *sink = new QAudioSink(format, this);
    sink->setVolume(kVolume);
    m_playbacks.append({sink, buffer});

    connect(sink, &QAudioSink::stateChanged, this, [this, sink](QAudio::State state) {
        if (state == QAudio::IdleState)
            sink->stop();
        else if (state == QAudio::StoppedState)
            releasePlayback(sink);
    });
    sink->start(buffer);
}

void SoundPlayer::releasePlayback(QAudioSink *sink)
{
    for (qsizetype i = 0; i < m_playbacks.size(); ++i) {
        if (m_playbacks.at(i).sink != sink)
            continue;
        const Playback playback = m_playbacks.takeAt(i);
        playback.sink->disconnect(this);
        playback.sink->deleteLater();
        playback.buffer->deleteLater();
        return;
    }
}

#else

void SoundPlayer::playWithPlatformPlayer(const QString &filePath)
{
    const QString nativePath = QDir::toNativeSeparators(QFileInfo(filePath).absoluteFilePath());

#if defined(Q_OS_WIN)
    const QString alias = QStringLiteral("eyetimer%1").arg(++m_mciCounter);
    const QString open = QStringLiteral("open \"%1\" type mpegvideo alias %2").arg(nativePath, alias);
    if (mciSendStringW(open.toStdWString().c_str(), nullptr, 0, nullptr) != 0) {
        QApplication::beep();
        return;
    }
    m_mciAliases << alias;
    mciSendStringW(QStringLiteral("play %1").arg(alias).toStdWString().c_str(), nullptr, 0, nullptr);
    // The tones are a few seconds long; release the device afterwards.
    QTimer::singleShot(15000, this, [this, alias] {
        if (m_mciAliases.removeOne(alias))
            mciSendStringW(QStringLiteral("close %1").arg(alias).toStdWString().c_str(), nullptr, 0, nullptr);
    });
#elif defined(Q_OS_MACOS)
    if (!QProcess::startDetached(QStringLiteral("afplay"),
                                 {QStringLiteral("-v"), QString::number(kVolume), nativePath}))
        QApplication::beep();
#else
    const QList<QPair<QString, QStringList>> players = {
        {QStringLiteral("mpg123"), {QStringLiteral("-q"), nativePath}},
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
