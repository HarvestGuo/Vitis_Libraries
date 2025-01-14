/*
 * Copyright 2022 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "xf_isp_types.h"

static uint32_t hist0_awb[NUM_STREAMS][3][HIST_SIZE] = {0};
static uint32_t hist1_awb[NUM_STREAMS][3][HIST_SIZE] = {0};
static constexpr int MinMaxVArrSize =
    LTMTile<BLOCK_HEIGHT, BLOCK_WIDTH, STRM_HEIGHT, XF_WIDTH, XF_NPPC>::MinMaxVArrSize;
static constexpr int MinMaxHArrSize =
    LTMTile<BLOCK_HEIGHT, BLOCK_WIDTH, STRM_HEIGHT, XF_WIDTH, XF_NPPC>::MinMaxHArrSize;
static XF_CTUNAME(XF_DST_T, XF_NPPC) omin_r[NUM_STREAMS][MinMaxVArrSize][MinMaxHArrSize];
static XF_CTUNAME(XF_DST_T, XF_NPPC) omax_r[NUM_STREAMS][MinMaxVArrSize][MinMaxHArrSize];
static XF_CTUNAME(XF_DST_T, XF_NPPC) omin_w[NUM_STREAMS][MinMaxVArrSize][MinMaxHArrSize];
static XF_CTUNAME(XF_DST_T, XF_NPPC) omax_w[NUM_STREAMS][MinMaxVArrSize][MinMaxHArrSize];

static int igain_0[3] = {0};
static int igain_1[3] = {0};
static bool flag[NUM_STREAMS] = {0};

template <int SRC_T, int DST_T, int ROWS, int COLS, int NPC, int XFCVDEPTH_IN_1, int XFCVDEPTH_OUT_1>
void fifo_copy(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& demosaic_out,
               xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_OUT_1>& ltm_in,
               unsigned short height,
               unsigned short width) {
// clang-format off
#pragma HLS INLINE OFF
    // clang-format on
    ap_uint<13> row, col;
    int readindex = 0, writeindex = 0;

    ap_uint<13> img_width = width >> XF_BITSHIFT(NPC);

Row_Loop:
    for (row = 0; row < height; row++) {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=ROWS max=ROWS
#pragma HLS LOOP_FLATTEN off
    // clang-format on
    Col_Loop:
        for (col = 0; col < img_width; col++) {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=COLS/NPC max=COLS/NPC
#pragma HLS PIPELINE
            // clang-format on
            XF_TNAME(SRC_T, NPC) tmp_src;
            tmp_src = demosaic_out.read(readindex++);
            ltm_in.write(writeindex++, tmp_src);
        }
    }
}
template <int SRC_T, int DST_T, int ROWS, int COLS, int NPC, int XFCVDEPTH_IN_1, int XFCVDEPTH_OUT_1>
void fifo_awb(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& demosaic_out,
              xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_OUT_1>& ltm_in,
              uint32_t hist0[3][HIST_SIZE],
              uint32_t hist1[3][HIST_SIZE],
              int gain0[3],
              int gain1[3],
              unsigned short height,
              unsigned short width,
              float thresh) {
// clang-format off
#pragma HLS INLINE OFF
    // clang-format on	
    xf::cv::Mat<XF_DST_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XFCVDEPTH_IN_1> impop(height, width);

    float inputMin = 0.0f;
    float inputMax = (1 << (XF_DTPIXELDEPTH(XF_SRC_T, XF_NPPC))) - 1; // 65535.0f;
    float outputMin = 0.0f;
    float outputMax = (1 << (XF_DTPIXELDEPTH(XF_SRC_T, XF_NPPC))) - 1; // 65535.0f;
	
// clang-format off
#pragma HLS DATAFLOW
    // clang-format on
    if (WB_TYPE) {
        xf::cv::AWBhistogram<XF_DST_T, XF_DST_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, WB_TYPE, HIST_SIZE, XFCVDEPTH_IN_1,
                             XFCVDEPTH_IN_1>(demosaic_out, impop, hist0, thresh, inputMin, inputMax, outputMin,
                                             outputMax);
        xf::cv::AWBNormalization<XF_DST_T, XF_DST_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, WB_TYPE, HIST_SIZE, XFCVDEPTH_IN_1,
                                 XFCVDEPTH_OUT_1>(impop, ltm_in, hist1, thresh, inputMin, inputMax, outputMin,
                                                  outputMax);

    } else {
        xf::cv::AWBChannelGain<XF_DST_T, XF_DST_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, 0, XFCVDEPTH_IN_1, XFCVDEPTH_IN_1>(
            demosaic_out, impop, thresh, gain0);
        xf::cv::AWBGainUpdate<XF_DST_T, XF_DST_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, 0, XFCVDEPTH_IN_1, XFCVDEPTH_OUT_1>(
            impop, ltm_in, thresh, gain1);
    }
}

template <int SRC_T, int DST_T, int ROWS, int COLS, int NPC, int XFCVDEPTH_IN_1, int XFCVDEPTH_OUT_1>
void function_awb(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& demosaic_out,
                  xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_OUT_1>& ltm_in,
                  uint32_t hist0[3][HIST_SIZE],
                  uint32_t hist1[3][HIST_SIZE],
                  int gain0[3],
                  int gain1[3],
                  unsigned short height,
                  unsigned short width,
                  // unsigned char mode_reg,
                  float thresh) {
// clang-format off
#pragma HLS INLINE OFF
    // clang-format on

    fifo_awb<XF_DST_T, XF_DST_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XFCVDEPTH_IN_1, XFCVDEPTH_OUT_1>(
        demosaic_out, ltm_in, hist0, hist1, gain0, gain1, height, width, thresh);
}

static constexpr int MAX_HEIGHT = STRM_HEIGHT * 2;
static constexpr int MAX_WIDTH = XF_WIDTH + NUM_H_BLANK;
void Streampipeline(ap_uint<INPUT_PTR_WIDTH>* img_inp,
                    ap_uint<OUTPUT_PTR_WIDTH>* img_out,
                    unsigned short height,
                    unsigned short width,
                    uint32_t hist0[3][HIST_SIZE],
                    uint32_t hist1[3][HIST_SIZE],
                    int gain0[3],
                    int gain1[3],
                    struct ispparams_config params,
                    unsigned char _gamma_lut[256 * 3],
                    short wr_hls[NO_EXPS * XF_NPPC * W_B_SIZE],
                    XF_CTUNAME(XF_DST_T, XF_NPPC) omin_r[MinMaxVArrSize][MinMaxHArrSize],
                    XF_CTUNAME(XF_DST_T, XF_NPPC) omax_r[MinMaxVArrSize][MinMaxHArrSize],
                    XF_CTUNAME(XF_DST_T, XF_NPPC) omin_w[MinMaxVArrSize][MinMaxHArrSize],
                    XF_CTUNAME(XF_DST_T, XF_NPPC) omax_w[MinMaxVArrSize][MinMaxHArrSize]) {
    int max_height, max_width;
    max_height = height * 2;
    max_width = width + NUM_H_BLANK;

    xf::cv::Mat<XF_SRC_T, MAX_HEIGHT, MAX_WIDTH, XF_NPPC, XF_CV_DEPTH_IN_0> imgInput(max_height, max_width);
    xf::cv::Mat<XF_SRC_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_LEF> LEF_Img(height, width);
    xf::cv::Mat<XF_SRC_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_SEF> SEF_Img(height, width);
    xf::cv::Mat<XF_SRC_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_IN_1> hdr_out(height, width);
    xf::cv::Mat<XF_SRC_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_IN_2> blc_out(height, width);
    //  xf::cv::Mat<XF_SRC_T, XF_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_IN_3> bpc_out(height, width);
    xf::cv::Mat<XF_SRC_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_IN_3> gain_out(height, width);
    xf::cv::Mat<XF_DST_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_OUT_0> demosaic_out(height, width);
    xf::cv::Mat<XF_DST_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_OUT_1> impop(height, width);
    xf::cv::Mat<XF_DST_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_OUT_2> ltm_in(height, width);
    xf::cv::Mat<XF_DST_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_OUT_3> lsc_out(height, width);
    xf::cv::Mat<XF_LTM_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_OUT_4> _dst(height, width);
    xf::cv::Mat<XF_LTM_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_OUT_5> aecin(height, width);
    xf::cv::Mat<XF_16UC1, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_OUT_6> imgOutput(height, width);

// clang-format off
#pragma HLS DATAFLOW
    // clang-format on
    const int Q_VAL = 1 << (XF_DTPIXELDEPTH(XF_SRC_T, XF_NPPC));

    float thresh = (float)params.pawb / 256;
    float inputMax = (1 << (XF_DTPIXELDEPTH(XF_SRC_T, XF_NPPC))) - 1; // 65535.0f;

    float mul_fact = (inputMax / (inputMax - params.black_level));

    xf::cv::Array2xfMat<INPUT_PTR_WIDTH, XF_SRC_T, MAX_HEIGHT, MAX_WIDTH, XF_NPPC, XF_CV_DEPTH_IN_0>(img_inp, imgInput);

    xf::cv::extractExposureFrames<XF_SRC_T, NUM_V_BLANK_LINES, NUM_H_BLANK, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_USE_URAM,
                                  XF_CV_DEPTH_IN_0, XF_CV_DEPTH_LEF, XF_CV_DEPTH_SEF>(imgInput, LEF_Img, SEF_Img);

    xf::cv::Hdrmerge_bayer<XF_SRC_T, XF_SRC_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, NO_EXPS, W_B_SIZE, XF_CV_DEPTH_LEF,
                           XF_CV_DEPTH_SEF, XF_CV_DEPTH_IN_1>(LEF_Img, SEF_Img, hdr_out, wr_hls);

    xf::cv::blackLevelCorrection<XF_SRC_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, 16, 15, 1, XF_CV_DEPTH_IN_1,
                                 XF_CV_DEPTH_IN_2>(hdr_out, blc_out, params.black_level, mul_fact);

    // xf::cv::badpixelcorrection<XF_SRC_T, XF_HEIGHT, XF_WIDTH, XF_NPPC, 0, 0>(imgInput2, bpc_out);

    xf::cv::gaincontrol<XF_SRC_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_IN_2, XF_CV_DEPTH_IN_3>(
        blc_out, gain_out, params.rgain, params.bgain, params.ggain, params.bayer_p);

    xf::cv::demosaicing<XF_SRC_T, XF_DST_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, 0, XF_CV_DEPTH_IN_3, XF_CV_DEPTH_OUT_0>(
        gain_out, demosaic_out, params.bayer_p);

    function_awb<XF_DST_T, XF_DST_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_OUT_0, XF_CV_DEPTH_OUT_2>(
        demosaic_out, ltm_in, hist0, hist1, gain0, gain1, height, width, thresh);

    xf::cv::colorcorrectionmatrix<XF_CCM_TYPE, XF_DST_T, XF_DST_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_OUT_2,
                                  XF_CV_DEPTH_OUT_3>(ltm_in, lsc_out);

    if (XF_DST_T == XF_8UC3) {
        fifo_copy<XF_DST_T, XF_LTM_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_OUT_3, XF_CV_DEPTH_OUT_5>(
            lsc_out, aecin, height, width);
    } else {
        xf::cv::LTM<XF_DST_T, XF_LTM_T, BLOCK_HEIGHT, BLOCK_WIDTH, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_OUT_3,
                    XF_CV_DEPTH_OUT_5>::process(lsc_out, params.blk_height, params.blk_width, omin_r, omax_r, omin_w,
                                                omax_w, aecin);
    }
    xf::cv::gammacorrection<XF_LTM_T, XF_LTM_T, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_OUT_5, XF_CV_DEPTH_OUT_4>(
        aecin, _dst, _gamma_lut);

    // ColorMat2AXIvideo<XF_LTM_T, XF_HEIGHT, XF_WIDTH, XF_NPPC>(_dst, m_axis_video);
    xf::cv::rgb2yuyv<XF_LTM_T, XF_16UC1, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_OUT_4, XF_CV_DEPTH_OUT_6>(
        _dst, imgOutput);

    xf::cv::xfMat2Array<OUTPUT_PTR_WIDTH, XF_16UC1, STRM_HEIGHT, XF_WIDTH, XF_NPPC, XF_CV_DEPTH_OUT_6>(imgOutput,
                                                                                                       img_out);

    return;
}

void Streampipeline_wrap(ap_uint<INPUT_PTR_WIDTH>* img_inp,
                         ap_uint<OUTPUT_PTR_WIDTH>* img_out,
                         unsigned short height,
                         unsigned short width,
                         uint32_t hist0[3][HIST_SIZE],
                         uint32_t hist1[3][HIST_SIZE],
                         int gain0[3],
                         int gain1[3],
                         struct ispparams_config params,
                         unsigned char _gamma_lut[256 * 3],
                         short wr_hls[NO_EXPS * XF_NPPC * W_B_SIZE],
                         XF_CTUNAME(XF_DST_T, XF_NPPC) omin_r[MinMaxVArrSize][MinMaxHArrSize],
                         XF_CTUNAME(XF_DST_T, XF_NPPC) omax_r[MinMaxVArrSize][MinMaxHArrSize],
                         XF_CTUNAME(XF_DST_T, XF_NPPC) omin_w[MinMaxVArrSize][MinMaxHArrSize],
                         XF_CTUNAME(XF_DST_T, XF_NPPC) omax_w[MinMaxVArrSize][MinMaxHArrSize],
                         bool& flag,
                         bool& eof) {
// clang-format off
 #pragma HLS INLINE OFF
    // clang-format on

    if (!flag) {
        Streampipeline(img_inp, img_out, height, width, hist0, hist1, gain0, gain1, params, _gamma_lut, wr_hls, omin_r,
                       omax_r, omin_w, omax_w);
        if (eof) flag = 1;

    } else {
        Streampipeline(img_inp, img_out, height, width, hist1, hist0, gain1, gain0, params, _gamma_lut, wr_hls, omin_w,
                       omax_w, omin_r, omax_r);
        if (eof) flag = 0;
    }

    return;
}

/*********************************************************************************
 * Function:    ISPPipeline_accel
 * Parameters:  input and output image pointers, image resolution
 * Return:
 * Description:
 **********************************************************************************/
