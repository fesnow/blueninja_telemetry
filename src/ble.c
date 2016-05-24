/**
 * @file BLE.c
 * @breaf Cerevo CDP-TZ01B Instant firmware.
 * BLE
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
#include "TZ10xx.h"
#include "PMU_TZ10xx.h"
#include "GPIO_TZ10xx.h"
#include "RNG_TZ10xx.h"

#include "twic_interface.h"
#include "blelib.h"

#include "TZ01_system.h"
#include "TZ01_console.h"
#include "MPU-9250.h"
#include "config.h"

extern HYRWGN_CONFIG gConfig;

typedef union {
    float   val;
    uint8_t buf[4];
}   FLOAT_BUF;

#define BNMSG_MTU    (30)
static uint16_t current_mtu = 23;

extern TZ10XX_DRIVER_PMU  Driver_PMU;
extern TZ10XX_DRIVER_RNG  Driver_RNG;
extern TZ10XX_DRIVER_GPIO Driver_GPIO;

static uint8_t msg[80];

static uint64_t hrgn_bdaddr  = 0xe26e00000000;   //BDアドレスの固定部分を定義

static void init_io_state(void);

/*--- GATT profile definition ---*/
uint8_t bnmsg_gap_device_name[] = "BlueNinja Telemetory";
uint8_t bnmsg_gap_appearance[] = {0x00, 0x00};

const uint8_t bnmsg_di_manufname[] = "Cerevo";
const uint8_t bnmsg_di_fw_version[] = "0.1";
const uint8_t bnmsg_di_sw_version[] = "0.1";
const uint8_t bnmsg_di_model_string[] = "CDP-TZ01B";

/* BLElib unique id. */
enum {
    GATT_UID_GAP_SERVICE = 0,
    GATT_UID_GAP_DEVICE_NAME,
    GATT_UID_GAP_APPEARANCE,
    
    GATT_UID_DI_SERVICE,
    GATT_UID_DI_MANUF_NAME,
    GATT_UID_DI_FW_VERSION,
    GATT_UID_DI_SW_VERSION,
    GATT_UID_DI_MODEL_STRING,
    /* BlueNinja Telemetry Config Service */
    GATT_UID_CONFIG_SERVICE,
    GATT_UID_CONFIG_NAME,
    /* BlueNinja Telemetry Motion sensor Service */
    GATT_UID_MOTION_SERVICE,
    GATT_UID_MOTION_INTERVAL,
    GATT_UID_MOTION,
    GATT_UID_MOTION_DESC,
};

/* GAP */
const BLELib_Characteristics gap_device_name = {
    GATT_UID_GAP_DEVICE_NAME, 0x2a00, 0, BLELIB_UUID_16,
    BLELIB_PROPERTY_READ,
    BLELIB_PERMISSION_READ | BLELIB_PERMISSION_WRITE,
    bnmsg_gap_device_name, sizeof(bnmsg_gap_device_name),
    NULL, 0
};
const BLELib_Characteristics gap_appearance = {
    GATT_UID_GAP_APPEARANCE, 0x2a01, 0, BLELIB_UUID_16,
    BLELIB_PROPERTY_READ,
    BLELIB_PERMISSION_READ,
    bnmsg_gap_appearance, sizeof(bnmsg_gap_appearance),
    NULL, 0
};
const BLELib_Characteristics *const gap_characteristics[] = { &gap_device_name, &gap_appearance };
const BLELib_Service gap_service = {
    GATT_UID_GAP_SERVICE, 0x1800, 0, BLELIB_UUID_16,
    true, NULL, 0,
    gap_characteristics, 2
};

