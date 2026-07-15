#include "videowidget.h"

#include <Tempest/Painter>
#include <Tempest/TextCodec>
#include <Tempest/Log>
#include <Tempest/Application>
#include <Tempest/Platform>

#include <cstddef>
#include <optional>
#include <stdexcept>

#include "bink/video.h"
#include "graphics/shaders.h"
#include "gamemusic.h"
#include "gothic.h"

using namespace Tempest;

struct VideoWidget::Input : Bink::Video::Input {
  Input(zenkit::Read& fin):fin(fin) {}

  void read(void *dest, size_t count) override {
    if(fin.read(dest,count)!=count)
      throw std::runtime_error("i/o error");
    at += count;
    }
  void skip(size_t count) override {
    fin.seek(ptrdiff_t(count), zenkit::Whence::CUR);
    at += count;
    }
  void seek(size_t pos) override {
    fin.seek(ptrdiff_t(pos), zenkit::Whence::BEG);
    at = pos;
    }

  zenkit::Read&   fin;
  size_t          at = 0;
  };

struct VideoWidget::Sound : Tempest::SoundProducer {
  Sound(SoundContext& c, uint16_t sampleRate, bool isMono)
    :Tempest::SoundProducer(sampleRate, isMono ? 1 : 2), ctx(c), channels(isMono ? 1 : 2) {
    }
  void renderSound(int16_t *out, size_t n) override;

  SoundContext& ctx;
  size_t        channels = 2;
  };

struct VideoWidget::SoundContext {
  SoundContext(Context& ctx, SoundDevice& dev, uint16_t sampleRate, bool isMono):
    ctx(ctx), sampleRate(sampleRate), numChannels(isMono ? 1 : 2) {
    snd.emplace(dev.load(std::unique_ptr<VideoWidget::Sound>(new VideoWidget::Sound(*this,sampleRate,isMono))));
    }

  ~SoundContext() {
    // Stop the OpenAL callback while syncSamples/samples are still alive.
    // optional::reset destroys the existing effect without allocating the
    // empty SoundEffect::Impl used by SoundEffect's default constructor.
    snd.reset();
    }

  void play() {
    if(snd)
      snd->play();
    }

  uint64_t tickCount() const {
    auto smp = processedSamples;
    return uint64_t(smp*1000)/uint64_t(std::max(sampleRate * numChannels, 1));
    }

  void pushSamples(const std::vector<float>& s) {
    std::lock_guard<std::mutex> guard(syncSamples);
    size_t sz = samples.size();
    samples.resize(sz+s.size());
    std::memcpy(samples.data()+sz, s.data(), s.size()*sizeof(s[0]));
    }

  Context&             ctx;
  std::optional<Tempest::SoundEffect> snd;
  std::mutex           syncSamples;
  std::vector<float>   samples;

  uint64_t             processedSamples = 0;
  uint16_t             sampleRate = 0;
  uint16_t             numChannels = 0;
  };

void VideoWidget::Sound::renderSound(int16_t *out, size_t n) {
  n = n*channels; // stereo

  std::lock_guard<std::mutex> guard(ctx.syncSamples);
  auto& s = ctx.samples;
  if(s.size()<n)
    return;
  for(size_t i=0; i<n; ++i) {
    float v = s[i];
    out[i] = (v < -1.00004566f ? int16_t(-32768) : (v > 1.00001514f ? int16_t(32767) : int16_t(v * 32767.5f)));
    }
  ctx.samples.erase(ctx.samples.begin(),ctx.samples.begin()+int(n));
  ctx.processedSamples += n;
  }

struct VideoWidget::Context {
  Context(std::unique_ptr<zenkit::Read>&& f) : fin(std::move(f)), input(*fin), vid(&input) {
    sndCtx.resize(vid.audioCount());
    for(size_t i=0; i<sndCtx.size(); ++i) {
      auto& aud = vid.audio(uint8_t(i));
      sndCtx[i].reset(new SoundContext(*this,sndDev,aud.sampleRate,aud.isMono));
      }

    const float volume = Gothic::settingsSoundVolume();
    sndDev.setGlobalVolume(volume);
    for(size_t i=0; i<vid.audioCount(); ++i)
      sndCtx[i]->play();

    frameTime = Application::tickCount();
    }

