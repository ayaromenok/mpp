/*
 * Copyright 2015 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "mpp_env.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "mpp_common.h"
#include "mpp_platform.h"
#include "mpp_service.h"

#define MAX_SOC_NAME_LENGTH     128

class MppPlatformService;

typedef enum RockchipSocType_e {
    ROCKCHIP_SOC_AUTO,
    ROCKCHIP_SOC_RK3036,
    ROCKCHIP_SOC_RK3066,
    ROCKCHIP_SOC_RK3188,
    ROCKCHIP_SOC_RK3288,
    ROCKCHIP_SOC_RK312X,
    ROCKCHIP_SOC_RK3368,
    ROCKCHIP_SOC_RK3399,
    ROCKCHIP_SOC_RK3228H,
    ROCKCHIP_SOC_RK3328,
    ROCKCHIP_SOC_RK3228,
    ROCKCHIP_SOC_RK3229,
    ROCKCHIP_SOC_RV1108,
    ROCKCHIP_SOC_RV1109,
    ROCKCHIP_SOC_RV1126,
    ROCKCHIP_SOC_RK3326,
    ROCKCHIP_SOC_RK3128H,
    ROCKCHIP_SOC_PX30,
    ROCKCHIP_SOC_RK1808,
    ROCKCHIP_SOC_BUTT,
} RockchipSocType;

typedef struct {
    const char *compatible;
    RockchipSocType soc_type;
    RK_U32 vcodec_type;
} MppVpuType;

static const MppVpuType mpp_vpu_version[] = {
    { "rk3036",  ROCKCHIP_SOC_RK3036,   HAVE_VDPU1 | HAVE_VEPU1 | HAVE_HEVC_DEC, },
    { "rk3066",  ROCKCHIP_SOC_RK3066,   HAVE_VDPU1 | HAVE_VEPU1,                 },
    { "rk3188",  ROCKCHIP_SOC_RK3188,   HAVE_VDPU1 | HAVE_VEPU1,                 },
    { "rk3288",  ROCKCHIP_SOC_RK3288,   HAVE_VDPU1 | HAVE_VEPU1 | HAVE_HEVC_DEC, },
    { "rk3126",  ROCKCHIP_SOC_RK312X,   HAVE_VDPU1 | HAVE_VEPU1 | HAVE_HEVC_DEC, },
    { "rk3128h", ROCKCHIP_SOC_RK3128H,  HAVE_VDPU2 | HAVE_VEPU2 | HAVE_RKVDEC,   },
    { "rk3128",  ROCKCHIP_SOC_RK312X,   HAVE_VDPU1 | HAVE_VEPU1 | HAVE_HEVC_DEC, },
    { "rk3368",  ROCKCHIP_SOC_RK3368,   HAVE_VDPU1 | HAVE_VEPU1 | HAVE_HEVC_DEC, },
    { "rk3399",  ROCKCHIP_SOC_RK3399,   HAVE_VDPU2 | HAVE_VEPU2 | HAVE_RKVDEC,   },
    /* 3228h first for string matching */
    { "rk3228h", ROCKCHIP_SOC_RK3228H,  HAVE_VDPU2 | HAVE_VEPU2 | HAVE_RKVDEC | HAVE_AVSDEC | HAVE_VEPU22, },
    { "rk3328",  ROCKCHIP_SOC_RK3328,   HAVE_VDPU2 | HAVE_VEPU2 | HAVE_RKVDEC | HAVE_VEPU22, },
    { "rk3228",  ROCKCHIP_SOC_RK3228,   HAVE_VDPU2 | HAVE_VEPU2 | HAVE_RKVDEC,   },
    { "rk3229",  ROCKCHIP_SOC_RK3229,   HAVE_VDPU2 | HAVE_VEPU2 | HAVE_RKVDEC,   },
    { "rv1108",  ROCKCHIP_SOC_RV1108,   HAVE_VDPU2 | HAVE_VEPU2 | HAVE_RKVDEC | HAVE_RKVENC, },
    { "rv1109",  ROCKCHIP_SOC_RV1109,   HAVE_VDPU2 | HAVE_VEPU2 | HAVE_RKVDEC | HAVE_RKVENC, },
    { "rv1126",  ROCKCHIP_SOC_RV1126,   HAVE_VDPU2 | HAVE_VEPU2 | HAVE_RKVDEC | HAVE_RKVENC, },
    { "rk3326",  ROCKCHIP_SOC_RK3326,   HAVE_VDPU2 | HAVE_VEPU2 | HAVE_HEVC_DEC, },
    { "px30",    ROCKCHIP_SOC_RK3326,   HAVE_VDPU2 | HAVE_VEPU2 | HAVE_HEVC_DEC, },
    { "rk1808",  ROCKCHIP_SOC_RK1808,   HAVE_VDPU2 | HAVE_VEPU2, },
};

