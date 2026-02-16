#pragma once

#include <cstdint>
#include <functional>

class GfxRenderer;

namespace xtc {
class XtcParser;
}

namespace sumi {

// XtcPageRenderer - Renders XTC/XTCH binary page data to GfxRenderer
// Supports 1-bit (B&W) and 2-bit (4-level grayscale) formats
class XtcPageRenderer {
 public:
  // Result of render operation
  enum class RenderResult { Success, EndOfBook, InvalidDimensions, AllocationFailed, PageLoadFailed };

  explicit XtcPageRenderer(GfxRenderer& renderer);

  // Render a page from the parser
  // refreshCallback is called when display refresh is needed (for pagesUntilFullRefresh logic)
  RenderResult render(xtc::XtcParser& parser, uint32_t pageNum, std::function<void()> refreshCallback);

 private:
  GfxRenderer& renderer_;

  // Render 1-bit B&W page (standard XTC)
  void render1Bit(const uint8_t* buffer, uint16_t width, uint16_t height);

  // Render 2-bit grayscale page (XTCH format)
  // Uses 4-pass rendering for e-ink grayscale display
  // Takes two separate plane buffers to handle heap fragmentation
  void render2BitGrayscale(const uint8_t* plane1, const uint8_t* plane2, uint16_t width, uint16_t height);
};

}  // namespace sumi
