# 📡 CurieBT

**🇨🇿 [Čeština](#-čeština) | 🇬🇧 [English](#-english)**

---

## 🇨🇿 Čeština

**Externí Bluetooth modul (ESP32) pro libovolný Geiger-Müllerův přístroj.**

CurieBT přidá bezdrátové připojení jakémukoliv stávajícímu Geiger-Müllerovu přístroji. Načítá impulzy z výstupu komparátoru a odesílá je přes Bluetooth do aplikace [CurieFinder](https://github.com/ATonda/CurieFinder_app) (nebo jiného SPP klienta).

### 🔧 Hardware

Firmware **pouze pro ESP32 WROOM-32** — ESP32-S3, C3 a S2 nejsou podporovány, protože nemají Bluetooth Classic SPP.

| Pin | Funkce |
|---|---|
| GPIO 23 | Vstup impulzů z komparátoru (FALLING, interní pull-up) — max 3,3 V, pro 5V logiku nutný dělič napětí |
| GPIO 34 | Měření napětí baterie (ADC, dělič R1=100k / R2=100k) — pouze vstup, max 3,3 V |

### 📡 Princip

- **Bluetooth Classic SPP** (ne BLE) — kontinuální stream bez connection intervalu, takže UI nezamrzá ani při vysokém CPS
- Výpočet CPS: kruhový buffer 4× 250 ms = okno 1 s, vyhlazení exponenciálním klouzavým průměrem (EMA, α = 0,3)
- Odesílání 4× za sekundu (1× za sekundu, dokud běží WiFi portál)

### 📤 Formát dat (SPP TX)

```
CPS=X.XX RATE=X.XX\n            každých 250 ms
CPS=X.XX RATE=X.XX VBAT=X.X\n   každých 5 s
```

### ⚙️ Konfigurace (WiFi portál)

Po zapnutí se na **60 sekund** spustí WiFi přístupový bod:

1. Připoj se k WiFi síti `CurieBT 192.168.4.1`
2. Otevři v prohlížeči `http://192.168.4.1`
3. Nastav název zařízení, GM faktor, parametry baterie atd.

Nastavení se ukládá do NVS (přežije restart). Po 5 minutách bez Bluetooth připojení modul přejde do hlubokého spánku (probuzení resetem napájení).

### 🔬 Podporované GM trubice

SBM-20 (výchozí faktor 0,5556 µSv/CPS), SBM-19, SI-3BG, SI-22G, LND-712, J305 nebo vlastní faktor.

### 🛠️ Sestavení (Arduino IDE)

Potřebné knihovny: `BluetoothSerial`, `WiFi`, `WebServer`, `DNSServer`, `ESPmDNS`, `Preferences`, `ArduinoJson`.
Vyber desku **ESP32 Dev Module** (WROOM-32) a nahraj `CurieBT.ino`.

### 📄 Licence

Volně k použití. Vyžadováno uvedení autora — **© ATonda**

---

## 🇬🇧 English

**External Bluetooth module (ESP32) for any Geiger-Mueller instrument.**

CurieBT adds wireless connectivity to any existing Geiger-Mueller instrument. It reads pulses from the comparator output and streams them over Bluetooth to the [CurieFinder](https://github.com/ATonda/CurieFinder_app) app (or any other SPP client).

### 🔧 Hardware

Firmware is for **ESP32 WROOM-32 only** — ESP32-S3, C3 and S2 are not supported, as they lack Bluetooth Classic SPP.

| Pin | Function |
|---|---|
| GPIO 23 | Pulse input from comparator (FALLING, internal pull-up) — 3.3 V max, voltage divider required for 5V logic |
| GPIO 34 | Battery voltage measurement (ADC, divider R1=100k / R2=100k) — input only, 3.3 V max |

### 📡 How it works

- **Bluetooth Classic SPP** (not BLE) — continuous stream without connection interval, so the UI never freezes even at high CPS
- CPS calculation: ring buffer 4× 250 ms = 1 s window, smoothed via Exponential Moving Average (EMA, α = 0.3)
- Transmits 4× per second (1× per second while the WiFi portal is active)

### 📤 Data format (SPP TX)

```
CPS=X.XX RATE=X.XX\n            every 250 ms
CPS=X.XX RATE=X.XX VBAT=X.X\n   every 5 s
```

### ⚙️ Configuration (WiFi portal)

On power-on, a WiFi access point runs for **60 seconds**:

1. Connect to the WiFi network `CurieBT 192.168.4.1`
2. Open `http://192.168.4.1` in a browser
3. Set device name, GM factor, battery parameters, etc.

Settings are stored in NVS (survive restart). After 5 minutes without a Bluetooth connection, the module enters deep sleep (wake by power reset).

### 🔬 Supported GM tubes

SBM-20 (default factor 0.5556 µSv/CPS), SBM-19, SI-3BG, SI-22G, LND-712, J305, or a custom factor.

### 🛠️ Build (Arduino IDE)

Required libraries: `BluetoothSerial`, `WiFi`, `WebServer`, `DNSServer`, `ESPmDNS`, `Preferences`, `ArduinoJson`.
Select board **ESP32 Dev Module** (WROOM-32) and upload `CurieBT.ino`.

### 📄 License

Free to use. Attribution required — **© ATonda**
