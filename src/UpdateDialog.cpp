#include "UpdateDialog.h"

#include "Theme.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QLabel *makeLabel(const QString &text, const QFont &font, const QColor &color, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setFont(font);
    label->setWordWrap(true);
    label->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(theme::css(color)));
    return label;
}

QPushButton *makeButton(const QString &text, const char *objectName, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setObjectName(QLatin1String(objectName));
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

} // namespace

UpdateDialog::UpdateDialog(UpdateChecker::Result result, const ReleaseInfo &release, const QString &currentVersion,
                           const QString &error, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Eye Timer updates"));
    setAttribute(Qt::WA_DeleteOnClose);
    setMinimumWidth(460);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 26, 28, 24);
    layout->setSpacing(10);

    QString eyebrow;
    QString title;
    QString body;
    switch (result) {
    case UpdateChecker::Result::UpdateAvailable:
        eyebrow = QStringLiteral("UPDATE AVAILABLE");
        title = QStringLiteral("Eye Timer %1").arg(release.version);
        body = QStringLiteral("You have %1.").arg(currentVersion);
        break;
    case UpdateChecker::Result::UpToDate:
        eyebrow = QStringLiteral("UP TO DATE");
        title = QStringLiteral("You have the latest version");
        body = QStringLiteral("Eye Timer %1 is the newest release.").arg(currentVersion);
        break;
    case UpdateChecker::Result::Failed:
        eyebrow = QStringLiteral("COULDN’T CHECK");
        title = QStringLiteral("No answer from GitHub");
        body = error.isEmpty() ? QStringLiteral("Check your connection and try again later.") : error;
        break;
    }

    layout->addWidget(makeLabel(eyebrow, theme::font(12, QFont::Medium, 114), theme::text(0.6), this));
    layout->addWidget(makeLabel(title, theme::font(26, QFont::ExtraLight, 99), theme::text(), this));
    layout->addWidget(makeLabel(body, theme::font(14, QFont::Light), theme::text(0.7), this));

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(10);

    if (result == UpdateChecker::Result::UpdateAvailable) {
        const QString summary = UpdateChecker::summarize(release.notes);
        if (!summary.isEmpty()) {
            layout->addSpacing(6);
            layout->addWidget(makeLabel(summary, theme::font(15, QFont::Light), theme::text(0.86), this));
        }

        // Download fetches this platform's installer directly; say what to do with it.
        const bool hasInstaller = !release.downloadUrl.isEmpty();
        if (hasInstaller) {
#if defined(Q_OS_WIN)
            const QString howTo = QStringLiteral("Download saves the installer. Run it to update; your settings stay.");
#elif defined(Q_OS_MACOS)
            const QString howTo =
                QStringLiteral("Download saves the disk image. Open it and replace the app; your settings stay.");
#else
            const QString howTo = QStringLiteral("Download saves the package. Open it to update; your settings stay.");
#endif
            layout->addSpacing(2);
            layout->addWidget(makeLabel(howTo, theme::font(13, QFont::Light), theme::text(0.62), this));
        }

        m_notes = new QTextBrowser(this);
        m_notes->setOpenExternalLinks(true);
        m_notes->setMarkdown(release.notes.isEmpty() ? QStringLiteral("_No release notes._") : release.notes);
        m_notes->setMinimumHeight(220);
        m_notes->setStyleSheet(QStringLiteral(
            "QTextBrowser { background: rgba(0, 0, 0, 0.25); color: #F4F5F8; border: 1px solid rgba(255, 255, 255, 0.10);"
            " border-radius: 11px; padding: 10px; font-size: 14px; }"));
        m_notes->hide();
        layout->addSpacing(6);
        layout->addWidget(m_notes, 1);

        auto *skip = makeButton(QStringLiteral("Skip this version"), "secondaryButton", this);
        m_notesToggle = makeButton(QStringLiteral("What’s new"), "secondaryButton", this);
        // No installer for this system (e.g. a non-Debian Linux): send people to
        // the release page instead, where they can pick or build.
        auto *download = makeButton(hasInstaller ? QStringLiteral("Download") : QStringLiteral("Open release page"),
                                    "primaryButton", this);
        download->setDefault(true);
        // Notes are missing when only the release page answered.
        m_notesToggle->setVisible(!release.notes.isEmpty());
        buttons->addWidget(skip);
        buttons->addStretch(1);
        buttons->addWidget(m_notesToggle);
        buttons->addWidget(download);

        const QString version = release.version;
        const QString target = hasInstaller ? release.downloadUrl : release.pageUrl;
        connect(skip, &QPushButton::clicked, this, [this, version] {
            emit skipVersionRequested(version);
            accept();
        });
        connect(m_notesToggle, &QPushButton::clicked, this, [this] {
            m_notes->setVisible(!m_notes->isVisible());
            m_notesToggle->setText(m_notes->isVisible() ? QStringLiteral("Hide notes") : QStringLiteral("What’s new"));
            adjustSize();
        });
        connect(download, &QPushButton::clicked, this, [this, target] {
            // Only ever this repository's releases, whatever the response said.
            // The browser downloads the installer straight away.
            QDesktopServices::openUrl(UpdateChecker::safeReleasePageUrl(target));
            accept();
        });
    } else {
        auto *ok = makeButton(QStringLiteral("OK"), "primaryButton", this);
        ok->setDefault(true);
        buttons->addStretch(1);
        buttons->addWidget(ok);
        connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    }

    layout->addSpacing(14);
    layout->addLayout(buttons);
}

void UpdateDialog::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), theme::ground());
    const theme::Palette &palette = theme::palette(QStringLiteral("aurora"));
    theme::paintField(painter, QRectF(-120, -220, 420, 360), palette.fields[0], 0.45);
    theme::paintField(painter, QRectF(width() - 300, -240, 420, 360), palette.fields[2], 0.4);
    theme::paintGrain(painter, rect(), 0.05);
}
