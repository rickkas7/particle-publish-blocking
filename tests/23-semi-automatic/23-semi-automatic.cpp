#include "Particle.h"

SYSTEM_MODE(SEMI_AUTOMATIC);

SerialLogHandler logHandler;

void buttonHandler();

bool publishNow = false;

void setup() {
	Serial.begin();
	System.on(button_click, buttonHandler);

	Particle.connect();
}

void loop() {
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
