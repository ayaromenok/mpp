/*
 * Copyright 2020 Rockchip Electronics Co. LTD
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

#define MODULE_TAG "hal_h265d_vdpu34x"

#include <stdio.h>
#include <string.h>

#include "mpp_env.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "mpp_bitread.h"
#include "mpp_bitput.h"

#include "h265d_syntax.h"
#include "hal_h265d_debug.h"
#include "hal_h265d_ctx.h"
#include "hal_h265d_com.h"
#include "hal_h265d_vdpu34x.h"
#include "vdpu34x_h265d.h"

/* #define dump */
#ifdef dump
static FILE *fp = NULL;
#endif

#define HW_RPS
#define PPS_SIZE                (112 * 64)//(96x64)

#define SET_REF_VALID(regs, index, value)\
    do{ \
        switch(index){\
        case 0: regs.hevc_ref_valid.hevc_ref_valid_0 = value; break;\
        case 1: regs.hevc_ref_valid.hevc_ref_valid_1 = value; break;\
        case 2: regs.hevc_ref_valid.hevc_ref_valid_2 = value; break;\
        case 3: regs.hevc_ref_valid.hevc_ref_valid_3 = value; break;\
        case 4: regs.hevc_ref_valid.hevc_ref_valid_4 = value; break;\
        case 5: regs.hevc_ref_valid.hevc_ref_valid_5 = value; break;\
        case 6: regs.hevc_ref_valid.hevc_ref_valid_6 = value; break;\
        case 7: regs.hevc_ref_valid.hevc_ref_valid_7 = value; break;\
        case 8: regs.hevc_ref_valid.hevc_ref_valid_8 = value; break;\
        case 9: regs.hevc_ref_valid.hevc_ref_valid_9 = value; break;\
        case 10: regs.hevc_ref_valid.hevc_ref_valid_10 = value; break;\
        case 11: regs.hevc_ref_valid.hevc_ref_valid_11 = value; break;\
        case 12: regs.hevc_ref_valid.hevc_ref_valid_12 = value; break;\
        case 13: regs.hevc_ref_valid.hevc_ref_valid_13 = value; break;\
        case 14: regs.hevc_ref_valid.hevc_ref_valid_14 = value; break;\
        default: break;}\
    }while(0)


static MPP_RET hal_h265d_alloc_res(void *hal)
{
    RK_S32 i = 0;
    RK_S32 ret = 0;
    HalH265dCtx *reg_cxt = (HalH265dCtx *)hal;
    if (reg_cxt->fast_mode) {
        for (i = 0; i < MAX_GEN_REG; i++) {
            reg_cxt->g_buf[i].hw_regs =
                mpp_calloc_size(void, sizeof(Vdpu34xH265dRegSet));
            ret = mpp_buffer_get(reg_cxt->group,
                                 &reg_cxt->g_buf[i].scaling_list_data,
                                 SCALING_LIST_SIZE);
            if (ret) {
                mpp_err("h265d scaling_list_data get buffer failed\n");
                return ret;
            }

            ret = mpp_buffer_get(reg_cxt->group, &reg_cxt->g_buf[i].pps_data,
                                 PPS_SIZE);
            if (ret) {
                mpp_err("h265d pps_data get buffer failed\n");
                return ret;
            }

            ret = mpp_buffer_get(reg_cxt->group, &reg_cxt->g_buf[i].rps_data,
                                 RPS_SIZE);
            if (ret) {
                mpp_err("h265d rps_data get buffer failed\n");
                return ret;
            }
        }
    } else {
        reg_cxt->hw_regs = mpp_calloc_size(void, sizeof(Vdpu34xH265dRegSet));
        ret = mpp_buffer_get(reg_cxt->group, &reg_cxt->scaling_list_data,
                             SCALING_LIST_SIZE);
        if (ret) {
            mpp_err("h265d scaling_list_data get buffer failed\n");
            return ret;
        }

        ret = mpp_buffer_get(reg_cxt->group, &reg_cxt->pps_data, PPS_SIZE);
        if (ret) {
            mpp_err("h265d pps_data get buffer failed\n");
            return ret;
        }

        ret = mpp_buffer_get(reg_cxt->group, &reg_cxt->rps_data, RPS_SIZE);
        if (ret) {
            mpp_err("h265d rps_data get buffer failed\n");
            return ret;
        }

    }
    return MPP_OK;
}

static MPP_RET hal_h265d_release_res(void *hal)
{
    RK_S32 ret = 0;
    HalH265dCtx *reg_cxt = ( HalH265dCtx *)hal;
    RK_S32 i = 0;

    mpp_buffer_put(reg_cxt->rcb_buf);
    if (reg_cxt->fast_mode) {
        for (i = 0; i < MAX_GEN_REG; i++) {
            if (reg_cxt->g_buf[i].scaling_list_data) {
                ret = mpp_buffer_put(reg_cxt->g_buf[i].scaling_list_data);
                if (ret) {
                    mpp_err("h265d scaling_list_data free buffer failed\n");
                    return ret;
                }
            }
            if (reg_cxt->g_buf[i].pps_data) {
                ret = mpp_buffer_put(reg_cxt->g_buf[i].pps_data);
                if (ret) {
                    mpp_err("h265d pps_data free buffer failed\n");
                    return ret;
                }
            }

            if (reg_cxt->g_buf[i].rps_data) {
                ret = mpp_buffer_put(reg_cxt->g_buf[i].rps_data);
                if (ret) {
                    mpp_err("h265d rps_data free buffer failed\n");
                    return ret;
                }
            }

            if (reg_cxt->g_buf[i].hw_regs) {
                mpp_free(reg_cxt->g_buf[i].hw_regs);
                reg_cxt->g_buf[i].hw_regs = NULL;
            }
        }
    } else {
        if (reg_cxt->scaling_list_data) {
            ret = mpp_buffer_put(reg_cxt->scaling_list_data);
            if (ret) {
                mpp_err("h265d scaling_list_data free buffer failed\n");
                return ret;
            }
        }
        if (reg_cxt->pps_data) {
            ret = mpp_buffer_put(reg_cxt->pps_data);
            if (ret) {
                mpp_err("h265d pps_data free buffer failed\n");
                return ret;
            }
        }

        if (reg_cxt->rps_data) {
            ret = mpp_buffer_put(reg_cxt->rps_data);
            if (ret) {
                mpp_err("h265d rps_data free buffer failed\n");
                return ret;
            }
        }

        if (reg_cxt->hw_regs) {
            mpp_free(reg_cxt->hw_regs);
            reg_cxt->hw_regs = NULL;
        }
    }
    return MPP_OK;
}

