#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "load_png.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <image.png>\n", argv[0]);
        return 1;
    }
    if (access(argv[1], F_OK) != 0) {
        fprintf(stderr, "File %s does not exist.\n", argv[1]);
        return 1;
    }
    if (access(argv[1], R_OK) != 0) {
        fprintf(stderr, "File %s is not readable.\n", argv[1]);
        return 1;
    }

    int fd = -1;
    drmModeRes *resources = NULL;
    drmModeConnector *conn = NULL;
    uint32_t fb = 0;
    struct drm_mode_create_dumb creq = {0};
    struct drm_mode_map_dumb mreq = {0};
    uint8_t *map = MAP_FAILED;
    unsigned char *pixels = NULL;

    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("open");
        goto cleanup;
    }
    
    resources = drmModeGetResources(fd);
    if (!resources) {
        perror("drmModeGetResources");
        goto cleanup;
    }

    for (int i = 0; i < resources->count_connectors; ++i) {
        drmModeConnector *tmp = drmModeGetConnector(fd, resources->connectors[i]);
        if (tmp && tmp->connection == DRM_MODE_CONNECTED) {
            conn = tmp;
            break;
        }
        if (tmp) drmModeFreeConnector(tmp);
    }
    if (!conn) {
        fprintf(stderr, "No connected connector found\n");
        goto cleanup;
    }

    drmModeModeInfo mode = conn->modes[0];
    uint32_t width = mode.hdisplay, height = mode.vdisplay;

    drmModeEncoder *enc = NULL;
    if (conn->encoder_id)
        enc = drmModeGetEncoder(fd, conn->encoder_id);
    if (!enc) {
        for (int i = 0; i < conn->count_encoders; ++i) {
            enc = drmModeGetEncoder(fd, conn->encoders[i]);
            if (enc) break;
        }
    }
    if (!enc) {
        fprintf(stderr, "No encoder found for connector\n");
        goto cleanup;
    }

    drmModeCrtc *orig_crtc = drmModeGetCrtc(fd, enc->crtc_id);

    creq.width = width;
    creq.height = height;
    creq.bpp = 32;
    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) != 0) {
        perror("DRM_IOCTL_MODE_CREATE_DUMB");
        goto cleanup;
    }

    if (drmModeAddFB(fd, width, height, 24, 32, creq.pitch, creq.handle, &fb) != 0) {
        perror("drmModeAddFB");
        goto cleanup;
    }

    mreq.handle = creq.handle;
    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq) != 0) {
        perror("DRM_IOCTL_MODE_MAP_DUMB");
        goto cleanup;
    }
    map = mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
    if (map == MAP_FAILED) {
        perror("mmap");
        goto cleanup;
    }

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t pixel = ((x * 255 / width) << 16) | ((y * 255 / height) << 8) | 0xFF;
            ((uint32_t*)map)[y * (creq.pitch / 4) + x] = pixel;
        }
    }

    int img_w, img_h, img_rowbytes;
    int x0 = 100, y0 = 100;
    int result_code = load_png(argv[1], &pixels, &img_w, &img_h, &img_rowbytes);
    if (result_code == 0) {
        for (int y = 0; y < img_h; ++y) {
            int fb_y = y0 + y;
            if (fb_y < 0 || fb_y >= (int)height) continue;
            int copy_width = img_w;
            if (x0 + copy_width > (int)width) copy_width = width - x0;
            if (copy_width <= 0) continue;
            uint32_t *dst = (uint32_t*)((uint8_t*)map + fb_y * creq.pitch) + x0;
            uint8_t *src_row = pixels + y * img_rowbytes;
            for (int x = 0; x < copy_width; ++x) {
                uint8_t r = src_row[x * 4 + 0];
                uint8_t g = src_row[x * 4 + 1];
                uint8_t b = src_row[x * 4 + 2];
                uint8_t a = src_row[x * 4 + 3];
                dst[x] = (b) | (g << 8) | (r << 16) | (a << 24);
            }
        }
        drmModeSetCrtc(fd, enc->crtc_id, fb, 0, 0, &conn->connector_id, 1, &mode);
    } else {
        fprintf(stderr, "ERROR:[%d] load image failed.\n", result_code);
    }

    if (pixels) free(pixels);

    printf("Press Enter to exit...\n");
    getchar();

    drmModeSetCrtc(fd, orig_crtc->crtc_id, orig_crtc->buffer_id,
               orig_crtc->x, orig_crtc->y,
               &conn->connector_id, 1, &orig_crtc->mode);
    drmModeFreeCrtc(orig_crtc);
cleanup:
    if (map != MAP_FAILED) munmap(map, creq.size);
    if (fb) drmModeRmFB(fd, fb);

    if (creq.handle) {
        struct drm_mode_destroy_dumb dreq = {0};
        dreq.handle = creq.handle;
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
    }
    if (enc) drmModeFreeEncoder(enc);
    if (conn) drmModeFreeConnector(conn);
    if (resources) drmModeFreeResources(resources);
    if (fd >= 0) close(fd);

    return 0;
}