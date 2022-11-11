/* ====================================================================================================================

  The copyright in this software is being made available under the License included below.
  This software may be subject to other third party and contributor rights, including patent rights, and no such
  rights are granted under this license.

  Copyright (c) 2018, HUAWEI TECHNOLOGIES CO., LTD. All rights reserved.
  Copyright (c) 2018, SAMSUNG ELECTRONICS CO., LTD. All rights reserved.
  Copyright (c) 2018, PEKING UNIVERSITY SHENZHEN GRADUATE SCHOOL. All rights reserved.
  Copyright (c) 2018, PENGCHENG LABORATORY. All rights reserved.

  Redistribution and use in source and binary forms, with or without modification, are permitted only for
  the purpose of developing standards within Audio and Video Coding Standard Workgroup of China (AVS) and for testing and
  promoting such standards. The following conditions are required to be met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
      the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
      the following disclaimer in the documentation and/or other materials provided with the distribution.
    * The name of HUAWEI TECHNOLOGIES CO., LTD. or SAMSUNG ELECTRONICS CO., LTD. may not be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

* ====================================================================================================================
*/

#ifndef _COM_H_
#define _COM_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define HPM_VERSION "10.0"

/* AVS3 phase-2 macros*/
//intra
#define EIPM                               1 // M4993: extended intra prediction mode
#define MIPF                               1 // M5079: multiple intra prediction filter
#define PMC                                1 // M5158: Prediction from Multiple Cross-components
#define IPF_CHROMA                         1 // M5385 M5418: enable intrapf for chroma components
#define IIP                                1 // M5826: Improved Intra Prediction
//inter
#define DMVR                               1 // M4813: decoder-side motion vector refinement
#define BIO                                1 // M4762: Bi-directional Optical Flow
#define BGC                                1 // M5398: Bi-directional Gradient Correction
#define INTERPF                            1 // M4812: inter prediction filtering
#define MVAP                               1 // M4926: motion vector angel prediction
#define AFFINE_UMVE                        1 // M5095: Enhanced Affine Skip/Direct Mode
#define AWP                                1 // M5142: angular weighted prediction & M5143: AWP list construction optimization
#define ETMVP                              1 // M5330(CE2-1.1):Enhanced temporal motion vector prediction
#define UMVE_ENH                           1 // M5379: enable the combination of interPF and UMVE modes; picture-level UMVE offset adaptation
#define SUB_TMVP                           1 // M5396(CE2-2.1) + M5397: NxN subblock TMVP
#define ASP                                1 // M5801 (CE2-3.2): affine secondary prediction
//transform & quantization
#define IST                                1 // M4772: implicit selection of transforms for intra residual
#define SBT                                1 // M4876: sub-block transform for inter residual
#define EST                                1 // M5159: Enhanced Secondary Transform
#define ST_CHROMA                          1 // M5874: Secondary transform for intra chroma residual
#define CUDQP                              1 // M5877: cu-level qp adjustment
//residual coding & CABAC
#define SRCC                               1 // M4763: scan region based coefficient coding
#define CABAC_MULTI_PROB                   1 // M4867 M5139: counter-based multi-probability CABAC
//scc tools (off for class A/B/C in CTC, and enabled for scc seqs)
#define USE_IBC                            1 // M4859: intra block copy [cannot off now]
#define FIMC                               1 // M4972: frequency-based intra mode coding
#if IST
#define ISTS                               1 // M5160: Implicit Selection of Transform Skip
#endif
#define USE_SP                             1 // M5226: String Prediction
#if ISTS
#define TS_INTER                           1 // M5601: Transform skip for inter-coded blocks
#define ETS                                1 // M5954: Extended Transform Skip
#endif
// in-loop filter tools
#define ESAO                               1 // M5374: Enhanced Sample Adaptive Offset
#define DBR                                1 // M5587: deblocking refinement
#define ALF_SHAPE                          1 // M5589: increased ALF coefficient
#define ALF_IMP                            1 // M5964: ALF improvements
#define CCSAO                              1 // M5800: Cross-component Sample Adaptive Offset
#define DBK_SCC                            1 // M5221 + M5869: improved deblock filter