static MPP_RET hal_h265d_vdpu34x_init(void *hal, MppHalCfg *cfg)
{
    RK_S32 ret = 0;
    HalH265dCtx *reg_cxt = (HalH265dCtx *)hal;

    mpp_slots_set_prop(reg_cxt->slots, SLOTS_HOR_ALIGN, hevc_hor_align);
    mpp_slots_set_prop(reg_cxt->slots, SLOTS_VER_ALIGN, hevc_ver_align);

    reg_cxt->scaling_qm = mpp_calloc(DXVA_Qmatrix_HEVC, 1);
    if (reg_cxt->scaling_qm == NULL) {
        mpp_err("scaling_org alloc fail");
        return MPP_ERR_MALLOC;
    }

    reg_cxt->scaling_rk = mpp_calloc(scalingFactor_t, 1);
    if (reg_cxt->scaling_rk == NULL) {
        mpp_err("scaling_rk alloc fail");
        return MPP_ERR_MALLOC;
    }

    if (reg_cxt->group == NULL) {
        ret = mpp_buffer_group_get_internal(&reg_cxt->group, MPP_BUFFER_TYPE_ION);
        if (ret) {
            mpp_err("h265d mpp_buffer_group_get failed\n");
            return ret;
        }
    }

    ret = mpp_buffer_get(reg_cxt->group, &reg_cxt->cabac_table_data, sizeof(cabac_table));
    if (ret) {
        mpp_err("h265d cabac_table get buffer failed\n");
        return ret;
    }

    ret = mpp_buffer_write(reg_cxt->cabac_table_data, 0, (void*)cabac_table, sizeof(cabac_table));
    if (ret) {
        mpp_err("h265d write cabac_table data failed\n");
        return ret;
    }

    ret = hal_h265d_alloc_res(hal);
    if (ret) {
        mpp_err("hal_h265d_alloc_res failed\n");
        return ret;
    }

#ifdef dump
    fp = fopen("/data/hal.bin", "wb");
#endif
    (void) cfg;
    return MPP_OK;
}

static MPP_RET hal_h265d_vdpu34x_deinit(void *hal)
{

    RK_S32 ret = 0;
    HalH265dCtx *reg_cxt = (HalH265dCtx *)hal;

    ret = mpp_buffer_put(reg_cxt->cabac_table_data);
    if (ret) {
        mpp_err("h265d cabac_table free buffer failed\n");
        return ret;
    }

    if (reg_cxt->scaling_qm) {
        mpp_free(reg_cxt->scaling_qm);
    }

    if (reg_cxt->scaling_rk) {
        mpp_free(reg_cxt->scaling_rk);
    }

    hal_h265d_release_res(hal);

    if (reg_cxt->group) {
        ret = mpp_buffer_group_put(reg_cxt->group);
        if (ret) {
            mpp_err("h265d group free buffer failed\n");
            return ret;
        }
    }
    return MPP_OK;
}

