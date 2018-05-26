#include "Particle.h"

SYSTEM_THREAD(ENABLED);

SerialLogHandler logHandler;

void buttonHandler();

bool publishNow = false;

void setup() {
	Serial.begin();
	System.on(button_click, buttonHandler);
}

void loop() {
	// Only try to publish when connected to the cloud
	if (publishNow) {
		publishNow = false;

		if (Particle.connected()) {
			Log.info("before publish");

			Particle.publish("testEvent", "x", PRIVATE);

			Log.info("after publish");
		}
		else {
			Log.info("publish skipped, not connected");
		}
	}
}

void buttonHandler() {
	publishNow = true;
}
