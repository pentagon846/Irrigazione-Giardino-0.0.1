#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// WiFi credentials
const char* ssid = "name you WIFI";
const char* password = "password of you WIFI";

// Define OLED display parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000);

// Define pin connections
#define RELAY_PIN 14  // D5 corresponds to GPIO14
#define BUTTON_PIN 12 // D6 corresponds to GPIO12

// Define watering duration (in minutes)
int manualWateringDuration = 10;  // Default duration for manual mode
int morningWateringDuration = 10;  // Default duration for morning
int eveningWateringDuration = 10;  // Default duration for evening
unsigned long relayOnTime = 0;
bool relayState = false;

// Variables for scheduled watering
int startHour1 = -1;
int startMinute1 = -1;
bool scheduledWatering1 = false;

int startHour2 = -1;
int startMinute2 = -1;
bool scheduledWatering2 = false;

// Create web server on port 80
ESP8266WebServer server(80);

void handleRoot() {
  String html = "<html>\
                  <head>\
                    <title>Watering System</title>\
                  </head>\
                  <body>\
                    <h1>Watering System</h1>\
                    <form action='/setManual' method='GET'>\
                      <label for='duration'>Manual Watering Duration (minutes):</label>\
                      <input type='number' id='manualDuration' name='manualDuration' value='" + String(manualWateringDuration) + "'>\
                      <input type='submit' value='Set Manual'>\
                    </form>\
                    <form action='/setMorning' method='GET'>\
                      <label for='hour1'>Morning Start Hour:</label>\
                      <input type='number' id='hour1' name='hour1' min='0' max='23' value='" + (startHour1 != -1 ? String(startHour1) : "") + "'>\
                      <label for='minute1'>Start Minute:</label>\
                      <input type='number' id='minute1' name='minute1' min='0' max='59' value='" + (startMinute1 != -1 ? String(startMinute1) : "") + "'>\
                      <label for='morningDuration'>Morning Watering Duration (minutes):</label>\
                      <input type='number' id='morningDuration' name='morningDuration' value='" + String(morningWateringDuration) + "'>\
                      <input type='submit' value='Set Morning'>\
                    </form>\
                    <form action='/setEvening' method='GET'>\
                      <label for='hour2'>Evening Start Hour:</label>\
                      <input type='number' id='hour2' name='hour2' min='0' max='23' value='" + (startHour2 != -1 ? String(startHour2) : "") + "'>\
                      <label for='minute2'>Start Minute:</label>\
                      <input type='number' id='minute2' name='minute2' min='0' max='59' value='" + (startMinute2 != -1 ? String(startMinute2) : "") + "'>\
                      <label for='eveningDuration'>Evening Watering Duration (minutes):</label>\
                      <input type='number' id='eveningDuration' name='eveningDuration' value='" + String(eveningWateringDuration) + "'>\
                      <input type='submit' value='Set Evening'>\
                    </form>\
                    <form action='/manual' method='GET'>\
                      <input type='submit' name='state' value='" + String(relayState ? "Turn Off" : "Turn On") + "'>\
                    </form>\
                  </body>\
                </html>";
  server.send(200, "text/html", html);
}

void handleSetManual() {
  if (server.hasArg("manualDuration")) {
    manualWateringDuration = server.arg("manualDuration").toInt();
    server.send(200, "text/html", "<html><body><h1>Manual Watering Duration Set to " + String(manualWateringDuration) + " minutes</h1><a href='/'>Go Back</a></body></html>");
  } else {
    server.send(400, "text/html", "<html><body><h1>Invalid Request</h1><a href='/'>Go Back</a></body></html>");
  }
}

void handleSetMorning() {
  if (server.hasArg("hour1") && server.hasArg("minute1") && server.hasArg("morningDuration")) {
    startHour1 = server.arg("hour1").toInt();
    startMinute1 = server.arg("minute1").toInt();
    morningWateringDuration = server.arg("morningDuration").toInt();
    scheduledWatering1 = true;
    server.send(200, "text/html", "<html><body><h1>Morning Watering Set to " + String(startHour1) + ":" + String(startMinute1) + " for " + String(morningWateringDuration) + " minutes</h1><a href='/'>Go Back</a></body></html>");
  } else {
    server.send(400, "text/html", "<html><body><h1>Invalid Request</h1><a href='/'>Go Back</a></body></html>");
  }
}