static RK_S32 hal_h265d_v345_output_pps_packet(void *hal, void *dxva)
{
    RK_S32 fifo_len = 14;//12
    RK_S32 i, j;
    RK_U32 addr;
    RK_U32 log2_min_cb_size;
    RK_S32 width, height;
    HalH265dCtx *reg_cxt = ( HalH265dCtx *)hal;
    Vdpu34xH265dRegSet *hw_reg = (Vdpu34xH265dRegSet*)(reg_cxt->hw_regs);
    h265d_dxva2_picture_context_t *dxva_cxt = (h265d_dxva2_picture_context_t*)dxva;
    BitputCtx_t bp;
    RK_U64 *pps_packet = mpp_calloc(RK_U64, fifo_len + 1);

    if (NULL == reg_cxt || dxva_cxt == NULL) {
        mpp_err("%s:%s:%d reg_cxt or dxva_cxt is NULL",
                __FILE__, __FUNCTION__, __LINE__);
        MPP_FREE(pps_packet);
        return MPP_ERR_NULL_PTR;
    }

    void *pps_ptr = mpp_buffer_get_ptr(reg_cxt->pps_data);
    if (NULL == pps_ptr) {
        mpp_err("pps_data get ptr error");
        return MPP_ERR_NOMEM;
    }
    memset(pps_ptr, 0, PPS_SIZE);//96
    // pps_packet = (RK_U64 *)(pps_ptr + dxva_cxt->pp.pps_id * 80);

    for (i = 0; i < 14; i++) pps_packet[i] = 0;

    mpp_set_bitput_ctx(&bp, pps_packet, fifo_len);

    // SPS
    mpp_put_bits(&bp, dxva_cxt->pp.vps_id                            , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.sps_id                            , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.chroma_format_idc                 , 2);

    log2_min_cb_size = dxva_cxt->pp.log2_min_luma_coding_block_size_minus3 + 3;
    width = (dxva_cxt->pp.PicWidthInMinCbsY << log2_min_cb_size);
    height = (dxva_cxt->pp.PicHeightInMinCbsY << log2_min_cb_size);

    mpp_put_bits(&bp, width                                          , 16);
    mpp_put_bits(&bp, height                                         , 16);
    mpp_put_bits(&bp, dxva_cxt->pp.bit_depth_luma_minus8 + 8         , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.bit_depth_chroma_minus8 + 8       , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.log2_max_pic_order_cnt_lsb_minus4 + 4      , 5);
    mpp_put_bits(&bp, dxva_cxt->pp.log2_diff_max_min_luma_coding_block_size   , 2); //log2_maxa_coding_block_depth
    mpp_put_bits(&bp, dxva_cxt->pp.log2_min_luma_coding_block_size_minus3 + 3 , 3);
    mpp_put_bits(&bp, dxva_cxt->pp.log2_min_transform_block_size_minus2 + 2   , 3);
    ///<-zrh comment ^  63 bit above
    mpp_put_bits(&bp, dxva_cxt->pp.log2_diff_max_min_transform_block_size     , 2);
    mpp_put_bits(&bp, dxva_cxt->pp.max_transform_hierarchy_depth_inter        , 3);
    mpp_put_bits(&bp, dxva_cxt->pp.max_transform_hierarchy_depth_intra        , 3);
    mpp_put_bits(&bp, dxva_cxt->pp.scaling_list_enabled_flag                  , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.amp_enabled_flag                           , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.sample_adaptive_offset_enabled_flag        , 1);
    ///<-zrh comment ^  68 bit above
    mpp_put_bits(&bp, dxva_cxt->pp.pcm_enabled_flag                           , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.pcm_enabled_flag ? (dxva_cxt->pp.pcm_sample_bit_depth_luma_minus1 + 1) : 0  , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.pcm_enabled_flag ? (dxva_cxt->pp.pcm_sample_bit_depth_chroma_minus1 + 1) : 0 , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.pcm_loop_filter_disabled_flag                                               , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.log2_diff_max_min_pcm_luma_coding_block_size                                , 3);
    mpp_put_bits(&bp, dxva_cxt->pp.pcm_enabled_flag ? (dxva_cxt->pp.log2_min_pcm_luma_coding_block_size_minus3 + 3) : 0, 3);

    mpp_put_bits(&bp, dxva_cxt->pp.num_short_term_ref_pic_sets             , 7);
    mpp_put_bits(&bp, dxva_cxt->pp.long_term_ref_pics_present_flag         , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.num_long_term_ref_pics_sps              , 6);
    mpp_put_bits(&bp, dxva_cxt->pp.sps_temporal_mvp_enabled_flag           , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.strong_intra_smoothing_enabled_flag     , 1);
    ///<-zrh comment ^ 100 bit above

    mpp_put_bits(&bp, 0                                                    , 7 ); //49bits
    //yandong change
    mpp_put_bits(&bp, dxva_cxt->pp.sps_max_dec_pic_buffering_minus1,       4);
    mpp_put_bits(&bp, 0, 3);
    mpp_put_align(&bp                                                        , 32, 0xf); //128
    // PPS
    mpp_put_bits(&bp, dxva_cxt->pp.pps_id                                    , 6 );
    mpp_put_bits(&bp, dxva_cxt->pp.sps_id                                    , 4 );
    mpp_put_bits(&bp, dxva_cxt->pp.dependent_slice_segments_enabled_flag     , 1 );
    mpp_put_bits(&bp, dxva_cxt->pp.output_flag_present_flag                  , 1 );
    mpp_put_bits(&bp, dxva_cxt->pp.num_extra_slice_header_bits               , 13);
    mpp_put_bits(&bp, dxva_cxt->pp.sign_data_hiding_enabled_flag , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.cabac_init_present_flag                   , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.num_ref_idx_l0_default_active_minus1 + 1  , 4);//31 bits
    mpp_put_bits(&bp, dxva_cxt->pp.num_ref_idx_l1_default_active_minus1 + 1  , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.init_qp_minus26                           , 7);
    mpp_put_bits(&bp, dxva_cxt->pp.constrained_intra_pred_flag               , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.transform_skip_enabled_flag               , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.cu_qp_delta_enabled_flag                  , 1); //164
    mpp_put_bits(&bp, log2_min_cb_size +
                 dxva_cxt->pp.log2_diff_max_min_luma_coding_block_size -
                 dxva_cxt->pp.diff_cu_qp_delta_depth                             , 3);

    h265h_dbg(H265H_DBG_PPS, "log2_min_cb_size %d %d %d \n", log2_min_cb_size,
              dxva_cxt->pp.log2_diff_max_min_luma_coding_block_size, dxva_cxt->pp.diff_cu_qp_delta_depth );

    mpp_put_bits(&bp, dxva_cxt->pp.pps_cb_qp_offset                            , 5);
    mpp_put_bits(&bp, dxva_cxt->pp.pps_cr_qp_offset                            , 5);
    mpp_put_bits(&bp, dxva_cxt->pp.pps_slice_chroma_qp_offsets_present_flag    , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.weighted_pred_flag                          , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.weighted_bipred_flag                        , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.transquant_bypass_enabled_flag              , 1 );
    mpp_put_bits(&bp, dxva_cxt->pp.tiles_enabled_flag                          , 1 );
    mpp_put_bits(&bp, dxva_cxt->pp.entropy_coding_sync_enabled_flag            , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.pps_loop_filter_across_slices_enabled_flag  , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.loop_filter_across_tiles_enabled_flag       , 1); //185
    mpp_put_bits(&bp, dxva_cxt->pp.deblocking_filter_override_enabled_flag     , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.pps_deblocking_filter_disabled_flag         , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.pps_beta_offset_div2                        , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.pps_tc_offset_div2                          , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.lists_modification_present_flag             , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.log2_parallel_merge_level_minus2 + 2        , 3);
    mpp_put_bits(&bp, dxva_cxt->pp.slice_segment_header_extension_present_flag , 1);
    mpp_put_bits(&bp, 0                                                        , 3);
    mpp_put_bits(&bp, dxva_cxt->pp.tiles_enabled_flag ? dxva_cxt->pp.num_tile_columns_minus1 + 1 : 0, 5);
    mpp_put_bits(&bp, dxva_cxt->pp.tiles_enabled_flag ? dxva_cxt->pp.num_tile_rows_minus1 + 1 : 0 , 5 );
    mpp_put_bits(&bp, 0, 4);//2 //mSps_Pps[i]->mMode
    mpp_put_align(&bp, 64, 0xf);
    {
        /// tiles info begin
        RK_U16 column_width[20];
        RK_U16 row_height[22];

        memset(column_width, 0, sizeof(column_width));
        memset(row_height, 0, sizeof(row_height));

        if (dxva_cxt->pp.tiles_enabled_flag) {

            if (dxva_cxt->pp.uniform_spacing_flag == 0) {
                RK_S32 maxcuwidth = dxva_cxt->pp.log2_diff_max_min_luma_coding_block_size + log2_min_cb_size;
                RK_S32 ctu_width_in_pic = (width +
                                           (1 << maxcuwidth) - 1) / (1 << maxcuwidth) ;
                RK_S32 ctu_height_in_pic = (height +
                                            (1 << maxcuwidth) - 1) / (1 << maxcuwidth) ;
                RK_S32 sum = 0;
                for (i = 0; i < dxva_cxt->pp.num_tile_columns_minus1; i++) {
                    column_width[i] = dxva_cxt->pp.column_width_minus1[i] + 1;
                    sum += column_width[i]  ;
                }
                column_width[i] = ctu_width_in_pic - sum;

                sum = 0;
                for (i = 0; i < dxva_cxt->pp.num_tile_rows_minus1; i++) {
                    row_height[i] = dxva_cxt->pp.row_height_minus1[i] + 1;
                    sum += row_height[i];
                }
                row_height[i] = ctu_height_in_pic - sum;
            } // end of (pps->uniform_spacing_flag == 0)
            else {

                RK_S32    pic_in_cts_width = (width +
                                              (1 << (log2_min_cb_size +
                                                     dxva_cxt->pp.log2_diff_max_min_luma_coding_block_size)) - 1)
                                             / (1 << (log2_min_cb_size +
                                                      dxva_cxt->pp.log2_diff_max_min_luma_coding_block_size));
                RK_S32 pic_in_cts_height = (height +
                                            (1 << (log2_min_cb_size +
                                                   dxva_cxt->pp.log2_diff_max_min_luma_coding_block_size)) - 1)
                                           / (1 << (log2_min_cb_size +
                                                    dxva_cxt->pp.log2_diff_max_min_luma_coding_block_size));

                for (i = 0; i < dxva_cxt->pp.num_tile_columns_minus1 + 1; i++)
                    column_width[i] = ((i + 1) * pic_in_cts_width) / (dxva_cxt->pp.num_tile_columns_minus1 + 1) -
                                      (i * pic_in_cts_width) / (dxva_cxt->pp.num_tile_columns_minus1 + 1);

                for (i = 0; i < dxva_cxt->pp.num_tile_rows_minus1 + 1; i++)
                    row_height[i] = ((i + 1) * pic_in_cts_height) / (dxva_cxt->pp.num_tile_rows_minus1 + 1) -
                                    (i * pic_in_cts_height) / (dxva_cxt->pp.num_tile_rows_minus1 + 1);
            }
        } // pps->tiles_enabled_flag
        else {
            RK_S32 MaxCUWidth = (1 << (dxva_cxt->pp.log2_diff_max_min_luma_coding_block_size + log2_min_cb_size));
            column_width[0] = (width  + MaxCUWidth - 1) / MaxCUWidth;
            row_height[0]   = (height + MaxCUWidth - 1) / MaxCUWidth;
        }

        for (j = 0; j < 20; j++) {
            if (column_width[j] > 0)
                column_width[j]--;
            mpp_put_bits(&bp, column_width[j], 12);
        }

        for (j = 0; j < 22; j++) {
            if (row_height[j] > 0)
                row_height[j]--;
            mpp_put_bits(&bp, row_height[j], 12);
        }
    }

    {
        RK_U8 *ptr_scaling = (RK_U8 *)mpp_buffer_get_ptr(reg_cxt->scaling_list_data);
        if (dxva_cxt->pp.scaling_list_data_present_flag) {
            addr = (dxva_cxt->pp.pps_id + 16) * 1360;
        } else if (dxva_cxt->pp.scaling_list_enabled_flag) {
            addr = dxva_cxt->pp.sps_id * 1360;
        } else {
            addr = 80 * 1360;
        }

        hal_h265d_output_scalinglist_packet(hal, ptr_scaling + addr, dxva);

        RK_U32 fd = mpp_buffer_get_fd(reg_cxt->scaling_list_data);
        /* need to config addr */

        addr = fd | (addr << 10);

        mpp_put_bits(&bp, addr, 32);
        hw_reg->h265d_addr.h26x_scanlist_base.scanlist_addr = addr;
        hw_reg->common.dec_sec_en.scanlist_addr_valid_en = 1;

        mpp_put_bits(&bp, 0, 70);//yandong change
        mpp_put_align(&bp, 64, 0xf);//128
    }

    for (i = 0; i < 64; i++)
        memcpy(pps_ptr + i * 112, pps_packet, 112);

#ifdef dump
    fwrite(pps_ptr, 1, 80 * 64, fp);
    RK_U32 *tmp = (RK_U32 *)pps_ptr;
    for (i = 0; i < 112 / 4; i++) {
        mpp_log("pps[%3d] = 0x%08x\n", i, tmp[i]);
    }
#endif
    MPP_FREE(pps_packet);
    return 0;
}

