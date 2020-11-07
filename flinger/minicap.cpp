#include "Minicap.hpp"

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <dlfcn.h>

#include <binder/ProcessState.h>

#include <binder/IServiceManager.h>
#include <binder/IMemory.h>

#include <gui/BufferQueue.h>
#include <gui/CpuConsumer.h>
#include <gui/ISurfaceComposer.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>

#include <private/gui/ComposerService.h>

#include <ui/DisplayInfo.h>
#include <ui/PixelFormat.h>
#include <ui/Rect.h>
#if (PLATFORM_SDK_VERSION == 30)
#include <input/DisplayViewport.h>
#include <ui/DisplayConfig.h>
#include <ui/Rotation.h>
#include <ui/Size.h>
#include <ui/DisplayState.h>
#endif /* PLATFORM_SDK_VERSION == 30 */

#include "mcdebug.h"

static const char*
error_name(int32_t err) {
  switch (err) {
  case android::NO_ERROR: // also android::OK
    return "NO_ERROR";
  case android::UNKNOWN_ERROR:
    return "UNKNOWN_ERROR";
  case android::NO_MEMORY:
    return "NO_MEMORY";
  case android::INVALID_OPERATION:
    return "INVALID_OPERATION";
  case android::BAD_VALUE:
    return "BAD_VALUE";
  case android::BAD_TYPE:
    return "BAD_TYPE";
  case android::NAME_NOT_FOUND:
    return "NAME_NOT_FOUND";
  case android::PERMISSION_DENIED:
    return "PERMISSION_DENIED";
  case android::NO_INIT:
    return "NO_INIT";
  case android::ALREADY_EXISTS:
    return "ALREADY_EXISTS";
  case android::DEAD_OBJECT: // also android::JPARKS_BROKE_IT
    return "DEAD_OBJECT";
  case android::FAILED_TRANSACTION:
    return "FAILED_TRANSACTION";
  case android::BAD_INDEX:
    return "BAD_INDEX";
  case android::NOT_ENOUGH_DATA:
    return "NOT_ENOUGH_DATA";
  case android::WOULD_BLOCK:
    return "WOULD_BLOCK";
  case android::TIMED_OUT:
    return "TIMED_OUT";
  case android::UNKNOWN_TRANSACTION:
    return "UNKNOWN_TRANSACTION";
  case android::FDS_NOT_ALLOWED:
    return "FDS_NOT_ALLOWED";
  default:
    return "UNMAPPED_ERROR";
  }
}

class FrameProxy: public android::ConsumerBase::FrameAvailableListener {
public:
  FrameProxy(Minicap::FrameAvailableListener* listener): mUserListener(listener) {
  }

  virtual void
  onFrameAvailable(const android::BufferItem& /* item */) {
    mUserListener->onFrameAvailable();
  }

private:
  Minicap::FrameAvailableListener* mUserListener;
};

class MinicapImpl: public Minicap
{
public:
  MinicapImpl(int32_t displayId)
    : mDisplayId(displayId),
      mRealWidth(0),
      mRealHeight(0),
      mDesiredWidth(0),
      mDesiredHeight(0),
      mDesiredOrientation(0),
      mHaveBuffer(false),
      mHaveRunningDisplay(false) {
  }

  virtual
  ~MinicapImpl() {
    release();
  }

  virtual int
  applyConfigChanges() {
    if (mHaveRunningDisplay) {
      destroyVirtualDisplay();
    }

    return createVirtualDisplay();
  }

  virtual int
  consumePendingFrame(Minicap::Frame* frame) {
    android::status_t err;

    if ((err = mConsumer->lockNextBuffer(&mBuffer)) != android::NO_ERROR) {
      if (err == -EINTR) {
        return err;
      }
      else {
        MCERROR("Unable to lock next buffer %s (%d)", error_name(err), err);
        return err;
      }
    }

    frame->data = mBuffer.data;
    frame->format = convertFormat(mBuffer.format);
    frame->width = mBuffer.width;
    frame->height = mBuffer.height;
    frame->stride = mBuffer.stride;
    frame->bpp = android::bytesPerPixel(mBuffer.format);
    frame->size = mBuffer.stride * mBuffer.height * frame->bpp;
    frame->pixelformat = mBuffer.format;

    mHaveBuffer = true;

    return 0;
  }