/* For vpu1 / vpu2 */
static const char *mpp_vpu_dev[] = {
    "/dev/vpu_service",
    "/dev/vpu-service",
    "/dev/mpp_service",
};

/* For hevc 4K decoder */
static const char *mpp_hevc_dev[] = {
    "/dev/hevc_service",
    "/dev/hevc-service",
    "/dev/mpp_service",
};

/* For H.264/H.265/VP9 4K decoder */
static const char *mpp_rkvdec_dev[] = {
    "/dev/rkvdec",
    "/dev/mpp_service",
};

/* For H.264 4K encoder */
static const char *mpp_rkvenc_dev[] = {
    "/dev/rkvenc",
    "/dev/mpp_service",
};

/* For avs+ decoder */
static const char *mpp_avsd_dev[] = {
    "/dev/avsd",
    "/dev/mpp_service",
};

/* For H.264 / jpeg encoder */
static const char *mpp_vepu_dev[] = {
    "/dev/vepu",
    "/dev/mpp_service",
};

/* For H.265 encoder */
static const char *mpp_h265e_dev[] = {
    "/dev/h265e",
    "/dev/mpp_service",
};

static const char *mpp_service_dev[] = {
    "/dev/mpp_service",
};

static const char *mpp_soc_name_path = "/proc/device-tree/compatible";

#define mpp_find_device(dev) _mpp_find_device(dev, MPP_ARRAY_ELEMS(dev))

static const char *_mpp_find_device(const char **dev, RK_U32 size)
{
    RK_U32 i;

    for (i = 0; i < size; i++) {
        if (!access(dev[i], F_OK))
            return dev[i];
    }
    return NULL;
}

static void read_soc_name(char *name, RK_S32 size)
{
    RK_S32 fd = open(mpp_soc_name_path, O_RDONLY);
    if (fd < 0) {
        mpp_err("open %s error\n", mpp_soc_name_path);
    } else {
        ssize_t soc_name_len = 0;

        snprintf(name, size, "unknown");
        soc_name_len = read(fd, name, size - 1);
        if (soc_name_len > 0) {
            name[soc_name_len] = '\0';
            /* replacing the termination character to space */
            for (char *ptr = name;; ptr = name) {
                ptr += strnlen(name, size);
                if (ptr >= name + soc_name_len - 1)
                    break;
                *ptr = ' ';
            }

            mpp_dbg(MPP_DBG_PLATFORM, "chip name: %s\n", name);
        }

        close(fd);
    }
}

static const MppVpuType *check_vpu_type_by_soc_name(const char *soc_name)
{
    RK_U32 i;

    for (i = 0; i < MPP_ARRAY_ELEMS(mpp_vpu_version); i++)
        if (strstr(soc_name, mpp_vpu_version[i].compatible))
            return &mpp_vpu_version[i];

    return NULL;
}