/* DIS(Device Informatin Service) */
const BLELib_Characteristics di_manuf_name = {
    GATT_UID_DI_MANUF_NAME, 0x2a29, 0, BLELIB_UUID_16,
    BLELIB_PROPERTY_READ,
    BLELIB_PERMISSION_READ,
    bnmsg_di_manufname, sizeof(bnmsg_di_manufname),
    NULL, 0
};
const BLELib_Characteristics di_fw_version = {
    GATT_UID_DI_FW_VERSION, 0x2a26, 0, BLELIB_UUID_16,
    BLELIB_PROPERTY_READ,
    BLELIB_PERMISSION_READ,
    bnmsg_di_fw_version, sizeof(bnmsg_di_fw_version),
    NULL, 0
};
const BLELib_Characteristics di_sw_version = {
    GATT_UID_DI_SW_VERSION, 0x2a28, 0, BLELIB_UUID_16,
    BLELIB_PROPERTY_READ,
    BLELIB_PERMISSION_READ,
    bnmsg_di_sw_version, sizeof(bnmsg_di_sw_version),
    NULL, 0
};
const BLELib_Characteristics di_model_string = {
    GATT_UID_DI_MODEL_STRING, 0x2a24, 0, BLELIB_UUID_16,
    BLELIB_PROPERTY_READ,
    BLELIB_PERMISSION_READ,
    bnmsg_di_model_string, sizeof(bnmsg_di_model_string),
    NULL, 0
};
const BLELib_Characteristics *const di_characteristics[] = {
    &di_manuf_name, &di_fw_version, &di_sw_version, &di_model_string
};
const BLELib_Service di_service = {
    GATT_UID_DI_SERVICE, 0x180a, 0, BLELIB_UUID_16,
    true, NULL, 0,
    di_characteristics, 4
};

/* BlueNinja Telemetry config Service */
const BLELib_Characteristics config_char = {
    GATT_UID_CONFIG_NAME, 0xaf25e5b125f0dfe3, 0x0001000143464c6d, BLELIB_UUID_128,
    BLELIB_PROPERTY_READ | BLELIB_PROPERTY_WRITE,
    BLELIB_PERMISSION_READ | BLELIB_PERMISSION_WRITE,
    &gConfig.shortened_local_name, 23,
    NULL, 0
};
//Service
const BLELib_Characteristics *const config_characteristics[] = {
    &config_char
};
const BLELib_Service config_service = {
    GATT_UID_CONFIG_SERVICE, 0xaf25e5b125f0dfe3, 0x0001000043464c6d, BLELIB_UUID_128,
    true, NULL, 0,
    config_characteristics, 1
};

/* BlueNinja Motion sensor Service */
static uint16_t motion_enable_val;
static uint8_t motion_val[18];
static int32_t gx, gy, gz, ax, ay, az;
static uint8_t motion_interval = 2;
//Motion sensor
const BLELib_Characteristics motion_interval_char = {
    GATT_UID_MOTION_INTERVAL, 0xaf25e5b125f0dfe3, 0x0002000143464c6d, BLELIB_UUID_128,
    BLELIB_PROPERTY_READ | BLELIB_PROPERTY_WRITE,
    BLELIB_PERMISSION_READ | BLELIB_PERMISSION_WRITE,
    &motion_interval, 1,
    NULL, 0
};

const BLELib_Descriptor motion_desc = {
    GATT_UID_MOTION_DESC, 0x2902, 0, BLELIB_UUID_16,
    BLELIB_PERMISSION_READ | BLELIB_PERMISSION_WRITE,
    (uint8_t *)&motion_enable_val, sizeof(motion_enable_val)
};
const BLELib_Descriptor *const motion_descs[] = { &motion_desc };
const BLELib_Characteristics motion_char = {
    GATT_UID_MOTION, 0xaf25e5b125f0dfe3, 0x0002000243464c6d, BLELIB_UUID_128,
    BLELIB_PROPERTY_NOTIFY,
    BLELIB_PERMISSION_READ,
    motion_val, sizeof(motion_val),
    motion_descs, 1
};
//Service
const BLELib_Characteristics *const motion_characteristics[] = {
    &motion_interval_char, &motion_char
};
const BLELib_Service motion_service = {
    GATT_UID_MOTION_SERVICE, 0xaf25e5b125f0dfe3, 0x0002000043464c6d, BLELIB_UUID_128,
    true, NULL, 0,
    motion_characteristics, 2
};

/* Service list */
const BLELib_Service *const hrgn_service_list[] = {
    &gap_service, &di_service, &config_service, &motion_service
};

/*- INDICATION data -*/
uint8_t bnmsg_advertising_data[] = {
    0x02, /* length of this data */
    0x01, /* AD type = Flags */
    0x06, /* LE General Discoverable Mode = 0x02 */
    /* BR/EDR Not Supported (i.e. bit 37
     * of LMP Extended Feature bits Page 0) = 0x04 */

    0x17, /* length of this data */
    0x09, /* AD type = Complete local name */
    /* HyouRowGan00 */
     'B',  'N',  'T',  'L',  'M',  '_',  '0',  '0',  '0',  '0',
     '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0',  '0',
     '0',  '0',};

