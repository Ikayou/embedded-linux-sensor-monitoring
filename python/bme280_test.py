import time
import json
import smbus2
import bme280
import paho.mqtt.client as mqtt

I2C_PORT = 1
BME280_ADDRESS = 0x77

MQTT_BROKER = "192.168.178.52"
MQTT_PORT = 1883
MQTT_TOPIC = "home/sensor/bme280/python"


def open_sensor():
    bus = smbus2.SMBus(I2C_PORT)

    calibration_params = bme280.load_calibration_params(
        bus,
        BME280_ADDRESS
    )

    return bus, calibration_params


def read_sensor(bus, calibration_params):

    data = bme280.sample(
        bus,
        BME280_ADDRESS,
        calibration_params
    )

    return {
        "temperature": round(data.temperature, 2),
        "humidity": round(data.humidity, 2),
        "pressure": round(data.pressure, 2)
    }


def publish_mqtt(client, sensor_data):

    payload = json.dumps(sensor_data)

    result = client.publish(
        MQTT_TOPIC,
        payload
    )

    if result.rc != mqtt.MQTT_ERR_SUCCESS:
        print("MQTT publish failed")


def main():

    bus, calibration_params = open_sensor()

    client = mqtt.Client()

    client.connect(
        MQTT_BROKER,
        MQTT_PORT,
        60
    )

    client.loop_start()

    try:
        while True:

            sensor_data = read_sensor(
                bus,
                calibration_params
            )

            print(
                f"Temperature: {sensor_data['temperature']:.2f} C"
            )

            print(
                f"Humidity: {sensor_data['humidity']:.2f} %"
            )

            print(
                f"Pressure: {sensor_data['pressure']:.2f} hPa"
            )

            publish_mqtt(
                client,
                sensor_data
            )

            print()

            time.sleep(5)

    except KeyboardInterrupt:
        print("Stopped")

    finally:
        client.loop_stop()
        client.disconnect()
        bus.close()


if __name__ == "__main__":
    main()