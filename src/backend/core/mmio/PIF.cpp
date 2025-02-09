#include <Netplay.hpp>
#include <cassert>
#include <cic_nus_6105/n64_cic_nus_6105.hpp>
#include <core/Mem.hpp>
#include <core/mmio/PIF.hpp>
#include <core/registers/Registers.hpp>
#include <log.hpp>

#define MEMPAK_SIZE 32768

namespace n64 {
void PIF::Reset() {
  movie.Reset();
  joybusDevices = {};
  bootrom = {};
  ram = {};
  std::error_code error;
  if (mempak.is_mapped()) {
    mempak.sync(error);
    if (error) {
      Util::panic("Could not sync {}", mempakPath);
    }
    mempak.unmap();
  }
  if (eeprom.is_mapped()) {
    eeprom.sync(error);
    if (error) {
      Util::panic("Could not sync {}", eepromPath);
    }
    eeprom.unmap();
  }

  mempakOpen = false;
  channel = 0;
}

void PIF::MaybeLoadMempak() {
  if (!mempakOpen) {
    fs::path mempakPath_ = mempakPath;
    if (!savePath.empty()) {
      mempakPath_ = savePath / mempakPath_.filename();
    }
    mempakPath = mempakPath_.replace_extension(".mempak").string();
    std::error_code error;
    if (mempak.is_mapped()) {
      mempak.sync(error);
      if (error) {
        Util::panic("Could not sync {}", mempakPath);
      }
      mempak.unmap();
    }

    auto mempakVec = Util::ReadFileBinary(mempakPath);
    if (mempak.empty()) {
      Util::WriteFileBinary(std::array<u8, MEMPAK_SIZE>{}, mempakPath);
      mempakVec = Util::ReadFileBinary(mempakPath);
    }

    if (mempakVec.size() != MEMPAK_SIZE) {
      Util::panic("Corrupt mempak!");
    }

    mempak = mio::make_mmap_sink(mempakPath, 0, mio::map_entire_file, error);
    if (error) {
      Util::panic("Could not open {}", mempakPath);
    }
    mempakOpen = true;
  }
}

FORCE_INLINE size_t GetSaveSize(SaveType saveType) {
  switch (saveType) {
  case SAVE_NONE:
    return 0;
  case SAVE_EEPROM_4k:
    return 512;
  case SAVE_EEPROM_16k:
    return 2048;
  case SAVE_SRAM_256k:
    return 32768;
  case SAVE_FLASH_1m:
    return 131072;
  default:
    Util::panic("Unknown save type!");
  }
}

void PIF::LoadEeprom(const SaveType saveType, const std::string &path) {
  if (saveType == SAVE_EEPROM_16k || saveType == SAVE_EEPROM_4k) {
    fs::path eepromPath_ = path;
    if (!savePath.empty()) {
      eepromPath_ = savePath / eepromPath_.filename();
    }
    eepromPath = eepromPath_.replace_extension(".eeprom").string();
    std::error_code error;
    if (eeprom.is_mapped()) {
      eeprom.sync(error);
      if (error) {
        Util::panic("Could not sync {}", eepromPath);
      }
      eeprom.unmap();
    }

    eepromSize = GetSaveSize(saveType);

    auto eepromVec = Util::ReadFileBinary(eepromPath);
    if (eepromVec.empty()) {
      std::vector<u8> dummy{};
      dummy.resize(GetSaveSize(saveType));
      Util::WriteFileBinary(dummy, eepromPath);
      eepromVec = Util::ReadFileBinary(eepromPath);
    }

    if (eepromVec.size() != eepromSize) {
      Util::panic("Corrupt eeprom!");
    }

    eeprom = mio::make_mmap_sink(eepromPath, 0, mio::map_entire_file, error);
    if (error) {
      Util::panic("Could not open {}", eepromPath);
    }
  }
}

enum CMDIndexes { CMD_LEN = 0, CMD_RES_LEN, CMD_IDX, CMD_START };

void PIF::CICChallenge() {
  u8 challenge[30];
  u8 response[30];

  // Split 15 bytes into 30 nibbles
  for (int i = 0; i < 15; i++) {
    challenge[i * 2 + 0] = (ram[0x30 + i] >> 4) & 0x0F;
    challenge[i * 2 + 1] = (ram[0x30 + i] >> 0) & 0x0F;
  }

  n64_cic_nus_6105(reinterpret_cast<char *>(challenge), reinterpret_cast<char *>(response), CHL_LEN - 2);

  for (int i = 0; i < 15; i++) {
    ram[0x30 + i] = (response[i * 2] << 4) + response[i * 2 + 1];
  }
}

FORCE_INLINE u8 DataCRC(const u8 *data) {
  u8 crc = 0;
  for (int i = 0; i <= 32; i++) {
    for (int j = 7; j >= 0; j--) {
      const u8 xorVal = ((crc & 0x80) != 0) ? 0x85 : 0x00;

      crc <<= 1;
      if (i < 32) {
        if ((data[i] & (1 << j)) != 0) {
          crc |= 1;
        }
      }

      crc ^= xorVal;
    }
  }

  return crc;
}

#define BCD_ENCODE(x) (((x) / 10) << 4 | ((x) % 10))
#define BCD_DECODE(x) (((x) >> 4) * 10 + ((x) & 15))

void PIF::ProcessCommands(const Mem &mem) {
  const u8 control = ram[63];
  if (control & 1) {
    channel = 0;
    int i = 0;
    while (i < 63) {
      u8 *cmd = &ram[i++];

      if (const u8 cmdlen = cmd[CMD_LEN] & 0x3F; cmdlen == 0 || cmdlen == 0x3D) {
        channel++;
      } else if (cmdlen == 0x3E) {
        break;
      } else if (cmdlen == 0x3F) {
      } else {
        const u8 r = ram[i++];
        if (r == 0xFE) {
          break;
        }
        const u8 reslen = r & 0x3F;
        u8 *res = &ram[i + cmdlen];

        switch (cmd[CMD_IDX]) {
        case 0:
        case 0xff:
          ControllerID(res);
          channel++;
          break;
        case 1:
          if (!ReadButtons(res)) {
            cmd[1] |= 0x80;
          }
          channel++;
          break;
        case 2:
          MempakRead(cmd, res);
          break;
        case 3:
          MempakWrite(cmd, res);
          break;
        case 4:
          EepromRead(cmd, res, mem);
          break;
        case 5:
          EepromWrite(cmd, res, mem);
          break;
        case 6:
          res[0] = 0x00;
          res[1] = 0x10;
          res[2] = 0x80;
          break;
        case 7:
          switch (cmd[CMD_START]) {
          case 0:
          case 1:
          case 3:
            break;
          case 2:
            {
              auto now = std::time(nullptr);
              const auto *gmtm = gmtime(&now);
              res[0] = BCD_ENCODE(gmtm->tm_sec);
              res[1] = BCD_ENCODE(gmtm->tm_min);
              res[2] = BCD_ENCODE(gmtm->tm_hour) + 0x80;
              res[3] = BCD_ENCODE(gmtm->tm_mday);
              res[4] = BCD_ENCODE(gmtm->tm_wday);
              res[5] = BCD_ENCODE(gmtm->tm_mon);
              res[6] = BCD_ENCODE(gmtm->tm_year);
              res[7] = (gmtm->tm_year - 1900) >= 100 ? 1 : 0;
            }
            break;
          default:
            Util::panic("Invalid read RTC block {}", cmd[CMD_START]);
          }
          break;
        case 8:
          break;
        default:
          Util::panic("Invalid PIF command: {:X}", cmd[2]);
        }

        i += cmdlen + reslen;
      }
    }
  }

  if (control & 0x02) {
    CICChallenge();
    ram[63] &= ~2;
  }

  if (control & 0x08) {
    ram[63] &= ~8;
  }

  if (control & 0x30) {
    ram[63] = 0x80;
  }
}

void PIF::MempakRead(const u8 *cmd, u8 *res) {
  MaybeLoadMempak();
  u16 offset = cmd[3] << 8;
  offset |= cmd[4];

  // low 5 bits are the CRC
  // byte crc = offset & 0x1F;
  // offset must be 32-byte aligned
  offset &= ~0x1F;

  switch (GetAccessoryType()) {
  case ACCESSORY_NONE:
    break;
  case ACCESSORY_MEMPACK:
    if (offset <= MEMPAK_SIZE - 0x20) {
      std::copy_n(mempak.begin() + offset, 32, res);
    }
    break;
  case ACCESSORY_RUMBLE_PACK:
    memset(res, 0x80, 32);
    break;
  }

  // CRC byte
  res[32] = DataCRC(res);
}

void PIF::MempakWrite(u8 *cmd, u8 *res) {
  MaybeLoadMempak();
  // First two bytes in the command are the offset
  u16 offset = cmd[3] << 8;
  offset |= cmd[4];

  // low 5 bits are the CRC
  // byte crc = offset & 0x1F;
  // offset must be 32-byte aligned
  offset &= ~0x1F;

  switch (GetAccessoryType()) {
  case ACCESSORY_NONE:
    break;
  case ACCESSORY_MEMPACK:
    if (offset <= MEMPAK_SIZE - 0x20) {
      std::copy_n(cmd + 5, 32, mempak.begin() + offset);
    }
    break;
  case ACCESSORY_RUMBLE_PACK:
    break;
  }
  // CRC byte
  res[0] = DataCRC(&cmd[5]);
}

void PIF::EepromRead(const u8 *cmd, u8 *res, const Mem &mem) const {
  assert(mem.saveType == SAVE_EEPROM_4k || mem.saveType == SAVE_EEPROM_16k);
  if (channel == 4) {
    const u8 offset = cmd[3];
    if ((offset * 8) >= GetSaveSize(mem.saveType)) {
      Util::panic("Out of range EEPROM read! offset: {:02X}", offset);
    }

    std::copy_n(eeprom.begin() + offset * 8, 8, res);
  } else {
    Util::panic("EEPROM read on bad channel {}", channel);
  }
}

void PIF::EepromWrite(const u8 *cmd, u8 *res, const Mem &mem) {
  assert(mem.saveType == SAVE_EEPROM_4k || mem.saveType == SAVE_EEPROM_16k);
  if (channel == 4) {
    const u8 offset = cmd[3];
    if ((offset * 8) >= GetSaveSize(mem.saveType)) {
      Util::panic("Out of range EEPROM write! offset: {:02X}", offset);
    }

    std::copy_n(cmd + 4, 8, eeprom.begin() + offset * 8);

    res[0] = 0; // Error byte, I guess it always succeeds?
  } else {
    Util::panic("EEPROM write on bad channel {}", channel);
  }
}

void PIF::HLE(const bool pal, const CICType cicType) const {
  mem.Write<u32>(regs, PIF_RAM_REGION_START + 0x24, cicSeeds[cicType]);

  switch (cicType) {
  case UNKNOWN_CIC_TYPE:
    Util::warn("Unknown CIC type!");
    break;
  case CIC_NUS_6101:
    regs.Write<u64>(0, 0x0000000000000000);
    regs.Write<u64>(1, 0x0000000000000000);
    regs.Write<u64>(2, 0xFFFFFFFFDF6445CC);
    regs.Write<u64>(3, 0xFFFFFFFFDF6445CC);
    regs.Write<u64>(4, 0x00000000000045CC);
    regs.Write<u64>(5, 0x0000000073EE317A);
    regs.Write<u64>(6, 0xFFFFFFFFA4001F0C);
    regs.Write<u64>(7, 0xFFFFFFFFA4001F08);
    regs.Write<u64>(8, 0x00000000000000C0);
    regs.Write<u64>(9, 0x0000000000000000);
    regs.Write<u64>(10, 0x0000000000000040);
    regs.Write<u64>(11, 0xFFFFFFFFA4000040);
    regs.Write<u64>(12, 0xFFFFFFFFC7601FAC);
    regs.Write<u64>(13, 0xFFFFFFFFC7601FAC);
    regs.Write<u64>(14, 0xFFFFFFFFB48E2ED6);
    regs.Write<u64>(15, 0xFFFFFFFFBA1A7D4B);
    regs.Write<u64>(16, 0x0000000000000000);
    regs.Write<u64>(17, 0x0000000000000000);
    regs.Write<u64>(18, 0x0000000000000000);
    regs.Write<u64>(19, 0x0000000000000000);
    regs.Write<u64>(20, 0x0000000000000001);
    regs.Write<u64>(21, 0x0000000000000000);
    regs.Write<u64>(23, 0x0000000000000001);
    regs.Write<u64>(24, 0x0000000000000002);
    regs.Write<u64>(25, 0xFFFFFFFF905F4718);
    regs.Write<u64>(26, 0x0000000000000000);
    regs.Write<u64>(27, 0x0000000000000000);
    regs.Write<u64>(28, 0x0000000000000000);
    regs.Write<u64>(29, 0xFFFFFFFFA4001FF0);
    regs.Write<u64>(30, 0x0000000000000000);
    regs.Write<u64>(31, 0xFFFFFFFFA4001550);

    regs.lo = 0xFFFFFFFFBA1A7D4Bll;
    regs.hi = 0xFFFFFFFF997EC317ll;
    break;
  case CIC_NUS_7102:
    regs.Write<u64>(0, 0x0000000000000000);
    regs.Write<u64>(1, 0x0000000000000001);
    regs.Write<u64>(2, 0x000000001E324416);
    regs.Write<u64>(3, 0x000000001E324416);
    regs.Write<u64>(4, 0x0000000000004416);
    regs.Write<u64>(5, 0x000000000EC5D9AF);
    regs.Write<u64>(6, 0xFFFFFFFFA4001F0C);
    regs.Write<u64>(7, 0xFFFFFFFFA4001F08);
    regs.Write<u64>(8, 0x00000000000000C0);
    regs.Write<u64>(9, 0x0000000000000000);
    regs.Write<u64>(10, 0x0000000000000040);
    regs.Write<u64>(11, 0xFFFFFFFFA4000040);
    regs.Write<u64>(12, 0x00000000495D3D7B);
    regs.Write<u64>(13, 0xFFFFFFFF8B3DFA1E);
    regs.Write<u64>(14, 0x000000004798E4D4);
    regs.Write<u64>(15, 0xFFFFFFFFF1D30682);
    regs.Write<u64>(16, 0x0000000000000000);
    regs.Write<u64>(17, 0x0000000000000000);
    regs.Write<u64>(18, 0x0000000000000000);
    regs.Write<u64>(19, 0x0000000000000000);
    regs.Write<u64>(20, 0x0000000000000000);
    regs.Write<u64>(21, 0x0000000000000000);
    regs.Write<u64>(22, 0x000000000000003F);
    regs.Write<u64>(23, 0x0000000000000007);
    regs.Write<u64>(24, 0x0000000000000000);
    regs.Write<u64>(25, 0x0000000013D05CAB);
    regs.Write<u64>(26, 0x0000000000000000);
    regs.Write<u64>(27, 0x0000000000000000);
    regs.Write<u64>(28, 0x0000000000000000);
    regs.Write<u64>(29, 0xFFFFFFFFA4001FF0);
    regs.Write<u64>(30, 0x0000000000000000);
    regs.Write<u64>(31, 0xFFFFFFFFA4001554);

    regs.lo = 0xFFFFFFFFF1D30682ll;
    regs.hi = 0x0000000010054A98;
    break;
  case CIC_NUS_6102_7101:
    regs.Write<u64>(0, 0x0000000000000000);
    regs.Write<u64>(1, 0x0000000000000001);
    regs.Write<u64>(2, 0x000000000EBDA536);
    regs.Write<u64>(3, 0x000000000EBDA536);
    regs.Write<u64>(4, 0x000000000000A536);
    regs.Write<u64>(5, 0xFFFFFFFFC0F1D859);
    regs.Write<u64>(6, 0xFFFFFFFFA4001F0C);
    regs.Write<u64>(7, 0xFFFFFFFFA4001F08);
    regs.Write<u64>(8, 0x00000000000000C0);
    regs.Write<u64>(9, 0x0000000000000000);
    regs.Write<u64>(10, 0x0000000000000040);
    regs.Write<u64>(11, 0xFFFFFFFFA4000040);
    regs.Write<u64>(12, 0xFFFFFFFFED10D0B3);
    regs.Write<u64>(13, 0x000000001402A4CC);
    regs.Write<u64>(14, 0x000000002DE108EA);
    regs.Write<u64>(15, 0x000000003103E121);
    regs.Write<u64>(16, 0x0000000000000000);
    regs.Write<u64>(17, 0x0000000000000000);
    regs.Write<u64>(18, 0x0000000000000000);
    regs.Write<u64>(19, 0x0000000000000000);
    regs.Write<u64>(20, 0x0000000000000001);
    regs.Write<u64>(21, 0x0000000000000000);
    regs.Write<u64>(23, 0x0000000000000000);
    regs.Write<u64>(24, 0x0000000000000000);
    regs.Write<u64>(25, 0xFFFFFFFF9DEBB54F);
    regs.Write<u64>(26, 0x0000000000000000);
    regs.Write<u64>(27, 0x0000000000000000);
    regs.Write<u64>(28, 0x0000000000000000);
    regs.Write<u64>(29, 0xFFFFFFFFA4001FF0);
    regs.Write<u64>(30, 0x0000000000000000);
    regs.Write<u64>(31, 0xFFFFFFFFA4001550);

    regs.hi = 0x000000003FC18657;
    regs.lo = 0x000000003103E121;

    if (pal) {
      regs.Write<u64>(20, 0x0000000000000000);
      regs.Write<u64>(23, 0x0000000000000006);
      regs.Write<u64>(31, 0xFFFFFFFFA4001554);
    }
    break;
  case CIC_NUS_6103_7103:
    regs.Write<u64>(0, 0x0000000000000000);
    regs.Write<u64>(1, 0x0000000000000001);
    regs.Write<u64>(2, 0x0000000049A5EE96);
    regs.Write<u64>(3, 0x0000000049A5EE96);
    regs.Write<u64>(4, 0x000000000000EE96);
    regs.Write<u64>(5, 0xFFFFFFFFD4646273);
    regs.Write<u64>(6, 0xFFFFFFFFA4001F0C);
    regs.Write<u64>(7, 0xFFFFFFFFA4001F08);
    regs.Write<u64>(8, 0x00000000000000C0);
    regs.Write<u64>(9, 0x0000000000000000);
    regs.Write<u64>(10, 0x0000000000000040);
    regs.Write<u64>(11, 0xFFFFFFFFA4000040);
    regs.Write<u64>(12, 0xFFFFFFFFCE9DFBF7);
    regs.Write<u64>(13, 0xFFFFFFFFCE9DFBF7);
    regs.Write<u64>(14, 0x000000001AF99984);
    regs.Write<u64>(15, 0x0000000018B63D28);
    regs.Write<u64>(16, 0x0000000000000000);
    regs.Write<u64>(17, 0x0000000000000000);
    regs.Write<u64>(18, 0x0000000000000000);
    regs.Write<u64>(19, 0x0000000000000000);
    regs.Write<u64>(20, 0x0000000000000001);
    regs.Write<u64>(21, 0x0000000000000000);
    regs.Write<u64>(23, 0x0000000000000000);
    regs.Write<u64>(24, 0x0000000000000000);
    regs.Write<u64>(25, 0xFFFFFFFF825B21C9);
    regs.Write<u64>(26, 0x0000000000000000);
    regs.Write<u64>(27, 0x0000000000000000);
    regs.Write<u64>(28, 0x0000000000000000);
    regs.Write<u64>(29, 0xFFFFFFFFA4001FF0);
    regs.Write<u64>(30, 0x0000000000000000);
    regs.Write<u64>(31, 0xFFFFFFFFA4001550);

    regs.lo = 0x0000000018B63D28;
    regs.hi = 0x00000000625C2BBE;

    if (pal) {
      regs.Write<u64>(20, 0x0000000000000000);
      regs.Write<u64>(23, 0x0000000000000006);
      regs.Write<u64>(31, 0xFFFFFFFFA4001554);
    }
    break;
  case CIC_NUS_6105_7105:
    regs.Write<u64>(0, 0x0000000000000000);
    regs.Write<u64>(1, 0x0000000000000000);
    regs.Write<u64>(2, 0xFFFFFFFFF58B0FBF);
    regs.Write<u64>(3, 0xFFFFFFFFF58B0FBF);
    regs.Write<u64>(4, 0x0000000000000FBF);
    regs.Write<u64>(5, 0xFFFFFFFFDECAAAD1);
    regs.Write<u64>(6, 0xFFFFFFFFA4001F0C);
    regs.Write<u64>(7, 0xFFFFFFFFA4001F08);
    regs.Write<u64>(8, 0x00000000000000C0);
    regs.Write<u64>(9, 0x0000000000000000);
    regs.Write<u64>(10, 0x0000000000000040);
    regs.Write<u64>(11, 0xFFFFFFFFA4000040);
    regs.Write<u64>(12, 0xFFFFFFFF9651F81E);
    regs.Write<u64>(13, 0x000000002D42AAC5);
    regs.Write<u64>(14, 0x00000000489B52CF);
    regs.Write<u64>(15, 0x0000000056584D60);
    regs.Write<u64>(16, 0x0000000000000000);
    regs.Write<u64>(17, 0x0000000000000000);
    regs.Write<u64>(18, 0x0000000000000000);
    regs.Write<u64>(19, 0x0000000000000000);
    regs.Write<u64>(20, 0x0000000000000001);
    regs.Write<u64>(21, 0x0000000000000000);
    regs.Write<u64>(23, 0x0000000000000000);
    regs.Write<u64>(24, 0x0000000000000002);
    regs.Write<u64>(25, 0xFFFFFFFFCDCE565F);
    regs.Write<u64>(26, 0x0000000000000000);
    regs.Write<u64>(27, 0x0000000000000000);
    regs.Write<u64>(28, 0x0000000000000000);
    regs.Write<u64>(29, 0xFFFFFFFFA4001FF0);
    regs.Write<u64>(30, 0x0000000000000000);
    regs.Write<u64>(31, 0xFFFFFFFFA4001550);

    regs.lo = 0x0000000056584D60;
    regs.hi = 0x000000004BE35D1F;

    if (pal) {
      regs.Write<u64>(20, 0x0000000000000000);
      regs.Write<u64>(23, 0x0000000000000006);
      regs.Write<u64>(31, 0xFFFFFFFFA4001554);
    }

    mem.Write<u32>(regs, IMEM_REGION_START + 0x00, 0x3C0DBFC0);
    mem.Write<u32>(regs, IMEM_REGION_START + 0x04, 0x8DA807FC);
    mem.Write<u32>(regs, IMEM_REGION_START + 0x08, 0x25AD07C0);
    mem.Write<u32>(regs, IMEM_REGION_START + 0x0C, 0x31080080);
    mem.Write<u32>(regs, IMEM_REGION_START + 0x10, 0x5500FFFC);
    mem.Write<u32>(regs, IMEM_REGION_START + 0x14, 0x3C0DBFC0);
    mem.Write<u32>(regs, IMEM_REGION_START + 0x18, 0x8DA80024);
    mem.Write<u32>(regs, IMEM_REGION_START + 0x1C, 0x3C0BB000);
    break;
  case CIC_NUS_6106_7106:
    regs.Write<u64>(0, 0x0000000000000000);
    regs.Write<u64>(1, 0x0000000000000000);
    regs.Write<u64>(2, 0xFFFFFFFFA95930A4);
    regs.Write<u64>(3, 0xFFFFFFFFA95930A4);
    regs.Write<u64>(4, 0x00000000000030A4);
    regs.Write<u64>(5, 0xFFFFFFFFB04DC903);
    regs.Write<u64>(6, 0xFFFFFFFFA4001F0C);
    regs.Write<u64>(7, 0xFFFFFFFFA4001F08);
    regs.Write<u64>(8, 0x00000000000000C0);
    regs.Write<u64>(9, 0x0000000000000000);
    regs.Write<u64>(10, 0x0000000000000040);
    regs.Write<u64>(11, 0xFFFFFFFFA4000040);
    regs.Write<u64>(12, 0xFFFFFFFFBCB59510);
    regs.Write<u64>(13, 0xFFFFFFFFBCB59510);
    regs.Write<u64>(14, 0x000000000CF85C13);
    regs.Write<u64>(15, 0x000000007A3C07F4);
    regs.Write<u64>(16, 0x0000000000000000);
    regs.Write<u64>(17, 0x0000000000000000);
    regs.Write<u64>(18, 0x0000000000000000);
    regs.Write<u64>(19, 0x0000000000000000);
    regs.Write<u64>(20, 0x0000000000000001);
    regs.Write<u64>(21, 0x0000000000000000);
    regs.Write<u64>(23, 0x0000000000000000);
    regs.Write<u64>(24, 0x0000000000000002);
    regs.Write<u64>(25, 0x00000000465E3F72);
    regs.Write<u64>(26, 0x0000000000000000);
    regs.Write<u64>(27, 0x0000000000000000);
    regs.Write<u64>(28, 0x0000000000000000);
    regs.Write<u64>(29, 0xFFFFFFFFA4001FF0);
    regs.Write<u64>(30, 0x0000000000000000);
    regs.Write<u64>(31, 0xFFFFFFFFA4001550);
    regs.lo = 0x000000007A3C07F4;
    regs.hi = 0x0000000023953898;

    if (pal) {
      regs.Write<u64>(20, 0x0000000000000000);
      regs.Write<u64>(23, 0x0000000000000006);
      regs.Write<u64>(31, 0xFFFFFFFFA4001554);
    }
    break;
  }

  regs.Write<u8>(22, (cicSeeds[cicType] >> 8) & 0xFF);
  regs.cop0.Reset();
  mem.Write<u32>(regs, 0x04300004, 0x01010101);
  std::copy_n(mem.rom.cart.begin(), 0x1000, mem.mmio.rsp.dmem.begin());
  regs.SetPC32(static_cast<s32>(0xA4000040));
}

void PIF::Execute() const {
  const CICType cicType = mem.rom.cicType;
  const bool pal = mem.rom.pal;
  mem.Write<u32>(regs, PIF_RAM_REGION_START + 0x24, cicSeeds[cicType]);
  switch (cicType) {
  case UNKNOWN_CIC_TYPE:
    Util::warn("Unknown CIC type!");
    break;
  case CIC_NUS_6101 ... CIC_NUS_6103_7103:
    mem.Write<u32>(regs, 0x318, RDRAM_SIZE);
    break;
  case CIC_NUS_6105_7105:
    mem.Write<u32>(regs, 0x3F0, RDRAM_SIZE);
    break;
  case CIC_NUS_6106_7106:
    break;
  }

  HLE(pal, cicType);
}

std::vector<u8> PIF::Serialize() {
  std::vector<u8> res{};
  res.resize(6 * sizeof(JoybusDevice) + PIF_BOOTROM_SIZE + PIF_RAM_SIZE + mempak.size() + eeprom.size() + sizeof(int));

  u32 index = 0;
  memcpy(res.data() + index, joybusDevices.data(), 6 * sizeof(JoybusDevice));
  index += 6 * sizeof(JoybusDevice);
  memcpy(res.data() + index, bootrom.data(), PIF_BOOTROM_SIZE);
  index += PIF_BOOTROM_SIZE;
  memcpy(res.data() + index, ram.data(), PIF_RAM_SIZE);
  index += PIF_RAM_SIZE;
  memcpy(res.data() + index, mempak.data(), mempak.size());
  index += mempak.size();
  memcpy(res.data() + index, eeprom.data(), eeprom.size());
  index += eeprom.size();
  memcpy(res.data() + index, &channel, sizeof(int));

  return res;
}
} // namespace n64