void handleSetEvening() {
  if (server.hasArg("hour2") && server.hasArg("minute2") && server.hasArg("eveningDuration")) {
    startHour2 = server.arg("hour2").toInt();
    startMinute2 = server.arg("minute2").toInt();
    eveningWateringDuration = server.arg("eveningDuration").toInt();
    scheduledWatering2 = true;
    server.send(200, "text/html", "<html><body><h1>Evening Watering Set to " + String(startHour2) + ":" + String(startMinute2) + " for " + String(eveningWateringDuration) + " minutes</h1><a href='/'>Go Back</a></body></html>");
  } else {
    server.send(400, "text/html", "<html><body><h1>Invalid Request</h1><a href='/'>Go Back</a></body></html>");
  }
}

void handleManual() {
  if (server.hasArg("state")) {
    relayState = !relayState;
    digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
    relayOnTime = timeClient.getEpochTime();  // Update the time when relay was turned on/off
    server.send(200, "text/html", "<html><body><h1>Relay " + String(relayState ? "Turned On" : "Turned Off") + "</h1><a href='/'>Go Back</a></body></html>");
  } else {
    server.send(400, "text/html", "<html><body><h1>Invalid Request</h1><a href='/'>Go Back</a></body></html>");
  }
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Address 0x3C for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.display();
  delay(2000);
  display.clearDisplay();

  // Initialize relay pin
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Initialize button pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Start the web server
  server.on("/", handleRoot);
  server.on("/setManual", handleSetManual);
  server.on("/setMorning", handleSetMorning);
  server.on("/setEvening", handleSetEvening);
  server.on("/manual", handleManual);
  server.begin();
  Serial.println("HTTP server started");

  // Initialize NTPClient
  timeClient.begin();

  // Display initial message
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("WiFi Connected");
  display.display();
  delay(2000);
}

void loop() {
  // Handle client requests
  server.handleClient();

  // Update NTP client
  timeClient.update();

  // Get the current time
  unsigned long currentTime = timeClient.getEpochTime();
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  
  // Display current time and watering duration
  displayTimeAndWateringDuration(currentTime);

  // Check if button is pressed
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50);  // Debounce delay
    if (digitalRead(BUTTON_PIN) == LOW) {
      relayState = !relayState;
      digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
      relayOnTime = currentTime;
    }
  }

  // Check if relay should be turned off
  if (relayState && (currentTime - relayOnTime >= manualWateringDuration * 60)) {
    relayState = false;
    digitalWrite(RELAY_PIN, LOW);
  }

  // Check if it's time to start scheduled watering
  if (scheduledWatering1 && currentHour == startHour1 && currentMinute == startMinute1 && !relayState) {
    relayState = true;
    digitalWrite(RELAY_PIN, HIGH);
    relayOnTime = currentTime;
  }

  if (relayState && scheduledWatering1 && (currentTime - relayOnTime >= morningWateringDuration * 60)) {
    relayState = false;
    digitalWrite(RELAY_PIN, LOW);
  }

  if (scheduledWatering2 && currentHour == startHour2 && currentMinute == startMinute2 && !relayState) {
    relayState = true;
    digitalWrite(RELAY_PIN, HIGH);
    relayOnTime = currentTime;
  }

  if (relayState && scheduledWatering2 && (currentTime - relayOnTime >= eveningWateringDuration * 60)) {
    relayState = false;
    digitalWrite(RELAY_PIN, LOW);
  }

  delay(1000);
}

void displayTimeAndWateringDuration(unsigned long currentTime) {
  // Clear display buffer
  display.clearDisplay();

  // Display current date and time
  display.setCursor(0, 0);
  display.print("Time: ");
  display.print(timeClient.getFormattedTime());

  // Display manual watering duration
  display.setCursor(0, 10);
  display.print("Manual: ");
  display.print(manualWateringDuration);
  display.print(" min");

  // Display scheduled times and durations
  if (scheduledWatering1) {
    display.setCursor(0, 20);
    display.print("Morning: ");
    display.print(startHour1);
    display.print(":");
    if (startMinute1 < 10) display.print("0");
    display.print(startMinute1);
    display.print(" (");
    display.print(morningWateringDuration);
    display.print(" min)");
  }

  if (scheduledWatering2) {
    display.setCursor(0, 30);
    display.print("Evening: ");
    display.print(startHour2);
    display.print(":");
    if (startMinute2 < 10) display.print("0");
    display.print(startMinute2);
    display.print(" (");
    display.print(eveningWateringDuration);
    display.print(" min)");
  }

  // Update the display
  display.display();
}
