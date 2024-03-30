# ESP Device Configuration and Control via MQTT

## 1. Project Overview
This project outlines the process of configuring an ESP device to connect in AP mode, receive configuration data via a web interface, switch to STA mode, connect to a WiFi network and MQTT broker, and subscribe to topics for configuring and controlling relays. The system supports up to 10 relays.

## 2. Initial Setup
The ESP device is initially set to AP mode with the following credentials:
- SSID: UPSWING
- Password: upswing@123

## 3. Web Interface Access
Upon connecting to the ESP device's network, accessing the provided IP address in a web browser opens an HTML page that accepts JSON configuration data.

## 4. Configuration Data Format
The web interface accepts the following JSON structure for configuring the WiFi and MQTT broker settings:
```json
{
  "ssid": "your ssid",
  "password": "your_password",
  "broker": "broker_name",
  "port": 1883
}
```
## 5. Switching to STA Mode
After receiving the configuration data, the ESP device disconnects from AP mode and connects to the specified WiFi network using the received credentials. It then starts MQTT and connects to the specified broker.

## 6. MQTT Subscription and Topics
The device subscribes to two MQTT topics for configuring and controlling relays. The JSON structures for each topic are as follows:
•	Topic `configRelay/rx` for configuring relays:
```json
{
    "relays": [
        {"number": 1, "pin": 5},
        {"number": 2, "pin": 18}
    ]
}
```
•	Topic `controlRelay/rx` for controlling relays:

```json
{
    "relays": [
        {"number": 1, "state": "on"},
        {"number": 2, "state": "off"}
    ]
}
```
## 7. Relay Configuration and Control
The device processes the received data to configure and control the relays accordingly. The number of relays can be increased up to 10, allowing for extensive control and customization.

## 8. Conclusion
The project successfully demonstrates how an ESP device can be configured and controlled remotely via MQTT. It highlights the device’s ability to switch between AP and STA modes, receive and process configuration data, and perform actions based on the received MQTT messages.



