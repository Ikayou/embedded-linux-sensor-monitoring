#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <mosquitto.h>
#include <string.h>
#define BME280_ADDRESS 0x77


// ========================================
// Messergebnis
// ========================================

struct SensorData {
    double temperature;
    double humidity;
    double pressure;
};


// ========================================
// Kalibrierwerte des BME280
// ========================================

struct CalibrationData {

    uint16_t T1;
    int16_t T2;
    int16_t T3;

    uint16_t P1;
    int16_t P2;
    int16_t P3;
    int16_t P4;
    int16_t P5;
    int16_t P6;
    int16_t P7;
    int16_t P8;
    int16_t P9;

    uint8_t H1;
    int16_t H2;
    uint8_t H3;
    int16_t H4;
    int16_t H5;
    int8_t H6;
};


// ========================================
// Rohdaten
// ========================================

struct RawData {
    int32_t temperature;
    int32_t humidity;
    int32_t pressure;
};


// ========================================
// I2C-Verbindung
// ========================================

int open_i2c(void) {

    int fd = open("/dev/i2c-1", O_RDWR);

    if (fd < 0) {
        perror("Failed to open I2C");
        return -1;
    }

    if (ioctl(fd, I2C_SLAVE, BME280_ADDRESS) < 0) {
        perror("Failed to connect BME280");
        close(fd);
        return -1;
    }

    return fd;
}


// ========================================
// Allgemeine Funktion zum Lesen von Registern
// ========================================

int read_registers(
    int fd,
    uint8_t reg,
    uint8_t *buffer,
    int length
) {

    if (write(fd, &reg, 1) != 1) {
        return -1;
    }

    if (read(fd, buffer, length) != length) {
        return -1;
    }

    return 0;
}


// ========================================
// Ein Byte in ein Register schreiben
// ========================================

int write_register(
    int fd,
    uint8_t reg,
    uint8_t value
) {

    uint8_t buffer[2];

    buffer[0] = reg;
    buffer[1] = value;

    if (write(fd, buffer, 2) != 2) {
        return -1;
    }

    return 0;
}


// ========================================
// 16-Bit-Little-Endian umwandeln
// ========================================

uint16_t to_u16(uint8_t low, uint8_t high) {
    return ((uint16_t)high << 8) | low;
}

int16_t to_s16(uint8_t low, uint8_t high) {
    return (int16_t)to_u16(low, high);
}


// ========================================
// Kalibrierwerte lesen
// ========================================

int read_calibration(
    int fd,
    struct CalibrationData *cal
) {

    uint8_t data1[26];
    uint8_t data2[7];

    if (read_registers(fd, 0x88, data1, 26) < 0) {
        return -1;
    }

    cal->T1 = to_u16(data1[0], data1[1]);
    cal->T2 = to_s16(data1[2], data1[3]);
    cal->T3 = to_s16(data1[4], data1[5]);

    cal->P1 = to_u16(data1[6], data1[7]);
    cal->P2 = to_s16(data1[8], data1[9]);
    cal->P3 = to_s16(data1[10], data1[11]);
    cal->P4 = to_s16(data1[12], data1[13]);
    cal->P5 = to_s16(data1[14], data1[15]);
    cal->P6 = to_s16(data1[16], data1[17]);
    cal->P7 = to_s16(data1[18], data1[19]);
    cal->P8 = to_s16(data1[20], data1[21]);
    cal->P9 = to_s16(data1[22], data1[23]);

    cal->H1 = data1[25];

    if (read_registers(fd, 0xE1, data2, 7) < 0) {
        return -1;
    }

    cal->H2 = to_s16(data2[0], data2[1]);
    cal->H3 = data2[2];

    cal->H4 =
        ((int16_t)(int8_t)data2[3] << 4) |
        (data2[4] & 0x0F);

    cal->H5 =
        ((int16_t)(int8_t)data2[5] << 4) |
        (data2[4] >> 4);

    cal->H6 = (int8_t)data2[6];

    return 0;
}


// ========================================
// BME280-Konfiguration
// ========================================

int configure_sensor(int fd) {

    // humidity oversampling x1
    if (write_register(fd, 0xF2, 0x01) < 0) {
        return -1;
    }

    // temperature x1
    // pressure x1
    // normal mode
    if (write_register(fd, 0xF4, 0x27) < 0) {
        return -1;
    }

    usleep(100000);

    return 0;
}


// ========================================
// Rohdaten lesen
// ========================================

int read_raw_data(
    int fd,
    struct RawData *raw
) {

    uint8_t data[8];

    if (read_registers(fd, 0xF7, data, 8) < 0) {
        return -1;
    }

    raw->pressure =
        ((int32_t)data[0] << 12) |
        ((int32_t)data[1] << 4) |
        (data[2] >> 4);

    raw->temperature =
        ((int32_t)data[3] << 12) |
        ((int32_t)data[4] << 4) |
        (data[5] >> 4);

    raw->humidity =
        ((int32_t)data[6] << 8) |
        data[7];

    return 0;
}


// ========================================
// Temperaturkompensation
// ========================================