  const Bink::Frame& decodeAndPace() {
    auto& f = vid.nextFrame();
    for(size_t i=0; i<vid.audioCount(); ++i)
      sndCtx[i]->pushSamples(f.audio(uint8_t(i)).samples);

    const uint64_t destTick = (1000*vid.fps().den*vid.currentFrame())/vid.fps().num;
    const uint64_t tickVis  = Application::tickCount() - frameTime;
    const uint64_t tickSnd  = tickSound();
    const uint64_t tick     = std::min(tickVis, tickSnd);

    if(destTick > tick) {
      Application::sleep(uint32_t(destTick-tick));
      }
    return f;
    }

  uint64_t tickSound() const {
    uint64_t tickSnd = uint64_t(-1);
    // return tickSnd;

    for(size_t i=0; i<vid.audioCount(); ++i)
      tickSnd = std::min(tickSnd, sndCtx[i]->tickCount());
    return tickSnd;
    }

  void prepareGpu(const Bink::Frame& f, Tempest::Device& device, uint8_t fId) {
    if(fId>=Resources::MaxFramesInFlight)
      throw std::out_of_range("VideoWidget frame slot is out of range");

    if(uint32_t(frameImg.w())!=f.width() || uint32_t(frameImg.h())!=f.height()) {
      Resources::recycle(std::move(frameImg));
      frameImg = device.attachment(Tempest::RGBA8, f.width(), f.height());
      }
    if(frameImg.isEmpty())
      throw std::runtime_error("unable to allocate the Bink render target");

    // alignment for ssbo offsets
    auto alignBuf = [](size_t size, size_t align) { return (size+align-1) & ~(align-1); };

    auto& planeY = f.plane(0);
    auto& planeU = f.plane(1);
    auto& planeV = f.plane(2);

    auto  align  = std::max<size_t>(device.properties().ssbo.offsetAlign, 4);
    auto  sizeY  = alignBuf(f.height()*planeY.stride(),     align);
    auto  sizeU  = alignBuf((f.height()/2)*planeU.stride(), align);
    auto  sizeV  = alignBuf((f.height()/2)*planeV.stride(), align);
    auto  size   = sizeY + sizeU + sizeV;

    auto& stage  = staging[fId];
    if(stage.byteSize()!=size) {
      Resources::recycle(std::move(stage));
      stage = device.ssbo(BufferHeap::Upload, Uninitialized, size);
      }
    if(stage.byteSize()!=size)
      throw std::runtime_error("unable to allocate the Bink upload buffer");
    stage.update(planeY.data(), 0,           (f.height())  *planeY.stride());
    stage.update(planeU.data(), sizeY,       (f.height()/2)*planeU.stride());
    stage.update(planeV.data(), sizeY+sizeU, (f.height()/2)*planeV.stride());

    auto& out    = prepared[fId];
    out.strideY  = planeY.stride();
    out.strideU  = planeU.stride();
    out.strideV  = planeV.stride();
    out.offsetU  = sizeY;
    out.offsetV  = sizeY + sizeU;
    out.valid    = true;
    }

  void encodePrepared(Tempest::Encoder<Tempest::CommandBuffer>& encoder, uint8_t fId) {
    if(fId>=Resources::MaxFramesInFlight)
      throw std::out_of_range("VideoWidget frame slot is out of range");

    auto& in = prepared[fId];
    if(!in.valid)
      return;

    encoder.setFramebuffer({{frameImg, Vec4(0), Tempest::Preserve}});

    struct Push { uint32_t strideY; uint32_t strideU; uint32_t strideV; } push = {};
    push.strideY = in.strideY;
    push.strideU = in.strideU;
    push.strideV = in.strideV;

    encoder.setDebugMarker("Bink");
    encoder.setPushData(push);
    encoder.setBinding(0, staging[fId], 0);
    encoder.setBinding(1, staging[fId], in.offsetU);
    encoder.setBinding(2, staging[fId], in.offsetV);
    encoder.setPipeline(Shaders::inst().bink);
    encoder.draw(nullptr, 0, 3);
    in.valid = false;
    }

