#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdint.h>

#define DEFAULT_NAME_PREFIX "BNTLM_"

#define CONFIG_MAGIC        "hrGC"
#define CONFIG_BASE_ADDR_1  (0x000FE000)
#define CONFIG_BASE_ADDR_2  (0x000FF000)

typedef struct {
    char    shortened_local_name[24];   //BLE advertise `Shorten local name'
}   HYRWGN_CONFIG;

#endif

bool config_init(void);
bool config_set(HYRWGN_CONFIG *config);
bool config_get(HYRWGN_CONFIG *config);
bool config_load(void);
bool config_save(void);
