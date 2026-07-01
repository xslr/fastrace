/**
 * \file tst_TimelineOverviewWidget.cpp
 * \brief Unit tests for TimelineOverviewWidget.
 *
 * Tests exercise the public API and test-accessor methods
 * (visibleStartUs(), visibleEndUs()) without requiring a real trace file.
 * Interaction tests use QTest::mouseClick / QSignalSpy to verify signal
 * emission behaviour under the no-analyzer guard path.
 */

#include <QCheckBox>
#include <QSignalSpy>
#include <QTest>

#include "TimelineOverviewWidget.h"

class tst_TimelineOverviewWidget : public QObject {
    Q_OBJECT

private slots:
    // ------------------------------------------------------------------
    // 1. Default construction
    // ------------------------------------------------------------------
    /**
     * \brief Constructing TimelineOverviewWidget without an analyzer and
     * triggering a paint cycle must not crash.
     *
     * Exercises the early-return guard in paintEvent() (\"if (!m_analyzer)\").
     */
    void construction()
    {
        TimelineOverviewWidget w;
        w.resize(800, 120);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        // Verify initial visible-window state
        QCOMPARE(w.visibleStartUs(), uint64_t(0));
        QCOMPARE(w.visibleEndUs(), uint64_t(0));
    }

    // ------------------------------------------------------------------
    // 2. setVisibleWindow stores values correctly
    // ------------------------------------------------------------------
    /**
     * \brief setVisibleWindow() must persist the supplied start/end timestamps
     * so they can be read back via the test accessors.
     */
    void setVisibleWindow_storesValues()
    {
        TimelineOverviewWidget w;

        w.setVisibleWindow(1'000'000, 5'000'000);

        QCOMPARE(w.visibleStartUs(), uint64_t(1'000'000));
        QCOMPARE(w.visibleEndUs(), uint64_t(5'000'000));
    }

    // ------------------------------------------------------------------
    // 3. setVisibleWindow can be called multiple times
    // ------------------------------------------------------------------
    /**
     * \brief Repeated calls to setVisibleWindow() must always reflect the
     * most recent values.
     */
    void setVisibleWindow_overwritesPreviousValues()
    {
        TimelineOverviewWidget w;

        w.setVisibleWindow(0, 1'000'000);
        w.setVisibleWindow(2'000'000, 8'000'000);

        QCOMPARE(w.visibleStartUs(), uint64_t(2'000'000));
        QCOMPARE(w.visibleEndUs(), uint64_t(8'000'000));
    }

    // ------------------------------------------------------------------
    // 4. Click without analyzer does not emit navigateRequested
    // ------------------------------------------------------------------
    /**
     * \brief A mouse click must not emit navigateRequested() when no
     * Analyzer is attached (the early-return guard in mousePressEvent()).
     */
    void noAnalyzer_click_doesNotEmitNavigate()
    {
        TimelineOverviewWidget w;
        w.resize(800, 120);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QSignalSpy spy(&w, &TimelineOverviewWidget::navigateRequested);

        // Click in the centre of the widget
        QTest::mouseClick(&w, Qt::LeftButton, Qt::NoModifier, w.rect().center());

        QCOMPARE(spy.count(), 0);
    }

    // ------------------------------------------------------------------
    // 5. Click fully outside the label area does not emit when no analyzer
    // ------------------------------------------------------------------
    /**
     * \brief Clicking at x=0 (inside the label strip, not the histogram
     * area) should also not emit navigateRequested().
     */
    void noAnalyzer_clickInLabelArea_doesNotEmitNavigate()
    {
        TimelineOverviewWidget w;
        w.resize(800, 120);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QSignalSpy spy(&w, &TimelineOverviewWidget::navigateRequested);

        QTest::mouseClick(&w, Qt::LeftButton, Qt::NoModifier, QPoint(10, 60));

        QCOMPARE(spy.count(), 0);
    }