//detailed change on each new coding tool
#if DBK_SCC
#define DBK_OPT                            1 // M5869(CE2-2.1): improve deblock filter
#endif

#if SBT
#define ISBT                               1 // M5952: Implicit signaling for SBT
#endif

#if IIP
#define IIP_BUGFIX                         1 // bug fix for iip codes, change the performance of HPM9.1
#endif

#if PMC
#define EPMC                               1 // M5659: extended PMC
#endif

#if AWP
#define AWP_MVR                            1 // M5380: angular weighted prediction with motion vector refinement
#endif

#if CUDQP
#define CUDQP_QP_MAP                       1 // generate 16x16 QP map based on texture variance
#define CUDQP_PLATFORM_BUGFIX              1 // [Note]: can be cleaned up in next HPM version
#define ONLY_ONE_DQP                       1 // only one delta QP in one QG (useful as 0 for conformance test, since we support multiple QPs in one QG)
#endif

#if ESAO
#define ESAO_PH_SYNTAX                     1
#endif

#if CCSAO
#define CCSAO_PH_SYNTAX                    1
#if CCSAO_PH_SYNTAX
#define CCSAO_ENHANCEMENT                  1 // M6051: alternative set and chroma BO for CCSAO
#endif
#endif

#if USE_IBC
#define IBC_REF_POS_CONS                   1 // M4940: position constraint of reference block in IBC mode
#define IBC_ABVR                           1 // M5070: IBC adaptive block vector resolution
#define IBC_BVP                            1 // M5081: IBC block vector prediction
#define CBVP_LIST_SIMP                     1 // M5810: simplification of CBVP list construction
#define SIBC                               1 // M5976: symmetric intra block copy
#define BVD_CODING                         1 // M5961: modification of BVD coding
#endif

#if USE_SP
#define SP_SVP                             0 // M5613: SP string vector prediction
#define SP_SLR                             1 // M5631: control of SP string length resolution
#define SP_CU_BUGFIX                       1 // M5611: cu block string prediction Tree_L distortion miss shift right
#define SP_CODE_TEXT_BUGFIX                1 // M5669: fix on the inconsistency between code and text for SP
#define EVS_UBVS_MODE                      1 // M5822: add evs_ubvs (equal value string and unit basic vector string) SP sub-mode
#define SP_REF_OVERLAP                     1 // M5994: allow the reference string above to overlap with current string
#define SP_DEC_SV_CHECK                    1 // M5864: check if string vector valid within search range
#endif

//code optimization
#if EST
#define EST_BUGFIX                         1 // M5944：EST bugfix
#endif

#define ALF_DEC_OPT                        1 // M5589: Remove unnecessary judgments in the filtering process
#define INTER_ME_MVLIB                     1 // M5527: improved EPZS motion estimation based on motion vector library
#define ENC_ME_IMP                         1 // M5806: gradient-based motion estimation and affine CPMV library
#define ETMVP_FLAG_BUG_FIX                 1 // M5886: add necessary condition for etmvp flag coding

#define AFFINE_UMVE_OPT                    1 //M5812: speedup for affine umve

#define AWP_MVR_ENCODER_OPT                1 // M5871: improve AWP_MVR idx cost estimation

#if USE_IBC && USE_SP
#define SCC_CROSSINFO_UTILIZE             1 // M5605: SCC cross info utilize
#endif

#if USE_SP
#define SP_BUGFIX                          1 // M5679: SP encoder bugfix
#define SP_ALIGN_SIGN_BUGFIX               1 // M5822: SP encoder bugfix
#define SP_SV_ENCODE_BUGIFX                1 // M5934: SP encode bugfix
#if SP_SLR
#define SP_SLR_BUGFIX                      1 // M5783: SP SLR bugfix
#endif
#define SP_FLAG_BUGFIX                     1 // M5992: encoder bugfix related to SP flag
#endif
#define SCC_BUGFIX                         1 // M5821: SCC encoder bugfix
#define SCC_BUGFIX2                        1 // M5882: SCC encoder bugfix
#if SCC_BUGFIX2
#define IBC_RDO_BUGFIX                     1
#define SP_RDO_BUGFIX                      1
#define CS_RDO_BUGFIX                      1
#endif

