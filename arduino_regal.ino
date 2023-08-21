#include "pitches.h" // buzzer
#include "LedControl.h" // LED 8x8
#include <SPI.h> // LAN
#include <Ethernet2.h> // LAN

// nastaveni PINu na desce
int btnConfirmPin = 2;
int btnCancelPin = 3;
int btnUpPin = 4;
int btnDownPin = 5;
int beeperPin = 1;

// globalni hodnoty
enum pickResult {prNone, prConfirm, prCancel}; // vysledek pickovani
int pickQuantity = 0;      // kolik se chce pickovat
int lastQuantity = 0;   // kolik je aktualne zobrazeno pro pickovani
pickResult lastPickup = prNone;

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // MAC adresa sitovky
IPAddress ip(10, 0, 1, 150); // IP adresa

EthernetServer server(80); // inicializace serveru na portu 80
LedControl lc=LedControl(8,6,7,1); // inicializace LED 8x8

// definice pro 8x8 LED displej
const byte numbers[][8] = {
  { 0b00000000,  0b00111100,  0b01100110,  0b01100110,  0b01100110,  0b01100110,  0b01100110,  0b00111100},
  { 0b00000000,  0b00011000,  0b00011000,  0b00111000,  0b00011000,  0b00011000,  0b00011000,  0b01111110},
  { 0b00000000,  0b00111100,  0b01100110,  0b00000110,  0b00001100,  0b00110000,  0b01100000,  0b01111110},
  { 0b00000000,  0b00111100,  0b01100110,  0b00000110,  0b00011100,  0b00000110,  0b01100110,  0b00111100},
  { 0b00000000,  0b00001100,  0b00011100,  0b00101100,  0b01001100,  0b01111110,  0b00001100,  0b00001100},
  { 0b00000000,  0b01111110,  0b01100000,  0b01111100,  0b00000110,  0b00000110,  0b01100110,  0b00111100},
  { 0b00000000,  0b00111100,  0b01100110,  0b01100000,  0b01111100,  0b01100110,  0b01100110,  0b00111100},
  { 0b00000000,  0b01111110,  0b01100110,  0b00001100,  0b00001100,  0b00011000,  0b00011000,  0b00011000},
  { 0b00000000,  0b00111100,  0b01100110,  0b01100110,  0b00111100,  0b01100110,  0b01100110,  0b00111100},
  { 0b00000000,  0b00111100,  0b01100110,  0b01100110,  0b00111110,  0b00000110,  0b01100110,  0b00111100},
  };
const byte images[][8] = {
  {  0b00000000,  0b00000001,  0b00000011,  0b00000110,  0b10001100,  0b11011000,  0b01110000,  0b00100000}, // like
  {  0b00111000,  0b00111000,  0b00111000,  0b00111000,  0b11111110,  0b01111100,  0b00111000,  0b00010000}, // down arrow
  {  0b00001000,  0b00001100,  0b11111110,  0b11111111,  0b11111110,  0b00001100,  0b00001000,  0b00000000}, // right arrow
  {  0b00010000,  0b00111000,  0b01111100,  0b11111110,  0b00111000,  0b00111000,  0b00111000,  0b00111000}, // up arrow
  {  0b00010000,  0b00110000,  0b01111111,  0b11111111,  0b01111111,  0b00110000,  0b00010000,  0b00000000}  // left arrow
  };

// inicializace
void setup() 
{
  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(btnConfirmPin, INPUT_PULLUP);  
  pinMode(btnCancelPin, INPUT_PULLUP); 
  pinMode(btnUpPin, INPUT_PULLUP);  
  pinMode(btnDownPin, INPUT_PULLUP);  

  // nastaveni jasu na stredni hodnotu
  lc.setIntensity(0,8);
  lc.clearDisplay(0);

  // zapnutí komunikace s Ethernet Shieldem
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("Server je na IP adrese: ");
  Serial.println(Ethernet.localIP());
}

