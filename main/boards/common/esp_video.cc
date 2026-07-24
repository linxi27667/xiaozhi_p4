#include "sdkconfig.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <unistd.h>
#include <errno.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

#ifdef CONFIG_ESP_HOSTED_ENABLED
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#endif

#include "esp_imgfx_color_convert.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "linux/videodev2.h"

#include "board.h"
#include "display.h"
#include "esp_video.h"
#include "esp_jpeg_common.h"
#include "jpg/image_to_jpeg.h"
#include "jpg/jpeg_to_image.h"
#include "lvgl_display.h"
#include "mcp_server.h"
#include "system_info.h"

#ifdef CONFIG_XIAOZHI_ENABLE_CAMERA_DEBUG_MODE
#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL MAX(CONFIG_LOG_DEFAULT_LEVEL, ESP_LOG_DEBUG)
#endif  // CONFIG_XIAOZHI_ENABLE_CAMERA_DEBUG_MODE
#include <esp_log.h> // should be after LOCAL_LOG_LEVEL definition

#ifdef CONFIG_IDF_TARGET_ESP32P4
#include "driver/ppa.h"
#endif

#ifdef CONFIG_XIAOZHI_ENABLE_ROTATE_CAMERA_IMAGE
#ifdef CONFIG_IDF_TARGET_ESP32P4
#if defined(CONFIG_XIAOZHI_CAMERA_IMAGE_ROTATION_ANGLE_90)
#define IMAGE_ROTATION_ANGLE (PPA_SRM_ROTATION_ANGLE_270)
#elif defined(CONFIG_XIAOZHI_CAMERA_IMAGE_ROTATION_ANGLE_270)
#define IMAGE_ROTATION_ANGLE (PPA_SRM_ROTATION_ANGLE_90)
#else
#error "CONFIG_XIAOZHI_CAMERA_IMAGE_ROTATION_ANGLE is not set"
#endif  // angle
#else   // target
#include "esp_imgfx_rotate.h"
#if defined(CONFIG_XIAOZHI_CAMERA_IMAGE_ROTATION_ANGLE_90)
#define IMAGE_ROTATION_ANGLE (90)
#elif defined(CONFIG_XIAOZHI_CAMERA_IMAGE_ROTATION_ANGLE_270)
#define IMAGE_ROTATION_ANGLE (270)
#else
#error "CONFIG_XIAOZHI_CAMERA_IMAGE_ROTATION_ANGLE is not set"
#endif  // angle
#endif  // target
#endif  // CONFIG_XIAOZHI_ENABLE_ROTATE_CAMERA_IMAGE


#define TAG "EspVideo"