#define REUSE_CU_SPLIT_OPT                 1 // M5900: Adjust threshold for reusing cu results

#define REFORMAT_HASH_RATIO                1 // fix issue 152: multiple calculation of hash ratio

#define BITSTREAM_MERGE_BUGFIX             1 // M5867: bugfix for bitstream merge

//high-level
#define PHASE_2_PROFILE                    1 //
/*end of AVS phase-2 macros*/

/* ------------------------BELOW HERE is related to AVS3 phase 1 -----------------------*/
/*non-normative bugfix for AVS3 phase-1 after HPM4.0.1 (if any, add here) */

/*end of non-normative bugfix */

/*normative bugfix for AVS3 phase-1 after HPM4.0.1 (if any, add here) */

/*end of normative bugfix */



//high level syntax
#define WRITE_MD5_IN_USER_DATA             1 // write per-frame MD5 into user_data after picture header
#define REPEAT_SEQ_HEADER                  1 // add sequence header before each I frame

#define HLS_RPL                            1
#if HLS_RPL
#define DOI_CYCLE_LENGTH                 256 // the length of the DOI cycle.
#endif

#define PATCH                              1
#if PATCH
#define PATCH_M4839                        1
#define PATCH_HEADER_PARAM_TEST            0 // different patches have different patch-header patch_sao_enable_flag parameters
#endif

//partition
#define EQT                                1
//debug
#define SPLIT_DEBUG                        1  // some debug code to check the split
#define PLATFORM_GENERAL_DEBUG             1  // some key check points at encoder for detecting potential bugs
/*  ---------------------------   conformance test related, start   -------------------------------------*/
#define PRINT_SQH_PARAM_DEC                1
#define FIXED_SPLIT                        0  // fixed split pattern [must be OFF in formal test] <<<<<<<<<----------NOTE
#if FIXED_SPLIT
#define FS_ALL_COMBINATION                 0  // test split combination of 6 depth
#if FS_ALL_COMBINATION
#define START_QT_DEPTH                     0
#define START_SPLIT_MODE                   0  // 0: not constrain; 1~5: constrain 1 split at start qt depth
#define VERIFY_SPLIT                       0  // print target split combination for each CTU
#define FS_SIMPLE_ORDER                    1  // simple order of split modes (from smallest mode value to the largest)
#endif
#define FS_SAME_SIZE_PER_X_CTU             1  // constrained size changed per X CTUs
#if FS_SAME_SIZE_PER_X_CTU
#define FS_SAME_SIZE_X_VAL                 1  // can be any value larger than 0
#endif
#endif
/*  ---------------------------   conformance test related, end     -------------------------------------*/

//coding mode constraint for region of 64 pixels
#define MODE_CONS                          1  // [this macro cannot be off]
//chroma no split for avoiding 2xN chroma blocks
#define CHROMA_NOT_SPLIT                   1  // [this macro cannot be off]

//for DT and PBT
#define TB_SPLIT_EXT                       1  // extend the framework to support multiple luma prediction & transform blocks (support PBT and Intra DT) [this macro cannot be off]
#if TB_SPLIT_EXT
//fast algorithm (common)
#define TR_SAVE_LOAD                       1  // fast algorithm for PBT
#define TR_EARLY_TERMINATE                 1  // fast algorithm for PBT
#endif
#define DT_PARTITION                       1  // [this macro cannot be off] (DT can be turn off ONLY by configure)
#if DT_PARTITION
#define DT_SYNTAX                          1  // syntax change to support DT (must be 1)
//DT_INTRA
#define DT_INTRA_FAST_BY_RD                1  // fast algorithm: early skip based on RD cost comparison
#define DT_SAVE_LOAD                       1  // fast algorithm: save & load best part_size
#endif
#define PRINT_CU                           0
#define PRINT_CU_LEVEL_2                   0
#define PRINT_HMVP_FIFO                    0
#define PRINT_TRANSFORM_TABLE              0
#define DEBUG_TEST_CHANGE_HORI_VERT_SIZE   0  //change horizontal_size and vertical_size to (8xN-1) in sqh coding
#define PSNR_1020                          0  //use 1020 as peak value for 10-bit picture in PSNR calculation (for comparing PSNR with HM and VTM)
//**************** This part needs clean-up later (End)