double compensate_temperature(
    int32_t adc_T,
    const struct CalibrationData *cal,
    int32_t *t_fine
) {

    int32_t var1;
    int32_t var2;

    var1 =
        ((((adc_T >> 3) -
        ((int32_t)cal->T1 << 1))) *
        ((int32_t)cal->T2)) >> 11;

    var2 =
        (((((adc_T >> 4) -
        ((int32_t)cal->T1)) *
        ((adc_T >> 4) -
        ((int32_t)cal->T1))) >> 12) *
        ((int32_t)cal->T3)) >> 14;

    *t_fine = var1 + var2;

    int32_t temp =
        (*t_fine * 5 + 128) >> 8;

    return temp / 100.0;
}


// ========================================
// Luftdruckkompensation
// ========================================

double compensate_pressure(
    int32_t adc_P,
    int32_t t_fine,
    const struct CalibrationData *cal
) {

    int64_t var1;
    int64_t var2;
    int64_t p;

    var1 = (int64_t)t_fine - 128000;

    var2 =
        var1 * var1 * cal->P6;

    var2 +=
        (var1 * cal->P5) << 17;

    var2 +=
        ((int64_t)cal->P4) << 35;

    var1 =
        ((var1 * var1 * cal->P3) >> 8) +
        ((var1 * cal->P2) << 12);

    var1 =
        (((((int64_t)1) << 47) + var1) *
        cal->P1) >> 33;

    if (var1 == 0) {
        return 0.0;
    }

    p = 1048576 - adc_P;

    p =
        (((p << 31) - var2) * 3125) /
        var1;

    var1 =
        ((int64_t)cal->P9 *
        (p >> 13) *
        (p >> 13)) >> 25;

    var2 =
        ((int64_t)cal->P8 * p) >> 19;

    p =
        ((p + var1 + var2) >> 8) +
        ((int64_t)cal->P7 << 4);

    return (p / 256.0) / 100.0;
}


// ========================================
// Luftfeuchtigkeitskompensation
// ========================================

double compensate_humidity(
    int32_t adc_H,
    int32_t t_fine,
    const struct CalibrationData *cal
) {

    int32_t h = t_fine - 76800;

    h =
        (((((adc_H << 14) -
        ((int32_t)cal->H4 << 20) -
        ((int32_t)cal->H5 * h)) +
        16384) >> 15) *

        (((((((h *
        cal->H6) >> 10) *
        (((h *
        cal->H3) >> 11) +
        32768)) >> 10) +
        2097152) *
        cal->H2 +
        8192) >> 14));

    h -=
        (((((h >> 15) *
        (h >> 15)) >> 7) *
        cal->H1) >> 4);

    if (h < 0)
        h = 0;

    if (h > 419430400)
        h = 419430400;

    return (h >> 12) / 1024.0;
}


// ========================================
// Sensor einmal auslesen
// ========================================

int read_sensor(
    int fd,
    const struct CalibrationData *cal,
    struct SensorData *sensor
) {

    struct RawData raw;
    int32_t t_fine;

    if (read_raw_data(fd, &raw) < 0) {
        return -1;
    }

    sensor->temperature =
        compensate_temperature(
            raw.temperature,
            cal,
            &t_fine
        );

    sensor->pressure =
        compensate_pressure(
            raw.pressure,
            t_fine,
            cal
        );

    sensor->humidity =
        compensate_humidity(
            raw.humidity,
            t_fine,
            cal
        );

    return 0;
}


// ========================================
// Anzeige
// ========================================

void print_sensor(
    const struct SensorData *sensor
) {

    printf(
        "Temperature: %.2f C\n",
        sensor->temperature
    );

    printf(
        "Humidity: %.2f %%\n",
        sensor->humidity
    );

    printf(
        "Pressure: %.2f hPa\n",
        sensor->pressure
    );
}


int publish_mqtt(const struct SensorData *sensor) {

    struct mosquitto *mosq;
    char message[256];

    mosquitto_lib_init();

    mosq = mosquitto_new(NULL, true, NULL);

    if (mosq == NULL) {
        printf("Mosquitto init failed\n");
        return -1;
    }

    if (mosquitto_connect(
            mosq,
            "192.168.178.52",
            1883,
            60
        ) != MOSQ_ERR_SUCCESS) {

        printf("MQTT connection failed\n");

        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();

        return -1;
    }

    snprintf(
        message,
        sizeof(message),
        "{\"temperature\":%.2f,\"humidity\":%.2f,\"pressure\":%.2f}",
        sensor->temperature,
        sensor->humidity,
        sensor->pressure
    );

    if (mosquitto_publish(
            mosq,
            NULL,
            "home/sensor/bme280",
            strlen(message),
            message,
            0,
            false
        ) != MOSQ_ERR_SUCCESS) {

        printf("MQTT publish failed\n");

        mosquitto_disconnect(mosq);
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();

        return -1;
    }

    mosquitto_loop(mosq, 1000, 1);

    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    return 0;
}
// ========================================
// main
// ========================================

int main(void) {

    struct CalibrationData calibration;
    struct SensorData sensor;

    int fd = open_i2c();

    if (fd < 0) {
        return 1;
    }

    if (read_calibration(fd, &calibration) < 0) {
        printf("Calibration failed\n");
        close(fd);
        return 1;
    }

    if (configure_sensor(fd) < 0) {
        printf("Configuration failed\n");
        close(fd);
        return 1;
    }

    while (1) {

        if (read_sensor(fd, &calibration, &sensor) < 0) {
            printf("Sensor read failed\n");
            break;
        }

        print_sensor(&sensor);

        if (publish_mqtt(&sensor) < 0) {
            printf("MQTT publish failed\n");
        }

        sleep(5);
    }

    close(fd);

    return 0;
}