static RK_S32 hal_h265d_output_pps_packet(void *hal, void *dxva)
{
    RK_S32 fifo_len = 10;
    RK_S32 i, j;
    RK_U32 addr;
    RK_U32 log2_min_cb_size;
    RK_S32 width, height;
    HalH265dCtx *reg_cxt = ( HalH265dCtx *)hal;
    h265d_dxva2_picture_context_t *dxva_cxt = (h265d_dxva2_picture_context_t*)dxva;
    BitputCtx_t bp;
    RK_U64 *pps_packet = mpp_calloc(RK_U64, fifo_len + 1);

    if (NULL == reg_cxt || dxva_cxt == NULL) {
        mpp_err("%s:%s:%d reg_cxt or dxva_cxt is NULL",
                __FILE__, __FUNCTION__, __LINE__);
        MPP_FREE(pps_packet);
        return MPP_ERR_NULL_PTR;
    }

    void *pps_ptr = mpp_buffer_get_ptr(reg_cxt->pps_data);
    if (NULL == pps_ptr) {
        mpp_err("pps_data get ptr error");
        return MPP_ERR_NOMEM;
    }
    memset(pps_ptr, 0, 80 * 64);
    // pps_packet = (RK_U64 *)(pps_ptr + dxva_cxt->pp.pps_id * 80);

    for (i = 0; i < 10; i++) pps_packet[i] = 0;

    mpp_set_bitput_ctx(&bp, pps_packet, fifo_len);

    // SPS
    mpp_put_bits(&bp, dxva_cxt->pp.vps_id                            , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.sps_id                            , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.chroma_format_idc                 , 2);

    log2_min_cb_size = dxva_cxt->pp.log2_min_luma_coding_block_size_minus3 + 3;
    width = (dxva_cxt->pp.PicWidthInMinCbsY << log2_min_cb_size);
    height = (dxva_cxt->pp.PicHeightInMinCbsY << log2_min_cb_size);

    mpp_put_bits(&bp, width                                          , 16);//yandong
    mpp_put_bits(&bp, height                                         , 16);//yandong
    mpp_put_bits(&bp, dxva_cxt->pp.bit_depth_luma_minus8 + 8         , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.bit_depth_chroma_minus8 + 8       , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.log2_max_pic_order_cnt_lsb_minus4 + 4      , 5);
    mpp_put_bits(&bp, dxva_cxt->pp.log2_diff_max_min_luma_coding_block_size   , 2); //log2_maxa_coding_block_depth
    mpp_put_bits(&bp, dxva_cxt->pp.log2_min_luma_coding_block_size_minus3 + 3 , 3);
    mpp_put_bits(&bp, dxva_cxt->pp.log2_min_transform_block_size_minus2 + 2   , 3);
    ///<-zrh comment ^  57 bit above
    mpp_put_bits(&bp, dxva_cxt->pp.log2_diff_max_min_transform_block_size     , 2);
    mpp_put_bits(&bp, dxva_cxt->pp.max_transform_hierarchy_depth_inter        , 3);
    mpp_put_bits(&bp, dxva_cxt->pp.max_transform_hierarchy_depth_intra        , 3);
    mpp_put_bits(&bp, dxva_cxt->pp.scaling_list_enabled_flag                  , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.amp_enabled_flag                           , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.sample_adaptive_offset_enabled_flag        , 1);
    ///<-zrh comment ^  68 bit above
    mpp_put_bits(&bp, dxva_cxt->pp.pcm_enabled_flag                           , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.pcm_enabled_flag ? (dxva_cxt->pp.pcm_sample_bit_depth_luma_minus1 + 1) : 0  , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.pcm_enabled_flag ? (dxva_cxt->pp.pcm_sample_bit_depth_chroma_minus1 + 1) : 0 , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.pcm_loop_filter_disabled_flag                                               , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.log2_diff_max_min_pcm_luma_coding_block_size                                , 3);
    mpp_put_bits(&bp, dxva_cxt->pp.pcm_enabled_flag ? (dxva_cxt->pp.log2_min_pcm_luma_coding_block_size_minus3 + 3) : 0, 3);

    mpp_put_bits(&bp, dxva_cxt->pp.num_short_term_ref_pic_sets             , 7);
    mpp_put_bits(&bp, dxva_cxt->pp.long_term_ref_pics_present_flag         , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.num_long_term_ref_pics_sps              , 6);
    mpp_put_bits(&bp, dxva_cxt->pp.sps_temporal_mvp_enabled_flag           , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.strong_intra_smoothing_enabled_flag     , 1);
    ///<-zrh comment ^ 100 bit above

    mpp_put_bits(&bp, 0                                                    , 7 );
    mpp_put_align(&bp                                                         , 32, 0xf);

    // PPS
    mpp_put_bits(&bp, dxva_cxt->pp.pps_id                                    , 6 );
    mpp_put_bits(&bp, dxva_cxt->pp.sps_id                                    , 4 );
    mpp_put_bits(&bp, dxva_cxt->pp.dependent_slice_segments_enabled_flag     , 1 );
    mpp_put_bits(&bp, dxva_cxt->pp.output_flag_present_flag                  , 1 );
    mpp_put_bits(&bp, dxva_cxt->pp.num_extra_slice_header_bits               , 13);
    mpp_put_bits(&bp, dxva_cxt->pp.sign_data_hiding_enabled_flag , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.cabac_init_present_flag                   , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.num_ref_idx_l0_default_active_minus1 + 1  , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.num_ref_idx_l1_default_active_minus1 + 1  , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.init_qp_minus26                           , 7);
    mpp_put_bits(&bp, dxva_cxt->pp.constrained_intra_pred_flag               , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.transform_skip_enabled_flag               , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.cu_qp_delta_enabled_flag                  , 1);

    mpp_put_bits(&bp, log2_min_cb_size +
                 dxva_cxt->pp.log2_diff_max_min_luma_coding_block_size -
                 dxva_cxt->pp.diff_cu_qp_delta_depth                             , 3);

    h265h_dbg(H265H_DBG_PPS, "log2_min_cb_size %d %d %d \n", log2_min_cb_size,
              dxva_cxt->pp.log2_diff_max_min_luma_coding_block_size, dxva_cxt->pp.diff_cu_qp_delta_depth );

    mpp_put_bits(&bp, dxva_cxt->pp.pps_cb_qp_offset                            , 5);
    mpp_put_bits(&bp, dxva_cxt->pp.pps_cr_qp_offset                            , 5);
    mpp_put_bits(&bp, dxva_cxt->pp.pps_slice_chroma_qp_offsets_present_flag    , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.weighted_pred_flag                          , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.weighted_bipred_flag                        , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.transquant_bypass_enabled_flag              , 1 );
    mpp_put_bits(&bp, dxva_cxt->pp.tiles_enabled_flag                          , 1 );
    mpp_put_bits(&bp, dxva_cxt->pp.entropy_coding_sync_enabled_flag            , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.pps_loop_filter_across_slices_enabled_flag  , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.loop_filter_across_tiles_enabled_flag       , 1);

    mpp_put_bits(&bp, dxva_cxt->pp.deblocking_filter_override_enabled_flag     , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.pps_deblocking_filter_disabled_flag         , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.pps_beta_offset_div2                        , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.pps_tc_offset_div2                          , 4);
    mpp_put_bits(&bp, dxva_cxt->pp.lists_modification_present_flag             , 1);
    mpp_put_bits(&bp, dxva_cxt->pp.log2_parallel_merge_level_minus2 + 2        , 3);
    mpp_put_bits(&bp, dxva_cxt->pp.slice_segment_header_extension_present_flag , 1);
    mpp_put_bits(&bp, 0                                                        , 3);
    mpp_put_bits(&bp, dxva_cxt->pp.num_tile_columns_minus1 + 1, 5);
    mpp_put_bits(&bp, dxva_cxt->pp.num_tile_rows_minus1 + 1 , 5 );
    mpp_put_bits(&bp, 3, 2); //mSps_Pps[i]->mMode
    mpp_put_align(&bp, 64, 0xf);

    {
        /// tiles info begin
        RK_U16 column_width[20];
        RK_U16 row_height[22];

        memset(column_width, 0, sizeof(column_width));
        memset(row_height, 0, sizeof(row_height));

        if (dxva_cxt->pp.tiles_enabled_flag) {
            if (dxva_cxt->pp.uniform_spacing_flag == 0) {
                RK_S32 maxcuwidth = dxva_cxt->pp.log2_diff_max_min_luma_coding_block_size + log2_min_cb_size;
                RK_S32 ctu_width_in_pic = (width +
                                           (1 << maxcuwidth) - 1) / (1 << maxcuwidth) ;
                RK_S32 ctu_height_in_pic = (height +
                                            (1 << maxcuwidth) - 1) / (1 << maxcuwidth) ;
                RK_S32 sum = 0;
                for (i = 0; i < dxva_cxt->pp.num_tile_columns_minus1; i++) {
                    column_width[i] = dxva_cxt->pp.column_width_minus1[i] + 1;
                    sum += column_width[i]  ;
                }
                column_width[i] = ctu_width_in_pic - sum;

                sum = 0;
                for (i = 0; i < dxva_cxt->pp.num_tile_rows_minus1; i++) {
                    row_height[i] = dxva_cxt->pp.row_height_minus1[i] + 1;
                    sum += row_height[i];
                }
                row_height[i] = ctu_height_in_pic - sum;
            } // end of (pps->uniform_spacing_flag == 0)
            else {

                RK_S32 pic_in_cts_width = (width +
                                           (1 << (log2_min_cb_size +
                                                  dxva_cxt->pp.log2_diff_max_min_luma_coding_block_size)) - 1)
                                          / (1 << (log2_min_cb_size +
                                                   dxva_cxt->pp.log2_diff_max_min_luma_coding_block_size));
                RK_S32 pic_in_cts_height = (height +
                                            (1 << (log2_min_cb_size +
                                                   dxva_cxt->pp.log2_diff_max_min_luma_coding_block_size)) - 1)
                                           / (1 << (log2_min_cb_size +
                                                    dxva_cxt->pp.log2_diff_max_min_luma_coding_block_size));

                for (i = 0; i < dxva_cxt->pp.num_tile_columns_minus1 + 1; i++)
                    column_width[i] = ((i + 1) * pic_in_cts_width) / (dxva_cxt->pp.num_tile_columns_minus1 + 1) -
                                      (i * pic_in_cts_width) / (dxva_cxt->pp.num_tile_columns_minus1 + 1);

                for (i = 0; i < dxva_cxt->pp.num_tile_rows_minus1 + 1; i++)
                    row_height[i] = ((i + 1) * pic_in_cts_height) / (dxva_cxt->pp.num_tile_rows_minus1 + 1) -
                                    (i * pic_in_cts_height) / (dxva_cxt->pp.num_tile_rows_minus1 + 1);
            }
        } // pps->tiles_enabled_flag
        else {
            RK_S32 MaxCUWidth = (1 << (dxva_cxt->pp.log2_diff_max_min_luma_coding_block_size + log2_min_cb_size));
            column_width[0] = (width  + MaxCUWidth - 1) / MaxCUWidth;
            row_height[0]   = (height + MaxCUWidth - 1) / MaxCUWidth;
        }

        for (j = 0; j < 20; j++) {
            if (column_width[j] > 0)
                column_width[j]--;
            mpp_put_bits(&bp, column_width[j], 12);// yandong 8bit -> 12bit
        }

        for (j = 0; j < 22; j++) {
            if (row_height[j] > 0)
                row_height[j]--;
            mpp_put_bits(&bp, row_height[j], 12);// yandong 8bit -> 12bit
        }
    }

    {
        RK_U8 *ptr_scaling = (RK_U8 *)mpp_buffer_get_ptr(reg_cxt->scaling_list_data);
        if (dxva_cxt->pp.scaling_list_data_present_flag) {
            addr = (dxva_cxt->pp.pps_id + 16) * 1360;
        } else if (dxva_cxt->pp.scaling_list_enabled_flag) {
            addr = dxva_cxt->pp.sps_id * 1360;
        } else {
            addr = 80 * 1360;
        }

        hal_h265d_output_scalinglist_packet(hal, ptr_scaling + addr, dxva);

        RK_U32 fd = mpp_buffer_get_fd(reg_cxt->scaling_list_data);
        /* need to config addr */
        addr = fd | (addr << 10);

        mpp_put_bits(&bp, addr, 32);
        mpp_put_align(&bp, 64, 0xf);
    }

    for (i = 0; i < 64; i++)
        memcpy(pps_ptr + i * 80, pps_packet, 80);

#ifdef dump
    fwrite(pps_ptr, 1, 80 * 64, fp);
    fflush(fp);
#endif

    MPP_FREE(pps_packet);
    return 0;
}

