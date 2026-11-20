#pragma once

#include "UpdateChecker.h"

#include <QDialog>

class QPushButton;
class QTextBrowser;

// Shows the result of an update check: a newer release with its notes and a
// download link, "you're up to date", or why the check failed.
class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    UpdateDialog(UpdateChecker::Result result, const ReleaseInfo &release, const QString &currentVersion,
                 const QString &error, QWidget *parent = nullptr);

signals:
    void skipVersionRequested(const QString &version);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QTextBrowser *m_notes = nullptr;
    QPushButton *m_notesToggle = nullptr;
};
