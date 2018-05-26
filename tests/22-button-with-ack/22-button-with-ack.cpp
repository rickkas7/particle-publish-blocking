#include "Particle.h"

SerialLogHandler logHandler;

void buttonHandler();

bool publishNow = false;

void setup() {
	Serial.begin();
	System.on(button_click, buttonHandler);
}

void loop() {
	// In the default mode (AUTOMATIC with threading disabled), loop is only called
	// when cloud connected
	if (publishNow) {
		publishNow = false;

		Log.info("before publish");

		bool bResult = Particle.publish("testEvent", "x", PRIVATE, WITH_ACK);

		Log.info("after publish bResult=%d", bResult);
	}
}

void buttonHandler() {
	publishNow = true;
}
