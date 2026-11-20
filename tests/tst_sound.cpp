#include "SoundPlayer.h"

#include <QTest>
#include <QtEndian>

// Windows only: the tones are decoded from the embedded MP3s into the WAV
// image PlaySound streams. A gap-free decode is what fixed the stutter.
class TestSound : public QObject
{
    Q_OBJECT

private slots:
    void decodesEmbeddedTones_data()
    {
        QTest::addColumn<QString>("path");
        QTest::newRow("start") << ":/eyetimer/universfield-new-notification-017-352293.mp3";
        QTest::newRow("end") << ":/eyetimer/universfield-new-notification-017-352293-pitch-changed.mp3";
    }

    void decodesEmbeddedTones()
    {
        QFETCH(QString, path);
        const QByteArray wav = SoundPlayer::decodeToWav(path);
        QVERIFY(wav.size() > 44);
        QCOMPARE(wav.left(4), QByteArray("RIFF"));
        QCOMPARE(wav.mid(8, 8), QByteArray("WAVEfmt "));

        const quint16 channels = qFromLittleEndian<quint16>(wav.constData() + 22);
        const quint32 rate = qFromLittleEndian<quint32>(wav.constData() + 24);
        const quint32 dataSize = qFromLittleEndian<quint32>(wav.constData() + 40);
        QVERIFY(channels == 1 || channels == 2);
        QVERIFY(rate >= 8000);
        QCOMPARE(qsizetype(dataSize), wav.size() - 44);

        // At least a second of audio, starting with the 700 ms silent lead-in.
        const quint32 bytesPerSecond = rate * channels * 2;
        QVERIFY(dataSize > bytesPerSecond);
        const auto *samples = reinterpret_cast<const qint16 *>(wav.constData() + 44);
        const quint32 leadInSamples = rate * channels * 7 / 10;
        for (quint32 i = 0; i < leadInSamples; ++i)
            QCOMPARE(samples[i], qint16(0));
    }

    void missingFileGivesNothing()
    {
        QVERIFY(SoundPlayer::decodeToWav(QStringLiteral(":/eyetimer/absent.mp3")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSound)
#include "tst_sound.moc"
