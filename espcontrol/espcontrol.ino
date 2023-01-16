#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include <mutex>

#include "private.h"
#include "dbg.h"

#include "colorcontrol.h"

#define NUM_PX 109
#define PX_PIN 23   // GPIO23

#define REFRESH_DELAY     18   // in ms
#define LONG_DELAY        2700
#define LONG_DELAY_FRAMES (LONG_DELAY / REFRESH_DELAY)

Adafruit_NeoPixel px(NUM_PX, PX_PIN, NEO_GRB + NEO_KHZ800);

AsyncUDP udp;

std::mutex ctxmux;
volatile color_context newctx;
volatile bool freshctx = false;

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

    if (ctxmux.try_lock()) {
      freshctx = parse_packet(packet.data(), len, const_cast<color_context*>(&newctx));
      ctxmux.unlock();
    }
    else {
      // Could happen if ISR hits during check
      dbgl("Refusing to parse packet, because lock is taken");
      // for now just wait for resend I guess
    }
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
  delay(LONG_DELAY);

  return;

  //TODO actual logic ---- 
  static color_context ctx = {};
  static uint16_t check_counter = LONG_DELAY_FRAMES;

  // check for update to context from a parsed packet
  if (check_counter <= 0) {
    check_counter = LONG_DELAY_FRAMES;
    if (freshctx && ctxmux.try_lock()) {
      // should probably disable interrupts during this?
      if (freshctx) {
        ctx = *const_cast<color_context*>(&newctx); // copy that over
        freshctx = false;
      }
      ctxmux.unlock();
    }
  }

  // render a frame from the context
  uint16_t frame_sleep = get_frame(&px, &ctx);
  if (frame_sleep == 0 || frame_sleep > LONG_DELAY_FRAMES) {
    // 0 means no planned update, so just loop for a while, so we can come back and check for an update
    delay(LONG_DELAY);
    check_counter = 0;
    return;
  }

  delay(REFRESH_DELAY * frame_sleep);
  check_counter -= frame_sleep;
}
