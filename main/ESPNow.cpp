#include "ESPNow.h"

#include <WiFi.h>
#include <esp_now.h>
#include <math.h>

#include "sensors.h"
#include "gps.h"

/* =========================================================
   CONFIG
   ========================================================= */

#define RAKSHAK_WATCH_ID 1

#define ESP_NOW_TX_INTERVAL 1000

/* =========================================================
   PACKET
   EXACTLY MATCHES XIAO
   ========================================================= */

typedef struct __attribute__((packed))
{
    uint8_t watch_id;

    float heading;

    float gx;
    float gy;
    float gz;

    float heartRate;

    uint8_t health;

    uint8_t gpsFix;
    uint8_t sats;

    float hdop;

    float lat;
    float lon;

} Packet;

/* =========================================================
   BROADCAST ADDRESS
   ========================================================= */

static uint8_t broadcastAddress[] =
{
    0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff
};

/* =========================================================
   STATE
   ========================================================= */

static volatile bool newData = false;

static Packet rxPacket;
static volatile int rxRSSI = 0;

static uint32_t lastTX = 0;

static bool espNowReady = false;

/* =========================================================
   RSSI → DISTANCE
   EXACT XIAO FORMULA
   ========================================================= */

static float rssi_to_distance(int rssi)
{
    const float TxPower = -40.0f;
    const float N = 2.5f;

    return pow(
        10.0f,
        (TxPower - rssi) / (10.0f * N)
    );
}

/* =========================================================
   HEALTH PARSER
   SAME VALUES USED BY XIAO
   ========================================================= */

static uint8_t parseHealth(String s)
{
    s.trim();

    if(s == "HEALTHY")
        return 0;

    if(s == "INJURED")
        return 1;

    if(s == "DEAD")
        return 2;

    return 0;
}

/* =========================================================
   ESP-NOW RECEIVE CALLBACK
   ========================================================= */

static void onReceive(
    const esp_now_recv_info_t *info,
    const uint8_t *data,
    int len
)
{
    if(info == nullptr)
        return;

    if(data == nullptr)
        return;

    if(len != sizeof(Packet))
        return;

    memcpy(
        &rxPacket,
        data,
        sizeof(Packet)
    );

    if(info->rx_ctrl != nullptr)
    {
        rxRSSI = info->rx_ctrl->rssi;
    }

    newData = true;
}

/* =========================================================
   INITIALIZE ESP-NOW
   ========================================================= */

void ESPNow_Init()
{
    Serial.println();
    Serial.println("================================");
    Serial.println("ESP-NOW INIT");
    Serial.println("================================");

    /*
     * IMPORTANT:
     *
     * Wi-Fi is already connected by WiFi_Init().
     *
     * DO NOT call WiFi.setChannel().
     *
     * The Waveshare must remain on the channel
     * used by the existing camera Wi-Fi.
     */

    Serial.print("[ESP-NOW] WiFi channel: ");
    Serial.println(WiFi.channel());

    if(esp_now_init() != ESP_OK)
    {
        Serial.println("[ESP-NOW] INIT FAILED");
        return;
    }

    esp_now_register_recv_cb(onReceive);

    esp_now_peer_info_t peer = {};

    memcpy(
        peer.peer_addr,
        broadcastAddress,
        6
    );

    /*
     * 0 = current Wi-Fi channel
     */

    peer.channel = 0;
    peer.encrypt = false;

    esp_err_t result =
        esp_now_add_peer(&peer);

    if(result != ESP_OK &&
       result != ESP_ERR_ESPNOW_EXIST)
    {
        Serial.print(
            "[ESP-NOW] PEER FAILED: "
        );

        Serial.println(
            (int)result
        );

        return;
    }

    espNowReady = true;

    Serial.println("[ESP-NOW] READY");
    Serial.println("[ESP-NOW] Broadcast peer ready");
}


/* =========================================================
   STOP ESP-NOW
   ========================================================= */

void ESPNow_Stop()
{
    if(!espNowReady)
        return;

    Serial.println();
    Serial.println("================================");
    Serial.println("ESP-NOW STOP");
    Serial.println("================================");

    esp_now_unregister_recv_cb();

    esp_err_t result = esp_now_deinit();

    if(result == ESP_OK)
    {
        Serial.println("[ESP-NOW] STOPPED");
    }
    else
    {
        Serial.print("[ESP-NOW] STOP FAILED: ");
        Serial.println((int)result);
    }

    espNowReady = false;
    newData = false;
}