uint8_t bnmsg_scan_resp_data[] = {
    0x02, /* length of this data */
    0x01, /* AD type = Flags */
    0x06, /* LE General Discoverable Mode = 0x02 */
    /* BR/EDR Not Supported (i.e. bit 37
     * of LMP Extended Feature bits Page 0) = 0x04 */

    0x02, /* length of this data */
    0x0A, /* AD type = TX Power Level (1 byte) */
    0x00, /* 0dB (-127...127 = 0x81...0x7F) */

    0x11, /* length of this data */
    0x07, /* AD type = Complete list of 128-bit UUIDs available */
    0xe3, 0xdf, 0xf0, 0x25, 0xb1, 0xe5, 0x25, 0xaf, 
    0x6d, 0x4c, 0x46, 0x43, 0x00, 0x00, 0x01, 0x00, 
};

/*=== BlueNinja messenger application ===*/
static uint64_t central_bdaddr;

/*= BLElib callback functions =*/
void connectionCompleteCb(const uint8_t status, const bool master, const uint64_t bdaddr, const uint16_t conn_interval)
{
    central_bdaddr = bdaddr;

    BLELib_requestMtuExchange(BNMSG_MTU);

    TZ01_system_tick_start(USRTICK_NO_BLE_MAIN, 100);
}

void connectionUpdateCb(const uint8_t status, const uint16_t conn_interval, const uint16_t conn_latency)
{
}

void disconnectCb(const uint8_t status, const uint8_t reason)
{
    init_io_state();
    
    Driver_GPIO.WritePin(10, 1);
    TZ01_system_tick_stop(USRTICK_NO_BLE_MAIN);
}

BLELib_RespForDemand mtuExchangeDemandCb(const uint16_t client_rx_mtu_size, uint16_t *resp_mtu_size)
{
    *resp_mtu_size = BNMSG_MTU;
    sprintf(msg, "%s(): client_rx_mtu_size=%d resp_mtu_size=%d\r\n", __func__, client_rx_mtu_size, *resp_mtu_size);
    TZ01_console_puts(msg);
    return BLELIB_DEMAND_ACCEPT;
}

void mtuExchangeResultCb(const uint8_t status, const uint16_t negotiated_mtu_size)
{
    sprintf(msg, "Negotiated MTU size:%d\r\n", negotiated_mtu_size);
    current_mtu = negotiated_mtu_size;
    TZ01_console_puts(msg);
}

void notificationSentCb(const uint8_t unique_id)
{
}

void indicationConfirmCb(const uint8_t unique_id)
{
}

void updateCompleteCb(const uint8_t unique_id)
{
}

void queuedWriteCompleteCb(const uint8_t status)
{
}

BLELib_RespForDemand readoutDemandCb(const uint8_t *const unique_id_array, const uint8_t unique_id_num)
{
    return BLELIB_DEMAND_ACCEPT;
}

BLELib_RespForDemand writeinDemandCb(const uint8_t unique_id, const uint8_t *const value, const uint8_t value_len)
{
    uint32_t  clock;
    FLOAT_BUF duty;
    uint8_t len;
    uint8_t pin;
    BLELib_RespForDemand ret = BLELIB_DEMAND_REJECT;
    
    switch (unique_id) {
    case GATT_UID_CONFIG_NAME:
        if ((value_len < 1) || (value_len > 23)) {
            ret = BLELIB_DEMAND_REJECT;
            break;
        }
        memset(&gConfig.shortened_local_name, 0, sizeof(gConfig.shortened_local_name));
        memcpy(&gConfig.shortened_local_name, value, value_len);
        config_set(&gConfig);
        config_save();
        BLELib_updateValue(GATT_UID_CONFIG_NAME, gConfig.shortened_local_name, value_len);
        ret = BLELIB_DEMAND_ACCEPT;
        break;
    case GATT_UID_MOTION_INTERVAL:
        if ((value[0] < 1) || (value[0] > 10)) {
            ret = BLELIB_DEMAND_REJECT;
            break;
        }
        motion_interval = value[0];
        BLELib_updateValue(GATT_UID_MOTION_INTERVAL, &motion_interval, 1);
        ret = BLELIB_DEMAND_ACCEPT;
        break;
    case GATT_UID_MOTION_DESC:
        motion_enable_val = value[0] | (value[1] << 8);
        ret = BLELIB_DEMAND_ACCEPT;
        break;
    }
    
    return ret;
}