  [[deprecated]]
  void yuvToRgba(const Bink::Frame& f, Pixmap& pm) {
    if(pm.w()!=f.width() || pm.h()!=f.height())
      pm = Pixmap(f.width(),f.height(),TextureFormat::RGBA8);
    auto& planeY = f.plane(0);
    auto& planeU = f.plane(1);
    auto& planeV = f.plane(2);
    auto  dst    = reinterpret_cast<uint8_t*>(pm.data());

    const uint32_t w = pm.w();
    for(uint32_t y=0; y<pm.h(); ++y)
      for(uint32_t x=0; x<w; ++x) {
        uint8_t* rgb = &dst[(x+y*w)*4];
        float Y = planeY.at(x,  y  );
        float U = planeU.at(x/2,y/2);
        float V = planeV.at(x/2,y/2);

        float r = 1.164f * (Y - 16.f) + 1.596f * (V - 128.f);
        float g = 1.164f * (Y - 16.f) - 0.813f * (V - 128.f) - 0.391f * (U - 128.f);
        float b = 1.164f * (Y - 16.f) + 2.018f * (U - 128.f);

        r = std::max(0.f,std::min(r,255.f));
        g = std::max(0.f,std::min(g,255.f));
        b = std::max(0.f,std::min(b,255.f));

        rgb[0] = uint8_t(r);
        rgb[1] = uint8_t(g);
        rgb[2] = uint8_t(b);
        rgb[3] = 255;
        }
    }

  bool isEof() const {
    return vid.currentFrame()>=vid.frameCount();
    }

  std::unique_ptr<zenkit::Read> fin;
  Input                         input;
  Bink::Video                   vid;
  uint64_t                      frameTime = 0;

  Tempest::Attachment           frameImg;
  Tempest::StorageBuffer        staging[Resources::MaxFramesInFlight];

  struct PreparedGpuFrame final {
    size_t   offsetU = 0;
    size_t   offsetV = 0;
    uint32_t strideY = 0;
    uint32_t strideU = 0;
    uint32_t strideV = 0;
    bool     valid   = false;
    } prepared[Resources::MaxFramesInFlight];

  Tempest::SoundDevice          sndDev;
  std::vector<std::unique_ptr<SoundContext>> sndCtx;
  };

VideoWidget::VideoWidget() {
  setCursorShape(CursorShape::Hidden);
  }

VideoWidget::~VideoWidget() {
  }

void VideoWidget::pushVideo(std::string_view filename) {
  const int scaleVideos = Gothic::settingsGetI("GAME", "scaleVideos");
  if(scaleVideos<0)
    return;
  std::lock_guard<std::mutex> guard(syncVideo);
  pendingVideo.emplace(filename);
  hasPendingVideo.store(true);
  }

bool VideoWidget::isActive() const {
  return ctx!=nullptr || hasPendingVideo.load();
  }

void VideoWidget::tick() {
  if(ctx!=nullptr && ctx->isEof()) {
    stopVideo();
    }

  if(ctx!=nullptr)
    return;

  if(!hasPendingVideo)
    return;

  std::string filename;
  {
  std::lock_guard<std::mutex> guard(syncVideo);
  if(pendingVideo.empty())
    return;
  filename = std::move(pendingVideo.front());
  pendingVideo.pop();
  if(pendingVideo.size()==0)
    hasPendingVideo.store(false);
  }

  std::unique_ptr<zenkit::Read> read;
  if(auto* entry = Resources::vdfsIndex().find(filename)) {
    read = entry->open_read();
    }
  else if(auto* entry = Resources::vdfsIndex().find(filename+".bik")) {
    // some api-calls are missing extension
    read = entry->open_read();
    }
  else {
    Log::e("unable to locate video file: \"",filename,"\"");
    stopVideo();
    return;
    }

  try {
    ctx = std::make_shared<Context>(std::move(read));
    if(!active) {
      active       = true;
      restoreMusic = GameMusic::inst().isEnabled();
      GameMusic::inst().setEnabled(false);
      }
    }
  catch(...){
    try {
      Log::e("unable to play video: \"",filename,"\"");
      }
    catch(...) {
      }
    stopVideo();
    }
  }

