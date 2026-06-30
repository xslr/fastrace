# Quickstart: Anomaly Detection Framework

This guide explains how to use the new Anomaly Detection Framework in Fastrace.

## Running Detection in the UI

1. Open `fastrace_ui` and load a trace file containing Ethernet/CAN traffic.
2. (Optional) Load an ARXML database for database-dependent anomaly checks.
3. Click the **Run Detectors** button in the Top Bar.
4. Watch the progress bar in the status area. You can click **Cancel** at any time.
5. When finished, detections will populate the **DetectionsWidget** in the right sidebar.

## Navigating Detections

1. Open the **Overview** or **Timeline** view.
2. In the **DetectionsWidget**, click any row in the detection table.
3. The **MessageListWidget** will automatically scroll to and select the exact trace message that triggered the anomaly.
4. The **MessageDetailsWidget** will update to show the raw payload and signals for that message.

## Filtering

1. Use the **Severity** dropdown to filter between All, Error, Warning, and Info.
2. Use the **Detector** dropdown to focus on a specific protocol (e.g., DoipDetector).

## Adding a New Detector (Backend)

1. Create a new class inheriting from `Detector` in `cpp/include/detectors/`.
2. Implement `name()`, `inspect()`, and `finalize()`.
3. If stateful, implement `isStateful()`, `clone()`, and `merge()`.
4. Register it in `DetectionEngine` constructor.