static void log_camera_memory(const char *stage)
{
    ESP_LOGI(TAG,
             "Camera memory [%s]: internal=%u min_internal=%u dma_largest=%u psram=%u psram_largest=%u",
             stage,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
}

static bool camera_dma_ready(const char *stage)
{
    constexpr size_t kMinimumDmaFree = 16 * 1024;
    constexpr size_t kMinimumDmaLargest = 4 * 1024;
    size_t dma_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    size_t dma_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (dma_free >= kMinimumDmaFree && dma_largest >= kMinimumDmaLargest) {
        return true;
    }

    ESP_LOGE(TAG, "CAM_FIX_V4 blocked %s: dma_free=%u dma_largest=%u required=%u/%u",
             stage, (unsigned)dma_free, (unsigned)dma_largest,
             (unsigned)kMinimumDmaFree, (unsigned)kMinimumDmaLargest);
    return false;
}

#if defined(CONFIG_CAMERA_SENSOR_SWAP_PIXEL_BYTE_ORDER) || defined(CONFIG_XIAOZHI_ENABLE_CAMERA_ENDIANNESS_SWAP)
#warning \
    "CAMERA_SENSOR_SWAP_PIXEL_BYTE_ORDER or CONFIG_XIAOZHI_ENABLE_CAMERA_ENDIANNESS_SWAP is enabled, which may cause image corruption in YUV422 format!"
#endif

#if CONFIG_XIAOZHI_ENABLE_CAMERA_DEBUG_MODE
#define CAM_PRINT_FOURCC(pixelformat)       \
    char fourcc[5];                         \
    fourcc[0] = pixelformat & 0xFF;         \
    fourcc[1] = (pixelformat >> 8) & 0xFF;  \
    fourcc[2] = (pixelformat >> 16) & 0xFF; \
    fourcc[3] = (pixelformat >> 24) & 0xFF; \
    fourcc[4] = '\0';                       \
    ESP_LOGD(TAG, "FOURCC: '%c%c%c%c'", fourcc[0], fourcc[1], fourcc[2], fourcc[3]);

// for compatibility with old esp_video version
#ifndef MAP_FAILED
#define MAP_FAILED nullptr
#endif

__attribute__((weak)) esp_err_t esp_video_deinit(void) {
    return ESP_ERR_NOT_SUPPORTED;
}
// end of for compatibility with old esp_video version

static void log_available_video_devices() {
    for (int i = 0; i < 50; i++) {
        char path[16];
        snprintf(path, sizeof(path), "/dev/video%d", i);
        int fd = open(path, O_RDONLY);
        if (fd >= 0) {
            ESP_LOGD(TAG, "found video device: %s", path);
            close(fd);
        }
    }
}
#else
#define CAM_PRINT_FOURCC(pixelformat) (void)0;
#endif  // CONFIG_XIAOZHI_ENABLE_CAMERA_DEBUG_MODE

EspVideo::EspVideo(const esp_video_init_config_t& config) {
    bool jpeg_encoder_ready = image_to_jpeg_init();
    ESP_LOGI(TAG, "Camera reliability path CAM_FIX_V4: JPEG encoder=%s",
             jpeg_encoder_ready ? "reserved" : "unavailable");
    log_camera_memory("jpeg-reserved");

    if (esp_video_init(&config) != ESP_OK) {
        ESP_LOGE(TAG, "esp_video_init failed");
        return;
    }

#ifdef CONFIG_XIAOZHI_ENABLE_CAMERA_DEBUG_MODE
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
#endif  // CONFIG_XIAOZHI_ENABLE_CAMERA_DEBUG_MODE

    const char* video_device_name = nullptr;

    if (false) { /* 用于构建 else if */
    }
#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
    else if (config.csi != nullptr) {
        video_device_name = ESP_VIDEO_MIPI_CSI_DEVICE_NAME;
    }
#endif
#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
    else if (config.dvp != nullptr) {
        video_device_name = ESP_VIDEO_DVP_DEVICE_NAME;
    }
#endif
#if CONFIG_ESP_VIDEO_ENABLE_HW_JPEG_VIDEO_DEVICE
    else if (config.jpeg != nullptr) {
        video_device_name = ESP_VIDEO_JPEG_DEVICE_NAME;
    }
#endif
#if CONFIG_ESP_VIDEO_ENABLE_SPI_VIDEO_DEVICE
    else if (config.spi != nullptr) {
        video_device_name = ESP_VIDEO_SPI_DEVICE_NAME;
    }
#endif
#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
    else if (config.usb_uvc != nullptr) {
        video_device_name = ESP_VIDEO_USB_UVC_DEVICE_NAME(0);
    }
#endif

    if (video_device_name == nullptr) {
        ESP_LOGE(TAG, "no video device is enabled");
        return;
    }

    video_fd_ = open(video_device_name, O_RDWR);

    if (video_fd_ < 0) {
        ESP_LOGE(TAG, "open %s failed, errno=%d(%s)", video_device_name, errno, strerror(errno));
#if CONFIG_XIAOZHI_ENABLE_CAMERA_DEBUG_MODE
        log_available_video_devices();
#endif  // CONFIG_XIAOZHI_ENABLE_CAMERA_DEBUG_MODE
        return;
    }

    struct v4l2_capability cap = {};
    if (ioctl(video_fd_, VIDIOC_QUERYCAP, &cap) != 0) {
        ESP_LOGE(TAG, "VIDIOC_QUERYCAP failed, errno=%d(%s)", errno, strerror(errno));
        close(video_fd_);
        video_fd_ = -1;
        return;
    }

    ESP_LOGD(
        TAG,
        "VIDIOC_QUERYCAP: driver=%s, card=%s, bus_info=%s, version=0x%08lx, capabilities=0x%08lx, device_caps=0x%08lx",
        cap.driver, cap.card, cap.bus_info, cap.version, cap.capabilities, cap.device_caps);

    struct v4l2_format format = {};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(video_fd_, VIDIOC_G_FMT, &format) != 0) {
        ESP_LOGE(TAG, "VIDIOC_G_FMT failed, errno=%d(%s)", errno, strerror(errno));
        close(video_fd_);
        video_fd_ = -1;
        return;
    }
    ESP_LOGD(TAG, "VIDIOC_G_FMT: pixelformat=0x%08lx, width=%ld, height=%ld", format.fmt.pix.pixelformat,
             format.fmt.pix.width, format.fmt.pix.height);
    CAM_PRINT_FOURCC(format.fmt.pix.pixelformat);

    struct v4l2_format setformat = {};
    setformat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    sensor_width_ = format.fmt.pix.width;
    sensor_height_ = format.fmt.pix.height;
    setformat.fmt.pix.width = format.fmt.pix.width;
    setformat.fmt.pix.height = format.fmt.pix.height;

    struct v4l2_fmtdesc fmtdesc = {};
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmtdesc.index = 0;
    uint32_t best_fmt = 0;
    int best_rank = 1 << 30;  // large number

    // 注: 当前版本 esp_video 中 YUV422P 实际输出为 YUYV。
#if defined(CONFIG_XIAOZHI_ENABLE_ROTATE_CAMERA_IMAGE) && defined(CONFIG_SOC_PPA_SUPPORTED)
    auto get_rank = [](uint32_t fmt) -> int {
        switch (fmt) {
            case V4L2_PIX_FMT_RGB24:
                return 0;
            case V4L2_PIX_FMT_RGB565:
                return 1;
#ifdef CONFIG_XIAOZHI_ENABLE_HARDWARE_JPEG_ENCODER
            case V4L2_PIX_FMT_YUV420:  // 软件 JPEG 编码器不支持 YUV420 格式
                return 2;
#endif  // CONFIG_XIAOZHI_ENABLE_HARDWARE_JPEG_ENCODER
            case V4L2_PIX_FMT_GREY:
            case V4L2_PIX_FMT_YUV422P:
            default:
                return 1 << 29;  // unsupported
        }
    };
#else
    auto get_rank = [](uint32_t fmt) -> int {
        switch (fmt) {
            case V4L2_PIX_FMT_RGB565:
                return 10;
            case V4L2_PIX_FMT_YUV422P:
                return 11;
            case V4L2_PIX_FMT_RGB24:
                return 12;
#ifdef CONFIG_XIAOZHI_ENABLE_HARDWARE_JPEG_ENCODER
            case V4L2_PIX_FMT_YUV420:
                return 13;
#endif  // CONFIG_XIAOZHI_ENABLE_HARDWARE_JPEG_ENCODER
#ifdef CONFIG_XIAOZHI_CAMERA_ALLOW_JPEG_INPUT
            case V4L2_PIX_FMT_JPEG:
                return 5;
#endif  // CONFIG_XIAOZHI_CAMERA_ALLOW_JPEG_INPUT
            case V4L2_PIX_FMT_GREY:
                return 20;
            default:
                return 1 << 29;  // unsupported
        }
    };
#endif
    while (ioctl(video_fd_, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        ESP_LOGD(TAG, "VIDIOC_ENUM_FMT: pixelformat=0x%08lx, description=%s", fmtdesc.pixelformat, fmtdesc.description);
        CAM_PRINT_FOURCC(fmtdesc.pixelformat);
        int rank = get_rank(fmtdesc.pixelformat);
        if (rank < best_rank) {
            best_rank = rank;
            best_fmt = fmtdesc.pixelformat;
        }
        fmtdesc.index++;
    }
    if (best_rank < (1 << 29)) {
        setformat.fmt.pix.pixelformat = best_fmt;
        sensor_format_ = best_fmt;
    }

    if (!setformat.fmt.pix.pixelformat) {
        ESP_LOGE(TAG, "no supported pixel format found");
        close(video_fd_);
        video_fd_ = -1;
        sensor_format_ = 0;
        return;
    }

    ESP_LOGD(TAG, "selected pixel format: 0x%08lx", setformat.fmt.pix.pixelformat);

    if (ioctl(video_fd_, VIDIOC_S_FMT, &setformat) != 0) {
        ESP_LOGE(TAG, "VIDIOC_S_FMT failed, errno=%d(%s)", errno, strerror(errno));
        close(video_fd_);
        video_fd_ = -1;
        sensor_format_ = 0;
        return;
    }

    sensor_stride_ = setformat.fmt.pix.bytesperline;
    if (sensor_stride_ == 0 && sensor_format_ == V4L2_PIX_FMT_RGB565) {
        sensor_stride_ = (size_t)setformat.fmt.pix.width * 2;
    }
    ESP_LOGI(TAG, "Camera format: %ldx%ld fourcc=0x%08lx stride=%u size=%lu",
             setformat.fmt.pix.width, setformat.fmt.pix.height,
             setformat.fmt.pix.pixelformat, (unsigned)sensor_stride_,
             (unsigned long)setformat.fmt.pix.sizeimage);

#ifdef CONFIG_XIAOZHI_ENABLE_ROTATE_CAMERA_IMAGE
    frame_.width = setformat.fmt.pix.height;
    frame_.height = setformat.fmt.pix.width;
#else
    frame_.width = setformat.fmt.pix.width;
    frame_.height = setformat.fmt.pix.height;
#endif

    // 申请缓冲并mmap
    struct v4l2_requestbuffers req = {};
    req.count = strcmp(video_device_name, ESP_VIDEO_MIPI_CSI_DEVICE_NAME) == 0 ? 2 : 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(video_fd_, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "VIDIOC_REQBUFS failed");
        close(video_fd_);
        video_fd_ = -1;
        sensor_format_ = 0;
        return;
    }
    mmap_buffers_.resize(req.count);
    for (uint32_t i = 0; i < req.count; i++) {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(video_fd_, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(TAG, "VIDIOC_QUERYBUF failed");
            close(video_fd_);
            video_fd_ = -1;
            sensor_format_ = 0;
            return;
        }
        void* start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, video_fd_, buf.m.offset);
        if (start == MAP_FAILED) {
            ESP_LOGE(TAG, "mmap failed");
            close(video_fd_);
            video_fd_ = -1;
            sensor_format_ = 0;
            return;
        }
        mmap_buffers_[i].start = start;
        mmap_buffers_[i].length = buf.length;

        if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "VIDIOC_QBUF failed");
            close(video_fd_);
            video_fd_ = -1;
            sensor_format_ = 0;
            return;
        }
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(video_fd_, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "VIDIOC_STREAMON failed");
        close(video_fd_);
        video_fd_ = -1;
        sensor_format_ = 0;
        return;
    }