void VideoWidget::keyDownEvent(KeyEvent& event) {
  if(event.key==Event::K_ESCAPE) {
    stopVideo();
    }
  }

void VideoWidget::keyUpEvent(KeyEvent&) {
  }

void VideoWidget::mouseDownEvent(Tempest::MouseEvent& event) {
  if(!active) {
    event.ignore();
    return;
    }
#if defined(__MOBILE_PLATFORM__)
  stopVideo();
#endif
  }

void VideoWidget::skip() {
  stopVideo();
  }

void VideoWidget::stopVideo() {
  ctx.reset();
  frame = nullptr;
  if(!hasPendingVideo) {
    if(restoreMusic && !GameMusic::inst().isEnabled())
      GameMusic::inst().setEnabled(true);
    active = false;
    }
  update();
  }

VideoWidget::PreparedFrame VideoWidget::prepareFrame(Tempest::Device& device, uint8_t fId) {
  auto context = ctx;
  if(context==nullptr)
    return {};
  if(fId>=Resources::MaxFramesInFlight)
    throw std::out_of_range("VideoWidget frame slot is out of range");
  // A canceled ticket or a recoverable decode error must never replay an
  // upload prepared for an older use of this slot.
  context->prepared[fId].valid = false;

  const Bink::Frame* decoded = nullptr;
  try {
    decoded = &context->decodeAndPace();
    }
  catch(const Bink::VideoDecodingException& e) { // video exception is recoverable
    try {
      Log::e("video decoding error. frame: ",context->vid.currentFrame(),", what: \"",e.what(),"\"");
      }
    catch(...) {
      }
    // paintEvent intentionally keeps showing the last decoded image after a
    // recoverable Bink error. Retain its Context until the main frame fence,
    // even though there is no new YUV upload to encode for this slot.
    if(ctx==context && frame!=nullptr)
      return PreparedFrame(std::move(context));
    return {};
    }
  catch(const std::exception& e) {
    try {
      Log::e("video decoding error. frame: ",context->vid.currentFrame(),", what: \"",e.what(),"\"");
      }
    catch(...) {
      }
    if(ctx==context)
      stopVideo();
    return {};
    }
  catch(...) {
    try {
      Log::e("video decoding error. frame: ",context->vid.currentFrame());
      }
    catch(...) {
      }
    if(ctx==context)
      stopVideo();
    return {};
    }

  if(decoded==nullptr)
    return {};

  // Application::sleep may yield to input handling. If the user skipped the
  // clip (or the next clip replaced it) while pacing, do not prepare a frame
  // that is no longer visible; the local shared_ptr still makes this check safe.
  if(ctx!=context)
    return {};

  // Allocation and upload errors are GPU preparation failures. Deliberately
  // let them escape so RendererIOS can enter its one-shot fatal state.
  context->prepareGpu(*decoded,device,fId);
  frame = &textureCast<Texture2d&>(context->frameImg);
  update();
  return PreparedFrame(std::move(context));
  }

void VideoWidget::encodePrepared(Tempest::Encoder<Tempest::CommandBuffer>& encoder,
                                 uint8_t fId, const PreparedFrame& frame) {
  if(!frame)
    return;
  frame.context->encodePrepared(encoder,fId);
  }

void VideoWidget::paintEvent(PaintEvent& e) {
  if(ctx==nullptr || frame==nullptr)
    return;
  float k  = float(w())/float(frame->w());
  int   vh = int(k*float(frame->h()));

  Painter p(e);
  p.setBrush(Color(0,0,0,1));
  p.drawRect(0,0,w(),h());

  p.setBrush(Brush(*frame,Painter::NoBlend,ClampMode::ClampToEdge));
  p.drawRect(0,(h()-vh)/2,w(),vh,
             0,0,p.brush().w(),p.brush().h());
  }