//intra
#define TSCPM                              1
#if TSCPM
#define ENHANCE_TSPCM                      1
#endif
#define IPCM                               1

//inter
#define BD_AFFINE_AMVR                     1  // M4565 combine Affine and AMVR

#define EXT_AMVR_HMVP                      1

#define SMVD                               1  // Symmetric MVD mode

#define INTER_CU_CONSTRAINT                1

#define SEP_CONTEXT                        1 // Separate context for useless bin

//transform


// filter
#define DEBLOCK_M4647                      1

// others
#define USE_RDOQ                           1 // Use RDOQ
#define RDO_DBK                            1 // include DBK changes to luma samples into distortion
#if RDO_DBK
#define RDO_DBK_LUMA_ONLY                  1 // M5315: DBK encoder speed up
#endif

#define AFFINE_MVF_VERIFY                  0 // for debugging, clean up later

#define CPMV_BIT_DEPTH                     18
#if CPMV_BIT_DEPTH == 18
typedef int                                CPMV;
#else
typedef short                              CPMV;
#endif
#define COM_CPMV_MAX                       ((s32)((1<<(CPMV_BIT_DEPTH - 1)) - 1))
#define COM_CPMV_MIN                       ((s32)(-(1<<(CPMV_BIT_DEPTH - 1))))

#define LIBVC_ON                           1 // use Library Picture
#if LIBVC_ON
#define IPPPCRR                            1
#if IPPPCRR
#define PB_REF_LIB                         1
#define LIB_PIC_UPDATE                     1 // M5422 adaptive library picture update
#endif
#define CRR_ENC_OPT_CFG                    1 // M5533 CRR encoding optimization about configuration
#endif
#define EXTENSION_USER_DATA                1
#if EXTENSION_USER_DATA
#define CRR_EXTENSION_DATA                 1 // M4822: cross random-access-point referencing extension
#define HLS_12_6_7                         0 // write sequence display extension data into bitstream
#define HLS_12_8                           0 // write CRR extension data into bitstream
#endif

#define WEIGHTED_SATD                      1 // M5382: Improved SATD based cost calculation

//fast algorithm
#define ENC_ECU_DEPTH                      4 // for early CU termination
#define ENC_ECU_ADAPTIVE                   1 // for early CU termination
#define MULTI_REF_ME_STEP                  1 // for ME speed-up
#define FAST_LD                            1 // M5557: fast me for lowdelay case
#define BET_SPLIT_DECISION                  1 // M5965: Test2, fast BET split decision based on QT split result

//bbv
#define BBV                                1 // for Bitstream Buffer Verifier
#if BBV
#define BBV_DELAY_MAX                      0xFFFFFFFF
#define BBV_CHECK_FRAMES_MAX               2000
#define BBV_LIBVC                          1 // whether the bbv is with libvc
#endif
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                              SIMD Optimizations                            //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
#define X86_SSE                            1
#if X86_SSE
#define SIMD_MC                            1
#define SIMD_SAD                           1
#define SIMD_SSD                           1
#define SIMD_DIFF                          1
#define SIMD_HAD_SAD                       1
#define SIMD_AFFINE                        1
#if ASP
#define SIMD_ASP                           1
#endif // ASP
#if ENC_ME_IMP
#define SIMD_GRAD_ME                       1
#endif
#else
#define SIMD_MC                            0
#define SIMD_SAD                           0
#define SIMD_SSD                           0
#define SIMD_DIFF                          0
#define SIMD_HAD_SAD                       0
#define SIMD_AFFINE                        0
#if ASP
#define SIMD_ASP                           0
#endif // ASP
#if ENC_ME_IMP
#define SIMD_GRAD_ME                       0
#endif
#endif

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                         Certain Tools Parameters                           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#define INC_QT_DEPTH(qtd, smode)           (smode == SPLIT_QUAD? (qtd + 1) : qtd)
#define INC_BET_DEPTH(betd, smode)         (smode != SPLIT_QUAD? (betd + 1) : betd)