#ifdef CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE
    // 当启用 ISP 时，ISP 需要一些照片来初始化参数，因此开启后后台拍摄5s照片并丢弃
    xTaskCreate(
        [](void* arg) {
            EspVideo* self = static_cast<EspVideo*>(arg);
            uint16_t capture_count = 0;
            TickType_t start = xTaskGetTickCount();
            TickType_t duration = 5000 / portTICK_PERIOD_MS;  // 5s
            while ((xTaskGetTickCount() - start) < duration) {
                struct v4l2_buffer buf = {};
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                if (ioctl(self->video_fd_, VIDIOC_DQBUF, &buf) != 0) {
                    ESP_LOGE(TAG, "VIDIOC_DQBUF failed during init");
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                    continue;
                }
                if (ioctl(self->video_fd_, VIDIOC_QBUF, &buf) != 0) {
                    ESP_LOGE(TAG, "VIDIOC_QBUF failed during init");
                }
                capture_count++;
            }
            ESP_LOGI(TAG, "Camera init success, captured %d frames in %lums", capture_count,
                     (unsigned long)((xTaskGetTickCount() - start) * portTICK_PERIOD_MS));
            self->streaming_on_ = true;
            vTaskDelete(NULL);
        },
        "CameraInitTask", 4096, this, 5, nullptr);
#else
    ESP_LOGI(TAG, "Camera init success");
    streaming_on_ = true;
#endif  // CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE
}

EspVideo::~EspVideo() {
#ifdef CONFIG_IDF_TARGET_ESP32P4
    if (scaled_capture_ppa_client_ != nullptr) {
        (void)ppa_unregister_client(scaled_capture_ppa_client_);
        scaled_capture_ppa_client_ = nullptr;
    }
#endif
    if (streaming_on_ && video_fd_ >= 0) {
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(video_fd_, VIDIOC_STREAMOFF, &type);
    }
    for (auto& b : mmap_buffers_) {
        if (b.start && b.length) {
            munmap(b.start, b.length);
        }
    }
    if (video_fd_ >= 0) {
        close(video_fd_);
        video_fd_ = -1;
    }
    sensor_format_ = 0;
    esp_video_deinit();
}

void EspVideo::SetExplainUrl(const std::string& url, const std::string& token) {
    explain_url_ = url;
    explain_token_ = token;
}

bool EspVideo::Capture() {
    ForegroundCaptureGuard foreground_guard(foreground_capture_requests_);
    std::lock_guard<std::mutex> capture_lock(capture_mutex_);
    log_camera_memory("capture-start");

    if (!streaming_on_ || video_fd_ < 0) {
        return false;
    }

    for (int i = 0; i < 3; i++) {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(video_fd_, VIDIOC_DQBUF, &buf) != 0) {
            ESP_LOGE(TAG, "VIDIOC_DQBUF failed");
            return false;
        }
        if (i == 2) {
            // 保存帧副本到PSRAM
            if (frame_.data) {
                heap_caps_free(frame_.data);
                frame_.data = nullptr;
                frame_.format = 0;
            }
            frame_.len = buf.bytesused;
            frame_.data = (uint8_t*)heap_caps_malloc(frame_.len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!frame_.data) {
                ESP_LOGE(TAG, "alloc frame copy failed: need allocate %lu bytes", buf.bytesused);
                if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
                    ESP_LOGE(TAG, "Cleanup: VIDIOC_QBUF failed");
                }
                return false;
            }

#ifdef CONFIG_XIAOZHI_ENABLE_ROTATE_CAMERA_IMAGE
            ESP_LOGI(TAG, "Photo frame: mmap=%d sensor=%dx%d",
                     mmap_buffers_[buf.index].length, sensor_width_, sensor_height_);
#else
            ESP_LOGI(TAG, "Photo frame: mmap=%d output=%dx%d",
                     mmap_buffers_[buf.index].length, frame_.width, frame_.height);
#endif  // CONFIG_XIAOZHI_ENABLE_ROTATE_CAMERA_IMAGE
            ESP_LOG_BUFFER_HEXDUMP(TAG, mmap_buffers_[buf.index].start, MIN(mmap_buffers_[buf.index].length, 256),
                                   ESP_LOG_DEBUG);

            switch (sensor_format_) {
                case V4L2_PIX_FMT_RGB565:
                case V4L2_PIX_FMT_RGB24:
                case V4L2_PIX_FMT_YUYV:
                case V4L2_PIX_FMT_YUV420:
                case V4L2_PIX_FMT_GREY:
#ifdef CONFIG_XIAOZHI_CAMERA_ALLOW_JPEG_INPUT
                case V4L2_PIX_FMT_JPEG:
#endif  // CONFIG_XIAOZHI_CAMERA_ALLOW_JPEG_INPUT
#ifdef CONFIG_XIAOZHI_ENABLE_CAMERA_ENDIANNESS_SWAP
                {
                    auto src16 = (uint16_t*)mmap_buffers_[buf.index].start;
                    auto dst16 = (uint16_t*)frame_.data;
                    size_t count = MIN(mmap_buffers_[buf.index].length, frame_.len) / 2;
                    for (size_t i = 0; i < count; i++) {
                        dst16[i] = __builtin_bswap16(src16[i]);
                    }
                }
#else
                    memcpy(frame_.data, mmap_buffers_[buf.index].start,
                           MIN(mmap_buffers_[buf.index].length, frame_.len));
#endif  // CONFIG_XIAOZHI_ENABLE_CAMERA_ENDIANNESS_SWAP
                    frame_.format = sensor_format_;
                    break;
                case V4L2_PIX_FMT_YUV422P: {
                    // 这个格式是 422 YUYV，不是 planer
                    frame_.format = V4L2_PIX_FMT_YUYV;
#ifdef CONFIG_XIAOZHI_ENABLE_CAMERA_ENDIANNESS_SWAP
                    {
                        auto src16 = (uint16_t*)mmap_buffers_[buf.index].start;
                        auto dst16 = (uint16_t*)frame_.data;
                        size_t count = (size_t)mmap_buffers_[buf.index].length / 2;
                        for (size_t i = 0; i < count; i++) {
                            dst16[i] = __builtin_bswap16(src16[i]);
                        }
                    }
#else
                    memcpy(frame_.data, mmap_buffers_[buf.index].start,
                           MIN(mmap_buffers_[buf.index].length, frame_.len));
#endif  // CONFIG_XIAOZHI_ENABLE_CAMERA_ENDIANNESS_SWAP
                    break;
                }
                case V4L2_PIX_FMT_RGB565X: {
                    // 大端序的 RGB565 需要转换为小端序
                    // 目前 esp_video 的大小端都会返回格式为 RGB565，不会返回格式为 RGB565X，此 case 用于未来版本兼容
                    auto src16 = (uint16_t*)mmap_buffers_[buf.index].start;
                    auto dst16 = (uint16_t*)frame_.data;
                    size_t pixel_count = (size_t)frame_.width * (size_t)frame_.height;
                    for (size_t i = 0; i < pixel_count; i++) {
                        dst16[i] = __builtin_bswap16(src16[i]);
                    }
                    frame_.format = V4L2_PIX_FMT_RGB565;
                    break;
                }
                default:
                    ESP_LOGE(TAG, "unsupported sensor format: 0x%08lx", sensor_format_);
                    if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
                        ESP_LOGE(TAG, "Cleanup: VIDIOC_QBUF failed");
                    }
                    return false;
            }