static MPP_RET hal_h265d_vdpu34x_gen_regs(void *hal,  HalTaskInfo *syn)
{
    RK_S32 i = 0;
    RK_S32 log2_min_cb_size;
    RK_S32 width, height;
    RK_S32 stride_y, stride_uv, virstrid_y, virstrid_yuv;
    Vdpu34xH265dRegSet *hw_regs;
    RK_S32 ret = MPP_SUCCESS;
    MppBuffer streambuf = NULL;
    RK_S32 aglin_offset = 0;
    RK_S32 valid_ref = -1;
    MppBuffer framebuf = NULL;
    RK_U32  sw_ref_valid = 0;

    if (syn->dec.flags.parse_err ||
        syn->dec.flags.ref_err) {
        h265h_dbg(H265H_DBG_TASK_ERR, "%s found task error\n", __FUNCTION__);
        return MPP_OK;
    }

    h265d_dxva2_picture_context_t *dxva_cxt =
        (h265d_dxva2_picture_context_t *)syn->dec.syntax.data;
    HalH265dCtx *reg_cxt = ( HalH265dCtx *)hal;

    void *rps_ptr = NULL;
    if (reg_cxt ->fast_mode) {
        for (i = 0; i < MAX_GEN_REG; i++) {
            if (!reg_cxt->g_buf[i].use_flag) {
                syn->dec.reg_index = i;
                reg_cxt->rps_data = reg_cxt->g_buf[i].rps_data;
                reg_cxt->scaling_list_data =
                    reg_cxt->g_buf[i].scaling_list_data;
                reg_cxt->pps_data = reg_cxt->g_buf[i].pps_data;
                reg_cxt->hw_regs = reg_cxt->g_buf[i].hw_regs;
                reg_cxt->g_buf[i].use_flag = 1;
                break;
            }
        }
        if (i == MAX_GEN_REG) {
            mpp_err("hevc rps buf all used");
            return MPP_ERR_NOMEM;
        }
    }
    rps_ptr = mpp_buffer_get_ptr(reg_cxt->rps_data);
    if (NULL == rps_ptr) {

        mpp_err("rps_data get ptr error");
        return MPP_ERR_NOMEM;
    }


    if (syn->dec.syntax.data == NULL) {
        mpp_err("%s:%s:%d dxva is NULL", __FILE__, __FUNCTION__, __LINE__);
        return MPP_ERR_NULL_PTR;
    }

    /* output pps */
    hw_regs = (Vdpu34xH265dRegSet*)reg_cxt->hw_regs;
    memset(hw_regs, 0, sizeof(Vdpu34xH265dRegSet));

    if (reg_cxt->is_v34x) {
        hal_h265d_v345_output_pps_packet(hal, syn->dec.syntax.data);
    } else {
        hal_h265d_output_pps_packet(hal, syn->dec.syntax.data);
    }

    if (NULL == reg_cxt->hw_regs) {
        return MPP_ERR_NULL_PTR;
    }


    log2_min_cb_size = dxva_cxt->pp.log2_min_luma_coding_block_size_minus3 + 3;

    width = (dxva_cxt->pp.PicWidthInMinCbsY << log2_min_cb_size);
    height = (dxva_cxt->pp.PicHeightInMinCbsY << log2_min_cb_size);

    stride_y = ((MPP_ALIGN(width, 64)
                 * (dxva_cxt->pp.bit_depth_luma_minus8 + 8)) >> 3);
    stride_uv = ((MPP_ALIGN(width, 64)
                  * (dxva_cxt->pp.bit_depth_chroma_minus8 + 8)) >> 3);

    stride_y = hevc_hor_align(stride_y);
    stride_uv = hevc_hor_align(stride_uv);
    virstrid_y = hevc_ver_align(height) * stride_y;
    virstrid_yuv  = virstrid_y + stride_uv * hevc_ver_align(height) / 2;

    hw_regs->common.dec_slice_num.slice_num = dxva_cxt->slice_count;
    hw_regs->common.dec_y_hor_stride.y_hor_virstride = stride_y >> 4;
    hw_regs->common.dec_uv_hor_stride.uv_hor_virstride = stride_uv >> 4;
    hw_regs->common.dec_y_stride.y_virstride = virstrid_y >> 4;
    //hw_regs->sw_yuv_virstride = virstrid_yuv >> 4;
    hw_regs->h265d_param.h26x_set.h26x_rps_mode = 0;
    hw_regs->h265d_param.h26x_set.h26x_frame_orslice = 0;
    hw_regs->h265d_param.h26x_set.h26x_stream_mode = 0;

    mpp_buf_slot_get_prop(reg_cxt->slots, dxva_cxt->pp.CurrPic.Index7Bits,
                          SLOT_BUFFER, &framebuf);
    hw_regs->common_addr.decout_base.decout_base  = mpp_buffer_get_fd(framebuf); //just index need map
    //add colmv base  offset(22bits) + addr(10bits)
    hw_regs->common_addr.colmv_cur_base.colmv_cur_base =
        hw_regs->common_addr.decout_base.decout_base + ((virstrid_yuv) << 10);
    /*if out_base is equal to zero it means this frame may error
    we return directly add by csy*/

    if (hw_regs->common_addr.decout_base.decout_base == 0) {
        return 0;
    }

    hw_regs->h265d_param.cur_poc.cur_top_poc = dxva_cxt->pp.CurrPicOrderCntVal;

    mpp_buf_slot_get_prop(reg_cxt->packet_slots, syn->dec.input, SLOT_BUFFER,
                          &streambuf);
    if ( dxva_cxt->bitstream == NULL) {
        dxva_cxt->bitstream = mpp_buffer_get_ptr(streambuf);
    }
    if (reg_cxt->is_v34x) {
#ifdef HW_RPS
        hw_regs->common.dec_sec_en.wait_reset_en = 1;
        hw_regs->h265d_param.hevc_mvc0.ref_pic_layer_same_with_cur = 0xffff;
        hal_h265d_slice_hw_rps(syn->dec.syntax.data, rps_ptr);
#else
        hw_regs->sw_sysctrl.sw_h26x_rps_mode = 1;
        hal_h265d_slice_output_rps(syn->dec.syntax.data, rps_ptr);
#endif
    } else {
        hal_h265d_slice_output_rps(syn->dec.syntax.data, rps_ptr);
    }
    hw_regs->h265d_addr.cabactbl_base.cabactbl_base  = mpp_buffer_get_fd(reg_cxt->cabac_table_data);
    hw_regs->h265d_addr.pps_base.pps_base            = mpp_buffer_get_fd(reg_cxt->pps_data);
    hw_regs->h265d_addr.rps_base.rps_base            = mpp_buffer_get_fd(reg_cxt->rps_data);
    hw_regs->common_addr.str_rlc_base.strm_rlc_base =  mpp_buffer_get_fd(streambuf);
    hw_regs->common_addr.rlcwrite_base.rlcwrite_base = mpp_buffer_get_fd(streambuf);
    hw_regs->common.dec_str_len.stream_len          = ((dxva_cxt->bitstream_size + 15)
                                                       & (~15)) + 64;
    aglin_offset =  hw_regs->common.dec_str_len.stream_len - dxva_cxt->bitstream_size;
    if (aglin_offset > 0) {
        memset((void *)(dxva_cxt->bitstream + dxva_cxt->bitstream_size), 0,
               aglin_offset);
    }
    hw_regs->common.dec_en.dec_e               = 1;
    hw_regs->common.dec_imp_en.dec_timeout_e   = 1;
    hw_regs->common.dec_sec_en.wr_ddr_align_en = dxva_cxt->pp.tiles_enabled_flag
                                                 ? 0 : 1;

    hw_regs->common.dec_cabac_err_en_lowbits.cabac_err_en_lowbits = 0xffffdfff;
    hw_regs->common.dec_cabac_err_en_highbits.cabac_err_en_highbits = 0x3ffbf9ff;

    valid_ref = hw_regs->common_addr.decout_base.decout_base;
    hw_regs->common_addr.error_ref_base.error_ref_base = valid_ref;
    for (i = 0; i < (RK_S32)MPP_ARRAY_ELEMS(dxva_cxt->pp.RefPicList); i++) {
        if (dxva_cxt->pp.RefPicList[i].bPicEntry != 0xff &&
            dxva_cxt->pp.RefPicList[i].bPicEntry != 0x7f) {
            hw_regs->h265d_param.ref0_15_poc[i].ref_poc = dxva_cxt->pp.PicOrderCntValList[i];
            mpp_buf_slot_get_prop(reg_cxt->slots,
                                  dxva_cxt->pp.RefPicList[i].Index7Bits,
                                  SLOT_BUFFER, &framebuf);
            if (framebuf != NULL) {
                hw_regs->h265d_addr.ref0_15_base[i].ref_base = mpp_buffer_get_fd(framebuf);
                valid_ref = hw_regs->h265d_addr.ref0_15_base[i].ref_base;
            } else {
                hw_regs->h265d_addr.ref0_15_base[i].ref_base = valid_ref;
            }
            hw_regs->h265d_addr.ref0_15_colmv_base[i].colmv_base =
                valid_ref + ((virstrid_yuv) << 10);
            sw_ref_valid          |=   (1 << i);
            SET_REF_VALID(hw_regs->h265d_param, i, 1);
        } else {
            hw_regs->h265d_addr.ref0_15_base[i].ref_base = hw_regs->common_addr.decout_base.decout_base;
            hw_regs->h265d_addr.ref0_15_colmv_base[i].colmv_base =
                hw_regs->common_addr.decout_base.decout_base + ((virstrid_yuv) << 10);
        }
    }

    hw_regs->common.dec_en_mode_set.colmv_error_mode = 1;
    hw_regs->common.dec_en_mode_set.timeout_mode = 1;
    hw_regs->common.dec_en_mode_set.cur_pic_is_idr = dxva_cxt->pp.IdrPicFlag;//p_hal->slice_long->idr_flag;
    hw_regs->common.dec_en_mode_set.h26x_error_mode = 1;
    hw_regs->common.dec_imp_en.buf_empty_en = 1;

    if (reg_cxt->rcb_buf == NULL) {
        RK_U32 rcb_buf_size =
            RCB_INTRAR_COEF * width +
            RCB_TRANSDR_COEF * width +
            RCB_TRANSDC_COEF * height +
            RCB_STRMDR_COEF * width +
            RCB_INTERR_COEF * width +
            RCB_INTERC_COEF * height +
            RCB_DBLKR_COEF * width +
            RCB_SAOR_COEF * width +
            RCB_FBCR_COEF * width +
            RCB_FILTC_COEF * height;
        ret = mpp_buffer_get(reg_cxt->group,
                             &reg_cxt->rcb_buf, rcb_buf_size);
    }
    if (reg_cxt->rcb_buf != NULL) {
        RK_U32 transdr_offset = RCB_INTRAR_COEF * width;
        RK_U32 transdc_offset = RCB_INTRAR_COEF * width + RCB_TRANSDR_COEF * width;
        RK_U32 strmdr_offset = RCB_INTRAR_COEF * width +
                               RCB_TRANSDR_COEF * width +
                               RCB_TRANSDC_COEF * height;
        RK_U32 interr_offset = RCB_INTRAR_COEF * width +
                               RCB_TRANSDR_COEF * width +
                               RCB_TRANSDC_COEF * height +
                               RCB_STRMDR_COEF * width;
        RK_U32 interc_offset = RCB_INTRAR_COEF * width +
                               RCB_TRANSDR_COEF * width +
                               RCB_TRANSDC_COEF * height +
                               RCB_STRMDR_COEF * width +
                               RCB_INTERR_COEF * width;
        RK_U32 dblkr_offset = RCB_INTRAR_COEF * width +
                              RCB_TRANSDR_COEF * width +
                              RCB_TRANSDC_COEF * height +
                              RCB_STRMDR_COEF * width +
                              RCB_INTERR_COEF * width +
                              RCB_INTERC_COEF * height;
        RK_U32 saor_offset = RCB_INTRAR_COEF * width +
                             RCB_TRANSDR_COEF * width +
                             RCB_TRANSDC_COEF * height +
                             RCB_STRMDR_COEF * width +
                             RCB_INTERR_COEF * width +
                             RCB_INTERC_COEF * height +
                             RCB_DBLKR_COEF * width;
        RK_U32 fbcr_offset = RCB_INTRAR_COEF * width +
                             RCB_TRANSDR_COEF * width +
                             RCB_TRANSDC_COEF * height +
                             RCB_STRMDR_COEF * width +
                             RCB_INTERR_COEF * width +
                             RCB_INTERC_COEF * height +
                             RCB_DBLKR_COEF * width +
                             RCB_SAOR_COEF * width;
        RK_U32 filtc_offset = RCB_INTRAR_COEF * width +
                              RCB_TRANSDR_COEF * width +
                              RCB_TRANSDC_COEF * height +
                              RCB_STRMDR_COEF * width +
                              RCB_INTERR_COEF * width +
                              RCB_INTERC_COEF * height +
                              RCB_DBLKR_COEF * width +
                              RCB_SAOR_COEF * width +
                              RCB_FBCR_COEF * width;

        hw_regs->common_addr.rcb_intra_base.rcb_intra_base = mpp_buffer_get_fd(reg_cxt->rcb_buf);
        hw_regs->common_addr.rcb_transd_row_base.rcb_transd_row_base = mpp_buffer_get_fd(reg_cxt->rcb_buf) + (transdr_offset << 10);
        hw_regs->common_addr.rcb_transd_col_base.rcb_transd_col_base = mpp_buffer_get_fd(reg_cxt->rcb_buf) + (transdc_offset << 10);
        hw_regs->common_addr.rcb_streamd_row_base.rcb_streamd_row_base = mpp_buffer_get_fd(reg_cxt->rcb_buf) + (strmdr_offset << 10);
        hw_regs->common_addr.rcb_inter_row_base.rcb_inter_row_base = mpp_buffer_get_fd(reg_cxt->rcb_buf) + (interr_offset << 10);
        hw_regs->common_addr.rcb_inter_col_base.rcb_inter_col_base = mpp_buffer_get_fd(reg_cxt->rcb_buf) + (interc_offset << 10);
        hw_regs->common_addr.rcb_dblk_base.rcb_dblk_base = mpp_buffer_get_fd(reg_cxt->rcb_buf) + (dblkr_offset << 10);
        hw_regs->common_addr.rcb_sao_base.rcb_sao_base = mpp_buffer_get_fd(reg_cxt->rcb_buf) + (saor_offset << 10);
        hw_regs->common_addr.rcb_fbc_base.rcb_fbc_base = mpp_buffer_get_fd(reg_cxt->rcb_buf) + (fbcr_offset << 10);
        hw_regs->common_addr.rcb_filter_col_base.rcb_filter_col_base = mpp_buffer_get_fd(reg_cxt->rcb_buf) + (filtc_offset << 10);
    }
    return ret;
}