/* =========================================================
   SEND WATCH 1 TELEMETRY
   ========================================================= */

static void sendTelemetry()
{
    Packet pkt = {};

    pkt.watch_id = RAKSHAK_WATCH_ID;

    /*
     * Heading
     */

    pkt.heading =
        Sensors_GetYaw();

    /*
     * Gyroscope
     */

    float gx;
    float gy;
    float gz;

    Sensors_GetGyro(
        gx,
        gy,
        gz
    );

    pkt.gx = gx;
    pkt.gy = gy;
    pkt.gz = gz;

    /*
     * Heart rate
     */

    pkt.heartRate =
        Sensors_GetHeartRate();

    /*
     * Health
     *
     * The existing Watch function is converted
     * into the exact numeric representation
     * expected by the XIAO.
     */

    String healthState =
        Sensors_GetHealthState();

    pkt.health =
        parseHealth(healthState);

    /*
     * GPS
     */

    pkt.gpsFix =
        GPS_Fix();

    pkt.sats =
        GPS_Sats();

    pkt.hdop =
        GPS_HDOP();

    pkt.lat =
        GPS_Lat();

    pkt.lon =
        GPS_Lon();

    /*
     * Broadcast
     */

    esp_err_t result =
        esp_now_send(
            broadcastAddress,
            (uint8_t *)&pkt,
            sizeof(pkt)
        );

    if(result != ESP_OK)
    {
        Serial.print(
            "[ESP-NOW TX] FAILED: "
        );

        Serial.println(
            (int)result
        );

        return;
    }

    /*
     * Debug once per transmission.
     */

    Serial.println();
    Serial.println(
        "====== ESP-NOW TX ======"
    );

    Serial.print("Watch: ");
    Serial.println(pkt.watch_id);

    Serial.print("Heading: ");
    Serial.println(pkt.heading);

    Serial.print("HR: ");
    Serial.println(pkt.heartRate);

    Serial.print("Health: ");
    Serial.println(pkt.health);

    Serial.print("GPS Fix: ");
    Serial.println(pkt.gpsFix);

    Serial.print("Sats: ");
    Serial.println(pkt.sats);

    Serial.print("HDOP: ");
    Serial.println(pkt.hdop);

    Serial.print("LAT: ");
    Serial.println(
        pkt.lat,
        6
    );

    Serial.print("LON: ");
    Serial.println(
        pkt.lon,
        6
    );

    Serial.println(
        "========================"
    );
}

/* =========================================================
   UPDATE
   ========================================================= */

void ESPNow_Update()
{
    if(!espNowReady)
        return;

    /*
     * -----------------------------------------------------
     * TX
     * -----------------------------------------------------
     */

    if(
        millis() - lastTX >=
        ESP_NOW_TX_INTERVAL
    )
    {
        lastTX = millis();

        sendTelemetry();
    }

    /*
     * -----------------------------------------------------
     * RX
     * -----------------------------------------------------
     */

    if(!newData)
        return;

    /*
     * Consume received packet.
     */

    newData = false;

    float distance =
        rssi_to_distance(
            rxRSSI
        );

    Serial.println();
    Serial.println(
        "------ ESP-NOW RX ------"
    );

    Serial.print("Watch: ");
    Serial.println(
        rxPacket.watch_id
    );

    Serial.print("RSSI: ");
    Serial.println(
        rxRSSI
    );

    Serial.print("Distance: ");
    Serial.println(
        distance
    );

    Serial.print("Heading: ");
    Serial.println(
        rxPacket.heading
    );

    Serial.println(
        "------------------------"
    );
}

/* =========================================================
   GETTERS
   ========================================================= */

bool ESPNow_HasRadarData()
{
    return newData;
}

uint8_t ESPNow_GetWatchID()
{
    return rxPacket.watch_id;
}

float ESPNow_GetHeading()
{
    return rxPacket.heading;
}

float ESPNow_GetDistance()
{
    return rssi_to_distance(
        rxRSSI
    );
}

int ESPNow_GetRSSI()
{
    return rxRSSI;
}