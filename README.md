# Raspberry Pi BME280 Monitoring mit MQTT, PostgreSQL und Grafana

Dieses Projekt ist ein kleines Embedded-Linux- und IoT-Homelab auf Basis eines Raspberry Pi 4 und eines BME280-Sensors.

Ziel des Projekts ist es, Sensordaten mit C und Python auszulesen, über MQTT an einen Linux-Server zu übertragen, in PostgreSQL zu speichern und anschließend mit Grafana zu visualisieren.

## Architektur

```text
BME280 Sensor
    ↓ I2C
Raspberry Pi 4
    ↓
C / Python
    ↓ JSON
MQTT Publish
    ↓
Proxmox Host
192.168.178.52
    ↓ DNAT Port 1883
Ubuntu Server VM
10.10.10.10
    ↓
Mosquitto
    ↓
n8n
    ↓
PostgreSQL
    ↓
Grafana
```

## Raspberry Pi

Auf dem Raspberry Pi wird der BME280 über I2C angesprochen.

Der Sensor liefert:

- Temperatur
- Luftfeuchtigkeit
- Luftdruck

Die I2C-Adresse des verwendeten Sensors ist:

```text
0x77
```

### C-Version

Die erste Version wurde in C umgesetzt.

Dabei wurden unter anderem folgende Themen praktisch verwendet:

- Linux I2C Device Interface
- `/dev/i2c-1`
- `open()`, `read()`, `write()` und `ioctl()`
- BME280 Register
- Bitoperationen
- Rohdatenverarbeitung
- Kalibrierungswerte des Sensors
- Pointer
- Structs
- Aufteilung des Programms in Funktionen
- MQTT mit `libmosquitto`

Die Messwerte werden in einer Struktur gespeichert:

```c
struct SensorData {
    double temperature;
    double humidity;
    double pressure;
};
```

Das Programm liest die Daten regelmäßig aus und veröffentlicht sie als JSON über MQTT.

Beispiel:

```json
{
  "temperature": 22.69,
  "humidity": 26.83,
  "pressure": 1010.68
}
```

### Python-Version

Zusätzlich wurde eine Python-Version erstellt, um die Unterschiede zwischen einer Low-Level-Implementierung in C und einer Umsetzung mit bestehenden Python-Bibliotheken zu vergleichen.

Verwendete Bibliotheken:

```text
smbus2
RPi.bme280
paho-mqtt
```

Die Python-Version verwendet Bibliotheken für die BME280-Kalibrierung und die I2C-Kommunikation, während diese Schritte in der C-Version wesentlich direkter umgesetzt werden.

## MQTT

Als MQTT Broker wird Mosquitto auf der Ubuntu-Server-VM verwendet.

MQTT-Port:

```text
1883
```

Verwendetes Topic:

```text
home/sensor/bme280
```

Für Tests wurden außerdem getrennte Topics verwendet, zum Beispiel:

```text
home/sensor/bme280/c
home/sensor/bme280/python
```

Dadurch können die C- und Python-Version miteinander verglichen werden.

## Proxmox Netzwerk

Die Ubuntu-VM befindet sich in einem internen Proxmox-Netzwerk:

```text
10.10.10.0/24
```

Ubuntu Server:

```text
10.10.10.10
```

Der Proxmox-Host ist über WLAN mit dem Heimnetz verbunden:

```text
192.168.178.52
```

Für den Zugriff auf interne Dienste werden DNAT-Regeln mit nftables verwendet.

Beispiele:

```text
192.168.178.52:2222 → 10.10.10.10:22
192.168.178.52:5678 → 10.10.10.10:5678
192.168.178.52:9443 → 10.10.10.10:9443
192.168.178.52:1883 → 10.10.10.10:1883
192.168.178.52:3000 → 10.10.10.10:3000
```

Zusätzlich wird NAT/Masquerading verwendet, damit die VM über den Proxmox-Host auf das Internet zugreifen kann.

Da die Proxmox Firewall aktiv ist, wurde für das NAT außerdem eine Conntrack-Zone konfiguriert.

