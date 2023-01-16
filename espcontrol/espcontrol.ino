#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <AsyncUDP.h>

#include "private.h"
#include "dbg.h"

#include "colorcontrol.h"

#define NUM_PX 109
#define PX_PIN 23   // GPIO23

#define REFRESH_DELAY     20   // in ms

Adafruit_NeoPixel px(NUM_PX, PX_PIN, NEO_GRB + NEO_KHZ800);

AsyncUDP udp;

void setup() {

#ifdef DBG
  Serial.begin(115200);
#endif

  // setup pixel strip
  px.begin();
  delay(100);
  px.clear();
  px.show();

  // setup the wifi connection
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // DEBUG
  dbgl("Scanning visible wifi networks:");
  for (int n = WiFi.scanNetworks(); n >=0; n--) {
    dbgl(WiFi.SSID(n));
  }
  delay(100);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    dbgf("Connecting to Wifi %d\n", i);
    delay(1000);
    i++;
  }

  dbgl("Connected!");
  dbgl(WiFi.localIP());

  // this uses upd multicast on the local network
  // if we wanted this to be more generic we could just poll some server for updates, but eh
  // setup the async udp listener
  if (!udp.listenMulticast(IPAddress(239,3,6,9), 369)) {
    dbgl("Unable to listen for multicast!");
    goto error;
  }

  udp.onPacket([](AsyncUDPPacket packet) {
    size_t len = packet.length();

    dbgf("Got packet of length %d\n", len);

    parse_packet(packet.data(), len);
  });


  dbgl("Initialized");
  return;
error:
  while (true) {
    delay(1000);
  };
}

void loop() {
  //DBG test just cycle color
  uint32_t c = 0x00002000;
  for (int i = 0; i < NUM_PX; i++) {
    if (i > 0) {
      // clear previous
      px.setPixelColor(i-1, 0);
    }
    px.setPixelColor(i, c);
    px.show();
    delay(REFRESH_DELAY);
  }

  px.clear();
  px.show();
  delay(900);
  //TODO actual logic
}