extern "C" {
void ISPPipeline_accel(ap_uint<INPUT_PTR_WIDTH>* img_inp1,
                       ap_uint<INPUT_PTR_WIDTH>* img_inp2,
                       ap_uint<INPUT_PTR_WIDTH>* img_inp3,
                       ap_uint<INPUT_PTR_WIDTH>* img_inp4,
                       ap_uint<OUTPUT_PTR_WIDTH>* img_out1,
                       ap_uint<OUTPUT_PTR_WIDTH>* img_out2,
                       ap_uint<OUTPUT_PTR_WIDTH>* img_out3,
                       ap_uint<OUTPUT_PTR_WIDTH>* img_out4,
                       unsigned short array_params[NUM_STREAMS][10],
                       unsigned char gamma_lut[NUM_STREAMS][256 * 3],
                       short wr_hls[NUM_STREAMS][NO_EXPS * XF_NPPC * W_B_SIZE]) {
// clang-format off
#pragma HLS INTERFACE m_axi     port=img_inp1  offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi     port=img_inp2  offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi     port=img_inp3  offset=slave bundle=gmem3
#pragma HLS INTERFACE m_axi     port=img_inp4  offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi     port=img_out1  offset=slave bundle=gmem5
#pragma HLS INTERFACE m_axi     port=img_out2  offset=slave bundle=gmem6
#pragma HLS INTERFACE m_axi     port=img_out3  offset=slave bundle=gmem7
#pragma HLS INTERFACE m_axi     port=img_out4  offset=slave bundle=gmem8
#pragma HLS INTERFACE m_axi     port=wr_hls  offset=slave bundle=gmem9
// clang-format on

// clang-format off
#pragma HLS ARRAY_PARTITION variable=hist0_awb complete dim=1
#pragma HLS ARRAY_PARTITION variable=hist1_awb complete dim=1
#pragma HLS ARRAY_PARTITION variable=hist0_awb complete dim=2
#pragma HLS ARRAY_PARTITION variable=hist1_awb complete dim=2

    // clang-format on

    struct ispparams_config params[NUM_STREAMS];

    uint32_t tot_rows = 0;
    int rem_rows[NUM_STREAMS];
    short wr_hls_tmp[NUM_STREAMS][NO_EXPS * XF_NPPC * W_B_SIZE];

PARAMS_SET_LOOP:
    for (int i = 0; i < NUM_STREAMS; i++) {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=1 max=NUM_STREAMS
        // clang-format on

        params[i].rgain = array_params[i][0];
        params[i].bgain = array_params[i][1];
        params[i].ggain = array_params[i][2];
        params[i].pawb = array_params[i][3];
        params[i].bayer_p = array_params[i][4];
        params[i].black_level = array_params[i][5];
        params[i].height = array_params[i][6];
        params[i].width = array_params[i][7];
        params[i].blk_height = array_params[i][8];
        params[i].blk_width = array_params[i][9];

        params[i].height = params[i].height * 2;
        tot_rows = tot_rows + params[i].height;
        rem_rows[i] = params[i].height;
    }

WR_HLS_INIT_LOOP:
    for (int n = 0; n < NUM_STREAMS; n++) {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=NUM_STREAMS max=NUM_STREAMS
        // clang-format on
        for (int k = 0; k < XF_NPPC; k++) {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=XF_NPPC max=XF_NPPC
            // clang-format on
            for (int i = 0; i < NO_EXPS; i++) {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=NO_EXPS max=NO_EXPS
                // clang-format on
                for (int j = 0; j < (W_B_SIZE); j++) {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=W_B_SIZE max=W_B_SIZE
                    // clang-format on
                    wr_hls_tmp[n][(i + k * NO_EXPS) * W_B_SIZE + j] = wr_hls[n][(i + k * NO_EXPS) * W_B_SIZE + j];
                }
            }
        }
    }

    const uint16_t pt[NUM_STREAMS] = {STRM1_ROWS, STRM2_ROWS, STRM3_ROWS, STRM4_ROWS};
    uint16_t max = STRM1_ROWS;
    for (int i = 1; i < NUM_STREAMS; i++) {
        if (pt[i] > max) max = pt[i];
    }

    const uint16_t TC = tot_rows / max;
    uint32_t num_rows;

    int idx = 0;
    bool eof[NUM_STREAMS] = {0};

    uint32_t rd_offset1 = 0, rd_offset2 = 0, rd_offset3 = 0, rd_offset4 = 0;
    uint32_t wr_offset1 = 0, wr_offset2 = 0, wr_offset3 = 0, wr_offset4 = 0;

TOTAL_ROWS_LOOP:
    for (int r = 0; r < tot_rows;) {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=(XF_HEIGHT/STRM_HEIGHT)*NUM_STREAMS max=(XF_HEIGHT/STRM_HEIGHT)*NUM_STREAMS
        // clang-format on        
        // Compute no.of rows to process
        if (rem_rows[idx] / 2 > pt[idx]) { // Check number for remaining rows of 1 interleaved image
            num_rows = pt[idx];
            eof[idx] = 0; // 1 interleaved image/stream is not done
        } else {
            num_rows = rem_rows[idx] / 2;
            eof[idx] = 1; // 1 interleaved image/stream done
        }

        if (idx == 0 && num_rows > 0) {
            Streampipeline_wrap(img_inp1 + rd_offset1, img_out1 + wr_offset1, num_rows, params[idx].width,
                                hist0_awb[idx], hist1_awb[idx], igain_0, igain_1, params[idx], gamma_lut[idx], wr_hls_tmp[idx],
                                omin_r[idx], omax_r[idx], omin_w[idx], omax_w[idx], flag[idx], eof[idx]);

            rd_offset1 += 2 * num_rows * ((params[idx].width + 8) >> XF_BITSHIFT(XF_NPPC));
            wr_offset1 += num_rows * (params[idx].width >> XF_BITSHIFT(XF_NPPC));

        } else if (idx == 1 && num_rows > 0) {
            Streampipeline_wrap(img_inp2 + rd_offset2, img_out2 + wr_offset2, num_rows, params[idx].width,
                                hist0_awb[idx], hist1_awb[idx], igain_0, igain_1, params[idx], gamma_lut[idx], wr_hls_tmp[idx],
                                omin_r[idx], omax_r[idx], omin_w[idx], omax_w[idx], flag[idx], eof[idx]);

            rd_offset2 += 2 * num_rows * ((params[idx].width + 8) >> XF_BITSHIFT(XF_NPPC));
            wr_offset2 += num_rows * (params[idx].width >> XF_BITSHIFT(XF_NPPC));

        } else if (idx == 2 && num_rows > 0) {
            Streampipeline_wrap(img_inp3 + rd_offset3, img_out3 + wr_offset3, num_rows, params[idx].width,
                                hist0_awb[idx], hist1_awb[idx], igain_0, igain_1, params[idx], gamma_lut[idx], wr_hls_tmp[idx],
                                omin_r[idx], omax_r[idx], omin_w[idx], omax_w[idx], flag[idx], eof[idx]);

            rd_offset3 += 2 * num_rows * ((params[idx].width + 8) >> XF_BITSHIFT(XF_NPPC));
            wr_offset3 += num_rows * (params[idx].width >> XF_BITSHIFT(XF_NPPC));
        } else if (idx == 3 && num_rows > 0) {
            Streampipeline_wrap(img_inp4 + rd_offset4, img_out4 + wr_offset4, num_rows, params[idx].width,
                                hist0_awb[idx], hist1_awb[idx], igain_0, igain_1, params[idx], gamma_lut[idx], wr_hls_tmp[idx],
                                omin_r[idx], omax_r[idx], omin_w[idx], omax_w[idx], flag[idx], eof[idx]);

            rd_offset4 += 2 * num_rows * ((params[idx].width + 8) >> XF_BITSHIFT(XF_NPPC));
            wr_offset4 += num_rows * (params[idx].width >> XF_BITSHIFT(XF_NPPC));
        }
        // Update remaining rows to process
        rem_rows[idx] = rem_rows[idx] - num_rows * 2;

        // Next stream selection
        if (idx == NUM_STREAMS - 1)
            idx = 0;

        else
            idx++;

        // Update total rows to process
        r += num_rows * 2;
    }

    return;
}
}