```bash
iptables -t raw -I PREROUTING -i fwbr+ -j CT --zone 1
```

Die Netzwerkkonfiguration wird über ein eigenes Skript automatisiert:

```text
/usr/local/sbin/homelab-nat.sh
```

Dieses Skript wird über einen systemd-Service beim Start des Proxmox-Hosts ausgeführt.

## Mosquitto

Mosquitto läuft direkt auf der Ubuntu-Server-VM.

Der Broker lauscht auf:

```text
0.0.0.0:1883
```

Für die aktuelle Homelab-Umgebung werden anonyme MQTT-Verbindungen verwendet.

Beispiel für einen manuellen Test:

```bash
mosquitto_pub \
  -h 192.168.178.52 \
  -p 1883 \
  -t home/sensor/test \
  -m "MQTT test"
```

Empfang auf dem Ubuntu Server:

```bash
mosquitto_sub \
  -h localhost \
  -t 'home/sensor/#' \
  -v
```

## n8n

n8n läuft als Docker-Container auf der Ubuntu-Server-VM.

Workflow:

```text
MQTT Trigger
    ↓
Code
    ↓
PostgreSQL Insert
```

Der MQTT Trigger empfängt die JSON-Daten des Raspberry Pi.

Anschließend werden die Werte verarbeitet:

```text
temperature
humidity
pressure
```

und in PostgreSQL gespeichert.

## PostgreSQL

PostgreSQL läuft als Docker-Container.

Verwendete Version:

```text
PostgreSQL 16
```

Die Messwerte werden in der Tabelle `sensor_data` gespeichert.

```sql
CREATE TABLE sensor_data (
    id SERIAL PRIMARY KEY,
    temperature DOUBLE PRECISION,
    humidity DOUBLE PRECISION,
    pressure DOUBLE PRECISION,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

Beispiel:

```text
 id | temperature | humidity | pressure | created_at
----+-------------+----------+----------+---------------------
  1 |       22.69 |    26.83 |  1010.68 | ...
```

## Grafana

Grafana läuft ebenfalls als Docker-Container.

Grafana greift direkt auf PostgreSQL zu und stellt die gespeicherten Messwerte dar.

Das Dashboard enthält getrennte Panels für:

- Temperatur in °C
- Luftfeuchtigkeit in %
- Luftdruck in hPa

Dadurch können die Messwerte über die Zeit beobachtet werden.

## Docker / Portainer

Auf dem Ubuntu Server werden mehrere Dienste mit Docker betrieben und über Portainer verwaltet:

```text
n8n
PostgreSQL
Grafana
Portainer
```

## Aktueller Datenfluss

```text
BME280
↓
Raspberry Pi
↓
C oder Python
↓
JSON
↓
MQTT
↓
Proxmox NAT / Firewall
↓
Ubuntu Server
↓
Mosquitto
↓
n8n
↓
PostgreSQL
↓
Grafana
```

## Lernziele

Mit dem Projekt werden verschiedene Bereiche miteinander verbunden:

- C
- Python
- Embedded Linux
- Raspberry Pi
- I2C
- Sensorik
- MQTT
- Linux Networking
- NAT
- nftables
- Proxmox
- Docker
- Portainer
- n8n
- PostgreSQL
- Grafana
- systemd
- Troubleshooting

Besonders wichtig ist dabei nicht nur die reine Sensorabfrage, sondern der komplette Datenfluss vom Embedded-Gerät bis zur Speicherung und Visualisierung auf einem Server.

## Nächste Schritte

Geplante Erweiterungen:

- Raspberry-Pi-Anwendung als systemd-Service betreiben
- Logging mit `journalctl`
- Fehler- und Ausfallerkennung
- Monitoring für MQTT und Sensorverfügbarkeit
- Benachrichtigungen bei Fehlern
- Firewall-Regeln weiter einschränken
- VPN / Site-to-Site-VPN testen
- weitere Sensoren hinzufügen
- Vergleich der C- und Python-Implementierung
- CI/CD für die Raspberry-Pi-Anwendung
