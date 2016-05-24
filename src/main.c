/**
 * @file   main.c
 * @brief  Application main.
 *
 * @author Cerevo Inc.
 */

/*
Copyright 2015 Cerevo Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
/* MCU support. */
#include "TZ10xx.h"
#include "PMU_TZ10xx.h"
#include "GPIO_TZ10xx.h"
#include "SPI_TZ10xx.h"
#include "Driver_I2C.h"
#include "TMR_TZ10xx.h"
/* Board support. */
#include "TZ01_system.h"
#include "TZ01_console.h"
#include "MPU-9250.h"
#include "BMP280.h"
#include "utils.h"

#include "ble.h"

#include "config.h"

HYRWGN_CONFIG gConfig;

static uint8_t buf[10];
static uint8_t msg[80];
static uint64_t uniq;

extern TZ10XX_DRIVER_PMU  Driver_PMU;
extern TZ10XX_DRIVER_GPIO Driver_GPIO;
extern TZ10XX_DRIVER_SPI  Driver_SPI3;  //9軸モーションセンサー
extern ARM_DRIVER_I2C     Driver_I2C1;  //気圧センサー
extern ARM_DRIVER_I2C     Driver_I2C2;  //充電IC
extern TZ10XX_DRIVER_TMR  Driver_TMR0;
static bool init(void)
{
    uint8_t id = 0;
    uint32_t val;
    
    Usleep(500000);
    if (BLE_init_dev() == 1) {
        return false;
    }
    //TZ01_system_init();
    
    /* PowerControl */
    //PowerLED
    Driver_GPIO.Configure(TZ01_SYSTEM_PWSW_PORT_LED, GPIO_DIRECTION_OUTPUT_2MA, GPIO_EVENT_DISABLE, NULL);
    //PowerHold
    Driver_GPIO.Configure(TZ01_SYSTEM_PWSW_PORT_HLD, GPIO_DIRECTION_OUTPUT_2MA, GPIO_EVENT_DISABLE, NULL);
    //PowerSW
    Driver_PMU.StandbyInputBuffer(TZ01_SYSTEM_PWSW_PORT_SW, 0);
    Driver_GPIO.Configure(TZ01_SYSTEM_PWSW_PORT_SW,  GPIO_DIRECTION_INPUT_HI_Z, GPIO_EVENT_DISABLE, NULL);
    //LowVoltDetection
    Driver_PMU.StandbyInputBuffer(TZ01_SYSTEM_PWSW_PORT_UVD, 0);
    Driver_GPIO.Configure(TZ01_SYSTEM_PWSW_PORT_UVD, GPIO_DIRECTION_INPUT_HI_Z, GPIO_EVENT_DISABLE, NULL);
    
    Driver_GPIO.WritePin(TZ01_SYSTEM_PWSW_PORT_HLD, 1);
    Driver_GPIO.WritePin(TZ01_SYSTEM_PWSW_PORT_LED, 1);
    
    /* TickTimer */
    /* TMR */
    if (Driver_TMR0.Initialize(NULL, 0) == TMR_OK) {
        Driver_TMR0.Configure(32, TMR_COUNT_MODE_FREE_RUN, 1);
    }
    if (Driver_TMR0.IsRunning() == false) {
        Driver_TMR0.PowerControl(ARM_POWER_FULL);
        if (Driver_TMR0.Start(0xfffffffe) == TMR_ERROR) {
            return false;
        }
    }
    
    Driver_PMU.SetPrescaler(PMU_CD_PPIER1, 1);
    TZ01_console_init();
    
    //LED
    Driver_GPIO.Configure(11, GPIO_DIRECTION_OUTPUT_2MA, GPIO_EVENT_DISABLE, NULL); 
    
    /* Peripheral */
    //GPIO
    Driver_PMU.StandbyInputBuffer(PMU_IO_FUNC_GPIO_16, 0);
    Driver_PMU.StandbyInputBuffer(PMU_IO_FUNC_GPIO_17, 0);
    Driver_PMU.StandbyInputBuffer(PMU_IO_FUNC_GPIO_18, 0);
    Driver_PMU.StandbyInputBuffer(PMU_IO_FUNC_GPIO_19, 0);
    Driver_GPIO.Configure(16, GPIO_DIRECTION_INPUT_HI_Z, GPIO_EVENT_DISABLE, NULL);
    Driver_GPIO.Configure(17, GPIO_DIRECTION_INPUT_HI_Z, GPIO_EVENT_DISABLE, NULL);
    Driver_GPIO.Configure(18, GPIO_DIRECTION_INPUT_HI_Z, GPIO_EVENT_DISABLE, NULL);
    Driver_GPIO.Configure(19, GPIO_DIRECTION_INPUT_HI_Z, GPIO_EVENT_DISABLE, NULL);
    Driver_GPIO.Configure(20, GPIO_DIRECTION_OUTPUT_2MA, GPIO_EVENT_DISABLE, NULL);
    Driver_GPIO.Configure(21, GPIO_DIRECTION_OUTPUT_2MA, GPIO_EVENT_DISABLE, NULL);
    Driver_GPIO.Configure(22, GPIO_DIRECTION_OUTPUT_2MA, GPIO_EVENT_DISABLE, NULL);
    Driver_GPIO.Configure(23, GPIO_DIRECTION_OUTPUT_2MA, GPIO_EVENT_DISABLE, NULL);
    
    //9軸モーションセンサー
    if (MPU9250_drv_init(&Driver_SPI3)) {
        MPU9250_drv_start_maesure(MPU9250_BIT_ACCEL_FS_SEL_16G, MPU9250_BIT_GYRO_FS_SEL_2000DPS, MPU9250_BIT_DLPF_CFG_20HZ, MPU9250_BIT_A_DLPFCFG_20HZ);
    } else {
        return false;
    }
    
    //Load config from NOR flash.
    if (!config_init()) {
        TZ01_console_puts("config_init(): failed.\r\n");
        return false;
    }
    if (!config_load()) {
        TZ01_console_puts("config_load(): failed.\r\n");
        return false;
    }
    config_get(&gConfig);
    TZ01_console_puts(gConfig.shortened_local_name);
    TZ01_console_puts("\r\n");
    
    //BLELib init
    BLE_init(0);
    
    TZ01_system_tick_start(SYSTICK_NO_PWSW_CHECK, 100);
    
    return true;
}


