#pragma once

#include <Tempest/Size>
#include <Tempest/SystemApi>
#include <Tempest/Pixmap>

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

namespace Tempest {
class Device;
class Painter;
class VectorImage;
}

class InventoryMenu;
class VideoWidget;

class RendererIOS final {
  private:
    struct TicketControl;

  public:
    class FrameTicket final {
      public:
        FrameTicket(const FrameTicket&) = delete;
        FrameTicket& operator=(const FrameTicket&) = delete;

        FrameTicket(FrameTicket&& other) noexcept;
        FrameTicket& operator=(FrameTicket&& other) noexcept;
        ~FrameTicket();

      private:
        FrameTicket(const std::shared_ptr<TicketControl>& control,
                    uint8_t slot, uint64_t serial) noexcept;
        void disarm() noexcept;

        std::weak_ptr<TicketControl> control;
        uint64_t                     serial    = 0;
        uint8_t                      frameSlot = 0;

      friend class RendererIOS;
      };

    struct FrameInput final {
      const Tempest::VectorImage& uiLayer;
      const Tempest::VectorImage& numberOverlay;
      InventoryMenu&              inventory;
      bool                        videoActive       = false;
      bool                        captureSavePreview = false;
      };

    struct SubmitResult final {
      bool savePreviewQueued = false;
      };

    RendererIOS(Tempest::Device& device, Tempest::SystemApi::Window* window);
    ~RendererIOS();

    RendererIOS(const RendererIOS&) = delete;
    RendererIOS& operator=(const RendererIOS&) = delete;

    std::optional<FrameTicket> beginFrame();
    void         prepareVideo(FrameTicket& frame, VideoWidget& video);
    SubmitResult submitFrame(FrameTicket&& frame, const FrameInput& input);

    Tempest::Size   drawableSize() const;
    std::string_view failureReason() const noexcept;
    void            resize();
    bool            waitIdle() noexcept;
    void            onWorldChanged();

    bool            savePreviewReady();
    Tempest::Pixmap takeSavePreview();
    Tempest::Pixmap screenshot();

    void dbgDraw(Tempest::Painter& painter);
    bool ssaoBuffersAllocated() const noexcept;

  private:
    struct Impl;

    void cancelFrame(uint64_t serial) noexcept;

    std::shared_ptr<TicketControl> ticketControl;
    std::unique_ptr<Impl>          impl;
  };
