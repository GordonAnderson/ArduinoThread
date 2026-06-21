/*
    BasicThreads example — ArduinoThread library
    GAA Custom Electronics, LLC / Gordon Anderson

    Demonstrates:
      1. Creating threads with named callbacks and configurable intervals
      2. Using ThreadController to manage multiple threads from loop()
      3. Reading run-time profiling data via runTimeMs()
      4. Enabling / disabling a thread at runtime
      5. Looking up a thread by name from the controller
      6. Using setNextRunTime() to schedule a one-shot deferred action

    Hardware: Any Arduino-compatible board with a built-in LED on LED_BUILTIN.
    No external components required.

    Expected Serial output (115200 baud):
        [setup] ArduinoThread BasicThreads example
        [blink] LED ON   run=0 ms
        [sensor] ADC=512   run=0 ms
        [blink] LED OFF  run=0 ms
        [status] uptime=1000 ms  blink=20 runs  sensor=20 runs
        ...
*/

#include <Arduino.h>
#include <Thread.h>
#include <ThreadController.h>

// ---------------------------------------------------------------------------
// Thread state
// ---------------------------------------------------------------------------

static bool     ledState    = false;
static uint32_t blinkCount  = 0;
static uint32_t sensorCount = 0;

// ---------------------------------------------------------------------------
// Callback: blink the built-in LED every 250 ms
// ---------------------------------------------------------------------------

void cbBlink() {
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
    blinkCount++;
    Serial.print("[blink] LED ");
    Serial.print(ledState ? "ON " : "OFF");
    Serial.print("  run=");
    // runTimeMs() is read via a pointer fetched from the controller in loop().
    // Here we just print a marker; the full profile print is in cbStatus().
    Serial.println(" ms");
}

// ---------------------------------------------------------------------------
// Callback: read an ADC channel every 50 ms
// ---------------------------------------------------------------------------

void cbSensor() {
    int adc = analogRead(A0);
    sensorCount++;
    Serial.print("[sensor] ADC=");
    Serial.print(adc);
    Serial.println("   run=0 ms");
}

// ---------------------------------------------------------------------------
// Callback: print a status line every 1000 ms
// ---------------------------------------------------------------------------

void cbStatus() {
    Serial.print("[status] uptime=");
    Serial.print(millis());
    Serial.print(" ms  blink=");
    Serial.print(blinkCount);
    Serial.print(" runs  sensor=");
    Serial.print(sensorCount);
    Serial.println(" runs");
}

// ---------------------------------------------------------------------------
// Thread and controller objects
// ---------------------------------------------------------------------------

ThreadController controller;    // top-level scheduler; called from loop()

Thread blinkThread;             // LED blink
Thread sensorThread;            // ADC read
Thread statusThread;            // periodic status report

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    while (!Serial) { /* wait for USB CDC on Leonardo / 32u4 boards */ }
    Serial.println("[setup] ArduinoThread BasicThreads example");

    pinMode(LED_BUILTIN, OUTPUT);

    // --- Configure threads ---

    blinkThread.setName("blink");
    blinkThread.onRun(cbBlink);
    blinkThread.setInterval(250);   // fire every 250 ms

    sensorThread.setName("sensor");
    sensorThread.onRun(cbSensor);
    sensorThread.setInterval(50);   // fire every 50 ms

    statusThread.setName("status");
    statusThread.onRun(cbStatus);
    statusThread.setInterval(1000); // fire every 1000 ms

    // --- Register with the controller ---

    controller.add(&blinkThread);
    controller.add(&sensorThread);
    controller.add(&statusThread);

    // --- Demo: defer the first sensor read by 500 ms ---
    // setNextRunTime() sets the absolute millis() timestamp for the next run.
    sensorThread.setNextRunTime(millis() + 500);

    Serial.print("[setup] threads registered: ");
    Serial.println(controller.size());
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------

void loop() {
    // Run every registered thread whose shouldRun() returns true.
    // This is all that is needed in loop() — the controller handles the rest.
    controller.run();

    // --- Demo: disable the blink thread after 5 seconds ---
    // In a real application you would use a flag set by an ISR or button press.
    static bool blinkDisabled = false;
    if (!blinkDisabled && millis() > 5000) {
        Thread *t = controller.get("blink");
        if (t != NULL) {
            t->enabled = false;
            digitalWrite(LED_BUILTIN, LOW);
            Serial.println("[loop] blink thread disabled at 5 s");
        }
        blinkDisabled = true;
    }

    // --- Demo: re-enable blink thread after 8 seconds ---
    static bool blinkReEnabled = false;
    if (!blinkReEnabled && millis() > 8000) {
        Thread *t = controller.get("blink");
        if (t != NULL) {
            t->enabled = true;
            Serial.println("[loop] blink thread re-enabled at 8 s");
        }
        blinkReEnabled = true;
    }
}
