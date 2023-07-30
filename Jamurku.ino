// Import Library yang digunakan
#include <ESP8266WiFi.h>
#include <FirebaseArduino.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// Konfigurasi WiFi dan Firebase
#define WIFI_SSID "ardiles"
#define WIFI_PASSWORD "manusia27"
#define FIREBASE_HOST "jamurku-26856-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "81BSKLN9T7FY6p57V4MCrdZT5Jy7ZB6HkeRC113P"

// Konfigurasi Pin
#define DHTPIN 2
#define Relay1 14
#define Relay2 12
#define Relay3 13

// Inisialisasi sensor DHT-22
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Konfigurasi LCD dan NTP Client
LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variabel untuk mengatur delay pengiriman data suhu dan kelembaban
unsigned long lastDataUpdate = 0;
const unsigned long dataInterval = 60000; // Delay 60 detik untuk pengiriman data suhu dan kelembaban

// Variabel untuk mengatur delay pengendalian relay
unsigned long lastRelayUpdate = 0;
const unsigned long relayInterval = 1000; // Delay 1 detik untuk pengendalian relay

// Variabel untuk menyimpan nilai relay sebelumnya
int lastKp = 0;
int lastLa = 0;
int lastPo = 0;

// Variabel untuk mengontrol apakah data relay berubah dalam satu periode
bool relayChanged = false;

void setup() {
  Serial.begin(9600);

  // Menghubungkan ke WiFi.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Menghubungkan");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("Terhubung ke WiFi. Alamat IP: ");
  Serial.println(WiFi.localIP());

  // Menginisialisasi sensor DHT
  dht.begin();

  // Menginisialisasi Firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  // Menginisialisasi LCD
  lcd.init();
  lcd.backlight();

  // Menginisialisasi pin relay
  pinMode(Relay1, OUTPUT);
  pinMode(Relay2, OUTPUT);
  pinMode(Relay3, OUTPUT);
  digitalWrite(Relay1, HIGH);
  digitalWrite(Relay2, HIGH);
  digitalWrite(Relay3, HIGH);

  // Menginisialisasi NTP Client
  timeClient.begin();
  timeClient.setTimeOffset(28800); // GMT +8

  // Mengirim data ke Firebase untuk pertama kalinya
  sendDataToFirebase();
  lastDataUpdate = millis();
}