#ifdef CONFIG_XIAOZHI_ENABLE_ROTATE_CAMERA_IMAGE
#ifndef CONFIG_SOC_PPA_SUPPORTED
            uint8_t* rotate_dst =
                (uint8_t*)heap_caps_aligned_alloc(64, frame_.len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (rotate_dst == nullptr) {
                ESP_LOGE(TAG, "Failed to allocate memory for rotate image");
                if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
                    ESP_LOGE(TAG, "Cleanup: VIDIOC_QBUF failed");
                }
                return false;
            }
            uint8_t* rotate_src = (uint8_t*)frame_.data;

            esp_imgfx_rotate_cfg_t rotate_cfg = {
                .in_res =
                    {
                        .width = static_cast<int16_t>(sensor_width_),
                        .height = static_cast<int16_t>(sensor_height_),
                    },
                .degree = IMAGE_ROTATION_ANGLE,
            };
            switch (frame_.format) {
                case V4L2_PIX_FMT_RGB565:
                    rotate_cfg.in_pixel_fmt = ESP_IMGFX_PIXEL_FMT_RGB565_LE;
                    break;
                case V4L2_PIX_FMT_YUYV:
                    rotate_cfg.in_pixel_fmt = ESP_IMGFX_PIXEL_FMT_RGB565_LE;
                    break;
                case V4L2_PIX_FMT_GREY:
                    rotate_cfg.in_pixel_fmt = ESP_IMGFX_PIXEL_FMT_Y;
                    break;
                case V4L2_PIX_FMT_RGB24:
                    rotate_cfg.in_pixel_fmt = ESP_IMGFX_PIXEL_FMT_RGB888;
                    break;
                default:
                    ESP_LOGE(TAG, "unsupported sensor format: 0x%08lx", sensor_format_);
                    if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
                        ESP_LOGE(TAG, "Cleanup: VIDIOC_QBUF failed");
                    }
                    return false;
            }
            esp_imgfx_rotate_handle_t rotate_handle = nullptr;
            esp_imgfx_err_t imgfx_err = esp_imgfx_rotate_open(&rotate_cfg, &rotate_handle);
            if (imgfx_err != ESP_IMGFX_ERR_OK || rotate_handle == nullptr) {
                ESP_LOGE(TAG, "esp_imgfx_rotate_create failed");
                if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
                    ESP_LOGE(TAG, "Cleanup: VIDIOC_QBUF failed");
                }
                return false;
            }

            esp_imgfx_data_t rotate_input_data = {
                .data = rotate_src,
                .data_len = frame_.len,
            };
            esp_imgfx_data_t rotate_output_data = {
                .data = rotate_dst,
                .data_len = frame_.len,
            };

            imgfx_err = esp_imgfx_rotate_process(rotate_handle, &rotate_input_data, &rotate_output_data);
            if (imgfx_err != ESP_IMGFX_ERR_OK) {
                ESP_LOGE(TAG, "esp_imgfx_rotate_process failed");
                heap_caps_free(rotate_dst);
                rotate_dst = nullptr;
                if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
                    ESP_LOGE(TAG, "Cleanup: VIDIOC_QBUF failed");
                }
                esp_imgfx_rotate_close(rotate_handle);
                rotate_handle = nullptr;
                return false;
            }

            frame_.data = rotate_dst;

            heap_caps_free(rotate_src);
            rotate_src = nullptr;

            esp_imgfx_rotate_close(rotate_handle);
            rotate_handle = nullptr;