static uint16_t pwsw_hist = 0xffff;
int main(void)
{
    uint32_t pin;
    /* Initialize */
    if (init() != true) {
        TZ01_console_puts("init(): failed.\r\n");
        goto term;
    }
    
    int div = Driver_PMU.GetPrescaler(PMU_CD_PPIER0);
    sprintf(msg, "SystemCoreClock=%ld PMU_CD_PPIER0 divider=%d\r\n", SystemCoreClock, div);
    TZ01_console_puts(msg);
    
    for (;;) {
        if (TZ01_system_tick_check_timeout(SYSTICK_NO_PWSW_CHECK)) {
            TZ01_system_tick_start(SYSTICK_NO_PWSW_CHECK, 100);
            Driver_GPIO.ReadPin(TZ01_SYSTEM_PWSW_PORT_SW, &pin);
            pwsw_hist <<= 1;
            pwsw_hist |= (pin & 0x01);
            if ((pwsw_hist & 0x03ff) == 0x0000) {   //10回連続(≒1秒)押されてたら
                break;
            }
        }
        
        if (BLE_main() < 0) {
            TZ01_console_puts("BLE_run() failed.\r\n");
        }
    }
    BLE_stop();
term:
    Driver_GPIO.WritePin(TZ01_SYSTEM_PWSW_PORT_HLD, 0);
    Driver_GPIO.WritePin(TZ01_SYSTEM_PWSW_PORT_LED, 0);
    Driver_GPIO.WritePin(11, 0);
    TZ01_console_puts("Program terminated.\r\n");
    return 0;
}

void WDT_IRQHandler(void)
{
    return;
}
