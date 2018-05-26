#include "Particle.h"

SYSTEM_THREAD(ENABLED);

SerialLogHandler logHandler;

const unsigned long PUBLISH_PERIOD_MS = 60000;
unsigned long lastPublish = 8000 - PUBLISH_PERIOD_MS;

void setup() {
	Serial.begin();
}

void loop() {
	// Only try to publish when connected to the cloud
	if (Particle.connected() && millis() - lastPublish >= PUBLISH_PERIOD_MS) {
		lastPublish = millis();

		Log.info("before publish");

		Particle.publish("testEvent", "x", PRIVATE, NO_ACK);

		Log.info("after publish");
	}
}

