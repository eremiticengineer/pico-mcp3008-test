#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#include <cstring>

#include "FreeRTOS.h"
#include "task.h"

#define MAIN_TASK_PRIORITY ( tskIDLE_PRIORITY + 2UL )

#define SPI_PORT spi0
#define CS_PIN 17   // GP17 22
#define CLK_PIN 18  // GP18 24
#define MOSI_PIN 19 // GP19 25
#define MISO_PIN 16 // GP16 21

char windDirectionName[4] = "NNE";
int windDirectionADCValue;

static inline void cs_select() {
    asm volatile("nop \n nop \n nop");
    gpio_put(CS_PIN, 0);  // Active low
    asm volatile("nop \n nop \n nop");
}
static inline void cs_deselect() {
    asm volatile("nop \n nop \n nop");
    gpio_put(CS_PIN, 1);
    asm volatile("nop \n nop \n nop");
}

// Compass points, N,NNE,NE,ENE,E etc are 11mm apart

enum class WindDirection {
    N,
    NNE,
    NE,
    ENE,
    E,
    ESE,
    SE,
    SSE,
    S,
    SSW,
    SW,
    WSW,
    W,
    WNW,
    NW,
    NNW,
    Unknown
};

WindDirection getDirectionFromADCValue(uint16_t adc) {
    if (adc <= 83) {
        return WindDirection::ESE;
    }
    else if (adc <= 96) {
        return WindDirection::ENE;
    }
    else if (adc <= 117) {
        return WindDirection::E;
    }
    else if (adc <= 162) {
        return WindDirection::SSE;
    }
    else if (adc <= 220) {
        return WindDirection::SE;
    }
    else if (adc <= 271) {
        return WindDirection::SSW;
    }
    else if (adc <= 350) {
        return WindDirection::S;
    }
    else if (adc <= 436) {
        return WindDirection::NNE;
    }
    else if (adc <= 532) {
        return WindDirection::NE;
    }
    else if (adc <= 616) {
        return WindDirection::WSW;
    }
    else if (adc <= 667) {
        return WindDirection::SW;
    }
    else if (adc <= 744) {
        return WindDirection::NNW;
    }
    else if (adc <= 806) {
        return WindDirection::N;
    }
    else if (adc <= 857) {
        return WindDirection::WNW;
    }
    else if (adc <= 915) {
        return WindDirection::NW;
    }
    else {
        return WindDirection::W;
    }
}

float getWindDirectionDegrees(uint16_t adc) {
    if (adc <= 83) {
        return 112.5f;
    }
    else if (adc <= 96) {
        return 67.5f;
    }
    else if (adc <= 117) {
        return 90.0f;
    }
    else if (adc <= 162) {
        return 157.5f;
    }
    else if (adc <= 220) {
        return 135.0f;
    }
    else if (adc <= 271) {
        return 202.5f;
    }
    else if (adc <= 350) {
        return 180.0f;
    }
    else if (adc <= 436) {
        return 22.5f;
    }
    else if (adc <= 532) {
        return 45.0f;
    }
    else if (adc <= 616) {
        return 247.5f;
    }
    else if (adc <= 667) {
        return 225.0f;
    }
    else if (adc <= 744) {
        return 337.5f;
    }
    else if (adc <= 806) {
        return 0.0f;
    }
    else if (adc <= 857) {
        return 292.5f;
    }
    else if (adc <= 915) {
        return 315.0f;
    }
    else {
        return 270.0f;
    }
}

const char* windDirectionToString(WindDirection direction) {
    switch (direction) {
        case WindDirection::N:   return "N";
        case WindDirection::NNE: return "NNE";
        case WindDirection::NE:  return "NE";
        case WindDirection::ENE: return "ENE";
        case WindDirection::E:   return "E";
        case WindDirection::ESE: return "ESE";
        case WindDirection::SE:  return "SE";
        case WindDirection::SSE: return "SSE";
        case WindDirection::S:   return "S";
        case WindDirection::SSW: return "SSW";
        case WindDirection::SW:  return "SW";
        case WindDirection::WSW: return "WSW";
        case WindDirection::W:   return "W";
        case WindDirection::WNW: return "WNW";
        case WindDirection::NW:  return "NW";
        case WindDirection::NNW: return "NNW";
        default:                 return "Unknown";
    }
}

uint16_t readWindDirectionMedianADC() {
    constexpr int samples = 9;

    uint16_t values[samples];

    constexpr int chan = 0;

    uint8_t buffer[3] = {1, static_cast<uint8_t>((8 + chan) << 4), 0};

    for (int i = 0; i < samples; ++i) {
        uint8_t returnData[3];

        cs_select();

        spi_write_read_blocking(
            SPI_PORT,
            buffer,
            returnData,
            sizeof(buffer)
        );

        cs_deselect();

        values[i] = ((returnData[1] & 0x03) << 8) | returnData[2];

        vTaskDelay(pdMS_TO_TICKS(2));
    }

    // Simple insertion sort; for 9 values this is perfectly fine.
    for (int i = 1; i < samples; ++i) {
        uint16_t value = values[i];
        int j = i - 1;

        while (j >= 0 && values[j] > value) {
            values[j + 1] = values[j];
            --j;
        }

        values[j + 1] = value;
    }

    return values[samples / 2];
}

void wind_direction_task(__unused void* pvParameters) {

    spi_init(SPI_PORT, 100000);

    spi_set_format(
        SPI_PORT,
        8,
        SPI_CPOL_0,
        SPI_CPHA_0,
        SPI_MSB_FIRST
    );

    gpio_set_function(CLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MISO_PIN, GPIO_FUNC_SPI);

    gpio_init(CS_PIN);
    gpio_set_dir(CS_PIN, GPIO_OUT);
    gpio_put(CS_PIN, 1);

    while (true) {
        uint16_t adc = readWindDirectionMedianADC();

        WindDirection wind_direction = getDirectionFromADCValue(adc);

        float wind_direction_degrees = getWindDirectionDegrees(adc);

        printf("WIND DIRECTION DATA = %u %s %.1f\n",
          adc, windDirectionToString(wind_direction), wind_direction_degrees);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main( void )
{
    stdio_init_all();

    xTaskCreate(wind_direction_task, "WindDirectionTask", 512, nullptr, MAIN_TASK_PRIORITY, nullptr);

    vTaskStartScheduler();

    return 0;
}