void writeinPostCb(const uint8_t unique_id, const uint8_t *const value, const uint8_t value_len)
{
}

void isrNewEventCb(void)
{
    /* this sample always call BLELib_run() */
}

void isrWakeupCb(void)
{
    /* this callback is not used currently */
}

BLELib_CommonCallbacks tz01_common_callbacks = {
    connectionCompleteCb,
    connectionUpdateCb,
    mtuExchangeResultCb,
    disconnectCb,
    NULL,
    isrNewEventCb,
    isrWakeupCb
};

BLELib_ServerCallbacks tz01_server_callbacks = {
    mtuExchangeDemandCb,
    notificationSentCb,
    indicationConfirmCb,
    updateCompleteCb,
    queuedWriteCompleteCb,
    readoutDemandCb,
    writeinDemandCb,
    writeinPostCb,
};

/** Global **/

int BLE_init_dev(void)
{
    if (TZ1EM_STATUS_OK != tz1emInitializeSystem())
        return 1; /* Must not use UART for LOG before twicIfLeIoInitialize. */
        
    /* create random bdaddr */
    uint32_t randval;
    Driver_PMU.SetPowerDomainState(PMU_PD_ENCRYPT, PMU_PD_MODE_ON);
    Driver_RNG.Initialize();
    Driver_RNG.PowerControl(ARM_POWER_FULL);
    Driver_RNG.Read(&randval);
    Driver_RNG.Uninitialize();
    hrgn_bdaddr |= (uint64_t)randval;
    
    return 0;
}

int BLE_init(uint8_t id)
{
    if (id > 3) {
        return 1;   //invalid id
    }
    
    memcpy(&bnmsg_advertising_data[5], gConfig.shortened_local_name, 22);
    
    //GPIO等ペリフェラルを初期状態に設定
    init_io_state();
    
    return 0;
}

static void ble_online_motion_sample(void)
{
    MPU9250_gyro_val  gyro;
    MPU9250_accel_val acel;
    
    if (MPU9250_drv_read_gyro(&gyro)) {
        gx += (int16_t)gyro.raw_x;
        gy += (int16_t)gyro.raw_y;
        gz += (int16_t)gyro.raw_z;
    }
    if (MPU9250_drv_read_accel(&acel)) {
        ax += (int16_t)acel.raw_x;
        ay += (int16_t)acel.raw_y;
        az += (int16_t)acel.raw_z;
    }
}

static void ble_online_motion_average(void)
{
    int16_t ave;
    int16_t div;
    
    MPU9250_magnetometer_val magm;
    
    div = motion_interval;

    //Gyro: X
    ave = gx / div;
    memcpy(&motion_val[0], &ave, 2);
    //Gyro: Y
    ave = gy / div;
    memcpy(&motion_val[2], &ave, 2);
    //Gyro: Z
    ave = gz / div;
    memcpy(&motion_val[4], &ave, 2);
    //Accel: X
    ave = ax / div;
    memcpy(&motion_val[6], &ave, 2);
    //Accel: Y
    ave = ay / div;
    memcpy(&motion_val[8], &ave, 2);
    //Accel: Z
    ave = az / div;
    memcpy(&motion_val[10], &ave, 2);
    
    gx = 0;
    gy = 0;
    gz = 0;
    ax = 0;
    ay = 0;
    az = 0;
    
    /* Magnetometer */
    if (MPU9250_drv_read_magnetometer(&magm)) {
        //Magnetometer: X
        memcpy(&motion_val[12], &magm.raw_x, 2);
        //Magnetometer: Y
        memcpy(&motion_val[14], &magm.raw_y, 2);
        //Magnetometer: Z
        memcpy(&motion_val[16], &magm.raw_z, 2);
    } else {
        TZ01_console_puts("MPU9250_drv_read_magnetometer() failed.\r\n");
    }
}

static void ble_online_motion_notify(void)
{
    int ret;
    int val_len;
    
    val_len = 18;
    //計測結果が保持られてる
    ret = BLELib_notifyValue(GATT_UID_MOTION, motion_val, val_len);
    if (ret != BLELIB_OK) {
        sprintf(msg, "GATT_UID_MOTION: Notify failed. ret=%d\r\n", ret);
        TZ01_console_puts(msg);
    }
}

