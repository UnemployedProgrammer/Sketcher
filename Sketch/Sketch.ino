#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

const char* ssid = "Set up your Sketcher"; // Setup wifi ssid (name)
const char* password = "abyz1290"; // Setup wifi password

//Onboarding Page
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Sketcher Setup</title>
    <style>
        :root { --bg: #f0f2f5; --card: #ffffff; --primary: #007aff; --text: #1d1d1f; }
        body { font-family: -apple-system, sans-serif; background: var(--bg); color: var(--text); display: flex; align-items: center; justify-content: center; height: 100vh; margin: 0; }
        .card { background: var(--card); padding: 2rem; border-radius: 20px; box-shadow: 0 10px 25px rgba(0,0,0,0.05); width: 90%; max-width: 320px; text-align: center; }
        svg { width: 50px; height: 50px; margin-bottom: 1rem; fill: var(--primary); }
        h1 { font-size: 1.5rem; margin: 0 0 0.5rem; }
        p { font-size: 0.9rem; color: #666; line-height: 1.4; margin-bottom: 1.5rem; }
        input { width: 100%; padding: 12px; margin: 8px 0; border: 1px solid #ddd; border-radius: 10px; box-sizing: border-box; font-size: 1rem; outline: none; transition: border 0.2s; }
        input:focus { border-color: var(--primary); }
        button { width: 100%; background: var(--primary); color: white; border: none; padding: 12px; border-radius: 10px; font-weight: bold; font-size: 1rem; cursor: pointer; margin-top: 10px; }
        button:active { transform: scale(0.98); }
    </style>
</head>
<body>
    <div class="card">
       <svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#1f1f1f"><path d="M480-120q-42 0-71-29t-29-71q0-42 29-71t71-29q42 0 71 29t29 71q0 42-29 71t-71 29ZM254-346l-84-86q59-59 138.5-93.5T480-560q92 0 171.5 35T790-430l-84 84q-44-44-102-69t-124-25q-66 0-124 25t-102 69ZM84-516 0-600q92-94 215-147t265-53q142 0 265 53t215 147l-84 84q-77-77-178.5-120.5T480-680q-116 0-217.5 43.5T84-516Z"/></svg>
        <h1>Connect to WiFi</h1>
        <p>Connect your Sketcher to the WiFi to send drawings. For help, check your manual.</p>
        <input type="text" name="ssid" placeholder="WiFi Name (SSID)" required>
        <input type="password" name="password" placeholder="Password" required>
        <button onclick="send()">Connect</button>
    </div>
    <script>
        function send() {
            const ssid = document.querySelector('input[name="ssid"]').value;
            const password = document.querySelector('input[name="password"]').value;
            if (!ssid || !password) {
                alert('Please fill in both fields.');
                return;
            }
            fetch('/finish'+ `?ssid=${encodeURIComponent(ssid)}&password=${encodeURIComponent(password)}`, {
                method: 'GET'
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    alert('Submitted. Check your manaual for further instructions.');
                    window.close()
                } else {
                    alert('Connection failed!');
                    console.error('Error:', data);
                }
            })
            .catch(error => {
                alert('Error: ' + error.message);
            });
        }
    </script>
</body>
</html>
)rawliteral";

//Code

WebServer server(80); // Webserver for communicating and setup
Preferences preferences; //Save preferences
static String wifi_ssid;
static String wifi_pass;

void blinkLedBlocking(int pin, int onDuration) {
  digitalWrite(pin, HIGH);
  delay(onDuration);
  digitalWrite(pin, LOW);
}

void restart() {
  Serial.println("Restarting...");
  ESP.restart();
}

void setupAPAndWebserver() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  server.on("/", [](){
    server.send(200, "text/html", HTML_PAGE);
  });

  server.on("/finish", [](){
    if (server.hasArg("ssid")) {
      String ssid = server.arg("ssid");
      Serial.print("Received SSID: ");
      Serial.println(ssid);
      preferences.putString("wifi_ssid", ssid);
    } else {
      Serial.println("No SSID provided");
    }

    if (server.hasArg("password")) {
      String pass = server.arg("password");
      Serial.print("Received PASSWORD: ");
      Serial.println(pass);
      preferences.putString("wifi_password", pass);
    }

    server.send(200, "text/plain", "{\"success\":true}");
    blinkLedBlocking(25, 1000);
    preferences.end();
    restart();
  });

  server.begin();
  Serial.println("Server is running...");
}

void setupWiFiConnection() {
  wifi_ssid = preferences.getString("wifi_ssid", "");
  wifi_pass = preferences.getString("wifi_password", "");

  if (wifi_ssid.isEmpty()) {
    Serial.println("No WiFi credentials stored");
    // restart(); // Ensure this is defined elsewhere
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname("Sketchingboard");
  delay(200);

  Serial.printf("Connecting to SSID: %s\n", wifi_ssid.c_str());

  // 2. Register events with non-capturing lambdas
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.print("Connected! IP address: ");
    Serial.println(IPAddress(info.got_ip.ip_info.ip.addr));
  }, ARDUINO_EVENT_WIFI_STA_GOT_IP);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.print("Disconnected. Reason: ");
    Serial.println(info.wifi_sta_disconnected.reason);
    
    // Use the global/static variables instead of trying to capture
    //WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str()); system does this for me
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
}

void setupNormalWebserver() {
  setupWiFiConnection();

  server.on("/", [](){
    server.send(200, "text/html", "Download SketchController and begin using this sketcher.");
  });

  server.begin();
}

void setup() {
  delay(2000); //Waiting for potential serial monitors
  pinMode(25, OUTPUT); // Pin als Ausgang
  Serial.begin(115200);
  Serial.println("Starting init...");
  preferences.begin("settings", false);

  if(!preferences.isKey("wifi_ssid") && preferences.getString("wifi_ssid").isEmpty()) {
    setupAPAndWebserver();
  } else {
    setupNormalWebserver();
    preferences.end();
  }

  blinkLedBlocking(25, 200);
  delay(200);
  blinkLedBlocking(25, 200);
}

void loop() {
  server.handleClient(); // handle server requests
}