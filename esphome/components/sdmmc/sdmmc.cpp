#include "sdmmc.h"
#include "sdmmc_file.h"

namespace esphome {
namespace sdmmc {

static const char *TAG = "sdmmc";

SdMMC::SdMMC() { this->mmc_io_ = new SdmmcIO(); }

void SdMMC::setup() {
  this->mmc_io_->set_clk_pin(clk_pin_ != NULL ? static_cast<gpio_num_t>(clk_pin_->get_pin()) : GPIO_NUM_NC);
  this->mmc_io_->set_cmd_pin(cmd_pin_ != NULL ? static_cast<gpio_num_t>(cmd_pin_->get_pin()) : GPIO_NUM_NC);
  this->mmc_io_->set_data0_pin(data0_pin_ != NULL ? static_cast<gpio_num_t>(data0_pin_->get_pin()) : GPIO_NUM_NC);
  this->mmc_io_->set_data1_pin(data1_pin_ != NULL ? static_cast<gpio_num_t>(data1_pin_->get_pin()) : GPIO_NUM_NC);
  this->mmc_io_->set_data2_pin(data2_pin_ != NULL ? static_cast<gpio_num_t>(data2_pin_->get_pin()) : GPIO_NUM_NC);
  this->mmc_io_->set_data3_pin(data3_pin_ != NULL ? static_cast<gpio_num_t>(data3_pin_->get_pin()) : GPIO_NUM_NC);
  this->mmc_io_->set_bus_width(bus_w_1bit ? 1 : 4);
  bool ret = this->mmc_io_->init();
  // this->pdrv_ = this->mmc_io_->get_pdrv();
  if (ret) {
    card_status_ = this->mmc_io_->init_card();
    if (card_status_ == SdCardStatus::RET_STATUS_OK) {
      fs_ = this->mmc_io_->mount(mount_point_);
    }
  }
}

void SdMMC::update() {
  // if ((!this->drv_->is_card()) || (this->get_state() != SD_SLOT_ST_MOUNT)) {
  //   this->set_state(SD_SLOT_ST_EMPTY);
  //   fs_ = NULL;
  //   if (this->drv_->attach_card()) {
  //     this->set_state(SD_SLOT_ST_CARD);
  //     card_present_ = true;
  //     if (this->drv_->mount(path_, false)) {
  //       this->set_state(SD_SLOT_ST_MOUNT);
  //     } else {
  //       ESP_LOGW(TAG, "Seems card present but cannot mount. %d", this->drv_->get_last_err());
  //     }
  //   } else {  // attach_card
  //     card_present_ = false;
  //   }
  // }
}

void SdMMC::dump_config() {
  ESP_LOGD(TAG, "SD MMC config");
  ESP_LOGCONFIG(" 1bit bus: ", YESNO(bus_w_1bit));
  ESP_LOGCONFIG(" 4bit bus: ", YESNO(!bus_w_1bit));
  LOG_PIN("  CLK Pin: ", clk_pin_);
  LOG_PIN("  CLK Pin: ", clk_pin_);
  LOG_PIN("  CLK Pin: ", cmd_pin_);
  LOG_PIN("  CLK Pin: ", data0_pin_);
  LOG_PIN("  CLK Pin: ", data1_pin_);
  LOG_PIN("  CLK Pin: ", data2_pin_);
  LOG_PIN("  CLK Pin: ", data3_pin_);
}

bool SdMMC::is_dir(std::string path) {
  last_err_ = f_stat(path.c_str(), &finfo);
  if (last_err_ == FR_OK) {
    return finfo.fattrib & AM_DIR;
  }
  return false;
}

size_t SdMMC::get_size(std::string path) {
  last_err_ = f_stat(path.c_str(), &finfo);
  if (last_err_ == FR_OK) {
    return finfo.fsize;
  }
  return 0;
}

bool SdMMC::get_attr(std::string path, uint8_t attr_name) {
  last_err_ = f_stat(path.c_str(), &finfo);
  if (last_err_ != FR_OK) {
    return false;
  }
  switch (attr_name) {
    case ATTR_HIDDEN:
      break;
    case ATTR_SYSTEM:
      break;
    case ATTR_PROTECTED:
      break;
  }
  last_err_ = FR_INVALID_PARAMETER;
  return false;
}

void SdMMC::set_attr(std::string path, uint8_t attr_name, bool attr) {
  ESP_LOGD(TAG, "set_attr, path=%s, attr=%d", path.c_str(), attr_name);
}

FileObj *SdMMC::open_file(std::string path, uint8_t mode) {
  File *fl = new File(path, mode);
  last_err_ = fl->error();
  return fl;
}

DirObj *SdMMC::open_dir(std::string path) {
  Dir *dir = new Dir(path);
  last_err_ = dir->error();
  return dir;
}

bool SdMMC::rename(std::string from_path, std::string to_path) {
  last_err_ = f_rename(from_path.c_str(), to_path.c_str());
  return last_err_ == FR_OK;
}

bool SdMMC::del(std::string path) {
  last_err_ = f_unlink(path.c_str());
  return last_err_ == FR_OK;
}
bool SdMMC::mk_dir(std::string path) {
  last_err_ = f_mkdir(path.c_str());
  return last_err_ == FR_OK;
}
uint8_t SdMMC::error() { return last_err_; }

const char *SdMMC::error_str() { return fs_errstr(last_err_); }

}  // namespace sdmmc
}  // namespace esphome