void loop() {
  // Membaca nilai sensor
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // Memeriksa apakah data sensor valid
  if (isnan(h) || isnan(t)) {
    Serial.println(F("Gagal membaca sensor DHT!"));
    ESP.reset();
  }

  // Memeriksa koneksi Firebase
  if (Firebase.failed()) {
    Serial.print("Terjadi Kesalahan: ");
    Serial.println(Firebase.error());
    ESP.reset();
  }

  // Mengendalikan relay berdasarkan nilai dari Firebase
  if (millis() - lastRelayUpdate >= relayInterval) {
    int kp = Firebase.getInt("Kontrol/kipas");
    int la = Firebase.getInt("Kontrol/lampu");
    int po = Firebase.getInt("Kontrol/pompa");
    int ot = Firebase.getInt("Kontrol/otomatis");

  if (ot == 1) {
    // Kondisi untuk kontrol otomatis
    if (t > 29 && h < 70) {
        // Kondisi 1
        digitalWrite(Relay1, LOW); // Kipas menyala
        digitalWrite(Relay2, HIGH); // Lampu mati
        digitalWrite(Relay3, LOW); // Pompa menyala
        Firebase.setInt("Kontrol/kipas", 1);
        Firebase.setInt("Kontrol/lampu", 0);
        Firebase.setInt("Kontrol/pompa", 1);
    } else if ((t < 26 && h > 90) || (t < 26 && h >= 70 && h <= 90) || (h > 90 && t >= 26 && t <= 29)) {
        // Kondisi 2
        digitalWrite(Relay1, HIGH); // Kipas mati
        digitalWrite(Relay2, LOW); // Lampu menyala
        digitalWrite(Relay3, HIGH); // Pompa mati
        Firebase.setInt("Kontrol/kipas", 0);
        Firebase.setInt("Kontrol/lampu", 1);
        Firebase.setInt("Kontrol/pompa", 0);
    } else if (t > 29 && h >= 70 && h <= 90) {
        // Kondisi 3
        digitalWrite(Relay1, LOW); // Kipas menyala
        digitalWrite(Relay2, HIGH); // Lampu mati
        digitalWrite(Relay3, HIGH); // Pompa mati
        Firebase.setInt("Kontrol/kipas", 1);
        Firebase.setInt("Kontrol/lampu", 0);
        Firebase.setInt("Kontrol/pompa", 0);
    } else if (t >= 26 && t <= 29 && h < 70) {
        // Kondisi 4
        digitalWrite(Relay1, HIGH); // Kipas mati
        digitalWrite(Relay2, HIGH); // Lampu mati
        digitalWrite(Relay3, LOW); // Pompa menyala
        Firebase.setInt("Kontrol/kipas", 0);
        Firebase.setInt("Kontrol/lampu", 0);
        Firebase.setInt("Kontrol/pompa", 1);
    } else {
        // Kondisi 5
        digitalWrite(Relay1, HIGH); // Kipas mati
        digitalWrite(Relay2, HIGH); // Lampu mati
        digitalWrite(Relay3, HIGH); // Pompa mati
        Firebase.setInt("Kontrol/kipas", 0);
        Firebase.setInt("Kontrol/lampu", 0);
        Firebase.setInt("Kontrol/pompa", 0);
    }
  } else {
      // Mode manual, relay dikendalikan dari Firebase
      digitalWrite(Relay1, kp == 1 ? LOW : HIGH);
      digitalWrite(Relay2, la == 1 ? LOW : HIGH);
      digitalWrite(Relay3, po == 1 ? LOW : HIGH);
  }


    // Mengecek perubahan nilai relay
    checkRelayChange(kp, la, po);
    lastRelayUpdate = millis();
  }

  // Mengirim data suhu dan kelembaban ke Firebase setiap 60 detik
  if (millis() - lastDataUpdate >= dataInterval) {
    sendDataToFirebase();
    lastDataUpdate = millis();
  }

  delay(100); // Delay 100ms untuk mengurangi beban pemrosesan
}

void checkRelayChange(int kp, int la, int po) {
  if (kp != lastKp || la != lastLa || po != lastPo) {
    relayChanged = true; // Set relayChanged menjadi true jika ada perubahan nilai relay
    lastKp = kp;
    lastLa = la;
    lastPo = po;
  }
}

void sendDataToFirebase() {
  // Mendapatkan waktu saat ini dari server NTP
  timeClient.update();
  String formattedTime = timeClient.getFormattedTime();

  // Membaca nilai sensor
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // Mengirim data realtime ke Firebase
  Firebase.setFloat("Data_Realtime/suhu", t);
  Firebase.setFloat("Data_Realtime/kelembaban", h);

  // Mengirim data kumulatif ke Firebase
  Firebase.setFloat("Data_Log/" + formattedTime + "/suhu", t);
  Firebase.setFloat("Data_Log/" + formattedTime + "/kelembaban", h);
  Firebase.setInt("Data_Log/" + formattedTime + "/kipas", lastKp);
  Firebase.setInt("Data_Log/" + formattedTime + "/lampu", lastLa);
  Firebase.setInt("Data_Log/" + formattedTime + "/pompa", lastPo);

  // Menampilkan data ke Serial Monitor
  Serial.print(F("Suhu: "));
  Serial.print(t);
  Serial.print(F("°C Kelembaban: "));
  Serial.print(h);
  Serial.println(F("%"));

  // Menampilkan data ke LCD
  lcd.setCursor(0, 0);
  lcd.print("Suhu: ");
  lcd.print(t);

  lcd.setCursor(0, 1);
  lcd.print("Kelembban: ");
  lcd.print(h);
}