#if EQT
#define MAX_SPLIT_NUM                      6
#define SPLIT_CHECK_NUM                    6
#else
#define MAX_SPLIT_NUM                      4
#define SPLIT_CHECK_NUM                    4
#endif

/*****************************************************************************
 * return values and error code
 *****************************************************************************/
/* no more frames, but it is OK */
#define COM_OK_NO_MORE_FRM              (205)
/* progress success, but output is not available temporarily */
#define COM_OK_OUT_NOT_AVAILABLE        (204)
/* frame dimension (width or height) has been changed */
#define COM_OK_DIM_CHANGED              (203)
/* decoding success, but output frame has been delayed */
#define COM_OK_FRM_DELAYED              (202)
#if IPPPCRR&&LIB_PIC_UPDATE
#define RL_UPDATE_TO_LIBPIC             (2)
#define COM_OK_SKIP                     (3)
#endif
#define COM_OK                          (0)
#define END_OF_VIDEO_SEQUENCE           (206)
#define NOT_END_OF_VIDEO_SEQUENCE       (207)

#define COM_ERR                         (-1) /* generic error */
#define COM_ERR_INVALID_ARGUMENT        (-101)
#define COM_ERR_OUT_OF_MEMORY           (-102)
#define COM_ERR_REACHED_MAX             (-103)
#define COM_ERR_UNSUPPORTED             (-104)
#define COM_ERR_UNEXPECTED              (-105)
#define COM_ERR_BAD_CRC                 (-130) /* not matched CRC value */

#define COM_ERR_UNSUPPORTED_COLORSPACE  (-201)
#define COM_ERR_MALFORMED_BITSTREAM     (-202)

#define COM_ERR_UNKNOWN                 (-32767) /* unknown error */


/* return value checking *****************************************************/
#define COM_SUCCEEDED(ret)              ((ret) >= 0)
#define COM_FAILED(ret)                 ((ret) < 0)

/* YUV planar ****************************************************************/
#define COM_COLORSPACE_YUV400          300 /* Y 8bit */
#define COM_COLORSPACE_YUV420          301 /* YUV420 8bit */
#define COM_COLORSPACE_YUV422          302 /* YUV422 8bit narrow chroma*/
#define COM_COLORSPACE_YUV444          303 /* YUV444 8bit */
#define COM_COLORSPACE_YUV422N         COM_COLORSPACE_YUV422
#define COM_COLORSPACE_YUV422W         310 /* YUV422 8bit wide chroma */

#define COM_COLORSPACE_YUV400A8        400 /* Y+alpha 8bit */
#define COM_COLORSPACE_YUV420A8        401 /* YUV420+alpha 8bit */
#define COM_COLORSPACE_YUV422A8        402 /* YUV422+alpha 8bit narrow chroma*/
#define COM_COLORSPACE_YUV444A8        403 /* YUV444+alpha 8bit */
#define COM_COLORSPACE_YUV422NA8       COM_COLORSPACE_YUV422A8
#define COM_COLORSPACE_YUV422WA8       414 /* YUV422+alpha 8bit wide chroma*/

/* RGB pack ******************************************************************/

/* RGB pack 8bit */
#define COM_COLORSPACE_RGB888          2200
#define COM_COLORSPACE_BGR888          2201

#define COM_COLORSPACE_RGBA8888        2220
#define COM_COLORSPACE_BGRA8888        2221
#define COM_COLORSPACE_ARGB8888        2222
#define COM_COLORSPACE_ABGR8888        2223

/*****************************************************************************
 * config types for decoder
 *****************************************************************************/
#define DEC_CFG_SET_USE_PIC_SIGNATURE  (301)

