#pragma once

namespace page_sht41 {

namespace {
constexpr uint8_t kAddress = 0x44;
constexpr uint8_t kMeasureHighPrecision = 0xFD;
constexpr uint32_t kMeasurementDelayMs = 10;
constexpr uint32_t kSampleIntervalMs = 1000;

bool backFocused = false;
bool screenDirty = true;
int32_t encSnapshot = 0;
uint32_t lastSampleMs = 0;
float temperatureC = 0.0f;
float humidityRh = 0.0f;
const char* sampleError = nullptr;

uint8_t crc8(const uint8_t* data)
{
    // SHT41 appends a CRC-8 with polynomial 0x31 to each two-byte value.
    uint8_t crc = 0xFF;
    for (size_t byte = 0; byte < 2; ++byte) {
        crc ^= data[byte];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            const bool highBit = (crc & 0x80) != 0;
            crc <<= 1;
            if (highBit) {
                crc ^= 0x31;
            }
        }
    }
    return crc;
}

const char* readMeasurement()
{
    Wire.beginTransmission(kAddress);
    Wire.write(kMeasureHighPrecision);
    const uint8_t status = Wire.endTransmission();
    if (status == 2) {
        return "Sensor not found at 0x44";
    }
    if (status != 0) {
        return "I2C write failed";
    }

    delay(kMeasurementDelayMs);
    uint8_t data[6];
    if (Wire.requestFrom(kAddress, sizeof(data), true) != sizeof(data)) {
        return "Incomplete sensor reply";
    }
    for (uint8_t& byte : data) {
        byte = static_cast<uint8_t>(Wire.read());
    }
    if (crc8(data) != data[2] || crc8(data + 3) != data[5]) {
        return "Sensor CRC check failed";
    }

    const uint16_t rawTemperature = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    const uint16_t rawHumidity = (static_cast<uint16_t>(data[3]) << 8) | data[4];
    temperatureC = -45.0f + 175.0f * rawTemperature / 65535.0f;
    humidityRh = constrain(-6.0f + 125.0f * rawHumidity / 65535.0f, 0.0f, 100.0f);
    return nullptr;
}

void sample()
{
    lastSampleMs = millis();
    sampleError = readMeasurement();
    if (sampleError != nullptr) {
        Serial.printf("[SHT41] %s\n", sampleError);
    } else {
        Serial.printf("[SHT41] Temperature: %.2f C, Humidity: %.2f %%RH\n",
                      temperatureC, humidityRh);
    }
    screenDirty = true;
}
}  // namespace

void init()
{
    backFocused = false;
    encSnapshot = g.encRaw;
    sample();
}

void update()
{
    if (updateBinaryBackFocus(encSnapshot, backFocused)) {
        screenDirty = true;
    }
    if (takeEncoderButton() && backFocused) {
        requestExitSubPage();
        return;
    }
    if (millis() - lastSampleMs >= kSampleIntervalMs) {
        sample();
    }
}

void render()
{
    if (!screenDirty) {
        return;
    }

    gSubPageGfx.beginFrame();
    tft.fillScreen(TFT_BLACK);
    drawPageHeader("SHT41 Sensor", TFT_CYAN);

    if (sampleError != nullptr) {
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString(sampleError, kMargin, 45, 2);
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        tft.drawString("Check the 3.3 V I2C connection.", kMargin, 76, 2);
    } else {
        char value[32];
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        tft.drawString("Temperature", kMargin, 36, 2);
        tft.drawString("Relative humidity", kMargin, 91, 2);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        snprintf(value, sizeof(value), "%.2f C", temperatureC);
        tft.drawString(value, kMargin, 54, 4);
        snprintf(value, sizeof(value), "%.2f %%RH", humidityRh);
        tft.drawString(value, kMargin, 109, 4);
    }

    drawPageFooter("I2C 0x44  |  USB serial", backFocused);
    gSubPageGfx.endFrame();
    screenDirty = false;
}

void deinit()
{
    // Wire is shared with the factory app's onboard peripherals.
}

}  // namespace page_sht41
