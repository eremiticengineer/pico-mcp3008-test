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

void getDirectionFromADCValue(int adcValue, char* buffer) {
  windDirectionADCValue = adcValue;
    // 7% with corrected min/max to prevent overlap
    if ((adcValue >= 730) && (adcValue <= 740)) strcpy(buffer, "N");   // 736
    if ((adcValue >= 374) && (adcValue <= 438)) strcpy(buffer, "NNE"); // 397
    if ((adcValue >= 425) && (adcValue <= 499)) strcpy(buffer, "NE");  // 449
    if ((adcValue >= 77) && (adcValue <= 91)) strcpy(buffer, "ENE");   // 85
    if ((adcValue >= 86) && (adcValue <= 100)) strcpy(buffer, "E");    // 95
    if ((adcValue >= 63) && (adcValue <= 73)) strcpy(buffer, "ESE");   // 67
    if ((adcValue >= 171) && (adcValue <= 201)) strcpy(buffer, "SE");  // 186
    if ((adcValue >= 117) && (adcValue <= 137)) strcpy(buffer, "SSE"); // 128
    if ((adcValue >= 265) && (adcValue <= 311)) strcpy(buffer, "S");   // 286
    if ((adcValue >= 225) && (adcValue <= 265)) strcpy(buffer, "SSW"); // 244
    if ((adcValue >= 590) && (adcValue <= 600)) strcpy(buffer, "SW");  // 598
    if ((adcValue >= 570) && (adcValue <= 580)) strcpy(buffer, "WSW"); // 572
    if ((adcValue >= 870) && (adcValue <= 890)) strcpy(buffer, "W");  // 873
    if ((adcValue >= 770) && (adcValue <= 780)) strcpy(buffer, "WNW"); // 772
    if ((adcValue >= 820) && (adcValue <= 830)) strcpy(buffer, "NW");  // 823
    if ((adcValue >= 660) && (adcValue <= 670)) strcpy(buffer, "NNW"); // 663
}

/*
void wind_direction_task(__unused void* pvParameters)
{
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

    while (true)
    {
        uint8_t tx[3] = {
            0x01,
            0x80,       // single ended, channel 0
            0x00
        };

        uint8_t rx[3] = {0};

        gpio_put(CS_PIN, 0);

        spi_write_read_blocking(
            SPI_PORT,
            tx,
            rx,
            3
        );

        gpio_put(CS_PIN, 1);

        int value =
            ((rx[1] & 0x03) << 8) |
            rx[2];

        printf(
            "TX: %02X %02X %02X  RX: %02X %02X %02X  ADC: %d\n",
            tx[0], tx[1], tx[2],
            rx[0], rx[1], rx[2],
            value
        );

        vTaskDelay(1000);
    }
}
*/

void wind_direction_task(__unused void* pvParameters) {
  spi_init(SPI_PORT, 100000);
  spi_set_format(SPI_PORT, 8, (spi_cpol_t)0, (spi_cpha_t)0, SPI_MSB_FIRST);
  gpio_set_function(CLK_PIN, GPIO_FUNC_SPI);
  gpio_set_function(MOSI_PIN, GPIO_FUNC_SPI);
  gpio_set_function(MISO_PIN, GPIO_FUNC_SPI);
  gpio_set_function(CS_PIN, GPIO_FUNC_SPI);
  gpio_init(CS_PIN);
  gpio_set_dir(CS_PIN, GPIO_OUT);
  gpio_put(CS_PIN, 1);

  int chan = 0;
  uint8_t buffer[3];
  buffer[0] = 1;
  buffer[1] = (8 + chan) << 4;
  buffer[2] = 0;

  while(true) {
    cs_select();
    sleep_ms(10);

    uint8_t returnData[10];
    spi_write_read_blocking(SPI_PORT, buffer, returnData, sizeof(buffer));
    int data = ( (returnData[1]&3) << 8 ) | returnData[2];
    printf("WIND DIRECTION DATA = %d\n", data);
    cs_deselect();
    sleep_ms(10);

    getDirectionFromADCValue(data, windDirectionName);

    printf("%d %s \n", windDirectionADCValue, windDirectionName);

    sleep_ms(100);

    vTaskDelay(1000);
  }
}

int main( void )
{
    stdio_init_all();

    xTaskCreate(wind_direction_task, "WindDirectionTask", 1024, nullptr, MAIN_TASK_PRIORITY, nullptr);

    vTaskStartScheduler();

    return 0;
}