class MppPlatformService
{
private:
    // avoid any unwanted function
    MppPlatformService();
    ~MppPlatformService() {};
    MppPlatformService(const MppPlatformService &);
    MppPlatformService &operator=(const MppPlatformService &);

    MppIoctlVersion     ioctl_version;
    RK_U32              vcodec_type;
    RK_U32              hw_ids[32];
    MppServiceCmdCap    mpp_service_cmd_cap;
    char                soc_name[MAX_SOC_NAME_LENGTH];
    RockchipSocType     soc_type;

public:
    static MppPlatformService *get_instance() {
        static MppPlatformService instance;
        return &instance;
    }

    MppIoctlVersion     get_ioctl_version(void) { return ioctl_version; };
    const char          *get_soc_name() { return soc_name; };
    RockchipSocType     get_soc_type() { return soc_type; };
    RK_U32              get_vcodec_type() { return vcodec_type; };
    MppServiceCmdCap    *get_mpp_service_cmd_cap() { return &mpp_service_cmd_cap; };
    RK_U32              get_hw_id(RK_S32 client_type);
};

MppPlatformService::MppPlatformService()
    : ioctl_version(IOCTL_VCODEC_SERVICE),
      vcodec_type(0)
{
    /* judge vdpu support version */
    MppServiceCmdCap *cap = &mpp_service_cmd_cap;

    /* default value */
    cap->support_cmd = 0;
    cap->query_cmd = MPP_CMD_QUERY_BASE + 1;
    cap->init_cmd = MPP_CMD_INIT_BASE + 1;
    cap->send_cmd = MPP_CMD_SEND_BASE + 1;
    cap->poll_cmd = MPP_CMD_POLL_BASE + 1;
    cap->ctrl_cmd = MPP_CMD_CONTROL_BASE + 0;

    mpp_env_get_u32("mpp_debug", &mpp_debug, 0);

    /* read soc name */
    read_soc_name(soc_name, sizeof(soc_name));

    /* set vpu1 defalut for old chip without dts */
    vcodec_type = HAVE_VDPU1 | HAVE_VEPU1;
    {
        const MppVpuType *hw_info = check_vpu_type_by_soc_name(soc_name);
        if (hw_info) {
            vcodec_type = hw_info->vcodec_type;
            soc_type = hw_info->soc_type;
        } else
            mpp_log("can not found match soc name: %s\n", soc_name);
    }

    /* if /dev/mpp_service not double check */
    if (mpp_find_device(mpp_service_dev)) {
        ioctl_version = IOCTL_MPP_SERVICE_V1;
        check_mpp_service_cap(&vcodec_type, hw_ids, cap);
        mpp_dbg(MPP_DBG_PLATFORM, "/dev/mpp_service not double check device\n");
        goto __return;
    }

    /*
     * NOTE: The following check is for kernel driver device double check
     * Some kernel does not have all device dts. So we need to remove the
     * feature if the kernel device does not exist.
     * The other case is customer changes the compatible name in dts then can
     * not find a match soc type then we try to add the feature.
     */
    /* for rk3288 / rk3368 /rk312x RK hevc decoder */
    if (!mpp_find_device(mpp_hevc_dev))
        vcodec_type &= ~HAVE_HEVC_DEC;
    else
        vcodec_type |= HAVE_HEVC_DEC;

    /* for rk3228 / rk3229 / rk3399 / rv1108 decoder */
    if (!mpp_find_device(mpp_rkvdec_dev))
        vcodec_type &= ~HAVE_RKVDEC;
    else
        vcodec_type |= HAVE_RKVDEC;

    /* for rk3228h avs+ decoder */
    if (!mpp_find_device(mpp_avsd_dev))
        vcodec_type &= ~HAVE_AVSDEC;
    else
        vcodec_type |= HAVE_AVSDEC;

    /* for rv1108 encoder */
    if (!mpp_find_device(mpp_rkvenc_dev))
        vcodec_type &= ~HAVE_RKVENC;
    else
        vcodec_type |= HAVE_RKVENC;

    /* for rk3228h / rk3328 H.264/jpeg encoder */
    if (!mpp_find_device(mpp_vepu_dev))
        vcodec_type &= ~HAVE_VEPU2_LITE;
    else
        vcodec_type |= HAVE_VEPU2_LITE;

    /* for rk3228h / rk3328 H.265 encoder */
    if (!mpp_find_device(mpp_h265e_dev))
        vcodec_type &= ~HAVE_VEPU22;
    else
        vcodec_type |= HAVE_VEPU22;
    /* for all chip vpu decoder */
    if (!mpp_find_device(mpp_vpu_dev))
        vcodec_type &= ~(HAVE_VDPU1 | HAVE_VEPU1 | HAVE_VDPU2 | HAVE_VEPU2);
__return:
    mpp_dbg(MPP_DBG_PLATFORM, "vcodec type %08x\n", vcodec_type);
}

