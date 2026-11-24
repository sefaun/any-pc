#include "ui/IncomingRequestDialog.h"

#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest>

using namespace anypc;

class TestDialogs : public QObject
{
    Q_OBJECT

private:
    static QPushButton *button(QDialog *dialog, const QString &text)
    {
        for (QPushButton *b : dialog->findChildren<QPushButton *>()) {
            if (b->text() == text)
                return b;
        }
        return nullptr;
    }

private Q_SLOTS:
    // Regression: the dialog stayed on screen after Accept was clicked.
    void acceptClosesTheDialog()
    {
        QPointer<IncomingRequestDialog> dialog =
            new IncomingRequestDialog(QStringLiteral("c1"), {QStringLiteral("123456789"), QStringLiteral("pc"), QStringLiteral("windows")}, 60000);
        QSignalSpy answered(dialog.data(), &IncomingRequestDialog::answered);
        dialog->show();
        QVERIFY(QTest::qWaitForWindowExposed(dialog.data()));

        button(dialog, QStringLiteral("Accept"))->click();
        QCOMPARE(answered.count(), 1);
        QCOMPARE(answered.at(0).at(1).toBool(), true);
        QTRY_VERIFY(dialog.isNull() || !dialog->isVisible());
        QTRY_VERIFY(dialog.isNull()); // WA_DeleteOnClose
    }

    void declineAndCloseAnswerOnce()
    {
        QPointer<IncomingRequestDialog> dialog =
            new IncomingRequestDialog(QStringLiteral("c2"), {QStringLiteral("123456789"), QStringLiteral("pc"), QStringLiteral("linux")}, 60000);
        QSignalSpy answered(dialog.data(), &IncomingRequestDialog::answered);
        dialog->show();
        button(dialog, QStringLiteral("Decline"))->click();
        dialog->close();
        QCOMPARE(answered.count(), 1);
        QCOMPARE(answered.at(0).at(1).toBool(), false);
        QTRY_VERIFY(dialog.isNull());
    }

    void windowCloseDeclines()
    {
        QPointer<IncomingRequestDialog> dialog =
            new IncomingRequestDialog(QStringLiteral("c3"), {QStringLiteral("123456789"), QStringLiteral("pc"), QStringLiteral("macos")}, 60000);
        QSignalSpy answered(dialog.data(), &IncomingRequestDialog::answered);
        dialog->show();
        dialog->close();
        QCOMPARE(answered.count(), 1);
        QCOMPARE(answered.at(0).at(1).toBool(), false);
        QTRY_VERIFY(dialog.isNull());
    }

    void timeoutDeclines()
    {
        QPointer<IncomingRequestDialog> dialog =
            new IncomingRequestDialog(QStringLiteral("c4"), {QStringLiteral("123456789"), QStringLiteral("pc"), QStringLiteral("linux")}, 1000);
        QSignalSpy answered(dialog.data(), &IncomingRequestDialog::answered);
        dialog->show();
        QTRY_COMPARE_WITH_TIMEOUT(answered.count(), 1, 4000);
        QCOMPARE(answered.at(0).at(1).toBool(), false);
        QTRY_VERIFY(dialog.isNull());
    }
};

QTEST_MAIN(TestDialogs)
#include "tst_dialogs.moc"
