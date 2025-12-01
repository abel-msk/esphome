#include "sdmmc_file.h"
#include "ff.h"

// static const uint8_t OPEN_READ = 0x01;
// static const uint8_t OPEN_WRITE = 0x02;
// static const uint8_t OPEN_APPEND = 0x04;
// static const uint8_t OPEN_CREATE = 0x08;

namespace esphome {
namespace sdmmc {
using namespace esphome::storage;

static const char *TAG = "sdmmc_file";

const char *fs_errstr(uint8_t errnum) {
  const char *fs_err2str[] = {"(0) Succeeded",
                              "(1) A hard error occurred in the low level disk I/O layer",
                              "(2) Assertion failed",
                              "(3) The physical drive cannot work",
                              "(4) Could not find the file",
                              "(5) Could not find the path",
                              "(6) The path name format is invalid",
                              "(7) Access denied due to prohibited access or directory full",
                              "(8) Access denied due to prohibited access",
                              "(9) The file/directory object is invalid",
                              "(10) The physical drive is write protected",
                              "(11) The logical drive number is invalid",
                              "(12) The volume has no work area",
                              "(13) There is no valid FAT volume",
                              "(14) The f_mkfs() aborted due to any problem",
                              "(15) Could not get a grant to access the volume within defined period",
                              "(16) The operation is rejected according to the file sharing policy",
                              "(17) LFN working buffer could not be allocated",
                              "(18) Number of open files > FF_FS_LOCK",
                              "(19) Given parameter is invalid"};
  return fs_err2str[errnum];
}

/*  ------------------------------------------------------------------
 */
File::File(std::string path, uint8_t mode) {
  uint8_t open_flag_ = 0;
  if (mode & OPEN_READ) {
    open_flag_ = FA_READ;
  } else if (mode & OPEN_WRITE) {
    open_flag_ = FA_WRITE;

    if (mode & OPEN_CREATE) {
      open_flag_ |= FA_CREATE_ALWAYS | FA_CREATE_NEW;  // FA_OPEN_ALWAYS;
    }
    if (mode & OPEN_APPEND) {
      open_flag_ |= FA_OPEN_APPEND;
    }
  } else {
    last_err_ = FR_INVALID_PARAMETER;
    return;
  }
  uint8_t res;
  res = f_open(&fptr_, path.c_str(), open_flag_);
  last_err_ = res;
}

/*  ------------------------------------------------------------------
 */
File::~File() {
  if (!last_err_) {
    f_close(&fptr_);
  }
}

/*  ------------------------------------------------------------------
 */
bool File::seek(int32_t pos) {
  if (!last_err_) {
    last_err_ = f_lseek(&fptr_, pos);
  }
  if (last_err_) {
    return false;
  } else {
    return true;
  }
}

/*  ------------------------------------------------------------------
 */
int32_t File::read(uint8_t *data, size_t length) {
  size_t res = 0;
  if (!last_err_) {
    last_err_ = f_read(&fptr_, data, length, &res);
  }
  return res;
}

/*  ------------------------------------------------------------------
 */

int32_t File::write(uint8_t *data, size_t length) {
  size_t res = 0;
  if (!last_err_) {
    last_err_ = f_write(&fptr_, data, length, &res);
  }
  return res;
}

/*  ------------------------------------------------------------------
 */
uint8_t File::error() { return last_err_; };

/*  ------------------------------------------------------------------
 */
const char *File::error_str() { return fs_errstr(last_err_); }

/*  ------------------------------------------------------------------
 */

Dir::Dir(std::string path) {
  ESP_LOGV(TAG, "Open dir %s", path.c_str());
  last_err_ = f_opendir(&dptr_, path.c_str());
}
/*  ------------------------------------------------------------------
 */
Dir::~Dir() { f_closedir(&dptr_); }
/*  ------------------------------------------------------------------
 */
std::string Dir::next() {
  FILINFO finfo_ptr;
  last_err_ = f_readdir(&dptr_, &finfo_ptr);
  ESP_LOGV(TAG, "next name: %s", finfo_ptr.fname);

  if (finfo_ptr.fname[0] == 0) {
    return std::string("");  //  Mean end of list
  } else {
    return std::string(finfo_ptr.fname);
  }
}
/*  ------------------------------------------------------------------
 */
uint8_t Dir::error() { return last_err_; }
const char *Dir::error_str() { return fs_errstr(last_err_); };

}  // namespace sdmmc
}  // namespace esphome
