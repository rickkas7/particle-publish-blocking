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

			bool bResult = Particle.publish("testEvent", "x", PRIVATE, WITH_ACK);

			Log.info("after publish bResult=%d", bResult);
		}
		else {
			Log.info("publish skipped, not connected");
		}
	}
}

void buttonHandler() {
	publishNow = true;
}