    // ------------------------------------------------------------------
    // 6. Toggling the CAN checkbox does not crash
    // ------------------------------------------------------------------
    /**
     * \brief Toggling the CAN checkbox (m_chkCan) must call onLaneToggled()
     * which triggers update(); the widget must not crash when there is no
     * Analyzer attached and the paint cycle fires.
     */
    void canCheckbox_toggle_doesNotCrash()
    {
        TimelineOverviewWidget w;
        w.resize(800, 120);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        // Find the CAN checkbox among direct children
        auto* chk = w.findChild<QCheckBox*>("", Qt::FindDirectChildrenOnly);
        // If the named search fails, fall back to any QCheckBox
        if (!chk) {
            const auto children = w.findChildren<QCheckBox*>();
            if (!children.isEmpty()) {
                chk = children.first();
            }
        }
        QVERIFY2(chk != nullptr, "Expected at least one QCheckBox child in TimelineOverviewWidget");

        // Toggle twice — should not crash
        chk->setChecked(false);
        QApplication::processEvents();
        chk->setChecked(true);
        QApplication::processEvents();

        QVERIFY(true); // Reaching here means no crash
    }

    // ------------------------------------------------------------------
    // 7. Toggling the Ethernet checkbox does not crash
    // ------------------------------------------------------------------
    /**
     * \brief Same as above but for the Ethernet checkbox (second QCheckBox).
     */
    void ethernetCheckbox_toggle_doesNotCrash()
    {
        TimelineOverviewWidget w;
        w.resize(800, 120);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        const auto children = w.findChildren<QCheckBox*>();
        QVERIFY2(children.size() >= 4, "Expected at least four QCheckBox children");

        QCheckBox* chk = children.at(1); // Ethernet is the second checkbox
        chk->setChecked(false);
        QApplication::processEvents();
        chk->setChecked(true);
        QApplication::processEvents();

        QVERIFY(true);
    }

    // ------------------------------------------------------------------
    // 8. windowPanRequested is not emitted when there is no analyzer
    // ------------------------------------------------------------------
    /**
     * \brief Dragging the mouse across the widget must not emit
     * windowPanRequested() when no Analyzer is attached (no overlay rect
     * exists to drag, and the drag-start guard will never activate).
     */
    void noAnalyzer_drag_doesNotEmitWindowPanRequested()
    {
        TimelineOverviewWidget w;
        w.resize(800, 120);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QSignalSpy spy(&w, &TimelineOverviewWidget::windowPanRequested);

        // Press + move + release across the widget
        QTest::mousePress(&w, Qt::LeftButton, Qt::NoModifier, QPoint(200, 60));
        QTest::mouseMove(&w, QPoint(400, 60));
        QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, QPoint(400, 60));

        QCOMPARE(spy.count(), 0);
    }

    // ------------------------------------------------------------------
    // 9. setVisibleWindow then mouseRelease clears drag state (no crash)
    // ------------------------------------------------------------------
    /**
     * \brief A mouseReleaseEvent arriving without a prior drag-start must
     * not crash (guards against out-of-order events).
     */
    void mouseRelease_withoutDrag_doesNotCrash()
    {
        TimelineOverviewWidget w;
        w.resize(800, 120);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        // Release without a prior press — should be a safe no-op
        QTest::mouseRelease(&w, Qt::LeftButton, Qt::NoModifier, QPoint(400, 60));

        QVERIFY(true);
    }

    // ------------------------------------------------------------------
    // 10. setVisibleWindow with equal start/end does not paint a rect
    // ------------------------------------------------------------------
    /**
     * \brief setVisibleWindow(t, t) (zero-width window) must be stored
     * but must not trigger any crash or assertion during paint.
     */
    void setVisibleWindow_zeroWidth_doesNotCrash()
    {
        TimelineOverviewWidget w;
        w.resize(800, 120);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        w.setVisibleWindow(3'000'000, 3'000'000);
        QApplication::processEvents(); // trigger repaint

        QCOMPARE(w.visibleStartUs(), uint64_t(3'000'000));
        QCOMPARE(w.visibleEndUs(), uint64_t(3'000'000));
        QVERIFY(true);
    }
};

QTEST_MAIN(tst_TimelineOverviewWidget)
#include "tst_TimelineOverviewWidget.moc"
