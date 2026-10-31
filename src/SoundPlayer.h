#pragma once

#include <QObject>
#include <QStringList>

#ifdef EYETIMER_HAS_MULTIMEDIA
#include <QAudioFormat>
#include <QList>

class QAudioDecoder;
class QAudioSink;
class QBuffer;
#endif

// Plays the short notification tones.
//
// With Qt Multimedia the MP3 is decoded up front and played with a short
// silent lead-in, so audio devices that wake up slowly (Bluetooth, HDMI) do
// not clip the start. Without it the platform player is used instead:
// MCI on Windows, afplay on macOS, and mpg123/ffplay/gst-play on Linux.
class SoundPlayer : public QObject
{
    Q_OBJECT

public:
    explicit SoundPlayer(QObject *parent = nullptr);
    ~SoundPlayer() override;

    void play(const QString &filePath);
    void stopAll();

private:
#ifdef EYETIMER_HAS_MULTIMEDIA
    struct Decode {
        QAudioDecoder *decoder = nullptr;
        QAudioFormat format;
        QByteArray pcm;
    };
    struct Playback {
        QAudioSink *sink = nullptr;
        QBuffer *buffer = nullptr;
    };

    void finishDecode(Decode *decode);
    void startPlayback(const QAudioFormat &format, const QByteArray &pcm);
    void releasePlayback(QAudioSink *sink);

    QList<Decode *> m_decodes;
    QList<Playback> m_playbacks;
#else
    void playWithPlatformPlayer(const QString &filePath);

    QStringList m_mciAliases;
    int m_mciCounter = 0;
#endif
};