// hlavni beh
void loop() 
{
  if (digitalRead(btnConfirmPin) == LOW) onConfirmPress();
  if (digitalRead(btnCancelPin) == LOW) onCancelPress();
  if (digitalRead(btnUpPin) == LOW) onUpPress();
  if (digitalRead(btnDownPin) == LOW) onDownPress();

  checkEthernetClients();
}

// *******************************************************
// ************** obsluha LAN ****************************
// *******************************************************

void checkEthernetClients()
{
  enum packetType {ptNone, ptSetPickup, ptGetPickup};
  int quantity;
  packetType packet = ptNone;

  EthernetClient client = server.available();
  if (client) {
    Serial.println("Client connected:");
    while (client.connected()) {
      if (client.available()) {
        char c = client.read(); // cteni packetu PyE
        Serial.write(c);
        if (c == 'P') // posila se packet s mnozstvim pro zobrazeni
        {
          packet = ptSetPickup;
          pickQuantity = 0;
          lastQuantity = 0;
          lastPickup = prNone;
        }
        else if (c == 'G') // posila se packet s dotazem na vysledek
        {
          client.write('G');
          Serial.println();
          Serial.print("Sent: G");
          client.write(lastQuantity);
          Serial.print(lastQuantity);
          if (lastPickup == prNone) { // jest se nenapickovalo
            client.write('0');
            Serial.print('0');
          } else
          {
            client.write(lastPickup == prConfirm ? '2' : '1');            
            Serial.print(lastPickup == prConfirm ? '2' : '1');
          }
          client.write('E');     
          Serial.println('E');
        }
        else if (c == 'E') 
        {
          packet = ptNone;
          break;
        }
        else if (packet == ptSetPickup) {
          pickQuantity = (c-'0');
          lastQuantity = pickQuantity;
          displayQuantity();
        }
      }
    }
    delay(1);
    // uzavření spojení
    client.stop();
    Serial.println("");
    Serial.println("Client disconnected.");
    Serial.println("---------------------");
  }
}

// *******************************************************
// ************** ovladani tlacitek **********************
// *******************************************************

// udalost po stisku OK
void onConfirmPress()
{
  if (pickQuantity>0) 
  {
    tone(beeperPin, NOTE_C5, 100);
    blink();
    pickQuantity=0;
    lastPickup = prConfirm;
    displayConfirm();
    lc.shutdown(0,true);
  } else 
  {
    // TEST *************************************************
    pickQuantity=9;
    lastQuantity=0;
    lastPickup = prNone;
    displayQuantity();
    delay(100);
  }
}

// udalost po stisku Cancel
void onCancelPress()
{
  if (pickQuantity>0)
  {
    blink(); 
    tone(beeperPin, NOTE_C4, 100);
    pickQuantity=0;
    lastPickup = prCancel;
    displayCancel();
    lc.shutdown(0,true);
  }
  delay(100);
}

// udalost po stisku Nahoru
void onUpPress()
{
  if (pickQuantity>0)
  {
    blink(); 
    tone(beeperPin, NOTE_C1, 50);
    if (lastQuantity<pickQuantity) lastQuantity++;
    displayQuantity();
  }
  delay(100);
}

// udalost po stisku Dolu
void onDownPress()
{
  if (pickQuantity>0)
  {
    blink();
    tone(beeperPin, NOTE_C1, 50);
    if (lastQuantity>0) lastQuantity--;
    displayQuantity();
  }
  delay(100);
}

// *******************************************************
// ******************** ovladani LED panelu **************
// *******************************************************

// zobrazeni mnozstvi
void displayQuantity() 
{
  if (pickQuantity == 0)  
  {
    lc.clearDisplay(0);
    lc.shutdown(0,true);
  }
  else
  if (lastQuantity>=0 && lastQuantity<=9) 
  {
    lc.shutdown(0,false);
    displayImage(numbers[lastQuantity]);
  }
}

// zobrazeni preddefinovaneho obrazku
void displayImage(const byte* image) 
{
  for (int i = 0; i < 8; i++) 
  {
    for (int j = 0; j < 8; j++) 
    {
      lc.setLed(0, i, j, bitRead(image[i], 7 - j));
    }
  }
}

