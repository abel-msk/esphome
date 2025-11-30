
#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/storage/file_provider.h"
#include "sdmmc.h"
#include "ff.h"

namespace esphome {
namespace sdmmc {
using namespace esphome::storage;
using fptr_t = FIL;
using dptr_t = FF_DIR;

const char *fs_errstr(uint8_t errnum);

class File : public FileObj {
 public:
  File(std::string path, uint8_t mode = FA_READ);
  ~File();
  bool seek(int32_t pos) override;
  int32_t read(uint8_t *data, size_t length) override;
  int32_t write(uint8_t *data, size_t length) override;
  uint8_t error() override;
  const char *error_str() override;

 protected:
  fptr_t fptr_;
  uint8_t last_err_ = 0;
  uint8_t open_flag_ = 0;
};

class Dir : public DirObj {
 public:
  Dir(std::string path);
  ~Dir();
  std::string next() override;
  uint8_t error() override;
  const char *error_str() override;

 protected:
  dptr_t dptr_;
  uint8_t last_err_ = 0;
};

}  // namespace sdmmc
}  // namespace esphome
