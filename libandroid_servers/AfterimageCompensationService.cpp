#define LOG_TAG "mAFPC_ABC"

#include <cutils/properties.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/SyncScreenCaptureListener.h>
#include <jni.h>
#include <nativehelper/JNIHelp.h>
#include <nativehelper/ScopedUtfChars.h>
#include <ui/GraphicBuffer.h>
#include <utils/Log.h>
#include <utils/RefBase.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace android {

// Forward-declared Samsung ABC engine structs & API
struct AbcDisplayParams {
  int32_t model;
  int32_t pad1;
  int32_t pad2;
  int32_t offset;
  int32_t width;
  int32_t height;

  uint8_t extended_buffer[0xf0438 - 0x18];

  double avg_lum;
  double max_bdi;
  double pad_bdi;
  double nbdi;
  double eff_avg_lum;
  double eff_max_bdi;
  double pad_eff;
  double eff_nbdi;

  uint8_t pad_after_stats[0x530 - 0x478];

  int32_t size_530;
  int32_t size_534;
  int32_t size_538;
  int32_t total_time;
  int32_t count_540;
  int32_t pad_544;

  char *path_poc;
  char *path_diff;
  char *path_time;
  char *path_mca_write;
  char *path_ioctl_dev;
  char *path_debug_raw;
  char *path_debug_float;
  char *path_wpc_lut;
  char *path_ap_main;
  char *path_ap_udc;
  char *path_org_vec;
  char *path_init_vec;
  char *path_mca_vec;
  char *path_lut_model;
  int32_t display_type;

  AbcDisplayParams();
  int setBufferSize(int, int, int);
  int setImageSize(int, int, int, int, int);
  bool use_one_by_one(int, int, int);
};

namespace ABC {
extern "C" {
int initProcess(AbcDisplayParams &);
int saProcess(uint8_t *, int, int, AbcDisplayParams &);
uint8_t *saProcess_one_by_one(uint8_t *, int, int, int, int, int, int,
                              AbcDisplayParams &);
int bpProcess(uint8_t *, float *, int, AbcDisplayParams &);
int acProcess(uint8_t *, float *, AbcDisplayParams &);
int analyzeBDI(float *, AbcDisplayParams &);
int analyzeEffBDI(float *, uint8_t *, AbcDisplayParams &);
int apply_wpc_lut(void *, int, int, int, int, int, const char *);
int getBurnInIndex(const char *, float *);
int getBurnInIndexWithSize(const char *, float *, int, int, int);
int makeMCAVector_Beyond0(const char *, const char *, int);
int makeMCAVector_Beyond12(const char *, const char *, int);
int getMCAVector_C1(const char *, uint8_t *, int);
}
} // namespace ABC

extern "C" {
int abc_read_file(const char *path, void *buf, size_t size, size_t offset);
int abc_write_file(const char *path, const void *buf, size_t size);
int abc_ioctl_MAFPC_ON(const char *dev);
int abc_ioctl_MAFPC_OFF(const char *dev);
int abc_ioctl_WRITE(const char *dev, const void *buf, size_t size);
int process_mafpc_on(AbcDisplayParams &);
uint8_t *update_impl(AbcDisplayParams &);
int aiqe_abc_update(AbcDisplayParams &, int);
int check_and_on(AbcDisplayParams &, bool, const char *, const char *);
int CheckPOCVector_Beyond0(const uint8_t *);
int CheckPOCVector_Beyond12(const uint8_t *);
int MIC_Core_Enc(uint8_t *, int, int, uint8_t *, int);
}

// Global BSS and State definitions
static sp<GraphicBuffer> mAFPC_outBuffer;
static int32_t mAFPC_outDataspace = 0;
static int32_t _privacy_index = 0;

static int32_t g_mainDisplayCount = 0;
static AbcDisplayParams *g_mainDisplayParams[2] = {nullptr, nullptr};
static uint8_t g_mainAiqeEnabled = 0;

static int32_t g_subDisplayCount = 0;
static AbcDisplayParams *g_subDisplayParams[2] = {nullptr, nullptr};
static uint8_t g_subAiqeEnabled = 0;

// =========================================================================
// JNI Method Implementations
// =========================================================================

