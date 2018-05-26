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

		Log.info("before publish");

		Particle.publish("testEvent", "x", PRIVATE);

		Log.info("after publish");
	}
}

void buttonHandler() {
	publishNow = true;
}