static MPP_RET hal_h265d_vdpu34x_start(void *hal, HalTaskInfo *task)
{
    MPP_RET ret = MPP_OK;
    RK_U8* p = NULL;
    Vdpu34xH265dRegSet *hw_regs = NULL;
    HalH265dCtx *reg_cxt = (HalH265dCtx *)hal;
    RK_S32 index =  task->dec.reg_index;

    RK_S32 i;

    if (task->dec.flags.parse_err ||
        task->dec.flags.ref_err) {
        h265h_dbg(H265H_DBG_TASK_ERR, "%s found task error\n", __FUNCTION__);
        return MPP_OK;
    }

    if (reg_cxt->fast_mode) {
        p = (RK_U8*)reg_cxt->g_buf[index].hw_regs;
        hw_regs = ( Vdpu34xH265dRegSet *)reg_cxt->g_buf[index].hw_regs;
    } else {
        p = (RK_U8*)reg_cxt->hw_regs;
        hw_regs = ( Vdpu34xH265dRegSet *)reg_cxt->hw_regs;
    }

    if (hw_regs == NULL) {
        mpp_err("hal_h265d_start hw_regs is NULL");
        return MPP_ERR_NULL_PTR;
    }
    for (i = 0; i < 68; i++) {
        h265h_dbg(H265H_DBG_REG, "RK_HEVC_DEC: regs[%02d]=%08X\n",
                  i, *((RK_U32*)p));
        //mpp_log("RK_HEVC_DEC: regs[%02d]=%08X\n", i, *((RK_U32*)p));
        p += 4;
    }

    do {
        MppDevRegWrCfg wr_cfg;
        MppDevRegRdCfg rd_cfg;

        wr_cfg.reg = &hw_regs->common;
        wr_cfg.size = sizeof(hw_regs->common);
        wr_cfg.offset = OFFSET_COMMON_REGS;

        ret = mpp_dev_ioctl(reg_cxt->dev, MPP_DEV_REG_WR, &wr_cfg);
        if (ret) {
            mpp_err_f("set register write failed %d\n", ret);
            break;
        }

        wr_cfg.reg = &hw_regs->h265d_param;
        wr_cfg.size = sizeof(hw_regs->h265d_param);
        wr_cfg.offset = OFFSET_CODEC_PARAMS_REGS;

        ret = mpp_dev_ioctl(reg_cxt->dev, MPP_DEV_REG_WR, &wr_cfg);
        if (ret) {
            mpp_err_f("set register write failed %d\n", ret);
            break;
        }

        wr_cfg.reg = &hw_regs->common_addr;
        wr_cfg.size = sizeof(hw_regs->common_addr);
        wr_cfg.offset = OFFSET_COMMON_ADDR_REGS;

        ret = mpp_dev_ioctl(reg_cxt->dev, MPP_DEV_REG_WR, &wr_cfg);
        if (ret) {
            mpp_err_f("set register write failed %d\n", ret);
            break;
        }

        wr_cfg.reg = &hw_regs->h265d_addr;
        wr_cfg.size = sizeof(hw_regs->h265d_addr);
        wr_cfg.offset = OFFSET_CODEC_ADDR_REGS;

        ret = mpp_dev_ioctl(reg_cxt->dev, MPP_DEV_REG_WR, &wr_cfg);
        if (ret) {
            mpp_err_f("set register write failed %d\n", ret);
            break;
        }

        rd_cfg.reg = &hw_regs->irq_status;
        rd_cfg.size = sizeof(hw_regs->irq_status);
        rd_cfg.offset = OFFSET_INTERRUPT_REGS;

        ret = mpp_dev_ioctl(reg_cxt->dev, MPP_DEV_REG_RD, &rd_cfg);
        if (ret) {
            mpp_err_f("set register read failed %d\n", ret);
            break;
        }

        ret = mpp_dev_ioctl(reg_cxt->dev, MPP_DEV_CMD_SEND, NULL);
        if (ret) {
            mpp_err_f("send cmd failed %d\n", ret);
            break;
        }
    } while (0);

    return ret;
}


