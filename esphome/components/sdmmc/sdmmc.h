#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/gpio.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/storage/file_provider.h"
#include "sdmmc_io.h"

#ifdef USE_ESP_IDF

#include "ff.h"
#include "diskio.h"

#if ESP_IDF_VERSION_MAJOR > 3
#include "diskio_impl.h"
#endif

#include "esp_vfs_fat.h"

#endif

namespace esphome {
namespace sdmmc {

using namespace esphome::storage;
using fsys_t = FATFS;

class File;
class Dir;

class SdMMC : public PollingComponent, FileProvider {
 public:
  SdMMC();
  void set_clk_pin(InternalGPIOPin *pin) { clk_pin_ = pin; };
  void set_cmd_pin(InternalGPIOPin *pin) { cmd_pin_ = pin; };
  void set_data0_pin(InternalGPIOPin *pin) { data0_pin_ = pin; };
  void set_data1_pin(InternalGPIOPin *pin) { data1_pin_ = pin; };
  void set_data2_pin(InternalGPIOPin *pin) { data2_pin_ = pin; };
  void set_data3_pin(InternalGPIOPin *pin) { data3_pin_ = pin; };
  void set_mode_1bit(bool mode) { bus_w_1bit = mode; };

  void setup() override;
  void update() override;
  void dump_config() override;

  bool is_dir(std::string path) override;
  size_t get_size(std::string path) override;
  bool get_attr(std::string path, uint8_t attr_name) override;
  void set_attr(std::string path, uint8_t attr_name, bool attr) override;
  FileObj *open_file(std::string path, uint8_t mode) override;
  DirObj *open_dir(std::string path) override;
  bool rename(std::string from_path, std::string to_path) override;
  bool del(std::string path) override;
  bool mk_dir(std::string path) override;
  uint8_t error() override;
  const char *error_str() override;

 protected:
  InternalGPIOPin *clk_pin_ = {nullptr};
  InternalGPIOPin *cmd_pin_ = {nullptr};
  InternalGPIOPin *data0_pin_ = {nullptr};
  InternalGPIOPin *data1_pin_ = {nullptr};
  InternalGPIOPin *data2_pin_ = {nullptr};
  InternalGPIOPin *data3_pin_ = {nullptr};
  bool bus_w_1bit = true;
  FILINFO finfo;
  uint8_t last_err_ = 0;
  SdmmcIO *mmc_io_ = {nullptr};
  SdCardStatus card_status_ = SdCardStatus::RET_STATUS_NOCARD;
  fsys_t *fs_ = {nullptr};
  std::string mount_point_ = std::string("/sdmmc");
};

}  // namespace sdmmc
}  // namespace esphome