static bool is_adv = false;
static bool is_reg = false;
static uint8_t  led_blink1 = 0;
static uint32_t led_blink2 = 0;
static uint32_t cnt = 0;

int BLE_main(void)
{
    int ret, res = 0;
    BLELib_State state;
    bool has_event;
    uint32_t pin;

    state = BLELib_getState();
    has_event = BLELib_hasEvent();

    switch (state) {
        case BLELIB_STATE_UNINITIALIZED:
            is_reg = false;
            is_adv = false;
            TZ01_console_puts("BLELIB_STATE_UNINITIALIZED\r\n");
            ret = BLELib_initialize(hrgn_bdaddr, BLELIB_BAUDRATE_2304, &tz01_common_callbacks, &tz01_server_callbacks, NULL, NULL);
            if (ret != BLELIB_OK) {
                TZ01_console_puts("BLELib_initialize() failed.\r\n");
                return -1;  //Initialize failed
            }
            break;
        case BLELIB_STATE_INITIALIZED:
            if (is_reg == false) {
                TZ01_console_puts("BLELIB_STATE_INITIALIZED\r\n");
                BLELib_setLowPowerMode(BLELIB_LOWPOWER_ON);
                if (BLELib_registerService(hrgn_service_list, 4) == BLELIB_OK) {
                    is_reg = true;
                } else {
                    return -1;  //Register failed
                }
            }
            break;
        case BLELIB_STATE_READY:
            if (is_adv == false) {
                TZ01_console_puts("BLELIB_STATE_READY\r\n");
                ret = BLELib_startAdvertising(bnmsg_advertising_data, sizeof(bnmsg_advertising_data), bnmsg_scan_resp_data, sizeof(bnmsg_scan_resp_data));
                if (ret == BLELIB_OK) {
                    is_adv = true;
                    Driver_GPIO.WritePin(11, 0);
                    cnt = 0;
                }
                
                led_blink1 = 0;
                led_blink2 = 0;
            }
            break;
        case BLELIB_STATE_ADVERTISING:
            is_adv = false;
            break;
        case BLELIB_STATE_ONLINE:
            if (TZ01_system_tick_check_timeout(USRTICK_NO_BLE_MAIN)) {
                TZ01_system_tick_start(USRTICK_NO_BLE_MAIN, 10);
                
                //LED点滅(0, 200, 400, 600, 800ms)
                if ((cnt % 50) == 0) {
                    led_blink1 = (led_blink1 == 0) ? 1 : 0;
                    Driver_GPIO.WritePin(10, led_blink1);
                }
                
                //モーションセンサーサンプリング(100ms毎)
                if ((cnt % 10) == 0) {
                    if (motion_enable_val == 1) {
                        ble_online_motion_sample();
                    }
                }
                
                //モーションセンサ集計
                //モーションセンサ通知
                if ((cnt % (motion_interval * 10)) == 0) {
                    if (motion_enable_val == 1) {
                        ble_online_motion_average();
                        ble_online_motion_notify();
                        Driver_GPIO.WritePin(11, 1);
                        led_blink2 = cnt + 5;
                    }
                }
                
                if ((led_blink2 > 0) && (cnt >= led_blink2)) {
                    led_blink2 = 0;
                    Driver_GPIO.WritePin(11, 0);
                }
                
                cnt++;  //約48日でWrap around.
            }
            break;
        default:
            break;
    }
    
    if (has_event) {
        ret = BLELib_run();
        if (ret != BLELIB_OK) {
            res = -1;
            sprintf(msg, "BLELib_run() ret: %d\r\n", ret);
            TZ01_console_puts(msg);
        }
    }

    return res;
}

void BLE_stop(void)
{
    switch (BLELib_getState()) {
        case BLELIB_STATE_ADVERTISING:
            BLELib_stopAdvertising();
            break;
        case BLELIB_STATE_ONLINE:
            BLELib_disconnect(central_bdaddr);
            break;
        default:
            break;
    }
    BLELib_finalize();
    Driver_GPIO.WritePin(11, 0);
}

/** local **/
static void init_io_state(void)
{
    /* Initialize values. */
    //Motion sensor
    motion_enable_val = 0;
    memset(motion_val, 0, sizeof(motion_val));
}

