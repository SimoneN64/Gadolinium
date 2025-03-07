#pragma once
#include <File.hpp>
#include <GameDB.hpp>
#include <Registers.hpp>
#include <algorithm>
#include <backend/MemoryRegions.hpp>
#include <backend/core/MMIO.hpp>
#include <common.hpp>
#include <log.hpp>
#include <vector>

namespace n64 {
struct ROMHeader {
  u8 initialValues[4];
  u32 clockRate;
  u32 programCounter;
  u32 release;
  u32 crc1;
  u32 crc2;
  u64 unknown;
  char imageName[20];
  u32 unknown2;
  u32 manufacturerId;
  u16 cartridgeId;
  char countryCode[2];
  u8 bootCode[4032];
};

struct ROM {
  std::vector<u8> cart;
  size_t mask;
  ROMHeader header;
  CICType cicType;
  char gameNameCart[20];
  std::string gameNameDB;
  char code[4];
  bool pal;
};

enum class FlashState : u8 { Idle, Erase, Write, Read, Status };

struct Flash {
  explicit Flash(mio::mmap_sink &);
  ~Flash() = default;
  void Reset();
  void Load(SaveType, const std::string &);
  FlashState state{};
  u64 status{};
  size_t eraseOffs{};
  size_t writeOffs{};
  std::array<u8, 128> writeBuf{};
  std::string flashPath{};
  mio::mmap_sink &saveData;

  enum FlashCommands : u8 {
    FLASH_COMMAND_EXECUTE = 0xD2,
    FLASH_COMMAND_STATUS = 0xE1,
    FLASH_COMMAND_SET_ERASE_OFFSET = 0x4B,
    FLASH_COMMAND_ERASE = 0x78,
    FLASH_COMMAND_SET_WRITE_OFFSET = 0xA5,
    FLASH_COMMAND_WRITE = 0xB4,
    FLASH_COMMAND_READ = 0xF0,
  };

  void CommandExecute() const;
  void CommandStatus();
  void CommandSetEraseOffs(u32);
  void CommandErase();
  void CommandSetWriteOffs(u32);
  void CommandWrite();
  void CommandRead();
  std::vector<u8> Serialize();
  void Deserialize(const std::vector<u8> &data);
  template <typename T>
  void Write(u32 index, T val);
  template <typename T>
  T Read(u32 index) const;
};

struct Mem {
  ~Mem() = default;
  Mem(Registers &, ParallelRDP &, JIT * = nullptr);
  void Reset();
  void LoadSRAM(SaveType, fs::path);
  static std::vector<u8> OpenROM(const std::string &, size_t &);
  static std::vector<u8> OpenArchive(const std::string &, size_t &);
  void LoadROM(bool, const std::string &);
  [[nodiscard]] auto GetRDRAMPtr() -> u8 * { return mmio.rdp.rdram.data(); }

  [[nodiscard]] auto GetRDRAM() -> std::vector<u8> & { return mmio.rdp.rdram; }

  std::vector<u8> Serialize();
  void Deserialize(const std::vector<u8> &);

  template <typename T>
  T Read(Registers &, u32);
  template <typename T>
  void Write(Registers &, u32, u32);
  void Write(const Registers &, u32, u64);

  template <typename T>
  T BackupRead(u32);
  template <typename T>
  void BackupWrite(u32, T);

  MMIO mmio;

  FORCE_INLINE void DumpRDRAM() const {
    std::vector<u8> temp{};
    temp.resize(RDRAM_SIZE);
    std::ranges::copy(mmio.rdp.rdram, temp.begin());
    Util::SwapBuffer<u32>(temp);
    Util::WriteFileBinary(temp, "rdram.bin");
  }

  FORCE_INLINE void DumpIMEM() const {
    std::array<u8, IMEM_SIZE> temp{};
    std::ranges::copy(mmio.rsp.imem, temp.begin());
    Util::SwapBuffer<u32>(temp);
    Util::WriteFileBinary(temp, "imem.bin");
  }

  FORCE_INLINE void DumpDMEM() const {
    std::array<u8, DMEM_SIZE> temp{};
    std::ranges::copy(mmio.rsp.dmem, temp.begin());
    Util::SwapBuffer<u32>(temp);
    Util::WriteFileBinary(temp, "dmem.bin");
  }
  ROM rom;
  SaveType saveType = SAVE_NONE;
  Flash flash;

private:
  template <typename T>
  void WriteInterpreter(Registers &, u32, u32);
  void WriteInterpreter(const Registers &, u32, u64);
  template <typename T>
  void WriteJIT(Registers &, u32, u32);
  void WriteJIT(const Registers &, u32, u64);

  JIT *jit = nullptr;
  friend struct SI;
  friend struct PI;
  friend struct AI;
  friend struct RSP;
  friend struct JIT;
  friend struct Core;
  std::array<u8, ISVIEWER_SIZE> isviewer{};
  std::string sramPath{};
  mio::mmap_sink saveData{};
  int mmioSize{}, flashSize{};

  [[nodiscard]] FORCE_INLINE bool IsROMPAL() const {
    static constexpr char pal_codes[] = {'D', 'F', 'I', 'P', 'S', 'U', 'X', 'Y'};
    return std::ranges::any_of(pal_codes, [this](char a) { return rom.cart[0x3d] == a; });
  }
};
} // namespace n64
