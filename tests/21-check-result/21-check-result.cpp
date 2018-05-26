#include "Particle.h"

SerialLogHandler logHandler;

const unsigned long PUBLISH_PERIOD_MS = 60000;
unsigned long lastPublish = 8000 - PUBLISH_PERIOD_MS;

void setup() {
	Serial.begin();
}

void loop() {
	// In the default mode (AUTOMATIC with threading disabled), loop is only called
	// when cloud connected
	if (millis() - lastPublish >= PUBLISH_PERIOD_MS) {
		lastPublish = millis();

		Log.info("before publish");

		bool bResult = Particle.publish("testEvent", "x", PRIVATE);

		Log.info("after publish bResult=%d", bResult);
	}
}

