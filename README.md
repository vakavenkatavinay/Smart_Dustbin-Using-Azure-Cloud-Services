# EcoSort – Smart Bin Using Microsoft Azure Cloud Services

### IoT-Based Smart Waste Management and Cloud Monitoring System

EcoSort is an IoT-based smart waste management system that combines **ESP32 microcontroller technology, ultrasonic sensors, automated lid control, and Microsoft Azure cloud services** to provide touchless waste disposal and remote monitoring of bin capacity.

The project demonstrates how physical IoT devices can communicate with cloud infrastructure using the MQTT protocol, process sensor data through serverless computing, and display the latest information on a web-based dashboard.

## 1. Project Objectives

- Develop a touchless smart bin using ESP32 and ultrasonic sensors.
- Automatically open and close the lid using a stepper motor.
- Measure the waste fill level and calculate its percentage.
- Activate a red LED warning when the bin reaches 90% capacity.
- Transmit sensor telemetry securely to Microsoft Azure.
- Process and store sensor readings using Azure Functions and Table Storage.
- Display bin information through a cloud-hosted Flask dashboard.

## 2. System Architecture

The project follows a device-to-cloud architecture:

```text
ESP32 Microcontroller
       |
       | Ultrasonic Sensor Readings
       v
Wi-Fi Connection
       |
       | MQTT over TLS (Port 8883)
       v
Azure IoT Hub
       |
       | Event Hub-Compatible Endpoint
       v
Azure Functions
(ProcessSmartBinTelemetry)
       |
       v
Azure Table Storage
(SmartBinStatus)
       |
       v
Python Flask Backend
       |
       v
Azure App Service
       |
       v
EcoSort Web Dashboard
```

The ESP32 handles sensor readings, motor operation, and LED alerts locally. Azure services handle telemetry reception, processing, storage, and remote visualization.

## 3. Hardware Components

| Component | Purpose |
|---|---|
| ESP32 Microcontroller | Main IoT controller |
| HC-SR04 Ultrasonic Sensor × 2 | Hand detection and waste-level measurement |
| 28BYJ-48 Stepper Motor | Automatic lid movement |
| ULN2003 Motor Driver | Controls the stepper motor |
| Red LED | Full-bin warning indicator |
| Resistors and jumper wires | Circuit connections |
| Power supply | Powers the hardware |

## 4. Technologies Used

**Embedded Systems**
- ESP32
- Arduino IDE
- C/C++
- Ultrasonic sensors and stepper motor control

**Microsoft Azure**
- Azure IoT Hub
- Azure Functions
- Azure Table Storage
- Azure App Service

**Web Development**
- Python
- Flask
- HTML5
- CSS3
- JavaScript

**Communication and Development Tools**
- MQTT 3.1.1 over TLS
- HTTPS
- JSON
- Git and GitHub
- Visual Studio Code

## 5. Working Principle

### Step 1: Hand Detection

An ultrasonic sensor continuously monitors the area near the bin lid.

When a hand is detected within approximately 15 cm, the ESP32 activates the stepper motor to open the lid.

When the hand is removed, the lid closes automatically.

### Step 2: Fill-Level Detection

A second ultrasonic sensor measures the distance between the top of the bin and the waste surface.

The ESP32 converts this measurement into a fill percentage.

The system categorizes the bin status as:

| Fill Percentage | Status |
|---|---|
| Below 75% | NORMAL |
| 75% to below 90% | NEAR FULL |
| 90% or above | FULL |

When the bin reaches 90%, the ESP32 activates a blinking red LED to indicate that the bin needs emptying.

### Step 3: Cloud Communication

The ESP32 connects to Wi-Fi and publishes sensor telemetry to Azure IoT Hub using MQTT over TLS.

An example telemetry message:

```json
{
  "deviceId": "SmartBin001",
  "fillPercentage": 92,
  "binStatus": "FULL",
  "handDetected": false,
  "lidStatus": "CLOSED"
}
```

### Step 4: Data Processing and Storage

Azure IoT Hub receives the telemetry messages.

