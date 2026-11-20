#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>

#if defined(EYETIMER_HAS_MULTIMEDIA) && !defined(Q_OS_WIN)
#define EYETIMER_QT_AUDIO 1
#include <QAudioFormat>
#include <QList>

class QAudioDecoder;
class QThread;
#endif

// Plays the short notification tones.
//
// Every path plays the tone after a short silent lead-in, so audio devices
// that wake up slowly (Bluetooth, HDMI) do not clip the start.
//   Windows: the MP3 is decoded once with Media Foundation and played from
//            memory with PlaySound, which streams on the system's own audio
//            thread. (MCI/DirectShow playback stuttered.)
//   macOS/Linux with Qt Multimedia: decoded with QAudioDecoder, played with
//            a QAudioSink on its own thread.
//   Otherwise: afplay on macOS, mpg123/ffplay/gst-play/paplay on Linux.
class SoundPlayer : public QObject
{
    Q_OBJECT

public:
    explicit SoundPlayer(QObject *parent = nullptr);
    ~SoundPlayer() override;

    void play(const QString &filePath);
    void stopAll();

#if defined(Q_OS_WIN)
    // The in-memory WAV that play() hands to PlaySound; empty on failure.
    static QByteArray decodeToWav(const QString &filePath);
#endif

private:
#if defined(Q_OS_WIN)
    // Decoded WAV images, kept alive while PlaySound reads from them.
    QHash<QString, QByteArray> m_wavCache;
#elif defined(EYETIMER_QT_AUDIO)
    struct Decode {
        QAudioDecoder *decoder = nullptr;
        QAudioFormat format;
        QByteArray pcm;
    };
    void finishDecode(Decode *decode);
    void startPlayback(const QAudioFormat &format, const QByteArray &pcm);

    QList<Decode *> m_decodes;
    // Sinks and their buffers live on a dedicated thread: Qt's audio backends
    // refill from the thread that owns the sink, so on the UI thread any
    // stall (painting, process calls) left gaps in the sound.
    QThread *m_audioThread = nullptr;
    QObject *m_audioContext = nullptr;
#else
    void playWithPlatformPlayer(const QString &filePath);
#endif
};