extern "C" {

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataAPInit(
    JNIEnv *env, jobject /* thiz */, jint display_type, jstring ap_name) {
  if (!ap_name) {
    g_mainAiqeEnabled = 0;
    g_subAiqeEnabled = 0;
    return 0;
  }

  const char *ap_str = env->GetStringUTFChars(ap_name, nullptr);
  if (!ap_str || strlen(ap_str) == 0) {
    if (ap_str)
      env->ReleaseStringUTFChars(ap_name, ap_str);
    g_mainAiqeEnabled = 0;
    g_subAiqeEnabled = 0;
    return 0;
  }

  std::string main_bin = std::string("/efs/afc/") + ap_str + "_main.bin";
  std::string udc_bin = std::string("/efs/afc/") + ap_str + "_udc.bin";

  if (display_type == 0 && g_mainDisplayCount >= 1) {
    ALOGD("[INIT] APInit Main - %s", ap_str);
    for (int i = 0; i < g_mainDisplayCount; i++) {
      if (g_mainDisplayParams[i]) {
        g_mainDisplayParams[i]->path_ap_main = strdup(main_bin.c_str());
        g_mainDisplayParams[i]->path_ap_udc = strdup(udc_bin.c_str());
      }
    }
  } else if (display_type == 1 && g_subDisplayCount >= 1) {
    ALOGD("[INIT] APInit Sub - %s", ap_str);
    for (int i = 0; i < g_subDisplayCount; i++) {
      if (g_subDisplayParams[i]) {
        g_subDisplayParams[i]->path_ap_main = strdup(main_bin.c_str());
        g_subDisplayParams[i]->path_ap_udc = strdup(udc_bin.c_str());
      }
    }
  }

  env->ReleaseStringUTFChars(ap_name, ap_str);
  return 0;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataInit(
    JNIEnv * /* env */, jobject /* thiz */, jint type, jint model, jint p1,
    jint p2, jint p3, jint p4) {
  ALOGD("[INIT] Start nativeDataInit");
  int idx = g_mainDisplayCount++;

  AbcDisplayParams *p = new AbcDisplayParams();
  g_mainDisplayParams[idx] = p;
  p->model = model;
  p->display_type = type;

  // Resolve persistent paths
  p->path_poc = const_cast<char *>("/efs/afc/poc_data");
  p->path_diff = const_cast<char *>("/efs/afc/diff_data");
  p->path_time = const_cast<char *>("/efs/afc/time_data");
  p->path_ioctl_dev = const_cast<char *>("/dev/mafpc");
  p->path_wpc_lut = const_cast<char *>("/vendor/etc/djsk");

  if (p->setBufferSize(p1, p2, p3) != 0) {
    ALOGE("[INIT] [TYPE : %d] fail setBufferSize", type);
    return -1;
  }

  if (abc_read_file(p->path_time, &p->total_time, sizeof(int), 0) != 0) {
    ALOGD("[INIT] [TYPE : %d] FAIL read time data", type);
    std::vector<uint8_t> default_poc(p->size_530, 0xff);
    *(uint32_t *)default_poc.data() = 0x8181814d; // Magic header
    abc_write_file(p->path_poc, default_poc.data(), p->size_530);

    std::vector<float> default_diff(p->count_540, 0.0f);
    abc_write_file(p->path_diff, default_diff.data(),
                   p->count_540 * sizeof(float));

    int init_time = 0;
    abc_write_file(p->path_time, &init_time, sizeof(int));
  }

  if (ABC::initProcess(*p) != 0) {
    ALOGE("[INIT] [TYPE : %d] initProcess failed", type);
    return -1;
  }

  ALOGD("[INIT] [TYPE : %d] End nativeDataInit", type);
  return 0;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataInitSub(
    JNIEnv * /* env */, jobject /* thiz */, jint type, jint model, jint p1,
    jint p2, jint p3, jint p4) {
  ALOGD("[INIT] Start nativeDataInitSub");
  int idx = g_subDisplayCount++;

  AbcDisplayParams *p = new AbcDisplayParams();
  g_subDisplayParams[idx] = p;
  p->model = model;
  p->display_type = type;

  p->path_poc = const_cast<char *>("/efs/afc1/poc_data");
  p->path_diff = const_cast<char *>("/efs/afc1/diff_data");
  p->path_time = const_cast<char *>("/efs/afc1/time_data");
  p->path_ioctl_dev = const_cast<char *>("/dev/mafpc1");
  p->path_wpc_lut = const_cast<char *>("/vendor/etc/djsk_sub");

  if (p->setBufferSize(p1, p2, p3) != 0) {
    ALOGE("[INIT] [TYPE : %d] fail setBufferSize", type);
    return -1;
  }

  if (abc_read_file(p->path_time, &p->total_time, sizeof(int), 0) != 0) {
    std::vector<uint8_t> default_poc(p->size_530, 0xff);
    *(uint32_t *)default_poc.data() = 0x8181814d;
    abc_write_file(p->path_poc, default_poc.data(), p->size_530);

    std::vector<float> default_diff(p->count_540, 0.0f);
    abc_write_file(p->path_diff, default_diff.data(),
                   p->count_540 * sizeof(float));

    int init_time = 0;
    abc_write_file(p->path_time, &init_time, sizeof(int));
  }

  if (ABC::initProcess(*p) != 0) {
    ALOGE("[INIT] [TYPE : %d] initProcess failed", type);
    return -1;
  }

  ALOGD("[INIT] [TYPE : %d] End nativeDataInitSub", type);
  return 0;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataStart(
    JNIEnv * /* env */, jobject /* thiz */, jint start_type) {
  AbcDisplayParams *p = g_mainDisplayParams[0];
  if (!p)
    return -1;

  ALOGD("[START] Start nativeDataStart : %d, %d, %d", start_type, p->size_534,
        p->size_530);
  if (g_mainAiqeEnabled & 1)
    return 0;

  int size = p->size_530;
  uint8_t *buf = new uint8_t[size];

  if (start_type == 0) {
    memset(buf, 0xff, size);
    *(uint32_t *)buf = 0x8181814d;
    if (abc_write_file(p->path_mca_write, buf, size) != 0) {
      delete[] buf;
      return -3;
    }
  } else if (start_type == 1) {
    if (abc_read_file(p->path_mca_write, buf, size, 0) != 0) {
      delete[] buf;
      return -3;
    }
  } else {
    delete[] buf;
    return -3;
  }

  if (abc_ioctl_WRITE(p->path_ioctl_dev, buf, size) != 0) {
    delete[] buf;
    return -2;
  }

  int res = (abc_ioctl_MAFPC_ON(p->path_ioctl_dev) != 0) ? -1 : 0;
  delete[] buf;
  ALOGD("[START] End nativeDataStart (%d)", res);
  return res;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeSetPrivacyIndex(
    JNIEnv * /* env */, jobject /* thiz */, jint index) {
  _privacy_index = index;
  ALOGD("[READ] nativeSetPrivacyIndex : %d", index);
  return 0;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataRead(
    JNIEnv * /* env */, jobject /* thiz */, jint luminance, jint count,
    jint orientation, jboolean flag) {
  ALOGD("[READ] Start nativeDataRead : size : %d", g_mainDisplayCount);
  if (g_mainDisplayCount < 1 || !g_mainDisplayParams[0])
    return -1;

  std::vector<PhysicalDisplayId> ids =
      SurfaceComposerClient::getPhysicalDisplayIds();
  if (ids.empty()) {
    ALOGE("[READ] No physical display IDs found");
    return -1;
  }

  sp<IBinder> token = SurfaceComposerClient::getPhysicalDisplayToken(ids[0]);
  if (!token)
    return -1;

  sp<SyncScreenCaptureListener> listener = new SyncScreenCaptureListener();
  gui::CaptureArgs captureArgs;
  ScreenshotClient::captureDisplay(ids[0], captureArgs, listener);
  gui::ScreenCaptureResults results = listener->waitForResults();

  if (results.result != 0 || !results.buffer) {
    ALOGE("[READ] Screen capture failed");
    return -1;
  }

  void *pixels = nullptr;
  status_t err =
      results.buffer->lock(GraphicBuffer::USAGE_SW_READ_OFTEN, &pixels);
  if (err != NO_ERROR || !pixels)
    return -1;

  int w = results.buffer->getWidth();
  int h = results.buffer->getHeight();
  int s = results.buffer->getStride();
  int bpp = bytesPerPixel(results.buffer->getPixelFormat());

  ALOGD("[READ] luminance = %d", luminance);
  ALOGD("[READ] w = %d, h = %d, s = %d, f = %d, s_size = %zu, luminance = %d, "
        "count = %d, captureOrientation = %d",
        w, h, s, bpp, (size_t)(s * h * bpp), luminance, count, orientation);

  for (int i = 0; i < g_mainDisplayCount; i++) {
    AbcDisplayParams *p = g_mainDisplayParams[i];
    if (p->display_type == 1 && _privacy_index >= 0) {
      ABC::apply_wpc_lut(pixels, w, h, s, bpp, _privacy_index, p->path_wpc_lut);
    }

    if (p->use_one_by_one(w, h, orientation)) {
      ALOGD("[READ] [TYPE : %d] use_one_by_one", p->display_type);
      ABC::saProcess_one_by_one((uint8_t *)pixels, 0, luminance, w, h, s,
                                orientation, *p);
    } else {
      p->setImageSize(w, h, s, bpp, orientation);
      int sa_res = ABC::saProcess((uint8_t *)pixels, 0, luminance, *p);
      ALOGD("[READ] [TYPE : %d] saProcess : %d", p->display_type, sa_res);
    }
  }

  results.buffer->unlock();
  ALOGD("[READ] END nativeDataRead");
  return 0;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataReadSub(
    JNIEnv * /* env */, jobject /* thiz */, jint luminance, jint count,
    jint orientation, jboolean flag) {
  ALOGD("[READ] Start nativeDataReadSub");
  if (g_subDisplayCount < 1 || !g_subDisplayParams[0])
    return -1;

  std::vector<PhysicalDisplayId> ids =
      SurfaceComposerClient::getPhysicalDisplayIds();
  if (ids.size() < 2)
    return -1;

  sp<SyncScreenCaptureListener> listener = new SyncScreenCaptureListener();
  gui::CaptureArgs captureArgs;
  ScreenshotClient::captureDisplay(ids[1], captureArgs, listener);
  gui::ScreenCaptureResults results = listener->waitForResults();

  if (results.result != 0 || !results.buffer)
    return -1;

  void *pixels = nullptr;
  if (results.buffer->lock(GraphicBuffer::USAGE_SW_READ_OFTEN, &pixels) !=
          NO_ERROR ||
      !pixels)
    return -1;

  int w = results.buffer->getWidth();
  int h = results.buffer->getHeight();
  int s = results.buffer->getStride();
  int bpp = bytesPerPixel(results.buffer->getPixelFormat());

  for (int i = 0; i < g_subDisplayCount; i++) {
    AbcDisplayParams *p = g_subDisplayParams[i];
    p->setImageSize(w, h, s, bpp, orientation);
    ABC::saProcess((uint8_t *)pixels, 0, luminance, *p);
  }

  results.buffer->unlock();
  return 0;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataSave(
    JNIEnv * /* env */, jobject /* thiz */, jint time) {
  ALOGD("[SAVE] Start nativeDataSave");
  for (int i = 0; i < g_mainDisplayCount; i++) {
    AbcDisplayParams *p = g_mainDisplayParams[i];
    if (!p)
      continue;

    uint8_t *byte_buf = new uint8_t[p->size_534];
    float *float_buf = new float[p->count_540];

    if (abc_read_file(p->path_poc, byte_buf, p->size_534, 0) == 0 &&
        abc_read_file(p->path_diff, float_buf, p->count_540 * sizeof(float),
                      0) == 0) {

      int bp_res = ABC::bpProcess(byte_buf + p->offset, float_buf, time, *p);
      ALOGD("[SAVE] [TYPE : %d] bpProcess End (%d)", p->display_type, bp_res);

      if (bp_res == 0) {
        ABC::analyzeBDI(float_buf, *p);
        ABC::analyzeEffBDI(float_buf, byte_buf + p->offset, *p);
        ALOGD("[SAVE] [TYPE : %d] analyzeEffBDI - %lf - %lf - %lf - %lf - %lf "
              "- %lf",
              p->display_type, p->avg_lum, p->max_bdi, p->nbdi, p->eff_avg_lum,
              p->eff_max_bdi, p->eff_nbdi);

        abc_write_file(p->path_poc, byte_buf, p->size_534);
        abc_write_file(p->path_diff, float_buf, p->count_540 * sizeof(float));

        p->total_time += time;
        abc_write_file(p->path_time, &p->total_time, sizeof(int));
      }
    }
    delete[] byte_buf;
    delete[] float_buf;
  }
  ALOGD("[SAVE] End nativeDataSave (0)");
  return 0;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataSaveSub(
    JNIEnv * /* env */, jobject /* thiz */, jint time) {
  ALOGD("[SAVE] Start nativeDataSaveSub");
  for (int i = 0; i < g_subDisplayCount; i++) {
    AbcDisplayParams *p = g_subDisplayParams[i];
    if (!p)
      continue;

    uint8_t *byte_buf = new uint8_t[p->size_534];
    float *float_buf = new float[p->count_540];

    if (abc_read_file(p->path_poc, byte_buf, p->size_534, 0) == 0 &&
        abc_read_file(p->path_diff, float_buf, p->count_540 * sizeof(float),
                      0) == 0) {

      if (ABC::bpProcess(byte_buf + p->offset, float_buf, time, *p) == 0) {
        ABC::analyzeBDI(float_buf, *p);
        ABC::analyzeEffBDI(float_buf, byte_buf + p->offset, *p);
        abc_write_file(p->path_poc, byte_buf, p->size_534);
        abc_write_file(p->path_diff, float_buf, p->count_540 * sizeof(float));

        p->total_time += time;
        abc_write_file(p->path_time, &p->total_time, sizeof(int));
      }
    }
    delete[] byte_buf;
    delete[] float_buf;
  }
  ALOGD("[SAVE] End nativeDataSaveSub");
  return 0;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataTerminate(
    JNIEnv * /* env */, jobject /* thiz */, jint time) {
  ALOGD("[TERMINATE] Start nativeDataTerminate");
  int res = 0;
  for (int i = 0; i < g_mainDisplayCount; i++) {
    AbcDisplayParams *p = g_mainDisplayParams[i];
    if (!p)
      continue;

    uint8_t *byte_buf = new uint8_t[p->size_534];
    float *float_buf = new float[p->count_540];

    if (abc_read_file(p->path_poc, byte_buf, p->size_534, 0) == 0 &&
        abc_read_file(p->path_diff, float_buf, p->count_540 * sizeof(float),
                      0) == 0) {

      if (ABC::bpProcess(byte_buf + p->offset, float_buf, time, *p) == 0) {
        ABC::analyzeBDI(float_buf, *p);
        ABC::analyzeEffBDI(float_buf, byte_buf + p->offset, *p);
        abc_write_file(p->path_poc, byte_buf, p->size_534);
        abc_write_file(p->path_diff, float_buf, p->count_540 * sizeof(float));

        p->total_time += time;
        abc_write_file(p->path_time, &p->total_time, sizeof(int));
      }
    } else {
      res = -4;
    }
    delete[] byte_buf;
    delete[] float_buf;
  }
  ALOGD("[TERMINATE] End nativeDataTerminate (%d)", res);
  return res;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataTerminateSub(
    JNIEnv * /* env */, jobject /* thiz */, jint time) {
  ALOGD("[TERMINATE] Start nativeDataTerminateSub");
  int res = 0;
  for (int i = 0; i < g_subDisplayCount; i++) {
    AbcDisplayParams *p = g_subDisplayParams[i];
    if (!p)
      continue;

    uint8_t *byte_buf = new uint8_t[p->size_534];
    float *float_buf = new float[p->count_540];

    if (abc_read_file(p->path_poc, byte_buf, p->size_534, 0) == 0 &&
        abc_read_file(p->path_diff, float_buf, p->count_540 * sizeof(float),
                      0) == 0) {

      if (ABC::bpProcess(byte_buf + p->offset, float_buf, time, *p) == 0) {
        ABC::analyzeBDI(float_buf, *p);
        ABC::analyzeEffBDI(float_buf, byte_buf + p->offset, *p);
        abc_write_file(p->path_poc, byte_buf, p->size_534);
        abc_write_file(p->path_diff, float_buf, p->count_540 * sizeof(float));

        p->total_time += time;
        abc_write_file(p->path_time, &p->total_time, sizeof(int));
      }
    } else {
      res = -4;
    }
    delete[] byte_buf;
    delete[] float_buf;
  }
  return res;
}

jfloat
Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataEvaluate(
    JNIEnv * /* env */, jobject /* thiz */) {
  ALOGD("[EVALUATE] Start nativeDataEvaluate");
  float burnInIndex = 0.0f;

  for (int i = 0; i < g_mainDisplayCount; i++) {
    AbcDisplayParams *p = g_mainDisplayParams[i];
    if (!p)
      continue;

    if (p->model <= 59138) {
      if (ABC::getBurnInIndex(p->path_diff, &burnInIndex) != 1) {
        ALOGE("[EVALUATE] [TYPE : %d] getBurnInIndex failed", p->display_type);
        burnInIndex = 0.0f;
      }
    } else {
      if (ABC::getBurnInIndexWithSize(p->path_diff, &burnInIndex, 1080, 2400,
                                      3) != 1) {
        ALOGE("[EVALUATE] [TYPE : %d] getBurnInIndexWithSize failed",
              p->display_type);
        burnInIndex = 0.0f;
      }
    }
  }

  ALOGD("[EVALUATE] End nativeDataEvaluate");
  return burnInIndex;
}

// Telemetry Getters (Main)
jdouble
Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataReadAvgLum(
    JNIEnv *, jobject) {
  return *(double *)((char *)g_mainDisplayParams[0] + 0xf0438);
}
jdouble
Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataReadMaxBDI(
    JNIEnv *, jobject) {
  return *(double *)((char *)g_mainDisplayParams[0] + 0xf0440);
}
jdouble
Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataReadNBDI(
    JNIEnv *, jobject) {
  return *(double *)((char *)g_mainDisplayParams[0] + 0xf0450);
}
jdouble
Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataReadEffAvgLum(
    JNIEnv *, jobject) {
  return *(double *)((char *)g_mainDisplayParams[0] + 0xf0458);
}
jdouble
Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataReadEffMaxBDI(
    JNIEnv *, jobject) {
  return *(double *)((char *)g_mainDisplayParams[0] + 0xf0460);
}
jdouble
Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataReadEffNBDI(
    JNIEnv *, jobject) {
  return *(double *)((char *)g_mainDisplayParams[0] + 0xf0470);
}

// Telemetry Getters (Sub)
jdouble
Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataReadAvgLumSub(
    JNIEnv *, jobject) {
  return *(double *)((char *)g_subDisplayParams[0] + 0xf0438);
}
jdouble
Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataReadMaxBDISub(
    JNIEnv *, jobject) {
  return *(double *)((char *)g_subDisplayParams[0] + 0xf0440);
}
jdouble
Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataReadNBDISub(
    JNIEnv *, jobject) {
  return *(double *)((char *)g_subDisplayParams[0] + 0xf0450);
}
jdouble
Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataReadEffAvgLumSub(
    JNIEnv *, jobject) {
  return *(double *)((char *)g_subDisplayParams[0] + 0xf0458);
}
jdouble
Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataReadEffMaxBDISub(
    JNIEnv *, jobject) {
  return *(double *)((char *)g_subDisplayParams[0] + 0xf0460);
}
jdouble
Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataReadEffNBDISub(
    JNIEnv *, jobject) {
  return *(double *)((char *)g_subDisplayParams[0] + 0xf0470);
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataWrite(
    JNIEnv * /* env */, jobject /* thiz */, jint scale_factor) {
  ALOGD("[WRITE] Start nativeDataWrite scale_factor : %d", scale_factor);
  for (int i = 0; i < g_mainDisplayCount; i++) {
    AbcDisplayParams *p = g_mainDisplayParams[i];
    if (!p)
      continue;

    uint8_t *byte_buf = new uint8_t[p->size_534];
    float *float_buf = new float[p->count_540];

    abc_read_file(p->path_poc, byte_buf, p->size_534, 0);
    abc_read_file(p->path_diff, float_buf, p->count_540 * sizeof(float), 0);

    if (ABC::acProcess(byte_buf, float_buf, *p) == 0) {
      if (p->model == 0x2e6f9) { // Beyond0
        ABC::makeMCAVector_Beyond0(p->path_diff, p->path_mca_vec, scale_factor);
      } else {
        ABC::makeMCAVector_Beyond12(p->path_diff, p->path_mca_vec,
                                    scale_factor);
      }
    }
    delete[] byte_buf;
    delete[] float_buf;
  }
  ALOGD("[WRITE] End nativeDataWrite");
  return 0;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataWriteV2(
    JNIEnv * /* env */, jobject /* thiz */, jint scale_factor) {
  ALOGD("[WRITE2] Start nativeDataWriteV2 scale_factor : %d", scale_factor);
  for (int i = 0; i < g_mainDisplayCount; i++) {
    AbcDisplayParams *p = g_mainDisplayParams[i];
    if (!p)
      continue;

    uint8_t *byte_buf = new uint8_t[p->size_534];
    float *float_buf = new float[p->count_540];

    abc_read_file(p->path_poc, byte_buf, p->size_534, 0);
    abc_read_file(p->path_diff, float_buf, p->count_540 * sizeof(float), 0);

    if (ABC::acProcess(byte_buf, float_buf, *p) == 0) {
      if (p->model == 200201) { // Canvas1
        uint8_t *mca_buf = new uint8_t[p->size_530];
        memset(mca_buf, 0, p->size_530);
        ABC::getMCAVector_C1(p->path_diff, mca_buf, scale_factor);
        delete[] mca_buf;
      }
    }
    delete[] byte_buf;
    delete[] float_buf;
  }
  ALOGD("[WRITE2] End nativeDataWriteV2");
  return 0;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativePrivacyCheckAndUpdate(
    JNIEnv * /* env */, jobject /* thiz */) {
  AbcDisplayParams *p = g_mainDisplayParams[0];
  if (!p)
    return 0;

  ALOGD("[UPDATE] check wide : avg lum : %lf", p->eff_avg_lum);
  ALOGD("[UPDATE] check wide : nbdi : %lf", p->eff_nbdi);

  uint8_t *buf = update_impl(*p);
  if (!buf)
    return 0;

  // MIC indicator area encoder
  MIC_Core_Enc(buf, 0, 0, nullptr, 0);

  abc_write_file(p->path_mca_write, buf, p->size_530);
  process_mafpc_on(*p);

  delete[] buf;
  ALOGD("[UPDATE] End nativePrivacyCheckAndUpdate : result : (0)");
  return 0;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativePrivacyCheckAndUpdateSub(
    JNIEnv * /* env */, jobject /* thiz */) {
  AbcDisplayParams *p = g_subDisplayParams[0];
  if (!p)
    return 0;

  uint8_t *buf = update_impl(*p);
  if (!buf)
    return 0;

  MIC_Core_Enc(buf, 0, 0, nullptr, 0);
  abc_write_file(p->path_mca_write, buf, p->size_530);
  process_mafpc_on(*p);

  delete[] buf;
  return 0;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataUpdate(
    JNIEnv * /* env */, jobject /* thiz */) {
  ALOGD("[UPDATE] Start nativeDataUpdate");
  AbcDisplayParams *p = g_mainDisplayParams[0];
  if (!p)
    return -2;

  uint8_t *buf = update_impl(*p);
  if (!buf) {
    ALOGD("[UPDATE] Fail Update impl : -2");
    return -2;
  }

  int res = 0;
  if (g_mainAiqeEnabled & 1) {
    char prop[PROPERTY_VALUE_MAX] = {0};
    property_get("vendor.display.enable_abc", prop, "-1");
    int val = atoi(prop);
    int mode = (val >= 1 && val <= 2) ? 1 : 2;
    aiqe_abc_update(*p, mode);
    delete[] buf;
  } else {
    if (abc_write_file(p->path_mca_write, buf, p->size_530) == 0) {
      res = (process_mafpc_on(*p) != 0) ? -1 : 0;
    } else {
      res = -2;
    }
    delete[] buf;
  }

  ALOGD("[UPDATE] End nativeDataUpdate result : %d", res);
  return res;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataUpdateSub(
    JNIEnv * /* env */, jobject /* thiz */) {
  ALOGD("[UPDATE] Start nativeDataUpdateSub");
  AbcDisplayParams *p = g_subDisplayParams[0];
  if (!p)
    return -2;

  uint8_t *buf = update_impl(*p);
  if (!buf)
    return -2;

  int res = 0;
  if (g_subAiqeEnabled & 1) {
    aiqe_abc_update(*p, 1);
    delete[] buf;
  } else {
    if (abc_write_file(p->path_mca_write, buf, p->size_530) == 0) {
      res = (process_mafpc_on(*p) != 0) ? -1 : 0;
    } else {
      res = -2;
    }
    delete[] buf;
  }

  ALOGD("[UPDATE] End nativeDataUpdateSub result : %d", res);
  return res;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataApply(
    JNIEnv * /* env */, jobject /* thiz */) {
  AbcDisplayParams *p = g_mainDisplayParams[0];
  if (!p)
    return 0;

  ALOGD("[APPLY] Start nativeDataApply : %d, %d", p->size_534, p->size_530);
  if (g_mainAiqeEnabled & 1)
    return 0;

  int res = (process_mafpc_on(*p) != 0) ? -1 : 0;
  if (g_subDisplayCount >= 1 && g_subDisplayParams[0]) {
    if (process_mafpc_on(*g_subDisplayParams[0]) != 0) {
      res = -1;
    }
  }

  ALOGD("[APPLY] End nativeDataApply (%d)", res);
  return res;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataOff(
    JNIEnv * /* env */, jobject /* thiz */) {
  ALOGD("[OFF] Start nativeDataOff");
  if (!(g_mainAiqeEnabled & 1) && g_mainDisplayParams[0]) {
    if (abc_ioctl_MAFPC_OFF(g_mainDisplayParams[0]->path_ioctl_dev) != 0) {
      ALOGD("[OFF] End nativeDataOff FAIL");
      return 0;
    }
  }
  ALOGD("[OFF] End nativeDataOff");
  return 0;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeDataVerify(
    JNIEnv * /* env */, jobject /* thiz */) {
  AbcDisplayParams *p = g_mainDisplayParams[0];
  if (!p)
    return -2;

  ALOGD("[DATA_VERIFY] model = %d", p->model);
  int size = p->size_534;
  uint8_t *buf = new uint8_t[size];

  if (abc_read_file("/efs/afc/org.vec", buf, size, 0) != 0) {
    delete[] buf;
    return -2;
  }

  bool valid = false;
  if (p->model == 0x2e6f9) { // Beyond0
    valid = (CheckPOCVector_Beyond0(buf) == 1);
  } else if (p->model == 0x2e6fa ||
             (p->model >= 190101 && p->model <= 190103)) { // Beyond12
    valid = (CheckPOCVector_Beyond12(buf) == 1);
  } else {
    ALOGE("[DATA_VERIFY] Unknown Model");
    delete[] buf;
    return -2;
  }

  if (!valid) {
    ALOGE("[DATA_VERIFY] Read vector is Wrong");
    delete[] buf;
    return -2;
  }

  if (abc_write_file("/efs/afc/init_vec", buf, size) != 0) {
    ALOGE("[DATA_VERIFY] Could not write init_vec");
    delete[] buf;
    return -1;
  }

  ALOGD("[DATA_VERIFY] Original Vector(/efs/afc/org.vec) - Complete");
  delete[] buf;
  ALOGD("[DATA_VERIFY] Done");
  return 0;
}

jint Java_com_samsung_android_hardware_display_AfterimageCompensationService_nativeMdnieBlockVerify(
    JNIEnv * /* env */, jobject /* thiz */, jboolean flag) {
  if (g_mainDisplayParams[0]) {
    check_and_on(*g_mainDisplayParams[0], flag != 0,
                 "/sys/class/lcd/panel/ldi_cell_id", "/efs/afc/ldi_cell_id");
  }

  if (g_subDisplayCount >= 1 && g_subDisplayParams[0]) {
    check_and_on(*g_subDisplayParams[0], flag != 0,
                 "/sys/class/lcd/panel/ldi_cell_id", "/efs/afc/ldi_cell_id");
  }

  return 0;
}

} // extern "C"
} // namespace android