The `ProcessSmartBinTelemetry` Azure Function processes the incoming data and updates the `SmartBinStatus` table in Azure Table Storage.

The system maintains the latest recorded status of the smart bin.

### Step 5: Cloud Dashboard

A Python Flask application hosted on Azure App Service retrieves the latest data from Azure Table Storage.

The dashboard is designed to display:

- Bin fill percentage
- Bin status
- Lid status
- Hand detection status, where available
- Last updated timestamp

The dashboard allows users to monitor the smart bin remotely through a web browser.

## 6. Project Features

- Touchless waste disposal
- Automatic lid opening and closing
- Waste fill-level monitoring
- Local full-bin LED warning
- Secure MQTT communication
- Serverless telemetry processing
- Cloud-based data storage
- Web-based monitoring dashboard
- Separation of local device control and cloud monitoring

## 7. Repository Structure

```text
Ecosort_Azure_iot_Project/
│
├── esp32/
│   └── SmartBin.ino
│
├── azure-function/
│   └── ProcessSmartBinTelemetry/
│       ├── index.js
│       └── function.json
│
├── templates/
│   └── index.html
│
├── app.py
├── requirements.txt
├── seed_table.py
├── .deployment
├── .gitignore
└── README.md
```

*Note: The structure above is illustrative. Refer to the repository for the actual filenames and configuration files.*

## 8. Running the Project

### Prerequisites

- ESP32 development board and required hardware
- Arduino IDE with ESP32 board support
- Python and pip
- Microsoft Azure subscription
- Configured Azure IoT Hub and device identity
- Azure Table Storage
- Azure Function App
- Azure App Service for cloud deployment

### Run the Flask Dashboard Locally

Clone the repository:

```bash
git clone https://github.com/vakavenkatavinay/Smart_Dustbin-Using-Azure-Cloud-Services.git
```

Navigate to the project folder:

```bash
cd Smart_Dustbin-Using-Azure-Cloud-Services
```

Create and activate a Python virtual environment:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
```

Install dependencies:

```bash
pip install -r requirements.txt
```

Configure the required Azure Storage environment variable securely:

```powershell
$env:AZURE_STORAGE_CONNECTION_STRING = "YOUR_STORAGE_CONNECTION_STRING"
```

Run the Flask application:

```bash
python app.py
```

Open the local URL displayed in the terminal.

### ESP32 Setup

1. Install the ESP32 board package in Arduino IDE.
2. Connect the sensors, motor driver, and LED according to the project wiring.
3. Configure Wi-Fi and Azure IoT Hub credentials securely.
4. Select the correct ESP32 board and serial port.
5. Upload the Arduino sketch.
6. Open Serial Monitor to verify sensor readings and cloud connectivity.

**Security:** Do not commit Wi-Fi passwords, Azure device keys, SAS tokens, or Azure Storage connection strings to GitHub.

## 9. Benefits of Using Microsoft Azure

Compared with a localhost-only application, Azure provides cloud-hosted services for remote access and IoT data processing.

Azure IoT Hub manages device communication, Azure Functions provides event-driven processing, Azure Table Storage stores sensor information, and Azure App Service hosts the monitoring website.

This architecture demonstrates the practical use of cloud computing, IoT communication, serverless processing, and web application deployment.

## 10. Future Enhancements

- Email or mobile notifications when the bin is full
- Monitoring multiple smart bins from one dashboard
- Historical waste-level analytics
- Automated waste collection notifications
- Device authentication and access management improvements
- Predictive analysis of waste collection requirements

## 11. Conclusion

EcoSort demonstrates the integration of embedded systems, IoT communication, and Microsoft Azure cloud services into a functional smart waste management solution.

The project combines local automation with cloud-based monitoring, providing a practical foundation for future smart-city and connected-device applications.

---

**Project:** EcoSort – Smart Bin Using Microsoft Azure Cloud Services

**Category:** Internet of Things (IoT), Cloud Computing, Embedded Systems

**Platform:** ESP32 + Microsoft Azure
