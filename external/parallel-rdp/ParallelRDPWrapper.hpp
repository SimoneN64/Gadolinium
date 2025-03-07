#pragma once
#include <rdp_device.hpp>
#include <wsi.hpp>
#include <common.hpp>

namespace n64 {
struct VI;
}

class ParallelRDP {
public:
  class WindowInfo {
  public:
    struct CoordinatePair {
      float x;
      float y;
    };
    virtual CoordinatePair get_window_size() = 0;
    virtual ~WindowInfo() = default;
  };

  void Init(const std::shared_ptr<Vulkan::InstanceFactory> &, const std::shared_ptr<Vulkan::WSIPlatform> &,
            const std::shared_ptr<WindowInfo> &, const u8 *);
  ParallelRDP() = default;

  void UpdateScreen(const n64::VI &) const;
  void EnqueueCommand(int, const u32 *) const;
  void OnFullSync() const;
  bool IsFramerateUnlocked() const;
  void SetFramerateUnlocked(bool) const;

  std::shared_ptr<Vulkan::WSI> wsi;
  std::shared_ptr<RDP::CommandProcessor> command_processor;
  std::shared_ptr<WindowInfo> windowInfo;

private:
  void LoadWSIPlatform(const std::shared_ptr<Vulkan::InstanceFactory> &, const std::shared_ptr<Vulkan::WSIPlatform> &,
                       const std::shared_ptr<WindowInfo> &);
  void DrawFullscreenTexturedQuad(Util::IntrusivePtr<Vulkan::Image>, Util::IntrusivePtr<Vulkan::CommandBuffer>) const;
  void UpdateScreen(Util::IntrusivePtr<Vulkan::Image>) const;
};