static MPP_RET hal_h265d_vdpu34x_wait(void *hal, HalTaskInfo *task)
{
    MPP_RET ret = MPP_OK;
    RK_S32 index =  task->dec.reg_index;
    HalH265dCtx *reg_cxt = (HalH265dCtx *)hal;
    RK_U8* p = NULL;
    Vdpu34xH265dRegSet *hw_regs = NULL;
    RK_S32 i;

    if (task->dec.flags.parse_err ||
        task->dec.flags.ref_err) {
        h265h_dbg(H265H_DBG_TASK_ERR, "%s found task error\n", __FUNCTION__);
        goto ERR_PROC;
    }

    if (reg_cxt->fast_mode) {
        hw_regs = ( Vdpu34xH265dRegSet *)reg_cxt->g_buf[index].hw_regs;
    } else {
        hw_regs = ( Vdpu34xH265dRegSet *)reg_cxt->hw_regs;
    }

    p = (RK_U8*)hw_regs;

    ret = mpp_dev_ioctl(reg_cxt->dev, MPP_DEV_CMD_POLL, NULL);
    if (ret)
        mpp_err_f("poll cmd failed %d\n", ret);

ERR_PROC:
    if (task->dec.flags.parse_err ||
        task->dec.flags.ref_err ||
        hw_regs->irq_status.sta_int.dec_error_sta ||
        hw_regs->irq_status.sta_int.buf_empty_sta) {
        if (!reg_cxt->fast_mode) {
            if (reg_cxt->int_cb.callBack)
                reg_cxt->int_cb.callBack(reg_cxt->int_cb.opaque, &task->dec);
        } else {
            MppFrame mframe = NULL;
            mpp_buf_slot_get_prop(reg_cxt->slots, task->dec.output,
                                  SLOT_FRAME_PTR, &mframe);
            if (mframe) {
                reg_cxt->fast_mode_err_found = 1;
                mpp_frame_set_errinfo(mframe, 1);
            }
        }
    } else {
        if (reg_cxt->fast_mode && reg_cxt->fast_mode_err_found) {
            for (i = 0; i < (RK_S32)MPP_ARRAY_ELEMS(task->dec.refer); i++) {
                if (task->dec.refer[i] >= 0) {
                    MppFrame frame_ref = NULL;

                    mpp_buf_slot_get_prop(reg_cxt->slots, task->dec.refer[i],
                                          SLOT_FRAME_PTR, &frame_ref);
                    h265h_dbg(H265H_DBG_FAST_ERR, "refer[%d] %d frame %p\n",
                              i, task->dec.refer[i], frame_ref);
                    if (frame_ref && mpp_frame_get_errinfo(frame_ref)) {
                        MppFrame frame_out = NULL;
                        mpp_buf_slot_get_prop(reg_cxt->slots, task->dec.output,
                                              SLOT_FRAME_PTR, &frame_out);
                        mpp_frame_set_errinfo(frame_out, 1);
                        break;
                    }
                }
            }
        }
    }

    for (i = 0; i < 68; i++) {
        if (i == 1) {
            h265h_dbg(H265H_DBG_REG, "RK_HEVC_DEC: regs[%02d]=%08X\n",
                      i, *((RK_U32*)p));
        }

        if (i == 45) {
            h265h_dbg(H265H_DBG_REG, "RK_HEVC_DEC: regs[%02d]=%08X\n",
                      i, *((RK_U32*)p));
        }
        p += 4;
    }

    if (reg_cxt->fast_mode) {
        reg_cxt->g_buf[index].use_flag = 0;
    }

    return ret;
}

