# Particle publish and blocking

It can be a bit confusing knowing whether a call to [Particle.publish](https://docs.particle.io/reference/firmware/electron/#particle-publish-) will be blocking. This note will hopefully clear some thing up.

Note: All of the behaviors described here are for system firmware 0.7.0. Earlier versions behave differently.

All of the logs were taken on an Electron, though the behaviors are similar (but generally faster) on the Photon/P1.

## The basics

Publish is a way to send some data to the cloud. There are four pieces of data of interest:

- Event name (up to 64 characters)
- Event data (up to 255 characters, 622 [in 0.8.0-rc.4 and later](https://github.com/particle-iot/firmware/pull/1537))
- TTL, time-to-live, currently ignored
- Scope (PUBLIC or PRIVATE)
- Acknowledgement (NO\_ACK, WITH\_ACK)

When the other side subscribes to events, it specifies an event name prefix, so you can make events begin with a common string and differentiate them after that.

The event data consists of UTF-8 text characters. You cannot send binary data, so if you have binary data you'll need to encode it, such as with Base64 or Ascii85 encoding.

The TTL value is currently ignored by the cloud. In the future it will allow the event to be stored on the cloud for a period of time. The default is 60 seconds, however this is ignored at this time. All events disappear immediately if not subscribed to.

Scope is PUBLIC or PRIVATE. In 0.8.0 and later, you need to explicitly specify one or the other. In 0.7.x and earlier, the default is PUBLIC, which is somewhat surprising. It's almost always desirable to specify PRIVATE, otherwise everyone can see the events that you publish.

The acknowledgement flag is more complicated than it would appear and most of this document is spent discussing that.

The Particle.publish function returns a bool success value, true if the publish succeeded or false if it did not, however the semantics are much more complicated than that, and are described below in *Testing the result*.

## Using SYSTEM_THREAD(ENABLED)

The most interesting cases are when using the system thread so we'll start with those. There's a shorter section at the end for non-threaded.

### Testing the result

One important change in system firmware 0.7.0 is that testing the result from Particle.publish makes a difference!

Fairly logically, if you want to know if the call succeeded or not, you need to wait until the call is complete. 

In 0.7.0 and later, Particle.publish returns a `Particle::future<bool>`. What this means is that if you test the return value, the current thread will be blocked until the result is available (that's the future part). 

If you don't test the value, execution proceeds along without waiting. This somewhat independent of NO\_ACK, WITH\_ACK, which will be explained in more detail below.

### Simple case

This is a simple program that publishes once a minute. It the code in 01-simple.

```
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

		Particle.publish("testEvent", "x", PRIVATE);

		Log.info("after publish");
	}
}
```

Serial log output. The number in the left column is the timestamp in milliseconds.

```
0000008000 [app] INFO: before publish
0000008211 [app] INFO: after publish
0000008693 [comm.protocol] INFO: rcv'd message type=13
0000068000 [app] INFO: before publish
0000069281 [app] INFO: after publish
0000071633 [comm.protocol] INFO: rcv'd message type=13
0000128000 [app] INFO: before publish
0000128731 [app] INFO: after publish
0000131083 [comm.protocol] INFO: rcv'd message type=13
0000188000 [app] INFO: before publish
0000188441 [app] INFO: after publish
0000190793 [comm.protocol] INFO: rcv'd message type=13
```

Each publish has the Electron sending 75 bytes (65 bytes of overhead + 10 bytes of event name and data). There's also a 61 byte ACK. That corresponds to message type=13 in the serial debug log.

This is all of the data that was transmitted to and from the Particle cloud in the period above. > is to cloud and < is from the cloud.

```
> 75
< 61
> 75
< 61
> 75
< 61
> 75
< 61
```

### With lost data

If you were to have a lossy connection and say the first two publish packets were lost, what does it look like?

The serial log looks the same. Note that publish returns before the data is acknowledged with the code above.

```
0000308000 [app] INFO: before publish
0000309381 [app] INFO: after publish
0000329003 [comm.protocol] INFO: rcv'd message type=13
```

The publish call returned after 1.3 seconds, but the data wasn't acknowledged for another 20 seconds because of the packet loss.

The first two 75-byte packets were lost, then on the third time, the data was successfully transferred and the ACK returned. Thus the additional data usage would be 150 bytes.

```
data off
> 75 discarded
> 75 discarded
data on
> 75
< 61
```

The data loss is simulated using the [Electron cloud manipulator](https://github.com/rickkas7/electron-cloud-manipulator) which allows me to cause packets to the cloud to be lost, among other things.

### With a disconnected antenna in blinking green

This test uses the code in 06-button. When the MODE button is pressed, a publish is attempted. It uses AUTOMATIC with SYSTEM\_THREAD(ENABLED).

```
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
```

With the antenna connected and the Electron breathing cyan:

```
0000090394 [app] INFO: before publish
0000091616 [app] INFO: after publish
0000094318 [comm.protocol] INFO: rcv'd message type=13
```

If I disconnect the antenna, the Electron will breathe cyan for a while, but after about 30 seconds it will start blinking green.

If I hit the publish button while in blinking green, even without checking the result, Particle.publish will block. This is because if the cloud connection is expected to be up, it will wait for it to be up.

It takes a long time for this to time out, almost 5 minutes!

```
0000159318 [system] INFO: Cloud: disconnecting
0000159318 [system] INFO: Cloud: disconnected
0000159318 [system] INFO: ARM_WLAN_WD 3
0000159430 [system] INFO: Sim Ready
0000159430 [system] INFO: ARM_WLAN_WD 1
0000176084 [app] INFO: before publish
0000467823 [system] WARN: Resetting WLAN due to WLAN_WD_TO()
0000467823 [app] INFO: after publish
0000467913 [system] INFO: DHCP fail, ARM_WLAN_WD 4
0000472533 [system] INFO: Sim Ready
0000472533 [system] INFO: ARM_WLAN_WD 1
```

That's not very good. Let's see if we can work around that.

### With Particle.connected check

I added a check for Particle.connected before publishing to the previous code. This code is in 07-button-check.

```
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
```

While in breathing cyan I again disconnected the antenna, and waited for the Electron to blink green. This took about 30 seconds. Then I pressed the MODE button to publish.

In this case, the publish was skipped and there was no blocking!

```
0000091251 [system] INFO: Cloud: disconnecting
0000091251 [system] INFO: Cloud: disconnected
0000091251 [system] INFO: ARM_WLAN_WD 3
0000091361 [system] INFO: Sim Ready
0000091361 [system] INFO: ARM_WLAN_WD 1
0000106204 [app] INFO: publish skipped, not connected
```

That's much better. The moral of the story is:

**Always check Particle.connected() before doing Particle.publish().**


### What about the period before blinking green?

Using the code in 07-button-check, what if I disconnect the antenna and then hit the MODE button to publish while still breathing cyan, but with the antenna disconnected.

```
0000176534 [app] INFO: before publish
0000178246 [app] INFO: after publish
0000199196 [system] INFO: Cloud: disconnecting
0000199196 [system] INFO: Cloud: disconnected
0000199196 [system] INFO: ARM_WLAN_WD 3
0000199306 [system] INFO: Sim Ready
0000199306 [system] INFO: ARM_WLAN_WD 1
0000232368 [system] INFO: ARM_WLAN_WD 2
0000232368 [system] INFO: CLR_WLAN_WD 1, DHCP success
0000232368 [system] INFO: Cloud: connecting
0000232642 [system] INFO: Read Server Address = type:0,domain:*,ip: 65.19.178.42, port: 65535
0000232664 [system] INFO: Cloud socket connected
0000232664 [system] INFO: Starting handshake: presense_announce=0
0000232666 [comm.protocol.handshake] INFO: Establish secure connection
0000232692 [comm.dtls] INFO: (CMPL,RENEG,NO_SESS,ERR) restoreStatus=0
0000232692 [comm.dtls] INFO: out_ctr 0,1,0,0,0,0,0,37, next_coap_id=18
0000232694 [comm.dtls] INFO: app state crc: 92e17132, expected: 92e17132
0000232694 [comm.dtls] WARN: skipping hello message
0000232694 [comm.dtls] INFO: restored session from persisted session data. next_msg_id=24
0000232696 [comm.dtls] INFO: session cmd (CLS,DIS,MOV,LOD,SAV): 2
0000232908 [comm.protocol.handshake] INFO: resumed session - not sending HELLO message
0000232908 [system] INFO: cloud connected from existing session.
0000232910 [system] INFO: Cloud connected
0000235262 [comm.protocol] INFO: rcv'd message type=13
```

In this case, Particle.publish still returns more or less immediately (1.7 seconds), and then some time later the Electron realizes it's not connected and goes into blinking green.

Note the last message, though. Once reconnected to the cloud that publish still goes out (ACK is type=13). This works for at least 4 small messages, maybe more, though curiously they may arrive out-of-order when queued up this way.


### With NO\_ACK

The code is in the 01-simple example, except the NO\_ACK option is specified. The full code is in 02-no-ack.

```
		Particle.publish("testEvent", "x", PRIVATE, NO_ACK);
```

The serial logs look similar, but notice that there is no message type=13 in the log now.

```
0000068000 [app] INFO: before publish
0000069231 [app] INFO: after publish
0000128000 [app] INFO: before publish
0000129711 [app] INFO: after publish
```

And the data transmission looks like this. No ACK packets, saving 61 bytes per publish.

```
> 75
> 75
```

### With NO\_ACK and lossy connection

Of course if you use NO\_ACK and have packet loss, the events are lost forever.

Serial log looks normal:

```
0000248000 [app] INFO: before publish
0000249321 [app] INFO: after publish
```

However there is only the one transmission that was discarded and no retry. The data usage is only 75 bytes (though the publish was lost).

```
> 75 discarded
```

### Checking the result

This code is like the example 01-simple, except it checks the result from Particle.publish and prints it. The full code is in 03-check-result.

```
		Log.info("before publish");

		bool bResult = Particle.publish("testEvent", "x", PRIVATE);

		Log.info("after publish bResult=%d", bResult);
	}
```

The serial logs look similar:

```
0000068000 [app] INFO: before publish
0000068351 [app] INFO: after publish bResult=1
0000070813 [comm.protocol] INFO: rcv'd message type=13
0000128000 [app] INFO: before publish
0000130651 [app] INFO: after publish bResult=1
0000133003 [comm.protocol] INFO: rcv'd message type=13
```

As do the data logs. That's a publish of 75 bytes, ACK of 61 bytes, repeated twice.

```
> 75
< 61
> 75
< 61
```

### Checking the result with a lossy connection

The first publish is normal, the second one has packet loss on the first two attempts to send:

```
0000068000 [app] INFO: before publish
0000068351 [app] INFO: after publish bResult=1
0000070813 [comm.protocol] INFO: rcv'd message type=13
0000128000 [app] INFO: before publish
0000130651 [app] INFO: after publish bResult=1
0000133003 [comm.protocol] INFO: rcv'd message type=13
```

```
> 75
< 61
data off
> 75 discarded
> 75 discarded
data on
$ > 75
< 61
```

The important thing to note is that in this version, you get a true result before the ACK is received. It doesn't fully test that the data was acknowledged by the server.

### Using WITH\_ACK

This is like 03-check-result, but this time I added in WITH\_ACK. The important thing about WITH\_ACK is that it doesn't use more data than the regular case (nothing specified). It just causes Particle.publish to block until the ACK is received.

```
		bool bResult = Particle.publish("testEvent", "x", PRIVATE, WITH_ACK);
```

The major thing you'll notice in the serial log is that Particle.publish doesn't return until after the ACK is received:

```
0000008000 [app] INFO: before publish
0000008693 [comm.protocol] INFO: rcv'd message type=13
0000008693 [app] INFO: after publish bResult=1
0000068000 [app] INFO: before publish
0000070563 [comm.protocol] INFO: rcv'd message type=13
0000070563 [app] INFO: after publish bResult=1
```

Data usage is the same, though.

```
> 75
< 61
> 75
< 61
```

### Using WITH\_ACK and a lossy connection

In this example, the first two attempts to send were lost, but it succeeds on the third.

```
0000128000 [app] INFO: before publish
0000147243 [comm.protocol] INFO: rcv'd message type=13
0000147243 [app] INFO: after publish bResult=1
```

```
data off
> 75 discarded
> 75 discarded
data on
> 75
< 61
```

The main thing to note is that that the call blocks until the ACK is received, in this case 19 seconds later.

### Using WITH\_ACK and a really lossy connection

In this example, data was lost for all three attempts to publish.

```
0000248000 [app] INFO: before publish
0000268001 [app] INFO: after publish bResult=0
```

```
data off
> 75 discarded
> 75 discarded
> 75 discarded
```

Note that the publish call returns 0 (failure) in this case but it takes 20 seconds to do so.

### Using WITH\_ACK and a disconnected antenna

This example uses the MODE button to initiate a publish so I can more precisely control when it happens. The code is in 08-button-with-ack.

```
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

```

In this example I was in breathing cyan, disconnected the antenna, then pressed the MODE button to publish. The serial log looked like this:

```
0000032984 [app] INFO: before publish
0000052946 [app] INFO: after publish bResult=0
0000054376 [system] INFO: Cloud: disconnecting
0000054376 [system] INFO: Cloud: disconnected
0000054376 [system] INFO: ARM_WLAN_WD 3
0000054488 [system] INFO: Sim Ready
0000054488 [system] INFO: ARM_WLAN_WD 1
```

After the 20 second timeout, Particle.publish returned false and went into blinking green.

If I then reconnected the antenna, note that the event still goes out, type=13 is the ACK.

```
0000104560 [system] INFO: cloud connected from existing session.
0000104562 [system] INFO: Cloud connected
0000105484 [comm.protocol] INFO: rcv'd message type=13
```


### SEMI\_AUTOMATIC

In SEMI\_AUTOMATIC and MANUAL modes, if you're in the not connected state, Particle.publish immediately fails with a false result and doesn't try to connect. This code is 09-semi-automatic-simple.

```
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
```

```
0000008000 [app] INFO: before publish
0000008000 [app] INFO: after publish bResult=0
0000068000 [app] INFO: before publish
0000068000 [app] INFO: after publish bResult=0
```


## Without system thread

### Simple case (non-thread)

This is example 20-simple.

```
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

		Particle.publish("testEvent", "x", PRIVATE);

		Log.info("after publish");
	}
}

```

Serial log output. The number in the left column is the timestamp in milliseconds.

```
0000071154 [app] INFO: before publish
0000073485 [app] INFO: after publish
0000075736 [comm.protocol] INFO: rcv'd message type=13
0000131156 [app] INFO: before publish
0000133177 [app] INFO: after publish
0000135408 [comm.protocol] INFO: rcv'd message type=13
```

Each publish has the Electron sending 75 bytes (65 bytes of overhead + 10 bytes of event name and data). There's also a 61 byte ACK.

```
> 75
< 61
```

### With lost data (non-thread)

If you were to have a lossy connection and say the first two publish packets were lost, what does it look like? It looks the same with system threading on and off, actually.

The serial log looks the same. Note that publish returns before the data is successfully sent with the code above.

```
0000191158 [app] INFO: before publish
0000192379 [app] INFO: after publish
0000211090 [comm.protocol] INFO: rcv'd message type=13
```

The first two 75-byte packets were lost, then on the third time, the data was successfully transferred and the ACK returned. Thus the additional data usage would be 150 bytes.

```
data off
> 75 discarded
> 75 discarded
data on
> 75
< 61
```

### With checking the result (non-thread)

This example uses non-threaded mode and checks the result. The code is in 21-check-result.

```
		Log.info("before publish");

		bool bResult = Particle.publish("testEvent", "x", PRIVATE);

		Log.info("after publish bResult=%d", bResult);
```

The serial log looks the same as threaded mode.

```
0000008005 [app] INFO: before publish
0000008216 [app] INFO: after publish bResult=1
0000008787 [comm.protocol] INFO: rcv'd message type=13
0000068007 [app] INFO: before publish
0000069378 [app] INFO: after publish bResult=1
0000071659 [comm.protocol] INFO: rcv'd message type=13
```

Note that it returns true before it gets the ACK.


### Using WITH\_ACK (non-thread)

This code is in 22-button-with-ack. When the MODE button is pressed the Electron will publish. It uses the default AUTOMATIC and non-threaded and checks the result from publishing.

```
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

```

The serial log for normal publishing while breathing cyan:

```
0000080925 [app] INFO: before publish
0000084477 [comm.protocol] INFO: rcv'd message type=13
0000084477 [app] INFO: after publish bResult=1
0000106157 [app] INFO: before publish
0000109769 [comm.protocol] INFO: rcv'd message type=13
0000109769 [app] INFO: after publish bResult=1
```

Data usage is normal:

```
> 75
< 61
> 75
< 61
```

### With lossy connection (non-thread)

With the code from the previous test and simulating a lossy connection where the first two publish packets are lost:

```
0000144559 [app] INFO: before publish
0000163581 [comm.protocol] INFO: rcv'd message type=13
0000163581 [app] INFO: after publish bResult=1
```

```
> 75 discarded
> 75 discarded
data on
> 75
< 61
```

The publish still works, but it takes longer, and publish blocks until it succeeds.

### With a really lossy connection (non-thread)

With all three attempts to send data failing, the publish call times out after 20 seconds and returns false.

```
0000235681 [app] INFO: before publish
0000255682 [app] INFO: after publish bResult=0
```

However, there's an invisible fourth try, and if that succeeds, then the publish goes out, even though false was returned.

The type=13 is the ACK, occurring 20 seconds after publish returned false.

```
0000275703 [comm.protocol] INFO: rcv'd message type=13
```

The data was turned back on after publish returned false, but you can see the fourth transmission and ACK.

```
data off
> 75 discarded
> 75 discarded
> 75 discarded
data on
> 75
< 61
```

### With a disconnected antenna, blinking green (non-thread)

Using the previous code, if I disconnect the antenna, after about 30 seconds the Electron will start blinking green. If I then hit the mode button, nothing happens.

This is because in the default AUTOMATIC with threading disabled, the loop is not run.

However, since the code sets a flag in the button handler, when the antenna is reconnected the loop is finally run and the publish goes out.

```
0000469153 [system] INFO: Cloud: disconnecting
0000469153 [system] INFO: Cloud: disconnected
0000469153 [system] INFO: ARM_WLAN_WD 3
0000469163 [system] INFO: Sim Ready
0000469163 [system] INFO: ARM_WLAN_WD 1
0000504234 [system] INFO: ARM_WLAN_WD 2
0000504234 [system] INFO: CLR_WLAN_WD 1, DHCP success
0000504234 [system] INFO: Cloud: connecting
0000504506 [system] INFO: Read Server Address = type:0,domain:*,ip: 65.19.178.42, port: 65535
0000504527 [system] INFO: Cloud socket connected
0000504528 [system] INFO: Starting handshake: presense_announce=0
0000504528 [comm.protocol.handshake] INFO: Establish secure connection
0000504541 [comm.dtls] INFO: (CMPL,RENEG,NO_SESS,ERR) restoreStatus=0
0000504541 [comm.dtls] INFO: out_ctr 0,1,0,0,0,0,0,73, next_coap_id=2f
0000504542 [comm.dtls] INFO: app state crc: 92e17132, expected: 113a6026
0000504542 [comm.dtls] INFO: restored session from persisted session data. next_msg_id=47
0000504543 [comm.dtls] INFO: session cmd (CLS,DIS,MOV,LOD,SAV): 2
0000504754 [comm.protocol.handshake] INFO: resumed session - not sending HELLO message
0000504754 [system] INFO: cloud connected from existing session.
0000504755 [system] INFO: Cloud connected
0000504765 [app] INFO: before publish
0000507078 [comm.protocol] INFO: rcv'd message type=13
0000507120 [comm.protocol] INFO: rcv'd message type=13
0000507120 [app] INFO: after publish bResult=1
```

### Antenna disconnected, still breathing cyan (non-thread)

If I disconnect the antenna and press the MODE button while still breathing cyan, the publish call times out after 20 seconds and the Electron goes into blinking green.

However, if I then reconnect the antenna, the publish still goes out. Note the type=13 ACK at the end.

```
0000575681 [app] INFO: before publish
0000595682 [app] INFO: after publish bResult=0
0000596372 [system] INFO: Cloud: disconnecting
0000596372 [system] INFO: Cloud: disconnected
0000596372 [system] INFO: ARM_WLAN_WD 3
0000596382 [system] INFO: Sim Ready
0000596382 [system] INFO: ARM_WLAN_WD 1
0000630363 [system] INFO: ARM_WLAN_WD 2
0000630363 [system] INFO: CLR_WLAN_WD 1, DHCP success
0000630363 [system] INFO: Cloud: connecting
0000630635 [system] INFO: Read Server Address = type:0,domain:*,ip: 65.19.178.42, port: 65535
0000630656 [system] INFO: Cloud socket connected
0000630656 [system] INFO: Starting handshake: presense_announce=0
0000630657 [comm.protocol.handshake] INFO: Establish secure connection
0000630670 [comm.dtls] INFO: (CMPL,RENEG,NO_SESS,ERR) restoreStatus=0
0000630670 [comm.dtls] INFO: out_ctr 0,1,0,0,0,0,0,78, next_coap_id=32
0000630671 [comm.dtls] INFO: app state crc: 92e17132, expected: 113a6026
0000630671 [comm.dtls] INFO: restored session from persisted session data. next_msg_id=50
0000630672 [comm.dtls] INFO: session cmd (CLS,DIS,MOV,LOD,SAV): 2
0000630883 [comm.protocol.handshake] INFO: resumed session - not sending HELLO message
0000630883 [system] INFO: cloud connected from existing session.
0000630884 [system] INFO: Cloud connected
0000633315 [comm.protocol] INFO: rcv'd message type=13
```

### SEMI\_AUTOMATIC (non-thread)

In SEMI\_AUTOMATIC mode, if I disconnect the antenna and wait around 30 seconds, the Electron will go into blinking green. If I then hit the MODE button, nothing happens.

The loop apparently does not run in this mode. If I reconnect the antenna and get back to breathing cyan, the publish goes out.

```
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

```

```
0000081985 [system] INFO: Cloud: disconnecting
0000081985 [system] INFO: Cloud: disconnected
0000081985 [system] INFO: ARM_WLAN_WD 3
0000081995 [system] INFO: Sim Ready
0000081995 [system] INFO: ARM_WLAN_WD 1
0000132026 [system] INFO: ARM_WLAN_WD 2
0000132026 [system] INFO: CLR_WLAN_WD 1, DHCP success
0000132026 [system] INFO: Cloud: connecting
0000132298 [system] INFO: Read Server Address = type:0,domain:*,ip: 65.19.178.42, port: 65535
0000132319 [system] INFO: Cloud socket connected
0000132319 [system] INFO: Starting handshake: presense_announce=0
0000132320 [comm.protocol.handshake] INFO: Establish secure connection
0000132333 [comm.dtls] INFO: (CMPL,RENEG,NO_SESS,ERR) restoreStatus=0
0000132333 [comm.dtls] INFO: out_ctr 0,1,0,0,0,0,0,100, next_coap_id=42
0000132334 [comm.dtls] INFO: app state crc: 1010c6d2, expected: 1010c6d2
0000132334 [comm.dtls] WARN: skipping hello message
0000132334 [comm.dtls] INFO: restored session from persisted session data. next_msg_id=66
0000132335 [comm.dtls] INFO: session cmd (CLS,DIS,MOV,LOD,SAV): 2
0000132546 [comm.protocol.handshake] INFO: resumed session - not sending HELLO message
0000132546 [system] INFO: cloud connected from existing session.
0000132547 [system] INFO: Cloud connected
0000132557 [app] INFO: before publish
0000132769 [app] INFO: after publish
0000134890 [comm.protocol] INFO: rcv'd message type=13
0000134932 [comm.protocol] INFO: rcv'd message type=13
```

## Using PublishQueueAsyncRK

Another solution to this problem is to use the [PublishQueueAsyncRK](https://github.com/rickkas7/PublishQueueAsyncRK) library.

This library is designed for fire-and-forget publishing of events. It allows you to publish, even when not connected to the cloud, and the events are saved until connected. It also buffers events so you can call it a bunch of times rapidly and the events are metered out one per second to stay within the publish limits.

Also, it's entirely non-blocking. The publishing occurs from a separate thread so the loop is never blocked.

And it uses retained memory, so the events are saved when you reboot or go into sleep mode. They'll be transmitted when you finally connect to the cloud again.



