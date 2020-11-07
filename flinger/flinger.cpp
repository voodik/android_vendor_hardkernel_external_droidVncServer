#include <fcntl.h>
#include <getopt.h>
#include <linux/fb.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>

#include <cmath>
#include <condition_variable>
#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

#include <Minicap.hpp>

#include "mcdebug.h"

#include "flinger.h"
#include "screenFormat.h"
#include "pf.h"

#define BANNER_VERSION 1
#define BANNER_SIZE 24

#define DEFAULT_SOCKET_NAME "minicap"
#define DEFAULT_DISPLAY_ID 0
#define DEFAULT_JPG_QUALITY 80

enum {
  QUIRK_DUMB            = 1,
  QUIRK_ALWAYS_UPRIGHT  = 2,
  QUIRK_TEAR            = 4,
};

Minicap::Frame frame;
bool haveFrame = false;

class FrameWaiter: public Minicap::FrameAvailableListener {
public:
  FrameWaiter()
    : mPendingFrames(0),
      mTimeout(std::chrono::milliseconds(100)),
      mStopped(false) {
  }

  int
  waitForFrame() {
    std::unique_lock<std::mutex> lock(mMutex);

    while (!mStopped) {
      if (mCondition.wait_for(lock, mTimeout, [this]{return mPendingFrames > 0;})) {
        return mPendingFrames--;
      }
    }

    return 0;
  }

  void
  reportExtraConsumption(int count) {
    std::unique_lock<std::mutex> lock(mMutex);
    mPendingFrames -= count;
  }

  void
  onFrameAvailable() {
    std::unique_lock<std::mutex> lock(mMutex);
    mPendingFrames += 1;
    mCondition.notify_one();
  }

  void
  stop() {
    mStopped = true;
  }

  bool
  isStopped() {
    return mStopped;
  }

private:
  std::mutex mMutex;
  std::condition_variable mCondition;
  std::chrono::milliseconds mTimeout;
  int mPendingFrames;
  bool mStopped;
};

extern "C" screenFormat getscreenformat_flinger()
{
	MCINFO("--getscreenformat_flinger--\n");


  //get format on PixelFormat struct
	android::PixelFormat f = frame.pixelformat;

	android::PixelFormatInfo pf;
	getPixelFormatInfo(f,&pf);

	screenFormat format;

	format.bitsPerPixel = pf.bitsPerPixel;
	format.width = frame.width;
	format.height = frame.height;
	format.size = pf.bitsPerPixel*format.width*format.height/CHAR_BIT;
	format.redShift = pf.l_red;
	format.redMax = pf.h_red;
	format.greenShift = pf.l_green;
	format.greenMax = pf.h_green-pf.h_red;
	format.blueShift = pf.l_blue;
	format.blueMax = pf.h_blue-pf.h_green;
	format.alphaShift = pf.l_alpha;
	format.alphaMax = pf.h_alpha-pf.h_blue;

	return format;
}


static FrameWaiter gWaiter;

/*
static void
signal_handler(int signum) {
  switch (signum) {
  case SIGINT:
    MCINFO("Received SIGINT, stopping");
    gWaiter.stop();
    break;
  case SIGTERM:
    MCINFO("Received SIGTERM, stopping");
    gWaiter.stop();
    break;
  default:
    abort();
    break;
  }
}
*/

