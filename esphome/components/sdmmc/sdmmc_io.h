#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#if defined(SOC_SDMMC_HOST_SUPPORTED)
// #include "ffconf.h"
#include "driver/sdmmc_host.h"
// #include "driver/sdmmc_types.h"  //  Req for card_int_struct
#include "ff.h"
#include "esp_vfs_fat.h"

#if defined(USE_ARDUINO)
#include "dirent.h"
#include "esp_system.h"
#include "SD_MMC.h"
#include "sdmmc_io.h"
#endif

namespace esphome {
namespace sdmmc {

// typedef enum { SDMMC_RET_STATUS_OK = 0, SDMMC_RET_STATUS_FAIL = 1, SDMMC_RET_STATUS_NOCARD = 2 } init_status_t;

// typedef struct {
//   // sdfs::SpiConnector *conn;
//   int frequency;
//   char *base_path;
//   // SdCardType type;
//   unsigned long sectors;
//   bool supports_crc;
//   int status;
// } ardu_sdmmc_card_t;

#define FREE(ptr) \
  do { \
    free(ptr); \
    (ptr) = NULL; \
  } while (0)

enum class SdCardStatus : uint8_t { RET_STATUS_OK = 0, RET_STATUS_FAIL = 1, RET_STATUS_NOCARD = 2 };

class SdmmcIO {
 public:
  SdmmcIO();

  void set_bus_width(uint8_t);
  // void set_bus_slot(uint8_t);

  void set_clk_pin(gpio_num_t);
  void set_cmd_pin(gpio_num_t);
  void set_data0_pin(gpio_num_t);
  void set_data1_pin(gpio_num_t);
  void set_data2_pin(gpio_num_t);
  void set_data3_pin(gpio_num_t);

  bool init();
  bool init_slot();
  SdCardStatus init_card();
  SdCardStatus get_disk_status();
  FATFS *mount(std::string);
  bool format();
  void unmount();
  uint8_t get_pdrv();
  bool is_card_mem();
  bool is_card_sdio();
  bool is_card_mmc();

  size_t sectors();      // card_info_->csd->capacity
  size_t sector_size();  // card_info_->csd->sector_size

 protected:
  sdmmc_host_t *host_config_{NULL};
  sdmmc_slot_config_t *slot_config_{NULL};
  sdmmc_card_t *card_info_{NULL};
  esp_vfs_fat_mount_config_t *mount_config_{NULL};
  std::string mountpoint_;
  uint8_t pdrv_;
  uint32_t last_err_;
  FATFS *fs_ = NULL;
  uint8_t spi_bus_width_ = 1;
  uint8_t bus_slot_ = 1;

  // uint8_t wp_pin_ = GPIO_NUM_NC;
  // uint8_t cd_pin_ = GPIO_NUM_NC;
  gpio_num_t clk_pin_ = GPIO_NUM_NC;
  gpio_num_t cmd_pin_ = GPIO_NUM_NC;
  gpio_num_t data0_pin_ = GPIO_NUM_NC;
  gpio_num_t data1_pin_ = GPIO_NUM_NC;
  gpio_num_t data2_pin_ = GPIO_NUM_NC;
  gpio_num_t data3_pin_ = GPIO_NUM_NC;
  // uint8_t data4_pin_ = GPIO_NUM_NC;
  // uint8_t data5_pin_ = GPIO_NUM_NC;
  // uint8_t data6_pin_ = GPIO_NUM_NC;
  // uint8_t data7_pin_ = GPIO_NUM_NC;
};

}  // namespace sdmmc
}  // namespace esphome
#endif
