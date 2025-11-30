// #include "sdspi_io.h"
#include "sdmmc_io.h"
#include "esphome/components/logger/logger.h"
#include "esp_vfs_fat.h"
#include "esp_err.h"

#if defined(SOC_SDMMC_HOST_SUPPORTED)
#include "diskio.h"
#if ESP_IDF_VERSION_MAJOR > 3
#include "diskio_impl.h"
#endif

// #ifdef USE_ESP_IDF
#include "diskio_sdmmc.h"
#include "sdmmc_cmd.h"  //  Low level sdmmc IO
// #endif

#include "driver/sdmmc_host.h"
// #include "driver/sdmmc_types.h"

#if defined(USE_ARDUINO)
#include "dirent.h"
#include "esp_system.h"
#include "SD_MMC.h"
#include "sdmmc_io.h"
#include "ff.h"
#endif

namespace esphome {
namespace sdmmc {

// #define FF_DRV_NOT_USED 0xFF
static const char *TAG = "sdmmc_io";

/**********************************************************************
 *
 * @brief Construct a new SdmmcIO object. Provide  interface to low level
 * io operations for MMC connection in ARDUINO framework.
 *
 */
SdmmcIO::SdmmcIO() {
  this->last_err_ = 0;

  ESP_LOGD(TAG, "SdfsHost init");
  //   Allocate mem for SPI HOST_CONFIG
  sdmmc_host_t new_config = SDMMC_HOST_DEFAULT();
  this->host_config_ = (sdmmc_host_t *) calloc(sizeof(sdmmc_host_t), 1);
  if (this->host_config_ == NULL) {
    last_err_ = ESP_ERR_NO_MEM;
    ESP_LOGE(TAG, "Init err. %s", esp_err_to_name(ESP_ERR_NO_MEM));
    return;
  }

  memcpy(this->host_config_, &new_config, sizeof(sdmmc_host_t));
  // this->host_config_ = &new_config;

  this->slot_config_ = (sdmmc_slot_config_t *) calloc(sizeof(sdmmc_slot_config_t), 1);
  if (this->slot_config_ == NULL) {
    last_err_ = ESP_ERR_NO_MEM;
    ESP_LOGE(TAG, "Init err. %s", esp_err_to_name(ESP_ERR_NO_MEM));
    FREE(this->host_config_);
    return;
  }

  //   Allocate mem for Detected  CARD_CONFIG
  this->card_info_ = (sdmmc_card_t *) calloc(sizeof(sdmmc_card_t) + sizeof(sdmmc_host_t), 1);
  if (this->card_info_ == NULL) {
    last_err_ = ESP_ERR_NO_MEM;
    ESP_LOGE(TAG, "Init err. %s", esp_err_to_name(ESP_ERR_NO_MEM));
    FREE(this->host_config_);
    FREE(this->slot_config_);
    return;
  }

  this->mount_config_ = (esp_vfs_fat_mount_config_t *) calloc(sizeof(esp_vfs_fat_mount_config_t), 1);
  if (this->card_info_ == NULL) {
    this->last_err_ = ESP_ERR_NO_MEM;
    ESP_LOGE(TAG, "Init err. %s", esp_err_to_name(ESP_ERR_NO_MEM));
    FREE(this->host_config_);
    FREE(this->slot_config_);
    FREE(this->card_info_);
    return;
  }
  this->mount_config_->format_if_mount_failed = false;
  this->mount_config_->max_files = 5;
  this->mount_config_->allocation_unit_size = 16 * 1024;
}
void SdmmcIO::set_bus_slot(uint8_t slot) { this->bus_slot_ = slot; }
void SdmmcIO::set_bus_width(uint8_t bw) { this->spi_bus_width_ = bw; }

// void SdmmcIO::set_wp_pin(gpio_num_t pin) { this->wp_pin_ = pin; }
// void SdmmcIO::set_cd_pin(gpio_num_t pin) { this->cd_pin_ = pin; }
void SdmmcIO::set_clk_pin(gpio_num_t pin) { this->clk_pin_ = pin; }
void SdmmcIO::set_cmd_pin(gpio_num_t pin) { this->cmd_pin_ = pin; }
void SdmmcIO::set_data0_pin(gpio_num_t pin) { this->data0_pin_ = pin; }
void SdmmcIO::set_data1_pin(gpio_num_t pin) { this->data1_pin_ = pin; }
void SdmmcIO::set_data2_pin(gpio_num_t pin) { this->data2_pin_ = pin; }
void SdmmcIO::set_data3_pin(gpio_num_t pin) { this->data3_pin_ = pin; }

/**********************************************************************
 *
 * @brief   Initialise pins,  SDMMC Host and slot
 *          Note: All pins and bus type should be defined before this call.
 *
 * @return uint8_t
 */
bool SdmmcIO::init() {
  ESP_LOGD(TAG, "Init SDMMC");

  // ESP_LOGD(TAG, "Init SDMMC : SOC_SDMMC_HOST_SUPPORTED");

  esp_err_t rc = ESP_OK;
  uint8_t slot_num;
  uint8_t pdrv;

  // host_config->max_freq_khz = SDMMC_FREQ_HIGHSPEED;
  // rc = (*host_config->init)();

  rc = sdmmc_host_init();
  if (rc != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init SDMMC host (0x%x)", rc);
    return false;
  }

  //   Select  drive for mount.
  rc = ff_diskio_get_drive(&pdrv);
  if (rc != ESP_OK || pdrv == FF_DRV_NOT_USED) {
    if (pdrv == FF_DRV_NOT_USED)
      rc = ESP_ERR_NOT_FOUND;
    else
      last_err_ = rc;
    ESP_LOGE(TAG, "Init err. %s", esp_err_to_name(last_err_));
    return false;
  }

  //
  //   Define bus connection pins
  //

  // this->slot_config_->flags = 0;
  this->slot_config_->flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
  this->slot_config_->width = this->spi_bus_width_;
  // if (this->wp_pin_ != GPIO_NUM_NC) {
  //   this->slot_config_->wp = static_cast<gpio_num_t>(this->wp_pin_);
  //   // gpio_set_pull_mode(this->slot_config_->wp , GPIO_PULLUP_ONLY);
  // } else
  //   this->slot_config_->wp = SDMMC_SLOT_NO_WP;

  // if (this->cd_pin_ != GPIO_NUM_NC) {
  //   this->slot_config_->cd = static_cast<gpio_num_t>(this->cd_pin_);
  //   // gpio_set_pull_mode(slot_config->cd , GPIO_PULLUP_ONLY);
  // } else
  //   this->slot_config_->cd = SDMMC_SLOT_NO_CD;

#ifdef SOC_SDMMC_USE_GPIO_MATRIX
  if ((this->clk_pin_ == GPIO_NUM_NC) || (this->cmd_pin_ == GPIO_NUM_NC) || (this->data0_pin_ == GPIO_NUM_NC)) {
    last_err_ = ESP_ERR_INVALID_ARG;
    ESP_LOGE(TAG, "Init sdmmc. clk_pin, cmd_pin, data0_pin must be defined.");
    return false;
  }
  this->slot_config_->clk = this->clk_pin_;
  this->slot_config_->cmd = this->cmd_pin_;
  this->slot_config_->d0 = this->data0_pin_;

  if (this->spi_bus_width_ != 1) {
    if ((this->data1_pin_ == GPIO_NUM_NC) || (this->data2_pin_ == GPIO_NUM_NC) || (this->data2_pin_ == GPIO_NUM_NC)) {
      ESP_LOGW(TAG, " For 4bit bus width data1_pin_, data2_pin_, data3_pin_ must be defined. Redefine to 1bit");
      this->spi_bus_width_ = 1;
    } else {
      this->slot_config_->d1 = this->data1_pin_;
      this->slot_config_->d2 = this->data2_pin_;
      this->slot_config_->d3 = this->data3_pin_;
    }
  }
  //
  //  Init slot
  //

  // slot_num = this->bus_slot_ == 0 ? SDMMC_HOST_SLOT_0 : SDMMC_HOST_SLOT_1;
  slot_num = SDMMC_HOST_SLOT_1;
  rc = sdmmc_host_init_slot(slot_num, this->slot_config_);
  if (rc != ESP_OK) {
    last_err_ = rc;
    ESP_LOGE(TAG, "Init slot. %s", esp_err_to_name(last_err_));
    return false;
  }
  ESP_LOGI(TAG, "SDMMC Init bus, slot %d, width %d", slot_num, this->slot_config_->width);
  this->host_config_->slot = slot_num;
  this->pdrv_ = pdrv;
  return true;
#else
  return false;
#endif
}

/**********************************************************************
 *
 * @brief  Return low level priver num
 *
 * @return uint8_t
 */
uint8_t SdmmcIO::get_pdrv() { return this->pdrv_; }

/**********************************************************************
 *
 * @brief
 *
 * @return true
 * @return false
 */
bool SdmmcIO::init_slot() {
  uint8_t slot_num = this->bus_slot_ == 0 ? SDMMC_HOST_SLOT_0 : SDMMC_HOST_SLOT_1;
  esp_err_t rc = sdmmc_host_init_slot(slot_num, this->slot_config_);
  if (rc != ESP_OK) {
    last_err_ = rc;
    ESP_LOGE(TAG, "Init slot. %s", esp_err_to_name(last_err_));
    return false;
  }
  ESP_LOGI(TAG, "Reset slot inicialization, slot %d, width %d", slot_num, this->slot_config_->width);
  this->host_config_->slot = slot_num;
  return true;
}

/**********************************************************************
 *
 * @brief   Check if card present
 *
 * @return true
 * @return false
 */
SdCardStatus SdmmcIO::get_disk_status() {  //  is_card
  assert(this->card_info_);
  esp_err_t rc = sdmmc_get_status(this->card_info_);
  if (unlikely(rc != ESP_OK)) {
    last_err_ = rc;
    ESP_LOGE(TAG, "Check status. %s", esp_err_to_name(last_err_));
    return SdCardStatus::RET_STATUS_FAIL;
  }
  return SdCardStatus::RET_STATUS_OK;
}

/**********************************************************************
 *
 * @brief
 *
 * @return init_status_t
 */
SdCardStatus SdmmcIO::init_card() {  //  attach_card
  esp_err_t rc;
  rc = sdmmc_card_init(this->host_config_, this->card_info_);
  if (rc != ESP_OK) {
    this->last_err_ = rc;
    if (rc = ESP_ERR_TIMEOUT) {
      ESP_LOGW(TAG, "Init card. No card. %s", esp_err_to_name(last_err_));
      return SdCardStatus::RET_STATUS_NOCARD;
    } else {
      ESP_LOGE(TAG, "Init card. %s", esp_err_to_name(last_err_));
      return SdCardStatus::RET_STATUS_FAIL;
    }
  }
  return SdCardStatus::RET_STATUS_OK;
}

/**********************************************************************
 *
 * @brief Mount FS
 *
 * @param mountpoint
 * @param format
 * @return FATFS*
 */
FATFS *SdmmcIO::mount(std::string mountpoint) {
  mountpoint_ = mountpoint;
  FATFS *fs = NULL;
  esp_err_t rc = ESP_OK;
  FRESULT mount_rc = FR_OK;
  this->last_err_ = 0;

  ff_diskio_register_sdmmc(this->pdrv_, this->card_info_);
  char drv[3] = {(char) ('0' + this->pdrv_), ':', 0};
  ESP_LOGD(TAG, "Mounting fs. pdrv=%i, drv=%s, path=%s", this->pdrv_, drv, mountpoint_.c_str());

  rc = esp_vfs_fat_register(mountpoint_.c_str(), drv, this->mount_config_->max_files, &fs);
  if ((rc == ESP_ERR_INVALID_STATE) || (rc == ESP_OK)) {
    // it's okay, already registered with VFS
  } else if (rc != ESP_OK) {
    last_err_ = rc;
    ESP_LOGE(TAG, "Register fs err. %s", esp_err_to_name(last_err_));
    esp_vfs_fat_unregister_path(mountpoint_.c_str());
    return NULL;
  }

  mount_rc = f_mount(fs, drv, 1);
  if (mount_rc != FR_OK) {
    last_err_ = mount_rc;
    if ((mount_rc == FR_NO_FILESYSTEM || mount_rc == FR_INT_ERR)) {  // sd card not formated
      ESP_LOGE(TAG, "Cannot mount. No fs on media. rc=%d", mount_rc);
    }
    ESP_LOGE(TAG, "Mount err. rc=%d", mount_rc);
    esp_vfs_fat_unregister_path(mountpoint_.c_str());
    return NULL;
  }

  return fs;
}

/**********************************************************************
 *
 * @brief unmount fs
 *
 * @return true
 * @return false
 */
void SdmmcIO::unmount() {
  esp_err_t rc = ESP_OK;

  //  Unmount filesystem
  char drv[3] = {(char) ('0' + this->pdrv_), ':', 0};
  FRESULT res = f_mount(0, drv, 0);
  if (res != FR_OK) {
    last_err_ = res;
    ESP_LOGE(TAG, "Unmount err. rc=%d", res);
  }

  //  Clear registration
  rc = esp_vfs_fat_unregister_path(mountpoint_.c_str());
  if ((rc != ESP_OK) && (rc != ESP_ERR_INVALID_STATE)) {
    last_err_ = res;
    ESP_LOGE(TAG, "Unmount err. Invalid state. rc=%d", res);
  }

  //  Reinit slot
  uint8_t slot_num = this->bus_slot_ == 0 ? SDMMC_HOST_SLOT_0 : SDMMC_HOST_SLOT_1;
  rc = sdmmc_host_init_slot(slot_num, this->slot_config_);
  if (rc != ESP_OK) {
    last_err_ = rc;
    ESP_LOGE(TAG, "Reset slot err. %s", esp_err_to_name);
    return;
  }
  ESP_LOGI(TAG, "Reset slot inicialization, slot %d, width %d", slot_num, this->slot_config_->width);
  this->host_config_->slot = slot_num;
}

/**********************************************************************
 *
 * @brief format cdcard with fat fs
 *
 * @return true
 * @return false
 */
bool SdmmcIO::format() {
  char drv[3] = {(char) ('0' + this->pdrv_), ':', 0};
  FRESULT res = FR_OK;
  esp_err_t err;
  const size_t workbuf_size = 4096;
  void *workbuf = NULL;
  ESP_LOGW(TAG, "Partitioning card");

  workbuf = ff_memalloc(workbuf_size);
  if (workbuf == NULL) {
    last_err_ = ESP_ERR_NO_MEM;
    ESP_LOGE(TAG, "Format err. %s", esp_err_to_name(last_err_));
    return false;
  }

#ifdef USE_ESP_IDF
  ESP_LOGD(TAG, "Formatting. Disk (f_fdisk)");
  LBA_t plist[] = {100, 0, 0, 0};
  res = f_fdisk(this->pdrv_, plist, workbuf);
  if (res != FR_OK) {
    last_err_ = res;
    ESP_LOGE(TAG, "Format err. rc=%d", res);
    free(workbuf);
    return false;
  }
#endif

  // size_t alloc_unit_size = this->mount_config_->allocation_unit_size;
  // const size_t max_sectors_per_cylinder = 128;
  // const size_t max_size = this->card_info_->csd.sector_size * max_sectors_per_cylinder;
  // alloc_unit_size = MAX(alloc_unit_size, this->card_info_->csd.sector_size);
  // alloc_unit_size = MIN(alloc_unit_size, max_size);

  // ESP_LOGD(TAG, "Prtitioning disk (f_mkfs), Allocation unit size=%d", alloc_unit_size);

#ifdef USE_ESP_IDF
  const MKFS_PARM opt = {(BYTE) FM_ANY, 0, 0, 0, 0};
  // const MKFS_PARM opt = {(BYTE) FM_ANY, 0, 0, 0, alloc_unit_size};
  res = f_mkfs(drv, &opt, workbuf, workbuf_size);
#elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  const MKFS_PARM opt = {(BYTE) FM_ANY, 0, 0, 0, 0};
  res = f_mkfs(drv, &opt, (void *) workbuf, workbuf_size);  // workbuf_size  (UINT)sizeof(work)
#else
  res = f_mkfs(drv, FM_ANY, 0, (void *) workbuf, workbuf_size);  // workbuf_size  (UINT)sizeof(work)
#endif
  // #else
  //   const MKFS_PARM opt = {(BYTE) FM_ANY, 0, 0, 0, alloc_unit_size};
  //   res = f_mkfs(drv, &opt, workbuf, workbuf_size);
  // #endif
  if (res != FR_OK) {
    last_err_ = res;
    ESP_LOGE(TAG, "mkfs err. rc=%d", res);
    free(workbuf);
    return false;
  }

  free(workbuf);
  ESP_LOGD(TAG, "Prtitioning disk %d completed", this->pdrv_);
  return true;
}

bool SdmmcIO::is_card_mem() { return this->card_info_->is_mem == 1; }
bool SdmmcIO::is_card_sdio() { return this->card_info_->is_sdio == 1; }
bool SdmmcIO::is_card_mmc() { return this->card_info_->is_mmc == 1; }
size_t SdmmcIO::sectors() { return this->card_info_->csd.capacity; }
size_t SdmmcIO::sector_size() { return this->card_info_->csd.sector_size; }

}  // namespace sdmmc
}  // namespace esphome
#endif
