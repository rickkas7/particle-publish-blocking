#include "Particle.h"

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

SerialLogHandler logHandler;

const unsigned long PUBLISH_PERIOD_MS = 60000;
unsigned long lastPublish = 8000 - PUBLISH_PERIOD_MS;

void setup() {
	Serial.begin();
}

void loop() {
	if (millis() - lastPublish >= PUBLISH_PERIOD_MS) {
		lastPublish = millis();

		Log.info("before publish");

		bool bResult = Particle.publish("testEvent", "x", PRIVATE, WITH_ACK);

		Log.info("after publish bResult=%d", bResult);
	}
}