#else   // CONFIG_SOC_PPA_SUPPORTED
            uint8_t* rotate_src = nullptr;

            ppa_srm_color_mode_t ppa_color_mode;
            switch (frame_.format) {
                case V4L2_PIX_FMT_RGB565:
                    rotate_src = (uint8_t*)frame_.data;
                    ppa_color_mode = PPA_SRM_COLOR_MODE_RGB565;
                    break;
                case V4L2_PIX_FMT_RGB24:
                    rotate_src = (uint8_t*)frame_.data;
                    ppa_color_mode = PPA_SRM_COLOR_MODE_RGB888;
                    break;
                case V4L2_PIX_FMT_YUYV: {
                    ESP_LOGW(TAG, "YUYV format is not supported for PPA rotation, using software conversion to RGB888");
                    rotate_src = (uint8_t*)heap_caps_malloc(frame_.width * frame_.height * 3,
                                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                    if (rotate_src == nullptr) {
                        ESP_LOGE(TAG, "Failed to allocate memory for rotate image");
                        if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
                            ESP_LOGE(TAG, "Cleanup: VIDIOC_QBUF failed");
                        }
                        return false;
                    }
                    esp_imgfx_color_convert_cfg_t convert_cfg = {
                        .in_res = {.width = static_cast<int16_t>(frame_.width),
                                   .height = static_cast<int16_t>(frame_.height)},
                        .in_pixel_fmt = ESP_IMGFX_PIXEL_FMT_YUYV,
                        .out_pixel_fmt = ESP_IMGFX_PIXEL_FMT_RGB888,
                    };
                    esp_imgfx_color_convert_handle_t convert_handle = nullptr;
                    esp_imgfx_err_t err = esp_imgfx_color_convert_open(&convert_cfg, &convert_handle);
                    if (err != ESP_IMGFX_ERR_OK || convert_handle == nullptr) {
                        ESP_LOGE(TAG, "esp_imgfx_color_convert_open failed");
                        heap_caps_free(rotate_src);
                        rotate_src = nullptr;
                        if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
                            ESP_LOGE(TAG, "Cleanup: VIDIOC_QBUF failed");
                        }
                        return false;
                    }
                    esp_imgfx_data_t convert_input_data = {
                        .data = frame_.data,
                        .data_len = frame_.len,
                    };
                    esp_imgfx_data_t convert_output_data = {
                        .data = rotate_src,
                        .data_len = static_cast<uint32_t>(frame_.width * frame_.height * 3),
                    };
                    err = esp_imgfx_color_convert_process(convert_handle, &convert_input_data, &convert_output_data);
                    if (err != ESP_IMGFX_ERR_OK) {
                        ESP_LOGE(TAG, "esp_imgfx_color_convert_process failed");
                        heap_caps_free(rotate_src);
                        rotate_src = nullptr;
                        esp_imgfx_color_convert_close(convert_handle);
                        convert_handle = nullptr;
                        if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
                            ESP_LOGE(TAG, "Cleanup: VIDIOC_QBUF failed");
                        }
                        return false;
                    }
                    esp_imgfx_color_convert_close(convert_handle);
                    convert_handle = nullptr;
                    ppa_color_mode = PPA_SRM_COLOR_MODE_RGB888;
                    heap_caps_free(frame_.data);
                    frame_.data = rotate_src;
                    frame_.len = frame_.width * frame_.height * 3;
                    break;
                }
                default:
                    ESP_LOGE(TAG, "unsupported sensor format for PPA rotation: 0x%08lx", sensor_format_);
                    if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
                        ESP_LOGE(TAG, "Cleanup: VIDIOC_QBUF failed");
                    }
                    return false;
            }

            uint8_t* rotate_dst = (uint8_t*)heap_caps_malloc(
                frame_.width * frame_.height * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT | MALLOC_CAP_CACHE_ALIGNED);
            if (rotate_dst == nullptr) {
                ESP_LOGE(TAG, "Failed to allocate memory for rotate image");
                if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
                    ESP_LOGE(TAG, "Cleanup: VIDIOC_QBUF failed");
                }
                return false;
            }

            ppa_client_handle_t ppa_client = nullptr;
            ppa_client_config_t client_cfg = {
                .oper_type = PPA_OPERATION_SRM,
                .max_pending_trans_num = 1,
            };
            esp_err_t err = ppa_register_client(&client_cfg, &ppa_client);
            if (err != ESP_OK || ppa_client == nullptr) {
                ESP_LOGE(TAG, "ppa_register_client failed: %d", (int)err);
                heap_caps_free(rotate_dst);
                rotate_dst = nullptr;
                if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
                    ESP_LOGE(TAG, "Cleanup: VIDIOC_QBUF failed");
                }
                return false;
            }

            ppa_srm_rotation_angle_t ppa_angle = IMAGE_ROTATION_ANGLE;

            ppa_srm_oper_config_t srm_cfg = {};
            srm_cfg.in.buffer = (void*)rotate_src;
            srm_cfg.in.pic_w = sensor_width_;
            srm_cfg.in.pic_h = sensor_height_;
            srm_cfg.in.block_w = sensor_width_;
            srm_cfg.in.block_h = sensor_height_;
            srm_cfg.in.block_offset_x = 0;
            srm_cfg.in.block_offset_y = 0;
            srm_cfg.in.srm_cm = ppa_color_mode;

            srm_cfg.out.buffer = (void*)rotate_dst;
            srm_cfg.out.buffer_size = frame_.len;
            srm_cfg.out.pic_w = frame_.width;
            srm_cfg.out.pic_h = frame_.height;
            srm_cfg.out.block_offset_x = 0;
            srm_cfg.out.block_offset_y = 0;
            srm_cfg.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;

            // 等比例缩放 1.0
            srm_cfg.scale_x = 1.0f;
            srm_cfg.scale_y = 1.0f;
            srm_cfg.rotation_angle = ppa_angle;
            srm_cfg.mode = PPA_TRANS_MODE_BLOCKING;
            srm_cfg.user_data = nullptr;

            err = ppa_do_scale_rotate_mirror(ppa_client, &srm_cfg);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "ppa_do_scale_rotate_mirror failed: %d", (int)err);
                heap_caps_free(rotate_dst);
                rotate_dst = nullptr;
                (void)ppa_unregister_client(ppa_client);
                if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
                    ESP_LOGE(TAG, "Cleanup: VIDIOC_QBUF failed");
                }
                return false;
            }

            (void)ppa_unregister_client(ppa_client);

            frame_.data = rotate_dst;
            frame_.len = frame_.width * frame_.height * 2;
            frame_.format = V4L2_PIX_FMT_RGB565;
            heap_caps_free(rotate_src);
            rotate_src = nullptr;
#endif  // CONFIG_SOC_PPA_SUPPORTED
#endif  // CONFIG_XIAOZHI_ENABLE_ROTATE_CAMERA_IMAGE
        }

        if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "VIDIOC_QBUF failed");
        }
    }

    // 显示预览图片
    auto display = dynamic_cast<LvglDisplay*>(Board::GetInstance().GetDisplay());
    if (display != nullptr) {
        if (!frame_.data) {
            ESP_LOGE(TAG, "frame.data is null");
            return false;
        }
        uint16_t w = frame_.width;
        uint16_t h = frame_.height;
        size_t lvgl_image_size = frame_.len;
        size_t stride = ((w * 2) + 3) & ~3;  // 4字节对齐
        lv_color_format_t color_format = LV_COLOR_FORMAT_RGB565;
        uint8_t* data = nullptr;

        switch (frame_.format) {
            // LVGL 显示 YUV 系的图像似乎都有问题，暂时转换为 RGB565 显示
            case V4L2_PIX_FMT_YUYV:
            case V4L2_PIX_FMT_YUV420:
            case V4L2_PIX_FMT_RGB24: {
                color_format = LV_COLOR_FORMAT_RGB565;
                data = (uint8_t*)heap_caps_malloc(w * h * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                if (data == nullptr) {
                    ESP_LOGE(TAG, "Failed to allocate memory for preview image");
                    return false;
                }
                esp_imgfx_color_convert_cfg_t convert_cfg = {
                    .in_res = {.width = static_cast<int16_t>(frame_.width),
                               .height = static_cast<int16_t>(frame_.height)},
                    .in_pixel_fmt = static_cast<esp_imgfx_pixel_fmt_t>(frame_.format),
                    .out_pixel_fmt = ESP_IMGFX_PIXEL_FMT_RGB565_LE,
                    .color_space_std = ESP_IMGFX_COLOR_SPACE_STD_BT601,
                };
                esp_imgfx_color_convert_handle_t convert_handle = nullptr;
                esp_imgfx_err_t err = esp_imgfx_color_convert_open(&convert_cfg, &convert_handle);
                if (err != ESP_IMGFX_ERR_OK || convert_handle == nullptr) {
                    ESP_LOGE(TAG, "esp_imgfx_color_convert_open failed");
                    heap_caps_free(data);
                    data = nullptr;
                    return false;
                }
                esp_imgfx_data_t convert_input_data = {
                    .data = frame_.data,
                    .data_len = frame_.len,
                };
                esp_imgfx_data_t convert_output_data = {
                    .data = data,
                    .data_len = static_cast<uint32_t>(w * h * 2),
                };
                err = esp_imgfx_color_convert_process(convert_handle, &convert_input_data, &convert_output_data);
                if (err != ESP_IMGFX_ERR_OK) {
                    ESP_LOGE(TAG, "esp_imgfx_color_convert_process failed");
                    heap_caps_free(data);
                    data = nullptr;
                    esp_imgfx_color_convert_close(convert_handle);
                    convert_handle = nullptr;
                    return false;
                }
                esp_imgfx_color_convert_close(convert_handle);
                convert_handle = nullptr;
                lvgl_image_size = w * h * 2;
                break;
            }

            case V4L2_PIX_FMT_RGB565:
                data = (uint8_t*)heap_caps_malloc(w * h * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                if (data == nullptr) {
                    ESP_LOGE(TAG, "Failed to allocate memory for preview image");
                    return false;
                }
                memcpy(data, frame_.data, frame_.len);
                lvgl_image_size = frame_.len;  // fallthrough 时兼顾 YUYV 与 RGB565
                break;

#ifdef CONFIG_XIAOZHI_CAMERA_ALLOW_JPEG_INPUT
            case V4L2_PIX_FMT_JPEG: {
                uint8_t* out_data = nullptr;  // out data is allocated by jpeg_to_image
                size_t out_len = 0;
                size_t out_width = 0;
                size_t out_height = 0;
                size_t out_stride = 0;

                esp_err_t ret =
                    jpeg_to_image(frame_.data, frame_.len, &out_data, &out_len, &out_width, &out_height, &out_stride);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to decode JPEG image: %d (%s)", (int)ret, esp_err_to_name(ret));
                    if (out_data) {
                        heap_caps_free(out_data);
                        out_data = nullptr;
                    }
                    return false;
                }

                data = out_data;
                w = out_width;
                h = out_height;
                lvgl_image_size = out_len;
                stride = out_stride;
                break;
            }
#endif
            default:
                ESP_LOGE(TAG, "unsupported frame format: 0x%08lx", frame_.format);
                return false;
        }

        auto image = std::make_unique<LvglAllocatedImage>(data, lvgl_image_size, w, h, stride, color_format);
        display->SetPreviewImage(std::move(image));
    }
    log_camera_memory("capture-done");
    return true;
}

