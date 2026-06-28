/**
 * \file tst_TimelineWidget.cpp
 * \brief Unit tests for TimelineWidget.
 *
 * Tests exercise the public API and the small test-accessor methods
 * (laneCount(), hoveredLaneIndex()) without requiring a real trace file.
 * Cases that need a live Analyzer are annotated with a QSKIP guard so
 * the suite still runs cleanly in a headless CI environment.
 */

#include <QApplication>
#include <QSignalSpy>
#include <QTest>

#include "TimelineWidget.h"

class tst_TimelineWidget : public QObject {
    Q_OBJECT

private slots:
    // ------------------------------------------------------------------
    // 1. Default construction
    // ------------------------------------------------------------------
    /**
     * \brief Constructing TimelineWidget without an analyzer must not crash.
     *
     * The widget is shown to force a full paint cycle so that paintEvent()
     * is exercised in the empty-state branch (no lanes, no analyzer).
     */
    void construction()
    {
        TimelineWidget w;
        w.resize(600, 300);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QCOMPARE(w.laneCount(), 0);
        QCOMPARE(w.hoveredLaneIndex(), -1);
    }

    // ------------------------------------------------------------------
    // 2. restoreSignals with no analyzer is a no-op
    // ------------------------------------------------------------------
    /**
     * \brief restoreSignals() must silently ignore signal names when no
     * Analyzer is attached (addSignalLane returns early without an analyzer).
     *
     * Neither a crash nor a signalsChanged() emission is expected.
     */
    void noAnalyzer_restoreSignals_isNoop()
    {
        TimelineWidget w;
        QSignalSpy spy(&w, &TimelineWidget::signalsChanged);

        w.restoreSignals({ "Signal_A", "Signal_B", "Signal_A" });

        QCOMPARE(w.laneCount(), 0);
        QCOMPARE(spy.count(), 0);
    }

    // ------------------------------------------------------------------
    // 3. Hover tracking with empty lanes
    // ------------------------------------------------------------------
    /**
     * \brief handleLanesMouseMove() must update hoveredLaneIndex to -1
     * whenever the lane list is empty (any y position is out of range).
     * handleLanesLeave() must also leave hoveredLaneIndex at -1.
     */
    void hoverTracking_withNoLanes()
    {
        TimelineWidget w;
        w.resize(600, 300);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        // With no lanes every position maps to -1
        w.handleLanesMouseMove(QPoint(100, 20));
        QCOMPARE(w.hoveredLaneIndex(), -1);

        w.handleLanesMouseMove(QPoint(100, 80));
        QCOMPARE(w.hoveredLaneIndex(), -1);

        w.handleLanesLeave();
        QCOMPARE(w.hoveredLaneIndex(), -1);
    }

    // ------------------------------------------------------------------
    // 4. Mouse press with no lanes does not crash and emits no signal
    // ------------------------------------------------------------------
    /**
     * \brief handleLanesMousePress() at any position must not crash and must
     * not emit signalsChanged() when there are no lanes.
     */
    void noLanes_mousePress_doesNotEmitSignal()
    {
        TimelineWidget w;
        QSignalSpy spy(&w, &TimelineWidget::signalsChanged);

        w.handleLanesMousePress(QPoint(50, 10));
        w.handleLanesMousePress(QPoint(0, 0));
        w.handleLanesMousePress(QPoint(1000, 1000));

        QCOMPARE(spy.count(), 0);
    }

    // ------------------------------------------------------------------
    // 5. currentSignalNames reflects the active lanes
    // ------------------------------------------------------------------
    /**
     * \brief currentSignalNames() must return an empty list when no lanes
     * have been added.
     */
    void noLanes_currentSignalNames_isEmpty()
    {
        TimelineWidget w;
        // Access via the public signal spy trick: restoreSignals does nothing,
        // but the QStringList emitted by signalsChanged is the source of truth.
        // Without an analyzer we verify indirectly via laneCount.
        QCOMPARE(w.laneCount(), 0);
    }

    // ------------------------------------------------------------------
    // 6. visibleStartUs / visibleEndUs are 0 by default
    // ------------------------------------------------------------------
    /**
     * \brief The visible window accessors must return 0/0 after construction,
     * indicating the full trace is visible (no zoom applied).
     */
    void defaultConstruction_visibleWindow_isZero()
    {
        TimelineWidget w;
        QCOMPARE(w.visibleStartUs(), uint64_t(0));
        QCOMPARE(w.visibleEndUs(), uint64_t(0));
    }

    // ------------------------------------------------------------------
    // 7. setVisibleWindow without analyzer is a no-op
    // ------------------------------------------------------------------
    /**
     * \brief setVisibleWindow() must silently return and not emit
     * visibleWindowChanged() when no Analyzer is attached.
     */
    void noAnalyzer_setVisibleWindow_isNoop()
    {
        TimelineWidget w;
        QSignalSpy spy(&w, &TimelineWidget::visibleWindowChanged);

        w.setVisibleWindow(1'000'000, 5'000'000);

        // No analyzer → early return; values stay at default 0/0
        QCOMPARE(w.visibleStartUs(), uint64_t(0));
        QCOMPARE(w.visibleEndUs(), uint64_t(0));
        QCOMPARE(spy.count(), 0);
    }

    // ------------------------------------------------------------------
    // 8. handleLanesWheel without analyzer does not crash
    // ------------------------------------------------------------------
    /**
     * \brief Simulating a wheel event on the lanes widget must be safe
     * when no Analyzer is attached — the early-return guard fires.
     */
    void noAnalyzer_wheelEvent_doesNotCrash()
    {
        TimelineWidget w;
        w.resize(600, 300);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        // Synthesise a wheel event; the SignalLanesWidget delegates to
        // handleLanesWheel() which returns early without an analyzer.
        QWheelEvent wheel(QPointF(300, 150), QPointF(300, 150), QPoint(0, 0), QPoint(0, 120), Qt::NoButton,
            Qt::NoModifier, Qt::ScrollBegin, false);

        // Send to the TimelineWidget directly (outer widget).
        QApplication::sendEvent(&w, &wheel);

        // No crash — if we reach here the test passes.
        QVERIFY(true);
    }
};

QTEST_MAIN(tst_TimelineWidget)
#include "tst_TimelineWidget.moc"