/*****************************************************************************
 * chunk type
 *****************************************************************************/
#define COM_CT_UNKNOWN                  (0)
#define COM_CT_PICTURE                  (1) /* picture header */
#define COM_CT_SQH                      (2) /* sequence header */
#define COM_CT_SLICE                    (3) /* slice header */
#define COM_CT_SIGN                     (6) /* picture signature */
#define COM_CT_SEQ_END                  (7)

/*****************************************************************************
 * slice type
 *****************************************************************************/
#define COM_ST_UNKNOWN                  (0)
#define COM_ST_I                        (1)
#define COM_ST_P                        (2)
#define COM_ST_B                        (3)

/*****************************************************************************
 * software version
 *****************************************************************************/
#define COM_VER_1                       (1)

/*****************************************************************************
 * type and macro for media time
 *****************************************************************************/
/* media time in 100-nanosec unit */
typedef long long                    COM_MTIME;

/*****************************************************************************
 * image buffer format
 *****************************************************************************
 baddr
    +---------------------------------------------------+ ---
    |                                                   |  ^
    |                                              |    |  |
    |    a                                         v    |  |
    |   --- +-----------------------------------+ ---   |  |
    |    ^  |  (x, y)                           |  y    |  |
    |    |  |   +---------------------------+   + ---   |  |
    |    |  |   |                           |   |  ^    |  |
    |    |  |   |                           |   |  |    |  |
    |    |  |   |                           |   |  |    |  |
    |    |  |   |                           |   |  |    |  |
    |       |   |                           |   |       |
    |    ah |   |                           |   |  h    |  e
    |       |   |                           |   |       |
    |    |  |   |                           |   |  |    |  |
    |    |  |   |                           |   |  |    |  |
    |    |  |   |                           |   |  v    |  |
    |    |  |   +---------------------------+   | ---   |  |
    |    v  |                                   |       |  |
    |   --- +---+-------------------------------+       |  |
    |     ->| x |<----------- w ----------->|           |  |
    |       |<--------------- aw -------------->|       |  |
    |                                                   |  v
    +---------------------------------------------------+ ---

    |<---------------------- stride-------------------->|

 *****************************************************************************/

#define COM_IMGB_MAX_PLANE              (4)

typedef struct _COM_IMGB COM_IMGB;
struct _COM_IMGB
{
	int                 cs; /* color space */
	int                 np; /* number of plane */
	int                 horizontal_size;
	int                 vertical_size;
	/* width (in unit of pixel) */
	int                 width[COM_IMGB_MAX_PLANE];
	/* height (in unit of pixel) */
	int                 height[COM_IMGB_MAX_PLANE];
	/* buffer stride (in unit of byte) */
	int                 stride[COM_IMGB_MAX_PLANE];
	/* address of each plane */
	void              * addr_plane[COM_IMGB_MAX_PLANE];

	/* time-stamps */
	COM_MTIME          ts[4];

	/* aligned width (in unit of pixel) */
	int                 width_aligned[COM_IMGB_MAX_PLANE];
	/* aligned height (in unit of pixel) */
	int                 height_aligned[COM_IMGB_MAX_PLANE];

	/* left padding size (in unit of pixel) */
	int                 pad_left[COM_IMGB_MAX_PLANE];
	/* right padding size (in unit of pixel) */
	int                 pad_right[COM_IMGB_MAX_PLANE];
	/* up padding size (in unit of pixel) */
	int                 pad_up[COM_IMGB_MAX_PLANE];
	/* bottom padding size (in unit of pixel) */
	int                 pad_down[COM_IMGB_MAX_PLANE];

	/* address of actual allocated buffer */
	void              * buf_addr[COM_IMGB_MAX_PLANE];
	/* actual allocated buffer size */
	int                 buf_size[COM_IMGB_MAX_PLANE];

	/* life cycle management */
	int                 refcnt;
	int                 (*addref)(COM_IMGB * imgb);
	int                 (*getref)(COM_IMGB * imgb);
	int                 (*release)(COM_IMGB * imgb);
};