  virtual Minicap::CaptureMethod
  getCaptureMethod() {
    return METHOD_VIRTUAL_DISPLAY;
  }

  virtual int32_t
  getDisplayId() {
    return mDisplayId;
  }

  virtual void
  release() {
    destroyVirtualDisplay();
  }

  virtual void
  releaseConsumedFrame(Minicap::Frame* /* frame */) {
    if (mHaveBuffer) {
      mConsumer->unlockBuffer(mBuffer);
      mHaveBuffer = false;
    }
  }

  virtual int
  setDesiredInfo(const Minicap::DisplayInfo& info) {
    mDesiredWidth = info.width;
    mDesiredHeight = info.height;
    mDesiredOrientation = info.orientation;
    return 0;
  }

  virtual void
  setFrameAvailableListener(Minicap::FrameAvailableListener* listener) {
    mUserFrameAvailableListener = listener;
  }

  virtual int
  setRealInfo(const Minicap::DisplayInfo& info) {
    mRealWidth = info.width;
    mRealHeight = info.height;
    return 0;
  }

private:
  int32_t mDisplayId;
  uint32_t mRealWidth;
  uint32_t mRealHeight;
  uint32_t mDesiredWidth;
  uint32_t mDesiredHeight;
  uint8_t mDesiredOrientation;
  android::sp<android::IGraphicBufferProducer> mBufferProducer;
  android::sp<android::IGraphicBufferConsumer> mBufferConsumer;
  android::sp<android::CpuConsumer> mConsumer;
  android::sp<android::IBinder> mVirtualDisplay;
  android::sp<FrameProxy> mFrameProxy;
  Minicap::FrameAvailableListener* mUserFrameAvailableListener;
  bool mHaveBuffer;
  bool mHaveRunningDisplay;
  android::CpuConsumer::LockedBuffer mBuffer;

