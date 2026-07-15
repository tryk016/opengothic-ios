#pragma once

#include <Tempest/CommandBuffer>
#include <Tempest/Widget>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string_view>
#include <utility>

#include "resources.h"

class RendererIOS;

class VideoWidget : public Tempest::Widget {
  private:
    struct Context;

  public:
    class PreparedFrame final {
      public:
        PreparedFrame() = default;
        PreparedFrame(PreparedFrame&&) noexcept = default;
        PreparedFrame& operator=(PreparedFrame&&) noexcept = default;

        PreparedFrame(const PreparedFrame&) = delete;
        PreparedFrame& operator=(const PreparedFrame&) = delete;

        explicit operator bool() const noexcept { return context!=nullptr; }

      private:
        explicit PreparedFrame(std::shared_ptr<Context> context) noexcept
          : context(std::move(context)) {
          }

        std::shared_ptr<Context> context;

      friend class VideoWidget;
      };

    VideoWidget();
    ~VideoWidget();

    void pushVideo(std::string_view filename);
    bool isActive() const;
    void skip();               // stop the current clip (used by touch skip)

    void tick();
    void paintEvent(Tempest::PaintEvent &event) override;

    void keyDownEvent(Tempest::KeyEvent&   event) override;
    void keyUpEvent  (Tempest::KeyEvent&   event) override;

    void mouseDownEvent(Tempest::MouseEvent& event) override;

  private:
    struct Input;
    struct Sound;
    struct SoundContext;

    PreparedFrame prepareFrame(Tempest::Device& device, uint8_t fId);
    static void encodePrepared(Tempest::Encoder<Tempest::CommandBuffer>& encoder,
                               uint8_t fId, const PreparedFrame& frame);

    void  stopVideo();

    std::shared_ptr<Context>      ctx;
    Tempest::Texture2d*           frame  = nullptr;
    bool                          active = false;
    bool                          restoreMusic = false;

    std::atomic_bool              hasPendingVideo{false};
    std::mutex                    syncVideo;
    std::queue<std::string>       pendingVideo;

  friend class RendererIOS;
  };