bool EspVideo::SetHMirror(bool enabled) {
    if (video_fd_ < 0)
        return false;
    struct v4l2_ext_controls ctrls = {};
    struct v4l2_ext_control ctrl = {};
    ctrl.id = V4L2_CID_HFLIP;
    ctrl.value = enabled ? 1 : 0;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_USER;
    ctrls.count = 1;
    ctrls.controls = &ctrl;
    if (ioctl(video_fd_, VIDIOC_S_EXT_CTRLS, &ctrls) != 0) {
        ESP_LOGE(TAG, "set HFLIP failed");
        return false;
    }
    return true;
}

bool EspVideo::SetVFlip(bool enabled) {
    if (video_fd_ < 0)
        return false;
    struct v4l2_ext_controls ctrls = {};
    struct v4l2_ext_control ctrl = {};
    ctrl.id = V4L2_CID_VFLIP;
    ctrl.value = enabled ? 1 : 0;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_USER;
    ctrls.count = 1;
    ctrls.controls = &ctrl;
    if (ioctl(video_fd_, VIDIOC_S_EXT_CTRLS, &ctrls) != 0) {
        ESP_LOGE(TAG, "set VFLIP failed");
        return false;
    }
    return true;
}

/**
 * @brief 将摄像头捕获的图像发送到远程服务器进行AI分析和解释
 *
 * 该函数将当前摄像头缓冲区中的图像编码为JPEG格式，并通过HTTP POST请求
 * 以multipart/form-data的形式发送到指定的解释服务器。服务器将根据提供的
 * 问题对图像进行AI分析并返回结果。
 *
 * 实现特点：
 * - 将当前帧一次性编码到PSRAM中的JPEG缓冲区
 * - 使用分块传输编码(chunked transfer encoding)上传
 * - 支持设备ID、客户端ID和认证令牌的HTTP头部配置
 *
 * @param question 要向AI提出的关于图像的问题，将作为表单字段发送
 * @return std::string 服务器返回的JSON格式响应字符串
 *         成功时包含AI分析结果，失败时包含错误信息
 *         格式示例：{"success": true, "result": "分析结果"}
 *                  {"success": false, "message": "错误信息"}
 *
 * @note 调用此函数前必须先调用SetExplainUrl()设置服务器URL
 * @warning 如果摄像头缓冲区为空或网络连接失败，将返回错误信息
 */
