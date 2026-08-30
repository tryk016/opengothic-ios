#pragma once

#include <Tempest/Widget>

class MainWindow;

// Native, platform-owned settings layer.  It deliberately has no connection
// to a Daedalus MENU.DAT instance, so stock and modded game menus stay intact.
class DeviceSettings final : public Tempest::Widget {
  public:
    explicit DeviceSettings(MainWindow& owner);

    bool isOpen() const;
    void open();
    void close();

    void keyDownEvent (Tempest::KeyEvent& event) override;
    void keyUpEvent   (Tempest::KeyEvent& event) override;
    void mouseDownEvent(Tempest::MouseEvent& event) override;

  protected:
    void paintEvent(Tempest::PaintEvent& event) override;

  private:
    bool          isFrameRateLocked() const;
    int           frameRate() const;
    void          setFrameRate(int value);
    void          cycleFrameRate(int direction);

    MainWindow& owner;
    bool        active = false;
  };