RK_U32 MppPlatformService::get_hw_id(RK_S32 client_type)
{
    RK_U32 hw_id = 0;

    if (vcodec_type & (1 << client_type))
        hw_id = hw_ids[client_type];

    return hw_id;
}

MppIoctlVersion mpp_get_ioctl_version(void)
{
    return MppPlatformService::get_instance()->get_ioctl_version();
}

const char *mpp_get_soc_name(void)
{
    static const char *soc_name = NULL;

    if (soc_name)
        return soc_name;

    soc_name = MppPlatformService::get_instance()->get_soc_name();
    return soc_name;
}

RK_U32 mpp_get_vcodec_type(void)
{
    static RK_U32 vcodec_type = 0;

    if (!vcodec_type)
        vcodec_type = MppPlatformService::get_instance()->get_vcodec_type();

    return vcodec_type;
}

RK_U32 mpp_get_2d_hw_flag(void)
{
    RK_U32 flag = 0;

    if (!access("/dev/rga", F_OK))
        flag |= HAVE_RGA;

    if (!access("/dev/iep", F_OK))
        flag |= HAVE_IEP;

    return flag;
}

const char *mpp_get_platform_dev_name(MppCtxType type, MppCodingType coding, RK_U32 platform)
{
    const char *dev = mpp_find_device(mpp_service_dev);
    if (dev) {
        mpp_log("got the %s\n", dev);
    } else if ((platform & HAVE_RKVDEC) && (type == MPP_CTX_DEC) &&
               (coding == MPP_VIDEO_CodingAVC ||
                coding == MPP_VIDEO_CodingHEVC ||
                coding == MPP_VIDEO_CodingVP9)) {
        dev = mpp_find_device(mpp_rkvdec_dev);
    } else if ((platform & HAVE_HEVC_DEC) && (type == MPP_CTX_DEC) &&
               (coding == MPP_VIDEO_CodingHEVC)) {
        dev = mpp_find_device(mpp_hevc_dev);
    } else if ((platform & HAVE_AVSDEC) && (type == MPP_CTX_DEC) &&
               (coding == MPP_VIDEO_CodingAVS)) {
        dev = mpp_find_device(mpp_avsd_dev);
    } else if ((platform & HAVE_RKVENC) && (type == MPP_CTX_ENC) &&
               (coding == MPP_VIDEO_CodingAVC)) {
        dev = mpp_find_device(mpp_rkvenc_dev);
    } else if ((platform & HAVE_VEPU22) && (type == MPP_CTX_ENC) &&
               (coding == MPP_VIDEO_CodingHEVC)) {
        dev = mpp_find_device(mpp_h265e_dev);
    } else if ((platform & (HAVE_VEPU2_LITE)) && (type == MPP_CTX_ENC) &&
               ((coding == MPP_VIDEO_CodingAVC ||
                 coding == MPP_VIDEO_CodingMJPEG))) {
        dev = mpp_find_device(mpp_vepu_dev);
    } else {
        if (type == MPP_CTX_ENC)
            dev = mpp_find_device(mpp_vepu_dev);

        if (dev == NULL)
            dev = mpp_find_device(mpp_vpu_dev);
    }

    return dev;
}