std::string EspVideo::Explain(const std::string& question) {
    ForegroundCaptureGuard foreground_guard(foreground_capture_requests_);
    if (explain_url_.empty()) {
        throw std::runtime_error("Image explain URL or token is not set");
    }

    if (!camera_dma_ready("photo-preflight")) {
        throw std::runtime_error("Camera DMA memory is busy, please try again");
    }
#if CONFIG_XIAOZHI_ENABLE_HARDWARE_JPEG_ENCODER
    if (!image_to_jpeg_init()) {
        throw std::runtime_error("Camera JPEG engine is unavailable");
    }
#endif

    uint8_t* jpeg_data = nullptr;
    size_t jpeg_len = 0;
    size_t source_len = 0;
    {
        std::lock_guard<std::mutex> capture_lock(capture_mutex_);
        if (frame_.data == nullptr || frame_.len == 0) {
            throw std::runtime_error("Camera frame is empty");
        }

        uint16_t w = frame_.width ? frame_.width : 320;
        uint16_t h = frame_.height ? frame_.height : 240;
        v4l2_pix_fmt_t enc_fmt = frame_.format;
        source_len = frame_.len;
        ESP_LOGI(TAG, "Photo encode start: source=%u", (unsigned)source_len);
        log_camera_memory("encode-start");

        bool encoded = image_to_jpeg(frame_.data, frame_.len, w, h, enc_fmt, 80,
                                     &jpeg_data, &jpeg_len);
        heap_caps_free(frame_.data);
        frame_.data = nullptr;
        frame_.len = 0;
        frame_.format = 0;

        if (!encoded || jpeg_data == nullptr || jpeg_len == 0) {
            if (jpeg_data != nullptr) {
                heap_caps_free(jpeg_data);
            }
            throw std::runtime_error("Failed to encode image to JPEG");
        }
        ESP_LOGI(TAG, "Photo encode done: jpeg=%u raw frame released", (unsigned)jpeg_len);
        log_camera_memory("encode-done");
    }
    std::unique_ptr<uint8_t, decltype(&heap_caps_free)> jpeg_guard(jpeg_data, heap_caps_free);

    if (!camera_dma_ready("upload-preflight")) {
        throw std::runtime_error("Camera network memory is busy, please try again");
    }

    std::string boundary = "----ESP32_CAMERA_BOUNDARY";
    std::string multipart_header;
    multipart_header += "--" + boundary + "\r\n";
    multipart_header += "Content-Disposition: form-data; name=\"question\"\r\n\r\n";
    multipart_header += question + "\r\n";
    multipart_header += "--" + boundary + "\r\n";
    multipart_header += "Content-Disposition: form-data; name=\"file\"; filename=\"camera.jpg\"\r\n";
    multipart_header += "Content-Type: image/jpeg\r\n\r\n";
    std::string multipart_footer = "\r\n--" + boundary + "--\r\n";
    std::string result;

    ESP_LOGI(TAG, "Photo upload start: jpeg=%u", (unsigned)jpeg_len);
    log_camera_memory("upload-start");

#ifdef CONFIG_ESP_HOSTED_ENABLED
    esp_http_client_config_t config = {};
    config.url = explain_url_.c_str();
    config.timeout_ms = 15000;
    config.buffer_size = 1024;
    config.buffer_size_tx = 1024;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    std::unique_ptr<esp_http_client, decltype(&esp_http_client_cleanup)> http(
        esp_http_client_init(&config), esp_http_client_cleanup);
    if (!http) {
        throw std::runtime_error("Failed to create photo HTTP client");
    }

    std::string content_type = "multipart/form-data; boundary=" + boundary;
    std::string device_id = SystemInfo::GetMacAddress();
    std::string client_id = Board::GetInstance().GetUuid();
    esp_http_client_set_method(http.get(), HTTP_METHOD_POST);
    esp_http_client_set_header(http.get(), "Content-Type", content_type.c_str());
    esp_http_client_set_header(http.get(), "Device-Id", device_id.c_str());
    esp_http_client_set_header(http.get(), "Client-Id", client_id.c_str());
    if (!explain_token_.empty()) {
        std::string authorization = "Bearer " + explain_token_;
        esp_http_client_set_header(http.get(), "Authorization", authorization.c_str());
    }

    size_t body_len = multipart_header.size() + jpeg_len + multipart_footer.size();
    if (esp_http_client_open(http.get(), static_cast<int>(body_len)) != ESP_OK) {
        throw std::runtime_error("Failed to connect to explain URL");
    }
    vTaskDelay(pdMS_TO_TICKS(15));

    // The generic HTTP wrapper creates a 4 KB internal receive task. The IDF client is
    // synchronous, so the camera worker's external stack handles both upload and reply.
    const auto write_http = [&http](const char* data, size_t len) {
        constexpr size_t kUploadChunkSize = 1024;
        size_t offset = 0;
        while (offset < len) {
            size_t chunk_len = std::min(kUploadChunkSize, len - offset);
            int written = esp_http_client_write(http.get(), data + offset,
                                                static_cast<int>(chunk_len));
            if (written <= 0) {
                throw std::runtime_error("Failed to upload photo data");
            }
            offset += static_cast<size_t>(written);
            // ESP-Hosted uses 1536-byte internal DMA blocks. Let the SDIO TX callback
            // return each block before queuing the next one.
            vTaskDelay(pdMS_TO_TICKS(15));
        }
    };

    write_http(multipart_header.data(), multipart_header.size());
    write_http(reinterpret_cast<const char*>(jpeg_data), jpeg_len);
    write_http(multipart_footer.data(), multipart_footer.size());

    if (esp_http_client_fetch_headers(http.get()) < 0) {
        throw std::runtime_error("Failed to read photo response headers");
    }
    int status_code = esp_http_client_get_status_code(http.get());
    if (status_code != 200) {
        ESP_LOGE(TAG, "Failed to upload photo, status code: %d", status_code);
        throw std::runtime_error("Failed to upload photo");
    }

    char response_buffer[512];
    while (true) {
        int read = esp_http_client_read(http.get(), response_buffer, sizeof(response_buffer));
        if (read < 0) {
            throw std::runtime_error("Failed to read photo response");
        }
        if (read == 0) {
            break;
        }
        result.append(response_buffer, static_cast<size_t>(read));
    }
    esp_http_client_close(http.get());
#else
    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(3);
    http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    http->SetHeader("Client-Id", Board::GetInstance().GetUuid().c_str());
    if (!explain_token_.empty()) {
        http->SetHeader("Authorization", "Bearer " + explain_token_);
    }
    http->SetTimeout(15000);
    http->SetHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    http->SetHeader("Transfer-Encoding", "chunked");
    if (!http->Open("POST", explain_url_)) {
        throw std::runtime_error("Failed to connect to explain URL");
    }

    const auto write_http = [&http](const char* data, size_t len) {
        int written = http->Write(data, len);
        if (written < 0 || (len > 0 && static_cast<size_t>(written) < len)) {
            throw std::runtime_error("Failed to upload photo data");
        }
    };
    write_http(multipart_header.data(), multipart_header.size());
    write_http(reinterpret_cast<const char*>(jpeg_data), jpeg_len);
    write_http(multipart_footer.data(), multipart_footer.size());
    write_http("", 0);

    int status_code = http->GetStatusCode();
    if (status_code != 200) {
        ESP_LOGE(TAG, "Failed to upload photo, status code: %d", status_code);
        throw std::runtime_error("Failed to upload photo");
    }
    result = http->ReadAll();
    http->Close();
#endif

    ESP_LOGI(TAG, "Photo upload done");
    log_camera_memory("upload-done");

    // Get remain task stack size
    size_t remain_stack_size = uxTaskGetStackHighWaterMark(nullptr);
    ESP_LOGI(TAG, "Explain image size=%d bytes, compressed size=%d, remain stack size=%d, question=%s\n%s",
             (int)source_len, (int)jpeg_len, (int)remain_stack_size, question.c_str(), result.c_str());
    return result;
}

bool EspVideo::CaptureFrame(CapturedFrame& frame) {
    ForegroundCaptureGuard foreground_guard(foreground_capture_requests_);
    std::lock_guard<std::mutex> capture_lock(capture_mutex_);

    frame = {};

    if (!streaming_on_ || video_fd_ < 0) {
        ESP_LOGE(TAG, "CaptureFrame: stream not on or fd invalid");
        return false;
    }

#ifdef CONFIG_XIAOZHI_ENABLE_ROTATE_CAMERA_IMAGE
    const int capture_width = sensor_width_;
    const int capture_height = sensor_height_;
#else
    const int capture_width = frame_.width;
    const int capture_height = frame_.height;
#endif

    for (int attempt = 0; attempt < 3; ++attempt) {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(video_fd_, VIDIOC_DQBUF, &buf) != 0) {
            ESP_LOGE(TAG, "CaptureFrame: VIDIOC_DQBUF failed");
            return false;
        }

        const auto requeue_buffer = [this, &buf]() {
            if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
                ESP_LOGE(TAG, "CaptureFrame: VIDIOC_QBUF failed");
                return false;
            }
            return true;
        };

        if (buf.index >= mmap_buffers_.size()) {
            ESP_LOGE(TAG, "CaptureFrame: invalid buffer index %lu", (unsigned long)buf.index);
            (void)requeue_buffer();
            return false;
        }

        const auto &mmap_buf = mmap_buffers_[buf.index];
        const size_t available_len = buf.bytesused > 0 ? buf.bytesused : mmap_buf.length;
        size_t output_len = available_len;
        size_t source_stride = sensor_stride_;

        if (sensor_format_ == V4L2_PIX_FMT_RGB565) {
            const size_t row_len = (size_t)capture_width * 2;
            if (source_stride < row_len) {
                source_stride = row_len;
            }
            const size_t required_len = source_stride * capture_height;
            output_len = row_len * capture_height;
            if (available_len < required_len || mmap_buf.length < required_len) {
                ESP_LOGW(TAG,
                         "CaptureFrame: discard incomplete RGB565 frame attempt=%d bytes=%u required=%u mmap=%u",
                         attempt + 1, (unsigned)available_len, (unsigned)required_len,
                         (unsigned)mmap_buf.length);
                if (!requeue_buffer()) {
                    return false;
                }
                continue;
            }
        } else if (available_len == 0 || available_len > mmap_buf.length) {
            ESP_LOGW(TAG, "CaptureFrame: discard invalid frame attempt=%d bytes=%u mmap=%u",
                     attempt + 1, (unsigned)available_len, (unsigned)mmap_buf.length);
            if (!requeue_buffer()) {
                return false;
            }
            continue;
        }

        frame.data = (uint8_t*)heap_caps_malloc(output_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!frame.data) {
            ESP_LOGE(TAG, "CaptureFrame: alloc failed, need %u bytes", (unsigned)output_len);
            (void)requeue_buffer();
            return false;
        }

        if (sensor_format_ == V4L2_PIX_FMT_RGB565) {
            const size_t row_len = (size_t)capture_width * 2;
            for (int y = 0; y < capture_height; ++y) {
                const uint8_t *src_row = static_cast<const uint8_t *>(mmap_buf.start) + y * source_stride;
                uint8_t *dst_row = frame.data + y * row_len;
#ifdef CONFIG_XIAOZHI_ENABLE_CAMERA_ENDIANNESS_SWAP
                const auto *src16 = reinterpret_cast<const uint16_t *>(src_row);
                auto *dst16 = reinterpret_cast<uint16_t *>(dst_row);
                for (int x = 0; x < capture_width; ++x) {
                    dst16[x] = __builtin_bswap16(src16[x]);
                }
#else
                memcpy(dst_row, src_row, row_len);
#endif
            }
        } else {
            memcpy(frame.data, mmap_buf.start, output_len);
        }

        frame.len = output_len;
        frame.format = sensor_format_;
        frame.width = capture_width;
        frame.height = capture_height;

        if (!requeue_buffer()) {
            heap_caps_free(frame.data);
            frame = {};
            return false;
        }

        ESP_LOGD(TAG, "CaptureFrame: %dx%d, fmt=0x%x, len=%lu",
                 frame.width, frame.height, (unsigned)frame.format, (unsigned long)frame.len);
        return true;
    }

    ESP_LOGE(TAG, "CaptureFrame: no complete frame after retries");
    return false;
}