/*****************************************************************************
 * Bitstream buffer
 *****************************************************************************/
typedef struct _COM_BITB
{
	/* user space address indicating buffer */
	void              * addr;
	void              * addr2;
	/* physical address indicating buffer, if any */
	void              * pddr;
	/* byte size of buffer memory */
	int                 bsize;
	/* byte size of bitstream in buffer */
	int                 ssize;
	/* bitstream has an error? */
	int                 err;
	/* arbitrary data, if needs */
	int                 ndata[4];
	/* arbitrary address, if needs */
	void              * pdata[4];
	/* time-stamps */
	COM_MTIME          ts[4];
} COM_BITB;

/*****************************************************************************
 * description for creating of decoder
 *****************************************************************************/
typedef struct _DEC_CDSC
{
	int            __na; /* nothing */
} DEC_CDSC;

/*****************************************************************************
 * status after decoder operation
 *****************************************************************************/
typedef struct _DEC_STAT
{
	/* byte size of decoded bitstream (read size of bitstream) */
	int            read;
	/* chunk type */
	int            ctype;
	/* slice type */
	int            stype;
	/* frame number monotonically increased whenever decoding a frame.
	note that it has negative value if the decoded data is not frame */
	int            fnum;
	/* picture order count */
	int            poc;
	/* number of reference pictures */
	int            refpic_num[2];
	/* list of reference pictures */
	int            refpic[2][16];
#if LIBVC_ON
	/* is only ref libpic */
	int            is_RLpic_flag;
#if IPPPCRR
	int            reflib[2][16];
#endif
#endif
	/* for printing sqh parameter */
#if PRINT_SQH_PARAM_DEC
	/* framework param */
#if PHASE_2_PROFILE
	int            profile_id;
#endif
	int            internal_bit_depth;
	/* tools */
	int            intra_tools;
	int            inter_tools;
	int            trans_tools;
	int            filte_tools;
	int            scc_tools;
#endif
} DEC_STAT;

/*****************************************************************************
 * status after encoder operation
 *****************************************************************************/
typedef struct _ENC_STAT
{
	/* encoded bitstream byte size */
	int            write;
	/* picture number increased whenever encoding a frame */
	unsigned long  fnum;
	/* chunk type */
	int            ctype;
	/* slice type */
	int            stype;
	/* quantization parameter used for encoding */
	int            qp;
	/* picture order count */
	int            poc;
	/* number of reference pictures */
	int            refpic_num[2];
	/* list of reference pictures */
	int            refpic[2][16];
#if LIBVC_ON
	/* is only ref libpic */
	int            is_RLpic_flag;
#if IPPPCRR
	int            reflib[2][16];
#endif
#endif
} ENC_STAT;

#define MAX_NUM_REF_PICS                   17
#define MAX_NUM_ACTIVE_REF_FRAME           4
#define MAX_NUM_RPLS                       32

extern int g_CountDOICyCleTime;             // number to count the DOI cycle time.
extern int g_DOIPrev;                       // the doi of previous frm.

#if HLS_RPL
/* rpl structure */
typedef struct _COM_RPL
{
	int slice_type;
	int poc;
	int tid;
	int ref_pic_num;
	int ref_pic_active_num;
	int ref_pics[MAX_NUM_REF_PICS];
	int ref_pics_ddoi[MAX_NUM_REF_PICS];
	int ref_pics_doi[MAX_NUM_REF_PICS];//ref pic list, doi info
#if LIBVC_ON
	int reference_to_library_enable_flag;
	int library_index_flag[MAX_NUM_REF_PICS];
#endif
} COM_RPL;
#endif
/*****************************************************************************
 * API for decoder only
 *****************************************************************************/
/* instance identifier for decoder */
typedef void  * DEC;

DEC dec_create(DEC_CDSC * cdsc, int * err);
void dec_delete(DEC id);
int dec_decode(DEC id, COM_BITB * bitb, DEC_STAT * stat);
int dec_config(DEC id, int cfg, void * buf, int * size);

#ifdef __cplusplus
}
#endif

#endif /* _COM_H_ */
