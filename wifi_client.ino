#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

#define ssid "home"
#define pass "Forest2020"

// Set your Static IP address
IPAddress local_IP(192, 168, 0, 188);
// Set your Gateway IP address
IPAddress gateway(192, 168, 0, 1);

IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8); // optional
IPAddress secondaryDNS(8, 8, 4, 4); // optional

ESP8266WebServer server(80);

#define STATUS_PIN 2

#define LS_ON "on"
#define LS_OFF "off"
struct LED_STATUS {
  uint8_t pin;
  String status;
  String name;
};

// name of the server. You reach it using http://webserver
#define HOSTNAME "arduino"

// TRACE output simplified, can be deactivated here
#define TRACE(...) Serial.printf(__VA_ARGS__)
#define TRACELN(...) Serial.println(__VA_ARGS__)

LED_STATUS led_list[] = {
  {D7,LS_OFF,"D7"},
  {D6,LS_OFF,"D6"},
  {D2,LS_OFF,"D2"},
  {D3,LS_OFF,"D3"},
};

#define MaxConnectionAttempts 20
int connectionAttempts = 0;

int ledcount() {
  return sizeof(led_list) / sizeof(LED_STATUS);
}

String htmlDocument = R"rawliteral(<!DOCTYPE html>
<html>
    <head>
        <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
        <title>LED Control</title>
        <style>
        html{ 
            font-family: Helvetica; 
            display: inline-block; 
            margin: 0px auto; 
            text-align: center;
        }
        body{
            margin-top: 50px;
        } 
        h1 {
            color: #444444;
            margin: 50px auto 30px;
        } 
        h3 {
            color: #444444;
            margin-bottom: 50px;
        }
        .button {
            display: block;
            width: 80px;
            background-color: #1abc9c;
            border: none;
            color: white;
            padding: 13px 30px;
            text-decoration: none;
            font-size: 25px;
            margin: 0px auto 35px;
            cursor: pointer;
            border-radius: 4px;
        }
        .button-on {
            background-color: #1abc9c;
        }
        .button-on:active {
            background-color: #16a085;
        }
        .button-off {
            background-color: #34495e;
        }
        .button-off:active {
            background-color: #2c3e50;
        }
        .status {
            font-size: 14px;
            color: #888;
            margin-bottom: 10px;
        }
        ul,li {
            list-style-type: none;
        }
        </style>
    </head>
    <body>
        <h1>ESP8266 Web Server (updated)</h1>
        <h3>Using WiFi client Mode</h3>

        <div id="leds"></div>

        <script>
        function SwitchLed(name) {
            var xmlhr = new XMLHttpRequest();
            xmlhr.open( "POST", `http://${window.location.host}/status`, false);

            console.log(`switch id=${name}`)
            let item = document.getElementById(name);
            
            console.log(`item text=${item.textContent}`)
            if(item.classList.contains('button-on')) {
                xmlhr.send( `{"leds":[{"name":"${name}","status":"off"}]}` );
            }else{
                xmlhr.send( `{"leds":[{"name":"${name}","status":"on"}]}` );
            }
            GetStatus();
        }

        function GetStatus() {
            let xmlhr = new XMLHttpRequest();
            xmlhr.open( "GET", `http://${window.location.host}/status`, false);
            xmlhr.send( null );
            let leds = JSON.parse(xmlhr.responseText);

            let list = document.createElement('ul');
            leds.forEach(element => {
                let item = document.createElement('li');
                item.innerHTML = `<div class="status">${element.name} Status: ${element.status}</div>
                                    <div id="${element.name}" class="button button-${element.status}" onclick="SwitchLed('${element.name}')">Switch</div>`;
                list.appendChild(item);
            });
            document.getElementById('leds').replaceChildren(list);
            console.log(list.outerHTML);
        }
        document.addEventListener("DOMContentLoaded", ()=>{
            GetStatus();
        })
        </script>
    </body>
</html>
  )rawliteral";

void blink(){
  digitalWrite(STATUS_PIN, LOW);
  delay(50);
  digitalWrite(STATUS_PIN, HIGH);
  delay(50);
}

bool SetLedByName(String name, String status) {
  for(int i = 0; i < ledcount(); i++) {
    if (led_list[i].name == name) {
      if(status == LS_ON) {
        led_list[i].status = LS_ON;
        digitalWrite(led_list[i].pin, HIGH);
      }else{
        led_list[i].status = LS_OFF;
        digitalWrite(led_list[i].pin, LOW);
      }
      return true;
    }
  }
  return false;
}

void handle_OnConnect() 
{ 
  server.send(200, "text/html", htmlDocument); 
  blink();
  blink();
  blink();
}

void handle_getstatus() 
{
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();
  for(int i = 0; i < ledcount(); i++) {
    JsonObject nested = array.createNestedObject();
    nested["name"] = led_list[i].name;
    nested["status"] = led_list[i].status;
  }
  String buf;
  serializeJson(array, buf);
  server.send(200, F("application/json"), buf);
  blink(); 
}

void handle_setstatus() 
{
  if (!server.hasArg("plain")) {
    TRACELN("setstatus: Body not received");
    return;
  }
  
  StaticJsonDocument<1024> parsed;
  DeserializationError error = deserializeJson(parsed, server.arg("plain"));
  if (error) {
    TRACE("deserializeJson() failed: ");
    TRACELN(error.c_str());
    return;
  }

  String name, status;
  
  for(int i = 0; i < parsed["leds"].size(); i++) {
    name = parsed["leds"][i]["name"].as<String>();
    status = parsed["leds"][i]["status"].as<String>();
    if(!SetLedByName(name, status)){
      TRACELN("Set pin failed");
      server.send(500, "text/plain", String("Set pin failed: ") + name);
      blink(); 
      blink(); 
      return;
    }
  }

  server.send(200);
  blink(); 
}

void handle_NotFound()
{
  server.send(404, "text/plain", "Not found");
  blink();
  blink();
}

void setup() 
{
  Serial.begin(115200);
  delay(500);
  TRACELN();
  TRACELN("Statrt");

  pinMode(STATUS_PIN, OUTPUT);
  delay(1);
  digitalWrite(STATUS_PIN, HIGH);
  
  for (int i = 0; i < ledcount(); i++)  {
    TRACE("Set pin %i (%s) mode OUTPUT\n", led_list[i].pin, led_list[i].name);
    pinMode(led_list[i].pin, OUTPUT);
    delay(1);
    digitalWrite(led_list[i].pin, LOW);
  }

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
    return;
  }
  
  WiFi.begin(ssid, pass);

  TRACE("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    TRACE(".");
    if(++connectionAttempts >= 20) {
      TRACE("Couldn't connect to wifi");
      return;
    }
    blink();
    delay(500);
  }
  TRACELN();

  TRACE("Connected, IP address: ");
  TRACELN(WiFi.localIP());

  WiFi.setHostname(HOSTNAME);
  
  server.on("/", handle_OnConnect);
  server.on("/status", HTTP_GET, handle_getstatus);
  server.on("/status", HTTP_POST, handle_setstatus);
  server.onNotFound(handle_NotFound);
  
  server.begin();
  Serial.println("HTTP server started");
}

void loop() 
{  
  server.handleClient();
}