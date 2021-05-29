#include <ArduinoJson.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_LIS2MDL.h>
#include <HttpClient.h>
#include <Adafruit_mfGFX.h>
#include "Arduino_ST7789.h"
#include <SPI.h>
#include <vector>


#define TFT_MOSI       13
#define TFT_MISO       A4
#define TFT_CLK        12
#define TFT_CS         A2
#define TFT_RST        A0                                            
#define TFT_DC         A1



Adafruit_LIS2MDL ped = Adafruit_LIS2MDL(12345);
Arduino_ST7789 tft = Arduino_ST7789(TFT_DC, TFT_RST, TFT_CS);

int HTTP_PORT = 3000;
String HTTP_METHOD = "POST";
char HOST_NAME[] = "192.168.0.116"; // REPLACE WITH YOUR PI IP

HttpClient http;
http_request_t request;
http_response_t response;

String device_id = System.deviceID();
float z = 0;
float prev_z;
int steps = 0;

// Cardiomon
int seed;
int level = 1;
int x_off;
uint8_t c1[3];
uint8_t c2[3];
uint8_t c3[3];
std::vector<int> cells;
int CELL_SIZE = 5;

// stepper
unsigned int nextTime = 0;    // Next time to contact the server

// Http headers
http_header_t headers[] = {
     { "Content-Type", "application/json" },
     { "Accept" , "application/json" },
    { NULL, NULL }
};

// http methods
void mon_request(String path) {
    DynamicJsonDocument MON(8192);
    request.hostname = HOST_NAME;
    request.port = HTTP_PORT;
    request.path = path;
    char bod[150];
    if (path == "/levelup") {
        String preJson = "{\"device_id\":\"%s\",\"seed\":\"%d\",\"level\":\"%d\"}";
        sprintf(bod, preJson, device_id.c_str(), seed, level);
        request.body = bod;
    } else {
        String preJson = "{\"device_id\":\"%s\"}";
        sprintf(bod, preJson, device_id.c_str());
        request.body = bod;
    }
    http.post(request, response, headers);
    int n = response.body.length();
    char char_array[n + 1];
    strcpy(char_array, response.body.c_str());

    deserializeJson(MON, char_array);
    seed = MON["seed"];
    level = MON["level"];
    x_off = MON["x_limit"];
    c1[0] = (int)MON["c1"][0]; c1[1] = (int)MON["c1"][1]; c1[2] = (int)MON["c1"][2];
    c2[0] = (int)MON["c2"][0]; c2[1] = (int)MON["c2"][1]; c2[2] = (int)MON["c2"][2];
    c3[0] = (int)MON["c3"][0]; c3[1] = (int)MON["c3"][1]; c3[2] = (int)MON["c3"][2];

    cells.clear();
    for (int i = 0; i < MON["cells"].size(); i++) {
        cells.push_back((int)MON["cells"][i]);
    }
}

// Pass 8-bit (each) R,G,B, get back 16-bit packed color
uint16_t get_color(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}


void draw_mon() {
    tft.fillRect(0, 0, 120, 120, BLACK);
    int y = 80;
    int x = 0;

    uint16_t c11 = get_color(c1[0], c1[1], c1[2]);
    uint16_t c22 = get_color(c2[0], c2[1], c2[2]);
    uint16_t c33 = get_color(c3[0], c3[1], c3[2]);
    int start_x = 67 - floor(x_off / 2) * CELL_SIZE;

    for(int i = 0; i < cells.size(); i++) {
        if ((i != 0) && (i % x_off == 0)) {
            y -= CELL_SIZE;
            x = 0;
        }
        int cell = cells[i];
        int dx = start_x + (x * CELL_SIZE);
        if (cell != 0) {
            uint16_t color = cell == 1 ? c11 : cell == 2 ? c33 : c22;
            tft.fillRect(dx, y, CELL_SIZE, CELL_SIZE, color);
        }
        x += 1;
    }
}

void draw_initial_text() {
    tft.setCursor(10, 160);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.setTextWrap(false);
    tft.print("Steps:");
    tft.setCursor(10, 190);
    tft.print("Level:");
}

void draw_stepcount() {
    tft.setCursor(100, 160);
    tft.fillRect(100, 160, 35, 14, BLACK);
    tft.print(steps);
}

void draw_level() {
    tft.setCursor(100, 190);
    tft.fillRect(100, 190, 14, 14, BLACK);
    tft.print(level);
}

void setup(void) {
  Serial.begin(9600);
  tft.init(135,240);
  tft.fillScreen(BLACK);
  if(!ped.begin()){
        Serial.println("Ooops, no LIS2MDL detected ... Check your wiring!");
    }

  mon_request("/reload");
  draw_mon();
  draw_initial_text();
  draw_level();
}

void loop() {
    if (nextTime > millis()) {
        return;
    }
    
    sensors_event_t event;
    ped.getEvent(&event);
    
    // get read from pedometer
    z = event.magnetic.z;
    
    if(abs(z - prev_z) > 2) {
        Serial.println(z);
        steps += 1;
        draw_stepcount();
        Serial.println(steps);
    }
    if (steps == 10) {
        steps = 0;
        draw_stepcount();
        if (level == 5) {
            mon_request("/newmon");
            draw_level();
        } else {
            mon_request("/levelup");
            draw_level();
        }
        draw_mon();
    }
    nextTime = millis() + 300;
    prev_z = z;
}