EspVideo::ScaledCaptureResult EspVideo::TryCaptureScaledRgb565(
    uint8_t* output, size_t output_len, int output_width, int output_height) {
#ifndef CONFIG_IDF_TARGET_ESP32P4
    (void)output;
    (void)output_len;
    (void)output_width;
    (void)output_height;
    return ScaledCaptureResult::UnsupportedFormat;
#else
    if (output == nullptr || output_width <= 0 || output_height <= 0) {
        return ScaledCaptureResult::InvalidArgument;
    }

    const size_t required_output_len =
        static_cast<size_t>(output_width) * static_cast<size_t>(output_height) * 2;
    if (output_len < required_output_len || (reinterpret_cast<uintptr_t>(output) & 0x3F) != 0) {
        return ScaledCaptureResult::InvalidArgument;
    }
    if (!streaming_on_ || video_fd_ < 0) {
        return ScaledCaptureResult::NotReady;
    }
    if (sensor_format_ != V4L2_PIX_FMT_RGB565) {
        return ScaledCaptureResult::UnsupportedFormat;
    }
    if (foreground_capture_requests_.load(std::memory_order_acquire) != 0) {
        return ScaledCaptureResult::Busy;
    }

    std::unique_lock<std::mutex> capture_lock(capture_mutex_, std::try_to_lock);
    if (!capture_lock.owns_lock() ||
        foreground_capture_requests_.load(std::memory_order_acquire) != 0) {
        return ScaledCaptureResult::Busy;
    }

    const size_t row_len = static_cast<size_t>(sensor_width_) * 2;
    const size_t source_stride = std::max(sensor_stride_, row_len);
    if (sensor_width_ == 0 || sensor_height_ == 0 || source_stride % 2 != 0) {
        return ScaledCaptureResult::Error;
    }

    if (scaled_capture_ppa_client_ == nullptr) {
        ppa_client_config_t client_config = {
            .oper_type = PPA_OPERATION_SRM,
            .max_pending_trans_num = 1,
        };
        esp_err_t err = ppa_register_client(&client_config, &scaled_capture_ppa_client_);
        if (err != ESP_OK || scaled_capture_ppa_client_ == nullptr) {
            ESP_LOGE(TAG, "Gesture capture: PPA client registration failed: %s",
                     esp_err_to_name(err));
            scaled_capture_ppa_client_ = nullptr;
            return ScaledCaptureResult::Error;
        }
    }

    struct v4l2_buffer buffer = {};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    if (ioctl(video_fd_, VIDIOC_DQBUF, &buffer) != 0) {
        ESP_LOGW(TAG, "Gesture capture: VIDIOC_DQBUF failed, errno=%d", errno);
        return ScaledCaptureResult::Error;
    }

    const auto requeue = [this, &buffer]() {
        if (ioctl(video_fd_, VIDIOC_QBUF, &buffer) != 0) {
            ESP_LOGE(TAG, "Gesture capture: VIDIOC_QBUF failed, errno=%d", errno);
            return false;
        }
        return true;
    };

    if (buffer.index >= mmap_buffers_.size()) {
        ESP_LOGE(TAG, "Gesture capture: invalid MMAP index %lu",
                 static_cast<unsigned long>(buffer.index));
        (void)requeue();
        return ScaledCaptureResult::Error;
    }

    const auto& mmap_buffer = mmap_buffers_[buffer.index];
    const size_t required_source_len = source_stride * sensor_height_;
    const size_t available_len = buffer.bytesused > 0 ? buffer.bytesused : mmap_buffer.length;
    if (mmap_buffer.start == nullptr || mmap_buffer.length < required_source_len ||
        available_len < required_source_len) {
        ESP_LOGW(TAG, "Gesture capture: incomplete frame bytes=%u required=%u mmap=%u",
                 static_cast<unsigned>(available_len),
                 static_cast<unsigned>(required_source_len),
                 static_cast<unsigned>(mmap_buffer.length));
        (void)requeue();
        return ScaledCaptureResult::Error;
    }

    ppa_srm_oper_config_t operation = {};
    operation.in.buffer = mmap_buffer.start;
    operation.in.pic_w = source_stride / 2;
    operation.in.pic_h = sensor_height_;
    operation.in.block_w = sensor_width_;
    operation.in.block_h = sensor_height_;
    operation.in.block_offset_x = 0;
    operation.in.block_offset_y = 0;
    operation.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
    operation.out.buffer = output;
    operation.out.buffer_size = required_output_len;
    operation.out.pic_w = output_width;
    operation.out.pic_h = output_height;
    operation.out.block_offset_x = 0;
    operation.out.block_offset_y = 0;
    operation.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
    operation.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
    operation.scale_x = static_cast<float>(output_width) / sensor_width_;
    operation.scale_y = static_cast<float>(output_height) / sensor_height_;
#ifdef CONFIG_XIAOZHI_ENABLE_CAMERA_ENDIANNESS_SWAP
    operation.byte_swap = true;
#endif
    operation.mode = PPA_TRANS_MODE_BLOCKING;

    const esp_err_t ppa_result =
        ppa_do_scale_rotate_mirror(scaled_capture_ppa_client_, &operation);
    const bool requeued = requeue();
    if (ppa_result != ESP_OK || !requeued) {
        ESP_LOGW(TAG, "Gesture capture: PPA scale failed: %s",
                 esp_err_to_name(ppa_result));
        return ScaledCaptureResult::Error;
    }
    return ScaledCaptureResult::Ok;
#endif
}