int
start_capture() {

  uint32_t displayId = DEFAULT_DISPLAY_ID;
  int framePeriodMs = 0;
  bool showInfo = true;
  bool skipFrames = false;

/*
  // Set up signal handler.
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);

*/
  // Start Android's thread pool so that it will be able to serve our requests.
  minicap_start_thread_pool();



    Minicap::DisplayInfo info;

    if (minicap_try_get_display_info(displayId, &info) != 0) {
        MCERROR("Unable to get display info");
        return EXIT_FAILURE;
    }

    int rotation;
    switch (info.orientation) {
    case Minicap::ORIENTATION_0:
      rotation = 0;
      break;
    case Minicap::ORIENTATION_90:
      rotation = 90;
      break;
    case Minicap::ORIENTATION_180:
      rotation = 180;
      break;
    case Minicap::ORIENTATION_270:
      rotation = 270;
      break;
    }

    std::cout.precision(2);
    std::cout.setf(std::ios_base::fixed, std::ios_base::floatfield);

    std::cout << "{"                                         << std::endl
              << "    \"id\": "       << displayId    << "," << std::endl
              << "    \"width\": "    << info.width   << "," << std::endl
              << "    \"height\": "   << info.height  << "," << std::endl
              << "    \"xdpi\": "     << info.xdpi    << "," << std::endl
              << "    \"ydpi\": "     << info.ydpi    << "," << std::endl
              << "    \"size\": "     << info.size    << "," << std::endl
              << "    \"density\": "  << info.density << "," << std::endl
              << "    \"fps\": "      << info.fps     << "," << std::endl
              << "    \"secure\": "   << (info.secure ? "true" : "false") << "," << std::endl
              << "    \"rotation\": " << rotation            << std::endl
              << "}"                                         << std::endl;




  std::cerr << "PID: " << getpid() << std::endl;


  // Disable STDOUT buffering.
  setbuf(stdout, NULL);

  // Set real display size.
  Minicap::DisplayInfo realInfo;
  realInfo.width = info.width;
  realInfo.height = info.height;

  // Figure out desired display size.
  Minicap::DisplayInfo desiredInfo;
  desiredInfo.width = info.width;
  desiredInfo.height = info.height;
  desiredInfo.orientation = info.orientation;


  // Set up minicap.
  Minicap* minicap = minicap_create(displayId);
  if (minicap == NULL) {
    return EXIT_FAILURE;
  }

  // Figure out the quirks the current capture method has.
  unsigned char quirks = 0;
  switch (minicap->getCaptureMethod()) {
  case Minicap::METHOD_FRAMEBUFFER:
    quirks |= QUIRK_DUMB | QUIRK_TEAR;
    break;
  case Minicap::METHOD_SCREENSHOT:
    quirks |= QUIRK_DUMB;
    break;
  case Minicap::METHOD_VIRTUAL_DISPLAY:
    quirks |= QUIRK_ALWAYS_UPRIGHT;
    break;
  }

  if (minicap->setRealInfo(realInfo) != 0) {
    MCERROR("Minicap did not accept real display info");
    goto disaster;
  }

  if (minicap->setDesiredInfo(desiredInfo) != 0) {
    MCERROR("Minicap did not accept desired display info");
    goto disaster;
  }

  minicap->setFrameAvailableListener(&gWaiter);

  if (minicap->applyConfigChanges() != 0) {
    MCERROR("Unable to start minicap with current config");
    goto disaster;
  }


  while (!gWaiter.isStopped()) {
    MCINFO("New client connection");

    int pending, err;
    while (!gWaiter.isStopped() && (pending = gWaiter.waitForFrame()) > 0) {
      auto frameAvailableAt = std::chrono::steady_clock::now();
      if (skipFrames && pending > 1) {
        // Skip frames if we have too many. Not particularly thread safe,
        // but this loop should be the only consumer anyway (i.e. nothing
        // else decreases the frame count).
        gWaiter.reportExtraConsumption(pending - 1);

        while (--pending >= 1) {
          if ((err = minicap->consumePendingFrame(&frame)) != 0) {
            if (err == -EINTR) {
              MCINFO("Frame consumption interrupted by EINTR");
              goto close;
            }
            else {
              MCERROR("Unable to skip pending frame");
              goto disaster;
            }
          }

          minicap->releaseConsumedFrame(&frame);
        }
      }

      if ((err = minicap->consumePendingFrame(&frame)) != 0) {
        if (err == -EINTR) {
          MCINFO("Frame consumption interrupted by EINTR");
          goto close;
        }
        else {
          MCERROR("Unable to consume pending frame");
          goto disaster;
        }
      }

      haveFrame = true;


      // This will call onFrameAvailable() on older devices, so we have
      // to do it here or the loop will stop.
      minicap->releaseConsumedFrame(&frame);
      haveFrame = false;
      if(framePeriodMs > 0) {
        std::this_thread::sleep_until(frameAvailableAt + std::chrono::milliseconds(framePeriodMs));
      }
    }

close:
    MCINFO("Closing client connection");

    // Have we consumed one frame but are still holding it?
    if (haveFrame) {
      minicap->releaseConsumedFrame(&frame);
    }
  }

  minicap_free(minicap);

  return EXIT_SUCCESS;

disaster:
  if (haveFrame) {
    minicap->releaseConsumedFrame(&frame);
  }

  minicap_free(minicap);

  return EXIT_FAILURE;
}

void start_capture_th()
{
	MCINFO("Starting capture thread");
    start_capture();
}

extern "C" int init_flinger()
{
	std::thread t(start_capture_th);
	t.detach();

	std::this_thread::sleep_for(std::chrono::seconds(1));

	MCINFO("Done: Starting capture thread");
	return 0;
}

extern "C" unsigned int *readfb_flinger()
{
	void *writePtr = NULL;
	size_t size = frame.size;

//	if(size > 0) {
//		writePtr=(unsigned int*)malloc(size+1);
//		memcpy(writePtr, frame.data, size);
//	}

	return (unsigned int*)frame.data;
}

extern "C" void close_flinger()
{
	MCINFO("close_flinger");
	gWaiter.stop();
}