  int
  createVirtualDisplay() {
    uint32_t sourceWidth, sourceHeight;
    uint32_t targetWidth, targetHeight;
    android::status_t err;

    switch (mDesiredOrientation) {
    case Minicap::ORIENTATION_90:
      sourceWidth = mRealHeight;
      sourceHeight = mRealWidth;
      targetWidth = mDesiredHeight;
      targetHeight = mDesiredWidth;
      break;
    case Minicap::ORIENTATION_270:
      sourceWidth = mRealHeight;
      sourceHeight = mRealWidth;
      targetWidth = mDesiredHeight;
      targetHeight = mDesiredWidth;
      break;
    case Minicap::ORIENTATION_180:
      sourceWidth = mRealWidth;
      sourceHeight = mRealHeight;
      targetWidth = mDesiredWidth;
      targetHeight = mDesiredHeight;
      break;
    case Minicap::ORIENTATION_0:
    default:
      sourceWidth = mRealWidth;
      sourceHeight = mRealHeight;
      targetWidth = mDesiredWidth;
      targetHeight = mDesiredHeight;
      break;
    }

    // Set up virtual display size.
    android::Rect layerStackRect(sourceWidth, sourceHeight);
    android::Rect visibleRect(targetWidth, targetHeight);

    // Create a Surface for the virtual display to write to.
    MCINFO("Creating SurfaceComposerClient");
    android::sp<android::SurfaceComposerClient> sc = new android::SurfaceComposerClient();

    MCINFO("Performing SurfaceComposerClient init check");
    if ((err = sc->initCheck()) != android::NO_ERROR) {
      MCERROR("Unable to initialize SurfaceComposerClient");
      return err;
    }

    // This is now REQUIRED in O Developer Preview 1 or there's a segfault
    // when the sp goes out of scope.
    sc = NULL;

    // Create virtual display.
    MCINFO("Creating virtual display");
    mVirtualDisplay = android::SurfaceComposerClient::createDisplay(
      /* const String8& displayName */  android::String8("minicap"),
      /* bool secure */                 true
    );

    MCINFO("Creating buffer queue");
    android::BufferQueue::createBufferQueue(&mBufferProducer, &mBufferConsumer, false);

    MCINFO("Setting buffer options");
    mBufferConsumer->setDefaultBufferSize(targetWidth, targetHeight);
    mBufferConsumer->setDefaultBufferFormat(android::PIXEL_FORMAT_RGBA_8888);

    MCINFO("Creating CPU consumer");
    mConsumer = new android::CpuConsumer(mBufferConsumer, 3, false);
    mConsumer->setName(android::String8("minicap"));

    MCINFO("Creating frame waiter");
    mFrameProxy = new FrameProxy(mUserFrameAvailableListener);
    mConsumer->setFrameAvailableListener(mFrameProxy);

    MCINFO("Publishing virtual display");
    android::SurfaceComposerClient::Transaction t;
    t.setDisplaySurface(mVirtualDisplay, mBufferProducer);
#if (PLATFORM_SDK_VERSION == 30)
    t.setDisplayProjection(mVirtualDisplay,
      android::ui::toRotation(android::DISPLAY_ORIENTATION_0), layerStackRect, visibleRect);
#else
    t.setDisplayProjection(mVirtualDisplay,
      android::DISPLAY_ORIENTATION_0, layerStackRect, visibleRect);
#endif /* PLATFORM_SDK_VERSION == 30 */
    t.setDisplayLayerStack(mVirtualDisplay, 0); // default stack
    t.apply();

    mHaveRunningDisplay = true;

    return 0;
  }

  void
  destroyVirtualDisplay() {
    MCINFO("Destroying virtual display");
    android::SurfaceComposerClient::destroyDisplay(mVirtualDisplay);

    if (mHaveBuffer) {
      mConsumer->unlockBuffer(mBuffer);
      mHaveBuffer = false;
    }

    mBufferProducer = NULL;
    mBufferConsumer = NULL;
    mConsumer = NULL;
    mFrameProxy = NULL;
    mVirtualDisplay = NULL;

    mHaveRunningDisplay = false;
  }

  static Minicap::Format
  convertFormat(android::PixelFormat format) {
    switch (format) {
    case android::PIXEL_FORMAT_NONE:
      return FORMAT_NONE;
    case android::PIXEL_FORMAT_CUSTOM:
      return FORMAT_CUSTOM;
    case android::PIXEL_FORMAT_TRANSLUCENT:
      return FORMAT_TRANSLUCENT;
    case android::PIXEL_FORMAT_TRANSPARENT:
      return FORMAT_TRANSPARENT;
    case android::PIXEL_FORMAT_OPAQUE:
      return FORMAT_OPAQUE;
    case android::PIXEL_FORMAT_RGBA_8888:
      return FORMAT_RGBA_8888;
    case android::PIXEL_FORMAT_RGBX_8888:
      return FORMAT_RGBX_8888;
    case android::PIXEL_FORMAT_RGB_888:
      return FORMAT_RGB_888;
    case android::PIXEL_FORMAT_RGB_565:
      return FORMAT_RGB_565;
    case android::PIXEL_FORMAT_BGRA_8888:
      return FORMAT_BGRA_8888;
    case android::PIXEL_FORMAT_RGBA_5551:
      return FORMAT_RGBA_5551;
    case android::PIXEL_FORMAT_RGBA_4444:
      return FORMAT_RGBA_4444;
    default:
      return FORMAT_UNKNOWN;
    }
  }
};