static MPP_RET hal_h265d_vdpu34x_reset(void *hal)
{
    MPP_RET ret = MPP_OK;
    HalH265dCtx *p_hal = (HalH265dCtx *)hal;
    p_hal->fast_mode_err_found = 0;
    (void)hal;
    return ret;
}

static MPP_RET hal_h265d_vdpu34x_flush(void *hal)
{
    MPP_RET ret = MPP_OK;

    (void)hal;
    return ret;
}

static MPP_RET hal_h265d_vdpu34x_control(void *hal, MpiCmd cmd_type, void *param)
{
    MPP_RET ret = MPP_OK;

    (void)hal;
    (void)param;
    switch ((MpiCmd)cmd_type) {
    case MPP_DEC_SET_OUTPUT_FORMAT: {
    } break;
    default:
        break;
    }
    return  ret;
}

const MppHalApi hal_h265d_vdpu34x = {
    .name = "h265d_vdpu34x",
    .type = MPP_CTX_DEC,
    .coding = MPP_VIDEO_CodingHEVC,
    .ctx_size = sizeof(HalH265dCtx),
    .flag = 0,
    .init = hal_h265d_vdpu34x_init,
    .deinit = hal_h265d_vdpu34x_deinit,
    .reg_gen = hal_h265d_vdpu34x_gen_regs,
    .start = hal_h265d_vdpu34x_start,
    .wait = hal_h265d_vdpu34x_wait,
    .reset = hal_h265d_vdpu34x_reset,
    .flush = hal_h265d_vdpu34x_flush,
    .control = hal_h265d_vdpu34x_control,
};
