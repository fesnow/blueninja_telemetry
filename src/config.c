#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "Driver_NOR.h"
#include "TZ10xx.h"
#include "NOR_TZ10xx.h"
#include "SDMAC_TZ10xx.h"
#include "PMU_TZ10xx.h"

#include "config.h"

#include "TZ01_console.h"

extern TZ10XX_DRIVER_PMU    Driver_PMU;
extern TZ10XX_DRIVER_NOR    Driver_NOR0;
extern TZ10XX_DRIVER_SDMAC  Driver_SDMAC;

static HYRWGN_CONFIG stored_config;

static uint8_t msg[80];
static uint8_t val[32];

static bool check_valid(uint8_t *cnf, uint8_t sz)
{
    bool ret = false;
    uint8_t cd = 0;
    //マジックコード確認
    if (memcmp(cnf, CONFIG_MAGIC, 4) == 0) {
        //チェックディジット確認
        for (int i = 0; i < sz; i++) {
            cd ^= cnf[i];
        }
        ret = (cd == 0);
    }
    
    return ret;
}

static bool write_to_addr(HYRWGN_CONFIG *config, uint32_t addr)
{
    int ret;
    uint8_t cd = 0;
    
    ret = Driver_NOR0.EraseSector(addr);
    if (ret != ARM_NOR_OK) {
        sprintf(msg, "Driver_NOR0.EraseSector() failed: %d\r\n", ret);
        TZ01_console_puts(msg);
        return false;
    }
    
    memcpy(val, CONFIG_MAGIC, 4);
    memcpy(&val[4], config->shortened_local_name, sizeof(config->shortened_local_name));
    for (int i = 0; i < 28; i++) {
        cd ^= val[i];
    }
    val[28] = cd;
    val[29] = '\0';
    
    ret = Driver_NOR0.WriteData(addr, val, 29);
    if (ret != ARM_NOR_OK) {
        sprintf(msg, "Driver_NOR0.WriteData() failed.: %d\r\n", ret);
        TZ01_console_puts(msg);
        return false;
    }
    
    return true;
}

bool config_init(void)
{
    int ret;
    
    Driver_PMU.SetPowerDomainState(PMU_PD_FLASH, PMU_PD_MODE_ON);
    
    Driver_PMU.SelectClockSource(PMU_CSM_CPUST, PMU_CLOCK_SOURCE_OSC12M);
    Driver_PMU.SetPrescaler(PMU_CD_SPIC, 4);
    
    ret = Driver_SDMAC.Initialize();
    if (ret != SDMAC_OK) {
        sprintf(msg, "Driver_SDMAC.Initialize() failed: %d\r\n", ret);
        TZ01_console_puts(msg);
        return false;
    }
    Driver_SDMAC.PowerControl(ARM_POWER_FULL);
    
    ret = Driver_NOR0.Initialize(0);
    if (ret != ARM_NOR_OK) {
        sprintf(msg, "Driver_NOR0.Initialize() falied: %d\r\n", ret);
        TZ01_console_puts(msg);
        return false;
    }
    Driver_NOR0.PowerControl(ARM_POWER_FULL);
    
    Driver_NOR0.WriteProtect(NOR_0KB, NOR_TOP);

    return true;
}

bool config_set(HYRWGN_CONFIG *config)
{
    stored_config = *config;
    return true;
}

bool config_get(HYRWGN_CONFIG *config)
{
    *config = stored_config;
    return true;
}

bool config_load(void)
{
    uint8_t *pcnf = (uint8_t *)CONFIG_BASE_ADDR_1;
    uint8_t *scnf = (uint8_t *)CONFIG_BASE_ADDR_2;
    
    bool valid_pcnf = check_valid(pcnf, 29);
    bool valid_scnf = check_valid(scnf, 29);
    
    if (valid_pcnf) {
        /* Primary OK */
        //Primary領域から読み込み
        memset(stored_config.shortened_local_name, 0, sizeof(stored_config.shortened_local_name));
        for (int i = 0; i < 24; i++) {
            stored_config.shortened_local_name[i] = pcnf[i + 4];
            if (pcnf[i + 4] == '\0') {
                break;
            }
        }
        
        if (valid_scnf) {
            /* Secondary OK */
            //なにもしないよ
        } else {
            /* Secondary NG */
            //Secondaryを復旧させるよ
            write_to_addr(&stored_config, CONFIG_BASE_ADDR_2);
        }
    } else {
        if (valid_scnf) {
            /* Secondary OK */
            //Secondary領域から読み込み
            memset(stored_config.shortened_local_name, 0, sizeof(stored_config.shortened_local_name));
            for (int i = 0; i < 24; i++) {
                stored_config.shortened_local_name[i] = scnf[i + 4];
                if (pcnf[i + 4] == '\0') {
                    break;
                }
            }
            
            //Primary領域を復旧させるよ
            write_to_addr(&stored_config, CONFIG_BASE_ADDR_1);
        } else {
            /* Secondary NG */
            //NOR flashのunique idを取得
            uint8_t *nor_manufacturer, *nor_device, *nor_unique;
            Driver_NOR0.GetID(nor_manufacturer, nor_device, nor_unique);
            sprintf(msg, "%02x%02x%02x%02x%02x%02x%02x%02x",
                nor_unique[0], nor_unique[1], nor_unique[2], nor_unique[3],
                nor_unique[4], nor_unique[5], nor_unique[6], nor_unique[7]);
            //デフォルト名:"BNTLM_xxxxxxxxxxxxxxxx"を生成
            memset(stored_config.shortened_local_name, 0, 24);
            strncpy(stored_config.shortened_local_name, DEFAULT_NAME_PREFIX, 6);
            memcpy(&stored_config.shortened_local_name[6], msg, 16);
            //Primary領域もSecondary領域もデフォルト値を書くよ
            if (!write_to_addr(&stored_config, CONFIG_BASE_ADDR_1)) {
                return false;
            }
            if (!write_to_addr(&stored_config, CONFIG_BASE_ADDR_2)) {
                return false;
            }
        }
    }
    
    return true;
}

bool config_save(void)
{
    write_to_addr(&stored_config, CONFIG_BASE_ADDR_1);
    write_to_addr(&stored_config, CONFIG_BASE_ADDR_2);
}