int
minicap_try_get_display_info(int32_t displayId, Minicap::DisplayInfo* info) {
#if (PLATFORM_SDK_VERSION == 30)
  android::sp<android::IBinder> dpy = android::SurfaceComposerClient::getInternalDisplayToken();

  android::DisplayInfo dInfo;
  android::status_t getInfoError = android::SurfaceComposerClient::getDisplayInfo(dpy, &dInfo);


  if (getInfoError != android::NO_ERROR) {
     MCERROR("SurfaceComposerClient::getDisplayInfo() failed: %s (%d)\n", error_name(getInfoError), getInfoError);
     return getInfoError;
  }


  android::DisplayConfig dConfig;
  android::status_t err = android::SurfaceComposerClient::getActiveDisplayConfig(dpy, &dConfig);
  if (err != android::NO_ERROR) {
     MCERROR("SurfaceComposerClient::getActiveDisplayConfig() failed: %s (%d)\n", error_name(err), err);
     return err;
  }
  
  android::ui::DisplayState displayState;
  android::status_t displayStateErr = android::SurfaceComposerClient::getDisplayState(dpy, &displayState);
  if (displayStateErr != android::NO_ERROR) {
     MCERROR("SurfaceComposerClient::getDisplayInfo() failed: %s (%d)\n", error_name(displayStateErr), displayStateErr);
     return displayStateErr;
  }

  
  android::ui::Size& resolution = dConfig.resolution;

  info->width = resolution.getWidth();
  info->height = resolution.getHeight();
  info->orientation = android::ui::toRotationInt(displayState.orientation);
  info->fps = dConfig.refreshRate;
  info->density = dInfo.density;
  info->xdpi = dConfig.xDpi;
  info->ydpi = dConfig.yDpi;
  info->secure = dInfo.secure;
  info->size = sqrt(pow(resolution.getWidth() / dConfig.xDpi, 2) + pow(resolution.getHeight() / dConfig.yDpi, 2));
#else
  #if (PLATFORM_SDK_VERSION == 29)
  android::sp<android::IBinder> dpy = android::SurfaceComposerClient::getPhysicalDisplayToken(displayId);
  if(!dpy) {
    MCINFO("could not get display for id: %d, using internal display", displayId);
    dpy = android::SurfaceComposerClient::getInternalDisplayToken();
  }
  #else
  android::sp<android::IBinder> dpy = android::SurfaceComposerClient::getBuiltInDisplay(displayId);
  #endif /* PLATFORM_SDK_VERSION == 29 */

  android::Vector<android::DisplayInfo> configs;
  android::status_t err = android::SurfaceComposerClient::getDisplayConfigs(dpy, &configs);

  if (err != android::NO_ERROR) {
    MCERROR("SurfaceComposerClient::getDisplayInfo() failed: %s (%d)\n", error_name(err), err);
    return err;
  }

  int activeConfig = android::SurfaceComposerClient::getActiveConfig(dpy);
  if(static_cast<size_t>(activeConfig) >= configs.size()) {
      MCERROR("Active config %d not inside configs (size %zu)\n", activeConfig, configs.size());
      return android::BAD_VALUE;
  }
  android::DisplayInfo dinfo = configs[activeConfig];

  info->width = dinfo.w;
  info->height = dinfo.h;
  info->orientation = dinfo.orientation;
  info->fps = dinfo.fps;
  info->density = dinfo.density;
  info->xdpi = dinfo.xdpi;
  info->ydpi = dinfo.ydpi;
  info->secure = dinfo.secure;
  info->size = sqrt(pow(dinfo.w / dinfo.xdpi, 2) + pow(dinfo.h / dinfo.ydpi, 2));
#endif /* PLATFORM_SDK_VERSION == 30 */
  return 0;
}

Minicap*
minicap_create(int32_t displayId) {
  return new MinicapImpl(displayId);
}

void
minicap_free(Minicap* mc) {
  delete mc;
}

void
minicap_start_thread_pool() {
  android::ProcessState::self()->startThreadPool();
}