// zobrazeni po stisku OK
void displayConfirm()
{
  for (int i = 7; i >=0; i--) 
  {
    for (int j = 0; j < 8; j++) 
    {
      lc.setLed(0, i, j, bitRead(images[0][i], 7 - j));
    }
    delay(30);
  }
  delay(500);
}

// zobrazeni po stisku Cancel
void displayCancel()
{
  lc.setRow(0,0,B11111111);
  lc.setRow(0,7,B11111111);
  delay(50);
  lc.setRow(0,0,(byte)0);
  lc.setRow(0,7,(byte)0);
  lc.setRow(0,1,B11111111);
  lc.setRow(0,6,B11111111);
  delay(50);
  lc.setRow(0,1,(byte)0);
  lc.setRow(0,6,(byte)0);
  lc.setRow(0,2,B11111111);
  lc.setRow(0,5,B11111111);
  delay(50);
  lc.setRow(0,2,(byte)0);
  lc.setRow(0,5,(byte)0);
  lc.setRow(0,3,B11111111);
  lc.setRow(0,4,B11111111);
  delay(50);
  lc.setRow(0,3,(byte)0);
  lc.setRow(0,4,(byte)0);
}

// *******************************************************
// ******************* pomocne metody ********************
// *******************************************************

// bliknuti stavove diody
void blink()
{
  digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
  delay(100);                      // wait for a second
  digitalWrite(LED_BUILTIN, LOW);
}

// *******************************************************
// predpripraveno pro wifi...
// *******************************************************

// #include <ESP8266WiFi.h>
// const char* ssid = "your_wifi_ssid";     // Název Wi-Fi sítě
// const char* password = "your_wifi_pass"; // Heslo Wi-Fi sítě

// const char* serverIP = "server_ip_address"; // IP adresa řídící jednotky
// const int serverPort = 1234;                // Port pro komunikaci

// WiFiClient client;
// bool pickInProgress = false;
// unsigned long pickStartTime = 0;

// void setup() {
//   Serial.begin(115200);
//   pinMode(LED_BUILTIN, OUTPUT);
//   // Připojení k Wi-Fi
//   WiFi.begin(ssid, password);
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(1000);
//     Serial.println("Connecting to WiFi...");
//   }
//   Serial.println("Connected to WiFi");
// }

// void loop() {
//   if (!client.connected()) {
//     // Připojení k řídící jednotce přes Wi-Fi
//     if (client.connect(serverIP, serverPort)) {
//       Serial.println("Connected to server");
//     } else {
//       Serial.println("Connection to server failed");
//     }
//   }

//   // Čekání na signál pro pick
//   if (client.available()) {
//     char startChar = client.read(); // Začátek příkazu
//     if (startChar == 'P') {
//       char command[3];
//       client.readBytes(command, 3); // Kód příkazu
//       command[3] = '\0'; // Nastavení ukončovacího znaku
//       if (strcmp(command, "PCK") == 0) {
//         char valueStr[8];
//         client.readBytes(valueStr, 8); // Hodnota (počet)
//         valueStr[8] = '\0'; // Nastavení ukončovacího znaku
//         int pocetKusu = atoi(valueStr);
        
//         char checksumChar = client.read(); // Checksum
//         char endChar = client.read(); // Konec příkazu

//         // Výpočet checksum (jednoduchý příklad pro ilustraci)
//         char expectedChecksum = calculateChecksum(command, valueStr);

//         if (checksumChar == expectedChecksum && endChar == '\n') {
//           displayNumberOnLED(pocetKusu); // Zobrazení počtu na LED
//       }
//     }
//   }
// }

// char calculateChecksum(const char* command, const char* value) {
//   // Jednoduchý výpočet checksum (XOR všech znaků)
//   char checksum = 0;
//   for (int i = 0; i < 3; i++) {
//     checksum ^= command[i];
//   }
//   for (int i = 0; i < 8; i++) {
//     checksum ^= value[i];
//   }
//   return checksum;
// }
