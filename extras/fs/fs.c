// #include "fs.h"
// #include "esp_spiffs.h"

// void fs_init(void)
// {
//     esp_vfs_spiffs_conf_t conf = {
//         .base_path = "/spiffs",
//         .partition_label = NULL,
//         .max_files = 5,
//         .format_if_mount_failed = true
//     };

//     esp_err_t ret = esp_vfs_spiffs_register(&conf);

//     if (ret != ESP_OK) {
//         printf("SPIFFS mount failed\n");
//     } else {
//         printf("SPIFFS mounted\n");
//     }
// }


#include "fs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "wear_levelling.h"

#define MOUNT_POINT "/fatfs"

static wl_handle_t wl_handle;

void fs_init(void)
{
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 5,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_fat_spiflash_mount(
        MOUNT_POINT,
        "storage",
        &mount_config,
        &wl_handle
    );

    if (ret != ESP_OK) {
        printf("FATFS mount failed: %s\n", esp_err_to_name(ret));
    } else {
        printf("FATFS mounted at %s\n", MOUNT_POINT);
    }
}