const char *mpp_get_vcodec_dev_name(MppCtxType type, MppCodingType coding)
{
    const char *dev = NULL;
    RockchipSocType soc_type = MppPlatformService::get_instance()->get_soc_type();

    switch (soc_type) {
    case ROCKCHIP_SOC_RK3036 : {
        /* rk3036 do NOT have encoder */
        if (type == MPP_CTX_ENC)
            dev = NULL;
        else if (coding == MPP_VIDEO_CodingHEVC && type == MPP_CTX_DEC)
            dev = mpp_find_device(mpp_hevc_dev);
        else
            dev = mpp_find_device(mpp_vpu_dev);
    } break;
    case ROCKCHIP_SOC_RK3066 :
    case ROCKCHIP_SOC_RK3188 : {
        /* rk3066/rk3188 have vpu1 only */
        dev = mpp_find_device(mpp_vpu_dev);
    } break;
    case ROCKCHIP_SOC_RK3288 :
    case ROCKCHIP_SOC_RK312X :
    case ROCKCHIP_SOC_RK3368 :
    case ROCKCHIP_SOC_RK3326 :
    case ROCKCHIP_SOC_PX30 : {
        /*
         * rk3288/rk312x/rk3368 have codec:
         * 1 - vpu1
         * 2 - RK hevc decoder
         */
        if (coding == MPP_VIDEO_CodingHEVC && type == MPP_CTX_DEC)
            dev = mpp_find_device(mpp_hevc_dev);
        else
            dev = mpp_find_device(mpp_vpu_dev);
    } break;
    case ROCKCHIP_SOC_RK3128H : {
        /*
         * rk3128H have codec:
         * 1 - vpu2
         * 2 - RK H.264/H.265 1080p@60fps decoder
         * NOTE: rk3128H do NOT have jpeg encoder
         */
        if (type == MPP_CTX_DEC &&
            (coding == MPP_VIDEO_CodingAVC ||
             coding == MPP_VIDEO_CodingHEVC))
            dev = mpp_find_device(mpp_rkvdec_dev);
        else if (type == MPP_CTX_ENC && coding == MPP_VIDEO_CodingMJPEG)
            dev = NULL;
        else if (type == MPP_CTX_DEC && coding == MPP_VIDEO_CodingVP9)
            dev = NULL;
        else
            dev = mpp_find_device(mpp_vpu_dev);
    } break;
    case ROCKCHIP_SOC_RK3399 :
    case ROCKCHIP_SOC_RK3229 : {
        /*
         * rk3399/rk3229 have codec:
         * 1 - vpu2
         * 2 - RK H.264/H.265/VP9 4K decoder
         */
        if (type == MPP_CTX_DEC &&
            (coding == MPP_VIDEO_CodingAVC ||
             coding == MPP_VIDEO_CodingHEVC ||
             coding == MPP_VIDEO_CodingVP9))
            dev = mpp_find_device(mpp_rkvdec_dev);
        else
            dev = mpp_find_device(mpp_vpu_dev);
    } break;
    case ROCKCHIP_SOC_RK3228 : {
        /*
         * rk3228 have codec:
         * 1 - vpu2
         * 2 - RK H.264/H.265 4K decoder
         * NOTE: rk3228 do NOT have jpeg encoder
         */
        if (type == MPP_CTX_DEC &&
            (coding == MPP_VIDEO_CodingAVC ||
             coding == MPP_VIDEO_CodingHEVC))
            dev = mpp_find_device(mpp_rkvdec_dev);
        else if (type == MPP_CTX_ENC && coding == MPP_VIDEO_CodingMJPEG)
            dev = NULL;
        else
            dev = mpp_find_device(mpp_vpu_dev);
    } break;
    case ROCKCHIP_SOC_RK3228H : {
        /*
         * rk3228h has codec:
         * 1 - vpu2
         * 2 - RK H.264/H.265 4K decoder
         * 3 - avs+ decoder
         * 4 - H.265 encoder
         */
        if (type == MPP_CTX_ENC) {
            if (coding == MPP_VIDEO_CodingHEVC)
                dev = mpp_find_device(mpp_h265e_dev);
            else
                dev = mpp_find_device(mpp_vepu_dev);
        } else if (type == MPP_CTX_DEC) {
            if (coding == MPP_VIDEO_CodingAVS)
                dev = mpp_find_device(mpp_avsd_dev);
            else if (coding == MPP_VIDEO_CodingAVC ||
                     coding == MPP_VIDEO_CodingHEVC)
                dev = mpp_find_device(mpp_rkvdec_dev);
            else
                dev = mpp_find_device(mpp_vpu_dev);
        }
    } break;
    case ROCKCHIP_SOC_RK3328 : {
        /*
         * rk3228 has codec:
         * 1 - vpu2
         * 2 - RK H.264/H.265/VP9 4K decoder
         * 4 - H.265 encoder
         */
        if (type == MPP_CTX_ENC) {
            if (coding == MPP_VIDEO_CodingHEVC)
                dev = mpp_find_device(mpp_h265e_dev);
            else
                dev = mpp_find_device(mpp_vepu_dev);
        } else if (type == MPP_CTX_DEC) {
            if (coding == MPP_VIDEO_CodingAVC ||
                coding == MPP_VIDEO_CodingHEVC ||
                coding == MPP_VIDEO_CodingVP9) {
                dev = mpp_find_device(mpp_rkvdec_dev);
            } else
                dev = mpp_find_device(mpp_vpu_dev);
        }
    } break;
    case ROCKCHIP_SOC_RV1108 : {
        /*
         * rv1108 has codec:
         * 1 - vpu2
         * 2 - RK H.264 4K decoder
         * 3 - RK H.264 4K encoder
         */
        if (coding == MPP_VIDEO_CodingAVC) {
            if (type == MPP_CTX_ENC)
                dev = mpp_find_device(mpp_rkvenc_dev);
            else
                dev = mpp_find_device(mpp_rkvdec_dev);
        } else if (coding == MPP_VIDEO_CodingMJPEG)
            dev = mpp_find_device(mpp_vpu_dev);
    } break;
    case ROCKCHIP_SOC_RV1109 :
    case ROCKCHIP_SOC_RV1126 : {
        /*
         * rv1108 has codec:
         * 1 - vpu2 for jpeg encoder and decoder
         * 2 - RK H.264/H.265 4K decoder
         * 3 - RK H.264/H.265 4K encoder
         */
        if (coding == MPP_VIDEO_CodingAVC || coding == MPP_VIDEO_CodingHEVC) {
            if (type == MPP_CTX_ENC)
                dev = mpp_find_device(mpp_rkvenc_dev);
            else
                dev = mpp_find_device(mpp_rkvdec_dev);
        } else if (coding == MPP_VIDEO_CodingMJPEG)
            dev = mpp_find_device(mpp_vpu_dev);
    } break;
    default : {
        /* default case for unknown compatible  */
        RK_U32 vcodec_type = mpp_get_vcodec_type();

        dev = mpp_get_platform_dev_name(type, coding, vcodec_type);
    } break;
    }

    return dev;
}

const MppServiceCmdCap *mpp_get_mpp_service_cmd_cap(void)
{
    return MppPlatformService::get_instance()->get_mpp_service_cmd_cap();
}

RK_U32 mpp_get_client_hw_id(RK_S32 client_type)
{
    return MppPlatformService::get_instance()->get_hw_id(client_type);
}
