#ifndef H266_GLOBAL_INC
#define H266_GLOBAL_INC

#include <linux/types.h>

#define USE_FULL_REF_LIST_BUFFER

//#define PRINT_LINE() printk("%s: line %d\n", __func__, __LINE__)
#define PRINT_LINE()
#define AML
#define REDUCE_SIZE
//#include "TypeDef.h"
#define GDR_ENABLED   1

#if GDR_ENABLED
#define GDR_LEAK_TEST  0
#define GDR_ENC_TRACE  0
#define GDR_DEC_TRACE  0
#endif

//#include "CommonDef.h"
#define        AFFINE_ME_LIST_SIZE                              4
#define        AFFINE_ME_LIST_SIZE_LD                           3
#define        AFFINE_ME_LIST_MVP_TH                         1.0

// ====================================================================================================================
// Common constants
// ====================================================================================================================
#define          MAX_UINT64                   0xFFFFFFFFFFFFFFFFU
#define          MAX_UINT                             0xFFFFFFFFU    ///< max. value of unsigned 32-bit integer
#define           MAX_INT                               2147483647    ///< max. value of signed 32-bit integer
#define         MAX_UCHAR                                    255
#define         MAX_SCHAR                                    127
#define        MAX_DOUBLE                              1.7e+308    ///< max. value of double-type value

// ====================================================================================================================
// Coding tool configuration
// ====================================================================================================================
// Most of these should not be changed - they resolve the meaning of otherwise magic numbers.

#define        MAX_GOP                                          64    ///< max. value of hierarchical GOP size
#define        MAX_NUM_REF_PICS                                 29    ///< max. number of pictures used for reference
#define        MAX_NUM_REF                                      16    ///< max. number of entries in picture reference list
#define        MAX_QP                                           63
#define        NOT_VALID                                        -1


//#define        AMVP_MAX_NUM_CANDS                                2    ///< AMVP: advanced motion vector prediction - max number of final candidates
//#define        AMVP_MAX_NUM_CANDS_MEM                            3    ///< AMVP: advanced motion vector prediction - max number of candidates
#define        AMVP_DECIMATION_FACTOR                            2
#define        MRG_MAX_NUM_CANDS                                 6    ///< MERGE
#define        AFFINE_MRG_MAX_NUM_CANDS                          5    ///< AFFINE MERGE
#define        IBC_MRG_MAX_NUM_CANDS                             6    ///< IBC MERGE

#define        MAX_TLAYER                                        7    ///< Explicit temporal layer QP offset - max number of temporal layer

#define        ADAPT_SR_SCALE                                    1    ///< division factor for adaptive search range

#define        MIN_TB_LOG2_SIZEY  2
#define        MAX_TB_LOG2_SIZEY  6

#define        MIN_TB_SIZEY  1 << MIN_TB_LOG2_SIZEY
#define        MAX_TB_SIZEY  1 << MAX_TB_LOG2_SIZEY

#define        MAX_NESTING_NUM_LAYER                            64

#define        MAX_VPS_LAYERS                                   64
#define        MAX_VPS_SUBLAYERS                                 7
#define        MAX_NUM_OLSS                                    256
#define        MAX_VPS_OLS_MODE_IDC                              2

#define        MIP_MAX_WIDTH                                    MAX_TB_SIZEY
#define        MIP_MAX_HEIGHT                                   MAX_TB_SIZEY

#define        MAX_NUM_ALF_ALTERNATIVES_CHROMA                   8
#define        MAX_NUM_ALF_CLASSES                              25
#define        MAX_NUM_ALF_LUMA_COEFF                           13
#define        MAX_NUM_ALF_CHROMA_COEFF                          7
#define        MAX_ALF_FILTER_LENGTH                             7
#define        MAX_ALF_PADDING_SIZE                              4
#define MAX_NUM_CC_ALF_FILTERS                                      4
#define        MAX_NUM_CC_ALF_CHROMA_COEFF                   8
#define        CCALF_DYNAMIC_RANGE                           6
#define        CCALF_BITS_PER_COEFF_LEVEL                    3

#define        ALF_FIXED_FILTER_NUM                             64
#define        ALF_CTB_MAX_NUM_APS                               8
#define        NUM_FIXED_FILTER_SETS                            16

#define        MAX_BDOF_APPLICATION_REGION                      16

#define        MAX_CPB_CNT                                      32    ///< Upper bound of (cpb_cnt_minus1 + 1)
#define        MAX_NUM_LAYER_IDS                                64
#define        COEF_REMAIN_BIN_REDUCTION                         5    ///< indicates the level at which the VLC transitions from Golomb-Rice to TU+EG(k)
#define        CU_DQP_TU_CMAX                                    5    ///< max number bins for truncated unary
#define        CU_DQP_EG_k                                       0    ///< expgolomb order

#define        SBH_THRESHOLD                                     4    ///< value of the fixed SBH controlling threshold

#define        MAX_NUM_VPS                                      16
#define        MAX_NUM_SPS                                      16
#define        MAX_NUM_PPS                                      64
#define        MAX_NUM_APS                                      32     //Currently APS ID has 5 bits
#define        NUM_APS_TYPE_LEN                                  3     //Currently APS Type has 3 bits
#define        MAX_NUM_APS_TYPE                                  8     //Currently APS Type has 3 bits so the max type is 8

#define        MAX_TILE_COLS  30      // Maximum number of tile columns
#define        MAX_TILES      990     // Maximum number of tiles
#define        MAX_SLICES     1000    // Maximum number of slices per picture

#define        MLS_GRP_NUM                                    1024    ///< Max number of coefficient groups, max(16, 256)

#define        MLS_CG_SIZE                                       4    ///< Coefficient group size of 4x4    = MLS_CG_LOG2_WIDTH + MLS_CG_LOG2_HEIGHT


#define        RVM_VCEGAM10_M                                    4

#define        MAX_REF_LINE_IDX                                  3    //highest refLine offset in the list
#define        MRL_NUM_REF_LINES                                 3    //number of candidates in the array
//#define        MULTI_REF_LINE_IDX[4]                { 0, 1, 2, 0 }

#define        PRED_REG_MIN_WIDTH                                4     // Minimum prediction region width for ISP subblocks

#define        NUM_LUMA_MODE                                    67    ///< Planar + DC + 65 directional mode (4*16 + 1)
#define        NUM_LMC_MODE                                     1 + 2    ///< LMC + MDLM_T + MDLM_L
#define        NUM_INTRA_MODE  (NUM_LUMA_MODE + NUM_LMC_MODE)

#define        NUM_EXT_LUMA_MODE                                28

#define        NUM_DIR            (((NUM_LUMA_MODE - 3) >> 2) + 1)
#define        PLANAR_IDX                                        0    ///< index for intra PLANAR mode
#define        DC_IDX                                            1    ///< index for intra DC     mode
#define        HOR_IDX                     (1 * (NUM_DIR - 1) + 2)    ///< index for intra HORIZONTAL mode
#define        DIA_IDX                     (2 * (NUM_DIR - 1) + 2)    ///< index for intra DIAGONAL   mode
#define        VER_IDX                     (3 * (NUM_DIR - 1) + 2)    ///< index for intra VERTICAL   mode
#define        VDIA_IDX                    (4 * (NUM_DIR - 1) + 2)    ///< index for intra VDIAGONAL  mode
#define        BDPCM_IDX                   (5 * (NUM_DIR - 1) + 2)    ///< index for intra VDIAGONAL  mode
#define        NOMODE_IDX                                MAX_UCHAR    ///< indicating uninitialized elements

//#define        NUM_CHROMA_MODE  (5 + NUM_LMC_MODE)    ///< total number of chroma modes
#define        LM_CHROMA_IDX  NUM_LUMA_MODE    ///< chroma mode index for derived from LM mode
#define        MDLM_L_IDX                           LM_CHROMA_IDX + 1    ///< MDLM_L
#define        MDLM_T_IDX                           LM_CHROMA_IDX + 2    ///< MDLM_T
//#define        DM_CHROMA_IDX                        NUM_INTRA_MODE    ///< chroma mode index for derived from luma intra mode

#define         NUM_TRAFO_MODES_MTS                             6    ///< Max Intra CU size applying EMT, supported values: 8, 16, 32, 64, 128
#define         MTS_INTRA_MAX_CU_SIZE                          32    ///< Max Intra CU size applying EMT, supported values: 8, 16, 32, 64, 128
#define         MTS_INTER_MAX_CU_SIZE                          32    ///< Max Inter CU size applying EMT, supported values: 8, 16, 32, 64, 128
#define        NUM_MOST_PROBABLE_MODES  6
#define        LM_SYMBOL_NUM  (1 + NUM_LMC_MODE)

#define        MAX_NUM_MIP_MODE                                 32    ///< maximum number of MIP pred. modes
#define        FAST_UDI_MAX_RDMODE_NUM  (NUM_LUMA_MODE + MAX_NUM_MIP_MODE)    ///< maximum number of RD comparison in fast-UDI estimation loop

#define        MAX_LFNST_COEF_NUM                               16

#define        LFNST_LAST_SIG_LUMA                               1
#define        LFNST_LAST_SIG_CHROMA                             1

#define        NUM_LFNST_NUM_PER_SET                             3

#define        CABAC_INIT_PRESENT_FLAG                           1

#define        MV_FRACTIONAL_BITS_INTERNAL                       4
#define        MV_FRACTIONAL_BITS_SIGNAL                         2
#define        MV_FRACTIONAL_BITS_DIFF  MV_FRACTIONAL_BITS_INTERNAL - MV_FRACTIONAL_BITS_SIGNAL
#define        LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS_SIGNAL  1 << MV_FRACTIONAL_BITS_SIGNAL
#define        LUMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS  1 << MV_FRACTIONAL_BITS_INTERNAL
#define        CHROMA_INTERPOLATION_FILTER_SUB_SAMPLE_POSITIONS  1 << (MV_FRACTIONAL_BITS_INTERNAL + 1)

#define        MAX_NUM_SUB_PICS                          (1 << 16)
#define        MAX_NUM_LONG_TERM_REF_PICS                       33
#define        NUM_LONG_TERM_REF_PIC_SPS                         0


#define        MAX_QP_OFFSET_LIST_SIZE                           6    ///< Maximum size of QP offset list is 6 entries
#define        MAX_NUM_CQP_MAPPING_TABLES                        3    ///< Maximum number of chroma QP mapping tables (Cb, Cr and joint Cb-Cr)
#define        MIN_QP_VALUE_FOR_16_BIT                         -48    ////< Minimum value for QP (-6*(bitdepth - 8) ) for bit depth 16     actual minimum QP value is bit depth dependent
#define        MAX_NUM_QP_VALUES     MAX_QP + 1 - MIN_QP_VALUE_FOR_16_BIT    ////< Maximum number of QP values possible - bit depth dependent

// Cost mode support
#define        LOSSLESS_AND_MIXED_LOSSLESS_RD_COST_TEST_QP       0    ///< QP to use for lossless coding.
#define        LOSSLESS_AND_MIXED_LOSSLESS_RD_COST_TEST_QP_PRIME 4    ///< QP' to use for mixed_lossy_lossless coding.
#define        RExt__GOLOMB_RICE_ADAPTATION_STATISTICS_SETS  MAX_NUM_COMPONENT

#define        RExt__PREDICTION_WEIGHTING_ANALYSIS_DC_PRECISION  0    ///< Additional fixed bit precision used during encoder-side weighting prediction analysis. Currently only used when high_precision_prediction_weighting_flag is set, for backwards compatibility reasons.

#define        MAX_TIMECODE_SEI_SETS                             3    ///< Maximum number of time sets

#define        MAX_CU_DEPTH                                      7    ///< log2(CTUSize)
#define        MAX_CU_SIZE                         1<<MAX_CU_DEPTH
#define        MIN_CU_LOG2                                       2
#define        MIN_PU_SIZE                                       4
#define        MAX_NUM_PARTS_IN_CTU                          ( ( MAX_CU_SIZE * MAX_CU_SIZE ) >> ( MIN_CU_LOG2 << 1 ) )
#define        MAX_NUM_TUS                                      16    ///< Maximum number of TUs within one CU. When max TB size is 32x32, up to 16 TUs within one CU (128x128) is supported
#define        MAX_LOG2_DIFF_CU_TR_SIZE                          3
#define        MAX_CU_TILING_PARTITIONS  1 << ( MAX_LOG2_DIFF_CU_TR_SIZE << 1 )

#define        JVET_C0024_ZERO_OUT_TH                           32

#define        MAX_NUM_PART_IDXS_IN_CTU_WIDTH  MAX_CU_SIZE/MIN_PU_SIZE    ///< maximum number of partition indices across the width of a CTU (or height of a CTU)
#define        SCALING_LIST_REM_NUM                              6

#define        QUANT_SHIFT                                      14    ///< Q(4) = 2^14
#define        IQUANT_SHIFT                                      6

#define           SCALE_BITS       15      // Precision for fractional bit estimates
#define        FRAC_BITS_SCALE  1.0 / (1 << SCALE_BITS)

#define        SCALING_LIST_PRED_MODES  2
#define        SCALING_LIST_NUM  MAX_NUM_COMPONENT * SCALING_LIST_PRED_MODES    ///< list number for quantization matrix

#define        SCALING_LIST_START_VALUE                          8    ///< start value for dpcm mode
#define        MAX_MATRIX_COEF_NUM                              64    ///< max coefficient number for quantization matrix
#define        MAX_MATRIX_SIZE_NUM                               8    ///< max size number for quantization matrix
#define        SCALING_LIST_BITS                                 8    ///< bit depth of scaling list entries
#define        LOG2_SCALING_LIST_NEUTRAL_VALUE                   4    ///< log2 of the value that, when used in a scaling list, has no effect on quantisation
#define        SCALING_LIST_DC                                  16    ///< default DC value

#define        LAST_SIGNIFICANT_GROUPS                          14

#define        AFFINE_MIN_BLOCK_SIZE                             4    ///< Minimum affine MC block size

#define        MMVD_REFINE_STEP                                  8    ///< max number of distance step
#define        MMVD_MAX_REFINE_NUM                               (MMVD_REFINE_STEP * 4)    ///< max number of candidate from a base candidate
#define        MMVD_BASE_MV_NUM                                  2    ///< max number of base candidate
#define        MMVD_ADD_NUM                                      (MMVD_MAX_REFINE_NUM * MMVD_BASE_MV_NUM)   ///< total number of mmvd candidate
#define        MMVD_MRG_MAX_RD_NUM                               MRG_MAX_NUM_CANDS
#define        MMVD_MRG_MAX_RD_BUF_NUM                           (MMVD_MRG_MAX_RD_NUM + 1)   ///< increase buffer size by 1

#define        MAX_TU_LEVEL_CTX_CODED_BIN_CONSTRAINT_LUMA       28
#define        MAX_TU_LEVEL_CTX_CODED_BIN_CONSTRAINT_CHROMA     28

#define        BIO_EXTEND_SIZE                                   1
#define        BIO_TEMP_BUFFER_SIZE                              (MAX_CU_SIZE + 2 * BIO_EXTEND_SIZE) * (MAX_CU_SIZE + 2 * BIO_EXTEND_SIZE)

#define        PROF_BORDER_EXT_W                                 1
#define        PROF_BORDER_EXT_H                                 1
#define        BCW_NUM                                           5    ///< the number of weight options
#define        BCW_DEFAULT                                       ((uint8_t)(BCW_NUM >> 1))    ///< Default weighting index representing for w=0.5
#define        BCW_SIZE_CONSTRAINT                             256    ///< disabling Bcw if cu size is smaller than 256
#define        MAX_NUM_HMVP_CANDS                               (MRG_MAX_NUM_CANDS-1)    ///< maximum number of HMVP candidates to be stored and used in merge list
#define        MAX_NUM_HMVP_AVMPCANDS                           4    ///< maximum number of HMVP candidates to be used in AMVP list

#define        ALF_VB_POS_ABOVE_CTUROW_LUMA  4
#define        ALF_VB_POS_ABOVE_CTUROW_CHMA  2

//#if W0038_DB_OPT
//#define        MAX_ENCODER_DEBLOCKING_QUALITY_LAYERS            8
//#endif

//#if SHARP_LUMA_DELTA_QP
//#define        LUMA_LEVEL_TO_DQP_LUT_MAXSIZE                 1024    ///< max LUT size for QP offset based on luma

//#endif
#define        DMVR_SUBCU_WIDTH  16
#define        DMVR_SUBCU_HEIGHT  16
#define        DMVR_SUBCU_WIDTH_LOG2  4
#define        DMVR_SUBCU_HEIGHT_LOG2  4
#define        MAX_NUM_SUBCU_DMVR  ((MAX_CU_SIZE * MAX_CU_SIZE) >> (DMVR_SUBCU_WIDTH_LOG2 + DMVR_SUBCU_HEIGHT_LOG2))
#define        DMVR_NUM_ITERATION  2

//QTBT high level parameters
//for I slice luma CTB configuration para.
#define           MAX_BT_DEPTH                                   4         ///<  <=7
                                                                            //for P/B slice CTU config. para.
#define           MAX_BT_DEPTH_INTER                             4         ///< <=7
                                                                            //for I slice chroma CTB configuration para. (in luma samples)
#define           MAX_BT_DEPTH_C                                 0         ///< <=7
#define           MIN_DUALTREE_CHROMA_WIDTH                      4
#define           MIN_DUALTREE_CHROMA_SIZE                      16
//static const SplitSeries SPLIT_BITS         =                       5
//static const SplitSeries SPLIT_DMULT        =                       5
//static const SplitSeries SPLIT_MASK         =                      31         ///< = (1 << SPLIT_BITS) - 1

#define           SKIP_DEPTH                                     3
#define           PICTURE_DISTANCE_TH                            1
#define           FAST_SKIP_DEPTH                                2

#define        PBINTRA_RATIO                                  1.1
#define           NUM_MRG_SATD_CAND                              4
#define        MRG_FAST_RATIO                                 1.25
#define           NUM_AFF_MRG_SATD_CAND                          2

#define        AMAXBT_TH32                                   15.0
#define        AMAXBT_TH64                                   30.0

// need to know for static memory allocation
#define        MAX_DELTA_QP                                      7         ///< maximum supported delta QP value
#define        MAX_TESTED_QPs    ( 1 + 1 + ( MAX_DELTA_QP << 1 ) )         ///< dqp=0 +- max_delta_qp + lossless mode

#define        COM16_C806_TRANS_PREC                             0

#define        NTAPS_LUMA                                        8    ///< Number of taps for luma
#define        NTAPS_CHROMA                                      4    ///< Number of taps for chroma
//#if LUMA_ADAPTIVE_DEBLOCKING_FILTER_QP_OFFSET
//#define        MAX_LADF_INTERVALS                                5    /// max number of luma adaptive deblocking filter qp offset intervals
//#endif

#define        NTAPS_BILINEAR                                    2    ///< Number of taps for bilinear filter

#define        ATMVP_SUB_BLOCK_SIZE                              3    ///< sub-block size for ATMVP
#define        GEO_MAX_NUM_UNI_CANDS                             6
#define        GEO_MAX_NUM_CANDS  GEO_MAX_NUM_UNI_CANDS * (GEO_MAX_NUM_UNI_CANDS - 1)
#define        GEO_MIN_CU_LOG2                                   3
#define        GEO_MAX_CU_LOG2                                   6
#define        GEO_MIN_CU_SIZE                1 << GEO_MIN_CU_LOG2
#define        GEO_MAX_CU_SIZE                1 << GEO_MAX_CU_LOG2
#define        GEO_NUM_CU_SIZE  ( GEO_MAX_CU_LOG2 - GEO_MIN_CU_LOG2 ) + 1
#define        GEO_NUM_PARTITION_MODE                           64
#define        GEO_NUM_ANGLES                                   32
#define        GEO_NUM_DISTANCES                                 4
#define        GEO_NUM_PRESTORED_MASK                            6
#define        GEO_WEIGHT_MASK_SIZE  3 * (GEO_MAX_CU_SIZE >> 3) * 2 + GEO_MAX_CU_SIZE
#define        GEO_MV_MASK_SIZE          GEO_WEIGHT_MASK_SIZE >> 2
#define        GEO_MAX_TRY_WEIGHTED_SAD  60
#define        GEO_MAX_TRY_WEIGHTED_SATD  8

#define        SBT_MAX_SIZE                                     64    ///< maximum CU size for using SBT
#define        SBT_NUM_SL                                       10    ///< maximum number of historical PU decision saved for a CU
#define        SBT_NUM_RDO                                       2    ///< maximum number of SBT mode tried for a PU

#define        NUM_INTER_CU_INFO_SAVE                            8    ///< maximum number of inter cu information saved for fast algorithm
#define        LDT_MODE_TYPE_INHERIT                             0    ///< No need to signal mode_constraint_flag, and the modeType of the region is inherited from its parent node
#define        LDT_MODE_TYPE_INFER                               1    ///< No need to signal mode_constraint_flag, and the modeType of the region is inferred as MODE_TYPE_INTRA
#define        LDT_MODE_TYPE_SIGNAL                              2    ///< Need to signal mode_constraint_flag, and the modeType of the region is determined by the flag

#define        IBC_MAX_CAND_SIZE  16    // max block size for ibc search
#define        IBC_NUM_CANDIDATES  64    ///< Maximum number of candidates to store/test
#define        CHROMA_REFINEMENT_CANDIDATES  8    /// 8 candidates BV to choose from
#define        IBC_FAST_METHOD_NOINTRA_IBCCBF0  0x01
#define        IBC_FAST_METHOD_BUFFERBV  0X02
#define        IBC_FAST_METHOD_ADAPTIVE_SEARCHRANGE  0X04
#define        MV_EXPONENT_BITCOUNT     4
#define        MV_MANTISSA_BITCOUNT     6
#define        MV_MANTISSA_UPPER_LIMIT  ((1 << (MV_MANTISSA_BITCOUNT - 1)) - 1)
#define        MV_MANTISSA_LIMIT        (1 << (MV_MANTISSA_BITCOUNT - 1))
#define        MV_EXPONENT_MASK         ((1 << MV_EXPONENT_BITCOUNT) - 1)

#define        MV_BITS                                    18
#define        MV_MAX               (1 << (MV_BITS - 1)) - 1
#define        MV_MIN                  -(1 << (MV_BITS - 1))

#define        MVD_MAX                             (1 << 17) - 1
#define        MVD_MIN                                -(1 << 17)

#define        PIC_ANALYZE_CW_BINS                            32
#define        PIC_CODE_CW_BINS                               16
#define        LMCS_SEG_NUM                                   32
#define        FP_PREC                                        11
#define        CSCALE_FP_PREC                                 11
#define        LOG2_PALETTE_CG_SIZE                            4
#define        RUN_IDX_THRE                                    4
#define        MAX_CU_BLKSIZE_PLT                             64
#define        NUM_TRELLIS_STATE                               3
#define        ENC_CHROMA_WEIGHTING                       0.8
#define        MAXPLTPREDSIZE  63
#define        MAXPLTSIZE  31
#define        MAXPLTPREDSIZE_DUALTREE  31
#define        MAXPLTSIZE_DUALTREE  15
#define        PLT_CHROMA_WEIGHTING                       0.8
#define        PLT_ENCBITDEPTH  8
#define        PLT_FAST_RATIO  100
//#if RExt__DECODER_DEBUG_TOOL_MAX_FRAME_STATS
//#define         EPBIN_WEIGHT_FACTOR                            4
//#endif
#define        ENC_PPS_ID_RPR                                  3
#define        SCALE_RATIO_BITS                               14
#define        MAX_SCALING_RATIO                               2     // max downsampling ratio for RPR
//static const std::pair<int, int> SCALE_1X = std::pair<int, int>( 1 << SCALE_RATIO_BITS, 1 << SCALE_RATIO_BITS )     // scale ratio 1x
//#define        DELTA_QP_ACT[4] =                  { -5, 1, 3, 1 }
#define        MAX_TSRC_RICE                                   8     ///<Maximum supported TSRC Rice parameter
#define        MIN_TSRC_RICE                                   1     ///<Minimum supported TSRC Rice parameter
#define        MAX_CTI_LUT_SIZE                               64     ///<Maximum colour transform LUT size for CTI SEI


//#include "TypeDef.h"
enum MsgLevel
{
  SILENT  = 0,
  ERROR   = 1,
  WARNING = 2,
  INFO    = 3,
  NOTICE  = 4,
  VERBOSE = 5,
  DETAILS = 6
};


typedef struct{
  void* _data; //underscore to note that it's not safe to touch variable, do not modify!
  unsigned int _size, _typeSize, _extra, extra; //size is mem_size_bytes / _typeSize, it's not raw size
  //_extra is how much extra space is left and "extra" is just how much extra space we should add
} cvector;

void cvector_init(cvector *v, unsigned int typeSize);
void cvector_allocate(cvector* v, unsigned int additionalSize);
void cvector_deallocate(cvector* v, unsigned int additionalSize);
void cvector_clear(cvector* v);
void cvector_push(cvector* v, void* element);
void cvector_pushi(cvector* v, long int element);
//void cvector_pushf(cvector* v, float element);
//void cvector_pushd(cvector* v, double element);
void cvector_pop(cvector* v);
void* cvector_get(const cvector* v, unsigned int index);
void cvector_insert(cvector* v, unsigned int index, void* element);
void cvector_inserti(cvector* v, unsigned int index, long int number); //for <=64bit integers
//void cvector_insertf(cvector* v, unsigned int index, float number); //for floating point numbers
//void cvector_insertd(cvector* v, unsigned int index, double number); //for double precision fp nums
void* cvector_remove(cvector* v, unsigned int index);

//typedef unsigned long uint64_t;
//typedef uint8_t bool;

#define CHECK(a,b)
#define nullptr NULL
#define false 0
#define true  1

#if 1
//#define REF_LIST_BUF_SIZE 0x4800
#define REF_LIST_BUF_SIZE 0x41000
#define SPS_BUF_SIZE   0x800
#include "param.h"
#else
#define RPM_BEGIN                                              0x0
#define ALF_BEGIN                                              0x100  //0x180
#define RPM_END                                                0x300  //0x280

typedef union param_u {
    struct {
        unsigned short data[RPM_END - RPM_BEGIN];
    } l;
    struct {

        /*sequence head*/
         unsigned short sqh_profile_id;
         unsigned short sqh_level_id;
         unsigned short sqh_progressive_sequence;
         unsigned short sqh_field_coded_sequence;
         unsigned short sqh_library_stream_flag;
         unsigned short sqh_library_picture_enable_flag;
         unsigned short sqh_horizontal_size;
         unsigned short sqh_vertical_size;
         unsigned short sqh_sample_precision;
         unsigned short sqh_encoding_precision;
         unsigned short sqh_aspect_ratio;
         unsigned short sqh_frame_rate_code;
         unsigned short sqh_low_delay;
         unsigned short sqh_temporal_id_enable_flag;
         unsigned short sqh_max_dpb_size;
         unsigned short sqh_num_ref_default_active_minus1[2];
         unsigned short sqh_log2_max_cu_width_height;
         unsigned short sqh_adaptive_leveling_filter_enable_flag;
         unsigned short sqh_num_of_hmvp_cand;
         unsigned short sqh_output_reorder_delay;
         unsigned short sqh_cross_patch_loop_filter;
        /*picture head*/
         unsigned short pic_header_decode_order_index;
         unsigned short pic_header_picture_output_delay;
         unsigned short pic_header_progressive_frame;
         unsigned short pic_header_top_field_first;
         unsigned short pic_header_repeat_first_field;
         unsigned short pic_header_ref_pic_list_sps_flag[2];
         unsigned short pic_header_rpl_l0_idx;
         unsigned short pic_header_rpl_l1_idx;
         unsigned short pic_header_rpl_l0_ref_pic_num;
         unsigned short pic_header_rpl_l0_ref_pics_ddoi[17];
         unsigned short pic_header_rpl_l1_ref_pic_num;
         unsigned short pic_header_rpl_l1_ref_pics_ddoi[17];
         unsigned short pic_header_rpl_l0_reference_to_library_enable_flag;
         unsigned short pic_header_rpl_l0_library_index_flag[17];
         unsigned short pic_header_rpl_l1_reference_to_library_enable_flag;
         unsigned short pic_header_rpl_l1_library_index_flag[17];
         unsigned short pic_header_loop_filter_disable_flag;
         unsigned short pic_header_random_access_decodable_flag;
         unsigned short pic_header_slice_type;
         unsigned short pic_header_num_ref_idx_active_override_flag;
         unsigned short pic_header_rpl_l0_ref_pic_active_num;
         unsigned short pic_header_rpl_l1_ref_pic_active_num;
        /*patch head*/
        /**/
         unsigned short sqh_adaptive_filter_shape_enable_flag;
         unsigned short pic_header_library_picture_index;
         unsigned short pic_header_top_field_picture_flag;
         unsigned short pic_header_alpha_c_offset;
         unsigned short pic_header_beta_offset;
         unsigned short pic_header_chroma_quant_param_delta_cb;
         unsigned short pic_header_chroma_quant_param_delta_cr;
    } p;
    struct {
        uint16_t padding[ALF_BEGIN - RPM_BEGIN];
        uint16_t picture_alf_enable_Y;
        uint16_t picture_alf_enable_Cb;
        uint16_t picture_alf_enable_Cr;
        uint16_t alf_filters_num_m_1;
        uint16_t dir_index;
        uint16_t region_distance[16];
        uint16_t alf_cb_coeffmulti[9];
        uint16_t alf_cr_coeffmulti[9];
        uint16_t alf_y_coeffmulti[16][9];
    } alf;
}param_t;
#endif

enum ChannelType
{
  CHANNEL_TYPE_LUMA    = 0,
  CHANNEL_TYPE_CHROMA  = 1,
  MAX_NUM_CHANNEL_TYPE = 2
};

enum ComponentID
{
  COMPONENT_Y         = 0,
  COMPONENT_Cb        = 1,
  COMPONENT_Cr        = 2,
  MAX_NUM_COMPONENT   = 3,
  JOINT_CbCr          = MAX_NUM_COMPONENT,
  MAX_NUM_TBLOCKS     = MAX_NUM_COMPONENT
};

typedef enum NalUnitType_
{
  NAL_UNIT_CODED_SLICE_TRAIL = 0,   // 0
  NAL_UNIT_CODED_SLICE_STSA,        // 1
  NAL_UNIT_CODED_SLICE_RADL,        // 2
  NAL_UNIT_CODED_SLICE_RASL,        // 3

  NAL_UNIT_RESERVED_VCL_4,
  NAL_UNIT_RESERVED_VCL_5,
  NAL_UNIT_RESERVED_VCL_6,

  NAL_UNIT_CODED_SLICE_IDR_W_RADL,  // 7
  NAL_UNIT_CODED_SLICE_IDR_N_LP,    // 8
  NAL_UNIT_CODED_SLICE_CRA,         // 9
  NAL_UNIT_CODED_SLICE_GDR,         // 10

  NAL_UNIT_RESERVED_IRAP_VCL_11,
  NAL_UNIT_OPI,                     // 12
  NAL_UNIT_DCI,                     // 13
  NAL_UNIT_VPS,                     // 14
  NAL_UNIT_SPS,                     // 15
  NAL_UNIT_PPS,                     // 16
  NAL_UNIT_PREFIX_APS,              // 17
  NAL_UNIT_SUFFIX_APS,              // 18
  NAL_UNIT_PH,                      // 19
  NAL_UNIT_ACCESS_UNIT_DELIMITER,   // 20
  NAL_UNIT_EOS,                     // 21
  NAL_UNIT_EOB,                     // 22
  NAL_UNIT_PREFIX_SEI,              // 23
  NAL_UNIT_SUFFIX_SEI,              // 24
  NAL_UNIT_FD,                      // 25

  NAL_UNIT_RESERVED_NVCL_26,
  NAL_UNIT_RESERVED_NVCL_27,

  NAL_UNIT_UNSPECIFIED_28,
  NAL_UNIT_UNSPECIFIED_29,
  NAL_UNIT_UNSPECIFIED_30,
  NAL_UNIT_UNSPECIFIED_31,
  NAL_UNIT_INVALID
} NalUnitType;

enum SliceType
{
  B_SLICE               = 0,
  P_SLICE               = 1,
  I_SLICE               = 2,
  NUMBER_OF_SLICE_TYPES = 3
};

enum ChromaFormat
{
  CHROMA_400        = 0,
  CHROMA_420        = 1,
  CHROMA_422        = 2,
  CHROMA_444        = 3,
  NUM_CHROMA_FORMAT = 4
};


typedef struct SliceMap_s
{
  uint32_t               m_sliceID;                           //!< slice identifier (slice index for rectangular slices, slice address for raser-scan slices)
  uint32_t               m_numTilesInSlice;                   //!< number of tiles in slice (raster-scan slices only)
  uint32_t               m_numCtuInSlice;                     //!< number of CTUs in the slice
  cvector     m_ctuAddrInSlice;       //std::vector<uint32_t>             //!< raster-scan addresses of all the CTUs in the slice
} SliceMap;

typedef struct NALUnit_s
{
  NalUnitType m_nalUnitType; ///< nal_unit_type
  uint32_t        m_temporalId;  ///< temporal_id
  uint32_t        m_nuhLayerId;  ///< nuh_layer_id
  uint32_t        m_forbiddenZeroBit;
  uint32_t        m_nuhReservedZeroBit;

#if 0
  /** construct an NALunit structure with given header values. */
  NALUnit(
    NalUnitType nalUnitType,
    int         temporalId = 0,
    uint32_t nuhReservedZeroBit = 0,
    uint32_t forbiddenZeroBit = 0,
    int         nuhLayerId = 0)
    :m_nalUnitType (nalUnitType)
    ,m_temporalId  (temporalId)
    ,m_nuhLayerId  (nuhLayerId)
#if JVET_O0179_PROPOSALB
    , m_forbiddenZeroBit(forbiddenZeroBit)
    , m_nuhReservedZeroBit(nuhReservedZeroBit)
#endif
#endif
  /** returns true if the NALunit is a slice NALunit */
  #define isSlice(nalUnit) \
  (\
    nalUnit.m_nalUnitType == NAL_UNIT_CODED_SLICE_TRAIL \
        || nalUnit.m_nalUnitType == NAL_UNIT_CODED_SLICE_STSA \
        || nalUnit.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL \
        || nalUnit.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP \
        || nalUnit.m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA \
        || nalUnit.m_nalUnitType == NAL_UNIT_CODED_SLICE_GDR \
        || nalUnit.m_nalUnitType == NAL_UNIT_CODED_SLICE_RADL \
        || nalUnit.m_nalUnitType == NAL_UNIT_CODED_SLICE_RASL \
  )
  #define isSei(nalUnit) \
  (\
    nalUnit.m_nalUnitType == NAL_UNIT_PREFIX_SEI \
        || nalUnit.m_nalUnitType == NAL_UNIT_SUFFIX_SEI \
  )

  #define isVcl(nalUnit) \
     isVclNalUnitType(nalUnit.m_nalUnitType)

  #define isVclNalUnitType(t) \
  (\
    t == NAL_UNIT_CODED_SLICE_TRAIL \
        || t == NAL_UNIT_CODED_SLICE_STSA \
        || t == NAL_UNIT_CODED_SLICE_RADL \
        || t == NAL_UNIT_CODED_SLICE_RASL \
        || t == NAL_UNIT_CODED_SLICE_IDR_W_RADL \
        || t == NAL_UNIT_CODED_SLICE_IDR_N_LP \
        || t == NAL_UNIT_CODED_SLICE_CRA \
        || t == NAL_UNIT_CODED_SLICE_GDR \
  )
} NALUnit;

typedef uint32_t SizeType;
typedef struct Size_s
{
  SizeType width;
  SizeType height;
} Size;

typedef struct OPI_s
{
  bool m_olsinfopresentflag;
  bool m_htidinfopresentflag;
  uint32_t  m_opiolsidx;
  uint32_t  m_opihtidplus1;
#if 0
public:
  OPI()
    : m_olsinfopresentflag (false)
    ,  m_htidinfopresentflag (false)
    ,  m_opiolsidx (-1)
    ,  m_opihtidplus1 (-1)
  {};

  virtual ~OPI() {};

  bool getOlsInfoPresentFlag() const { return m_olsinfopresentflag; }
  void setOlsInfoPresentFlag(bool val) { m_olsinfopresentflag = val; }
  bool getHtidInfoPresentFlag() const { return m_htidinfopresentflag; }
  void setHtidInfoPresentFlag(bool val) { m_htidinfopresentflag = val; }
  uint32_t getOpiOlsIdx() const { return m_opiolsidx; }
  void setOpiOlsIdx(uint32_t val) { m_opiolsidx = val; }
  uint32_t getOpiHtidPlus1() const { return m_opihtidplus1; }
  void setOpiHtidPlus1(uint32_t val) { m_opihtidplus1 = val; }
#endif
} OPI;

typedef struct VPS_s
{
  int                   m_VPSId;
  uint32_t              m_maxLayers;

  uint32_t              m_vpsMaxSubLayers;
  uint32_t              m_vpsLayerId[MAX_VPS_LAYERS];
  bool                  m_vpsDefaultPtlDpbHrdMaxTidFlag;
  bool                  m_vpsAllIndependentLayersFlag;
  uint32_t              m_vpsCfgPredDirection[MAX_VPS_SUBLAYERS];
  bool                  m_vpsIndependentLayerFlag[MAX_VPS_LAYERS];
  bool                  m_vpsDirectRefLayerFlag[MAX_VPS_LAYERS][MAX_VPS_LAYERS];
  cvector                m_vpsMaxTidIlRefPicsPlus1; //std::vector<std::vector<uint32_t>>
  bool                  m_vpsEachLayerIsAnOlsFlag;
  uint32_t              m_vpsOlsModeIdc;
  uint32_t              m_vpsNumOutputLayerSets;
  bool                  m_vpsOlsOutputLayerFlag[MAX_NUM_OLSS][MAX_VPS_LAYERS];
  uint32_t              m_directRefLayerIdx[MAX_VPS_LAYERS][MAX_VPS_LAYERS];
  uint32_t              m_generalLayerIdx[MAX_VPS_LAYERS];

  uint32_t              m_vpsNumPtls;
  bool                  m_ptPresentFlag[MAX_NUM_OLSS];
  uint32_t              m_ptlMaxTemporalId[MAX_NUM_OLSS];
  cvector                 m_vpsProfileTierLevel; //std::vector<ProfileTierLevel> m_vpsProfileTierLevel;
  uint32_t              m_olsPtlIdx[MAX_NUM_OLSS];

  // stores index ( ilrp_idx within 0 .. NumDirectRefLayers ) of the dependent reference layers
  uint32_t              m_interLayerRefIdx[MAX_VPS_LAYERS][MAX_VPS_LAYERS];
  bool                  m_vpsExtensionFlag;
  bool                  m_vpsGeneralHrdParamsPresentFlag;
  bool                  m_vpsSublayerCpbParamsPresentFlag;
  uint32_t              m_numOlsTimingHrdParamsMinus1;
  uint32_t              m_hrdMaxTid[MAX_NUM_OLSS];
  uint32_t              m_olsTimingHrdIdx[MAX_NUM_OLSS];
  //GeneralHrdParams      m_generalHrdParams;
  cvector              m_olsDpbPicSize; //std::vector<Size>
  cvector              m_olsDpbParamsIdx;         //std::vector<int>
  cvector              m_outputLayerIdInOls;      //std::vector<std::vector<int>>
  cvector              m_numSubLayersInLayerInOLS;//std::vector<std::vector<int>>

  cvector  m_multiLayerOlsIdxToOlsIdx; //std::vector<int> // mapping from multi-layer OLS index to OLS index. Initialized in deriveOutputLayerSets()
                                               // m_multiLayerOlsIdxToOlsIdx[n] is the OLSidx of the n-th multi-layer OLS.
  cvector  m_olsHrdParams; //std::vector<std::vector<OlsHrdParams>>
  int                           m_totalNumOLSs;
  int                           m_numMultiLayeredOlss;
  uint32_t                      m_multiLayerOlsIdx[MAX_NUM_OLSS];
  int                           m_numDpbParams;
  cvector               m_dpbParameters; //std::vector<DpbParameters>
  bool                          m_sublayerDpbParamsPresentFlag;
  cvector              m_dpbMaxTemporalId;     //std::vector<int>
  cvector              m_targetOutputLayerIdSet;   //std::vector<int>       ///< set of LayerIds to be outputted
  cvector              m_targetLayerIdSet;       //std::vector<int>         ///< set of LayerIds to be included in the sub-bitstream extraction process.
  int                           m_targetOlsIdx;
  cvector m_numOutputLayersInOls;  //std::vector<int>
  cvector m_numLayersInOls;        //std::vector<int>
  cvector m_layerIdInOls;          //std::vector<std::vector<int>>
  cvector m_olsDpbChromaFormatIdc; //std::vector<int>
  cvector m_olsDpbBitDepthMinus8;  //std::vector<int>
} VPS;


typedef struct Window_s
{
  bool m_enabledFlag;
  int  m_winLeftOffset;
  int  m_winRightOffset;
  int  m_winTopOffset;
  int  m_winBottomOffset;

} Window;

typedef struct PPS_s
{
  int              m_PPSId;                    // pic_parameter_set_id
  int              m_SPSId;                    // seq_parameter_set_id
  int              m_picInitQPMinus26;
  bool             m_useDQP;
  bool             m_usePPSChromaTool;
  bool             m_bSliceChromaQpFlag;       // slicelevel_chroma_qp_flag

  int              m_layerId;
  int              m_temporalId;
  int              m_puCounter;

  // access channel

  int              m_chromaCbQpOffset;
  int              m_chromaCrQpOffset;
  bool             m_chromaJointCbCrQpOffsetPresentFlag;
  int              m_chromaCbCrQpOffset;

  // Chroma QP Adjustments
  int              m_chromaQpOffsetListLen; // size (excludes the null entry used in the following array).
  //ChromaQpAdj      m_ChromaQpAdjTableIncludingNullEntry[1+MAX_QP_OFFSET_LIST_SIZE]; //!< Array includes entry [0] for the null offset used when cu_chroma_qp_offset_flag=0, and entries [cu_chroma_qp_offset_idx+1...] otherwise

  uint32_t             m_numRefIdxL0DefaultActive;
  uint32_t             m_numRefIdxL1DefaultActive;

  bool             m_rpl1IdxPresentFlag;

  bool             m_bUseWeightPred;                    //!< Use of Weighting Prediction (P_SLICE)
  bool             m_useWeightedBiPred;                 //!< Use of Weighting Bi-Prediction (B_SLICE)
  bool             m_OutputFlagPresentFlag;             //!< Indicates the presence of output_flag in slice header
  uint32_t         m_numSubPics;                        //!< number of sub-pictures used - must match SPS
  bool             m_subPicIdMappingInPpsFlag;
  uint32_t         m_subPicIdLen;                       //!< sub-picture ID length in bits
  //std::vector<uint16_t> m_subPicId;                     //!< sub-picture ID for each sub-picture in the sequence
  bool             m_noPicPartitionFlag;                //!< no picture partitioning flag - single slice, single tile
  uint8_t          m_log2CtuSize;                       //!< log2 of the CTU size - required to match corresponding value in SPS
  uint8_t          m_ctuSize;                           //!< CTU size
  uint32_t         m_picWidthInCtu;                     //!< picture width in units of CTUs
  uint32_t         m_picHeightInCtu;                    //!< picture height in units of CTUs
  uint32_t         m_numExpTileCols;                    //!< number of explicitly specified tile columns
  uint32_t         m_numExpTileRows;                    //!< number of explicitly specified tile rows
  uint32_t         m_numTileCols;                       //!< number of tile columns
  uint32_t         m_numTileRows;                       //!< number of tile rows
  //std::vector<uint32_t> m_tileColWidth;                 //!< tile column widths in units of CTUs
  //std::vector<uint32_t> m_tileRowHeight;                //!< tile row heights in units of CTUs
  //std::vector<uint32_t> m_tileColBd;                    //!< tile column left-boundaries in units of CTUs
  //std::vector<uint32_t> m_tileRowBd;                    //!< tile row top-boundaries in units of CTUs
  //std::vector<uint32_t> m_ctuToTileCol;                 //!< mapping between CTU horizontal address and tile column index
  //std::vector<uint32_t> m_ctuToTileRow;                 //!< mapping between CTU vertical address and tile row index
  bool             m_rectSliceFlag;                     //!< rectangular slice flag
  bool             m_singleSlicePerSubPicFlag;          //!< single slice per sub-picture flag
  //std::vector<uint32_t> m_ctuToSubPicIdx;               //!< mapping between CTU and Sub-picture index
  uint32_t         m_numSlicesInPic;                    //!< number of rectangular slices in the picture (raster-scan slice specified at slice level)
  bool             m_tileIdxDeltaPresentFlag;           //!< tile index delta present flag
  //std::vector<RectSlice> m_rectSlices;                  //!< list of rectangular slice signalling parameters
  cvector      m_sliceMap; //std::vector<SliceMap>                      //!< list of CTU maps for each slice in the picture
  cvector      m_subPics;       //std::vector<SubPic>            //!< list of subpictures in the picture
  bool             m_loopFilterAcrossTilesEnabledFlag;  //!< loop filtering applied across tiles flag
  bool             m_loopFilterAcrossSlicesEnabledFlag; //!< loop filtering applied across slices flag


  bool             m_cabacInitPresentFlag;

  bool             m_pictureHeaderExtensionPresentFlag;   //< picture header extension flags present in picture headers or not
  bool             m_sliceHeaderExtensionPresentFlag;
  bool             m_deblockingFilterControlPresentFlag;
  bool             m_deblockingFilterOverrideEnabledFlag;
  bool             m_ppsDeblockingFilterDisabledFlag;
  int              m_deblockingFilterBetaOffsetDiv2;    //< beta offset for deblocking filter
  int              m_deblockingFilterTcOffsetDiv2;      //< tc offset for deblocking filter
  int              m_deblockingFilterCbBetaOffsetDiv2;    //< beta offset for Cb deblocking filter
  int              m_deblockingFilterCbTcOffsetDiv2;      //< tc offset for Cb deblocking filter
  int              m_deblockingFilterCrBetaOffsetDiv2;    //< beta offset for Cr deblocking filter
  int              m_deblockingFilterCrTcOffsetDiv2;      //< tc offset for Cr deblocking filter
  bool             m_listsModificationPresentFlag;

  bool             m_rplInfoInPhFlag;
  bool             m_dbfInfoInPhFlag;
  bool             m_saoInfoInPhFlag;
  bool             m_alfInfoInPhFlag;
  bool             m_wpInfoInPhFlag;
  bool             m_qpDeltaInfoInPhFlag;
  bool             m_mixedNaluTypesInPicFlag;

  bool             m_conformanceWindowFlag;
  uint32_t         m_picWidthInLumaSamples;
  uint32_t         m_picHeightInLumaSamples;
  //Window           m_conformanceWindow;
  Window           m_scalingWindow;

  bool             m_wrapAroundEnabledFlag;               //< reference wrap around enabled or not
  unsigned         m_picWidthMinusWrapAroundOffset;          // <pic_width_in_minCbSizeY - wraparound_offset_in_minCbSizeY
  unsigned         m_wrapAroundOffset;                    //< reference wrap around offset in luma samples

  //PreCalcValues   *pcv;
} PPS;

struct CodingStructure_s;
struct Slice_s;

struct PIC_s;

typedef struct Picture_s
{
  uint32_t margin;
  //Window        m_conformanceWindow;
  Window        m_scalingWindow;
  int           m_decodingOrderNumber;
  NalUnitType   m_pictureType;
  bool m_isSubPicBorderSaved;

  //PelStorage m_bufSubPicAbove;
  //PelStorage m_bufSubPicBelow;
  //PelStorage m_bufSubPicLeft;
  //PelStorage m_bufSubPicRight;

  //PelStorage m_bufWrapSubPicAbove;
  //PelStorage m_bufWrapSubPicBelow;

  bool m_bIsBorderExtended;
  bool m_wrapAroundValid;
  unsigned m_wrapAroundOffset;
  bool referenced;
  bool reconstructed;
  bool neededForOutput;
  bool usedByCurr;
  bool longTerm;
  bool topField;
  bool fieldPic;
  //int  m_prevQP[MAX_NUM_CHANNEL_TYPE];
  bool precedingDRAP; // preceding a DRAP picture in decoding order
  int  edrapRapId;
  bool nonReferencePictureFlag;

  int  poc;
  uint32_t temporalId;
  int      layerId;
  //std::vector<SubPic> subPictures;
  int numSlices;
  //std::vector<int> sliceSubpicIdx;

  bool subLayerNonReferencePictureDueToSTSA;

  int* m_spliceIdx;
  int  m_ctuNums;
  int m_lossyQP;
  //std::vector<bool> m_lossylosslessSliceArray;
  bool interLayerRefPicFlag;
  bool mixedNaluTypesInPicFlag;

  //PelStorage m_bufs[NUM_PIC_TYPES];
  struct Picture_s*           unscaledPic;

  //TComHash           m_hashMap;
  //TComHash*          getHashMap() { return &m_hashMap; }
  //const TComHash*    getHashMap() const { return &m_hashMap; }
  //void               addPictureToHashMapForInter();

  struct CodingStructure_s *cs;
//#define SLICE_MAX_NUM 128
#define SLICE_MAX_NUM MAX_SLICES
  struct Slice_s *slices[SLICE_MAX_NUM];  //std::deque<Slice*> slices;
  //SEIMessages        SEIs;

  //MCTSInfo     mctsInfo;
  //std::vector<AQpLayer*> aqlayer;
  //UnitArea m_ctuArea;
#ifdef AML
    struct PIC_s *buf_cfg;
#endif

} Picture;

enum RefPicList
{
  REF_PIC_LIST_0               = 0,   ///< reference list 0
  REF_PIC_LIST_1               = 1,   ///< reference list 1
  NUM_REF_PIC_LIST_01          = 2,
  REF_PIC_LIST_X               = 100  ///< special mark
};

#define L0 REF_PIC_LIST_0
#define L1 REF_PIC_LIST_1

typedef struct BitDepths_s
{
  int recon[MAX_NUM_CHANNEL_TYPE]; ///< the bit depth as indicated in the SPS
} BitDepths;

typedef struct DpbParameters_s
{
  int m_maxDecPicBuffering[MAX_TLAYER];// = { 0 };
  int m_maxNumReorderPics[MAX_TLAYER];// = { 0 };
  int m_maxLatencyIncreasePlus1[MAX_TLAYER];// = { 0 };
} DpbParameters;

typedef struct ReferencePictureList_s
{
  int   m_numberOfShorttermPictures;
  int   m_numberOfLongtermPictures;
  int   m_isLongtermRefPic[MAX_NUM_REF_PICS];
  int   m_refPicIdentifier[MAX_NUM_REF_PICS];  //This can be delta POC for STRP or POC LSB for LTRP
  int   m_POC[MAX_NUM_REF_PICS];
  int   m_numberOfActivePictures;
  bool  m_deltaPocMSBPresentFlag[MAX_NUM_REF_PICS];
  int   m_deltaPOCMSBCycleLT[MAX_NUM_REF_PICS];
  bool  m_ltrp_in_slice_header_flag;
  bool  m_interLayerPresentFlag;
  bool  m_isInterLayerRefPic[MAX_NUM_REF_PICS];
  int   m_interLayerRefPicIdx[MAX_NUM_REF_PICS];
  int   m_numberOfInterLayerPictures;
} ReferencePictureList;

typedef struct PicHeader_s
{
  bool                        m_valid;                                                  //!< picture header is valid yet or not
  Picture*                    m_pcPic;                                                  //!< pointer to picture structure
  int                         m_pocLsb;                                                 //!< least significant bits of picture order count
  bool                        m_nonReferencePictureFlag;                                //!< non-reference picture flag
  bool                        m_gdrOrIrapPicFlag;                                       //!< gdr or irap picture flag
  bool                        m_gdrPicFlag;                                             //!< gradual decoding refresh picture flag
#if GDR_ENABLED
  bool                        m_inGdrInterval;
  int                         m_lastGdrIntervalPoc;
#endif
  uint32_t                    m_recoveryPocCnt;                                         //!< recovery POC count
  bool                        m_noOutputBeforeRecoveryFlag;                             //!< NoOutputBeforeRecoveryFlag
  bool                        m_handleCraAsCvsStartFlag;                                //!< HandleCraAsCvsStartFlag
  bool                        m_handleGdrAsCvsStartFlag;                                //!< HandleGdrAsCvsStartFlag
  int                         m_spsId;                                                  //!< sequence parameter set ID
  int                         m_ppsId;                                                  //!< picture parameter set ID
  bool                        m_pocMsbPresentFlag;                                      //!< ph_poc_msb_present_flag
  int                         m_pocMsbVal;                                              //!< poc_msb_val
  bool                        m_virtualBoundariesEnabledFlag;                           //!< loop filtering across virtual boundaries disabled
  bool                        m_virtualBoundariesPresentFlag;                           //!< loop filtering across virtual boundaries disabled
  unsigned                    m_numVerVirtualBoundaries;                                //!< number of vertical virtual boundaries
  unsigned                    m_numHorVirtualBoundaries;                                //!< number of horizontal virtual boundaries
  unsigned                    m_virtualBoundariesPosX[3];                               //!< horizontal virtual boundary positions
  unsigned                    m_virtualBoundariesPosY[3];                               //!< vertical virtual boundary positions
  bool                        m_picOutputFlag;                                          //!< picture output flag
  ReferencePictureList        m_RPL0;                                              //!< RPL for L0 when present in picture header
  ReferencePictureList        m_RPL1;                                              //!< RPL for L1 when present in picture header
  int                         m_rpl0Idx;                                                //!< index of used RPL in the SPS or -1 for local RPL in the picture header
  int                         m_rpl1Idx;                                                //!< index of used RPL in the SPS or -1 for local RPL in the picture header
  bool                        m_picInterSliceAllowedFlag;                               //!< inter slice allowed flag in PH
  bool                        m_picIntraSliceAllowedFlag;                               //!< intra slice allowed flag in PH
  bool                        m_splitConsOverrideFlag;                                  //!< partitioning constraint override flag
  uint32_t                    m_cuQpDeltaSubdivIntra;                                   //!< CU QP delta maximum subdivision for intra slices
  uint32_t                    m_cuQpDeltaSubdivInter;                                   //!< CU QP delta maximum subdivision for inter slices
  uint32_t                    m_cuChromaQpOffsetSubdivIntra;                            //!< CU chroma QP offset maximum subdivision for intra slices
  uint32_t                    m_cuChromaQpOffsetSubdivInter;                            //!< CU chroma QP offset maximum subdivision for inter slices
  bool                        m_enableTMVPFlag;                                         //!< enable temporal motion vector prediction
  bool                        m_picColFromL0Flag;                                       //!< syntax element collocated_from_l0_flag
  uint32_t                    m_colRefIdx;
  bool                        m_mvdL1ZeroFlag;                                          //!< L1 MVD set to zero flag
  uint32_t                    m_maxNumAffineMergeCand;                                  //!< max number of sub-block merge candidates
  bool                        m_disFracMMVD;                                            //!< fractional MMVD offsets disabled flag
  bool                        m_bdofDisabledFlag;                                       //!< picture level BDOF disable flag
  bool                        m_dmvrDisabledFlag;                                       //!< picture level DMVR disable flag
  bool                        m_profDisabledFlag;                                       //!< picture level PROF disable flag
  bool                        m_jointCbCrSignFlag;                                      //!< joint Cb/Cr residual sign flag
  int                         m_qpDelta;                                                //!< value of Qp delta
  bool                        m_saoEnabledFlag[MAX_NUM_CHANNEL_TYPE];                   //!< sao enabled flags for each channel
  bool                        m_alfEnabledFlag[MAX_NUM_COMPONENT];                      //!< alf enabled flags for each component
  int                         m_numAlfApsIdsLuma;                                       //!< number of alf aps active for the picture
  //std::vector<int>            m_alfApsIdsLuma;                                          //!< list of alf aps for the picture
  int                         m_alfApsIdChroma;                                         //!< chroma alf aps ID
  bool m_ccalfEnabledFlag[MAX_NUM_COMPONENT];
  int  m_ccalfCbApsId;
  int  m_ccalfCrApsId;
  bool                        m_deblockingFilterOverrideFlag;                           //!< deblocking filter override controls enabled
  bool                        m_deblockingFilterDisable;                                //!< deblocking filter disabled flag
  int                         m_deblockingFilterBetaOffsetDiv2;                         //!< beta offset for deblocking filter
  int                         m_deblockingFilterTcOffsetDiv2;                           //!< tc offset for deblocking filter
  int                         m_deblockingFilterCbBetaOffsetDiv2;                       //!< beta offset for deblocking filter
  int                         m_deblockingFilterCbTcOffsetDiv2;                         //!< tc offset for deblocking filter
  int                         m_deblockingFilterCrBetaOffsetDiv2;                       //!< beta offset for deblocking filter
  int                         m_deblockingFilterCrTcOffsetDiv2;                         //!< tc offset for deblocking filter
  bool                        m_lmcsEnabledFlag;                                        //!< lmcs enabled flag
  int                         m_lmcsApsId;                                              //!< lmcs APS ID
  //APS*                        m_lmcsAps;                                                //!< lmcs APS
  bool                        m_lmcsChromaResidualScaleFlag;                            //!< lmcs chroma residual scale flag
  bool                        m_explicitScalingListEnabledFlag;                         //!< explicit quantization scaling list enabled
  int                         m_scalingListApsId;                                       //!< quantization scaling list APS ID
  //APS*                        m_scalingListAps;                                         //!< quantization scaling list APS
  unsigned                    m_minQT[3];                                               //!< minimum quad-tree size  0: I slice luma; 1: P/B slice; 2: I slice chroma
  unsigned                    m_maxMTTHierarchyDepth[3];                                //!< maximum MTT depth
  unsigned                    m_maxBTSize[3];                                           //!< maximum BT size
  unsigned                    m_maxTTSize[3];                                           //!< maximum TT size

 // WPScalingParam              m_weightPredTable[NUM_REF_PIC_LIST_01][MAX_NUM_REF][MAX_NUM_COMPONENT];   // [REF_PIC_LIST_0 or REF_PIC_LIST_1][refIdx][0:Y, 1:U, 2:V]
  int                         m_numL0Weights;                                           //!< number of weights for L0 list
  int                         m_numL1Weights;                                           //!< number of weights for L1 list
} PicHeader;

#define RPLLIST_MAX 32
typedef struct RPLList_s
{
  ReferencePictureList m_referencePictureLists[RPLLIST_MAX];
} RPLList;

typedef struct SPS_s
{
  int               m_SPSId;
  int               m_VPSId;
  int               m_layerId;
  bool              m_affineAmvrEnabledFlag;
  bool              m_DMVR;
  bool              m_MMVD;
  bool              m_SBT;
  bool              m_ISP;
  enum ChromaFormat      m_chromaFormatIdc;

  uint32_t              m_uiMaxTLayers;           // maximum number of temporal layers

  bool              m_ptlDpbHrdParamsPresentFlag;
  bool              m_SubLayerDpbParamsFlag;

  // Structure
  uint32_t              m_maxWidthInLumaSamples;
  uint32_t              m_maxHeightInLumaSamples;
  //Window                m_conformanceWindow;
  bool                  m_subPicInfoPresentFlag;                // indicates the presence of sub-picture info
  uint32_t              m_numSubPics;                        //!< number of sub-pictures used
  bool                  m_independentSubPicsFlag;
  bool                  m_subPicSameSizeFlag;
#if 0
  std::vector<uint32_t> m_subPicCtuTopLeftX;
  std::vector<uint32_t> m_subPicCtuTopLeftY;
  std::vector<uint32_t> m_subPicWidth;
  std::vector<uint32_t> m_subPicHeight;
  std::vector<bool>     m_subPicTreatedAsPicFlag;
  std::vector<bool>     m_loopFilterAcrossSubpicEnabledFlag;
#endif
  bool                  m_subPicIdMappingExplicitlySignalledFlag;
  bool                  m_subPicIdMappingPresentFlag;
  uint32_t              m_subPicIdLen;                       //!< sub-picture ID length in bits
  //std::vector<uint16_t> m_subPicId;                          //!< sub-picture ID for each sub-picture in the sequence

  int               m_log2MinCodingBlockSize;
  unsigned    m_CTUSize;
  unsigned    m_partitionOverrideEnabled;       // enable partition constraints override function
  unsigned    m_minQT[3];   // 0: I slice luma; 1: P/B slice; 2: I slice chroma
  unsigned    m_maxMTTHierarchyDepth[3];
  unsigned    m_maxBTSize[3];
  unsigned    m_maxTTSize[3];
  bool        m_idrRefParamList;
  unsigned    m_dualITree;
  uint32_t              m_uiMaxCUWidth;
  uint32_t              m_uiMaxCUHeight;

  RPLList           m_RPLList0;
  RPLList           m_RPLList1;
  uint32_t          m_numRPL0;
  uint32_t          m_numRPL1;

  bool              m_rpl1CopyFromRpl0Flag;
  bool              m_rpl1IdxPresentFlag;
  bool              m_allRplEntriesHasSameSignFlag;
  bool              m_bLongTermRefsPresent;
  bool              m_SPSTemporalMVPEnabledFlag;
  int               m_maxNumReorderPics[MAX_TLAYER];

  // Tool list

  bool              m_transformSkipEnabledFlag;
  int               m_log2MaxTransformSkipBlockSize;
  bool              m_BDPCMEnabledFlag;
  bool              m_JointCbCrEnabledFlag;
  // Parameter
  BitDepths         m_bitDepths;
  bool              m_entropyCodingSyncEnabledFlag;                    //!< Flag for enabling WPP
  bool              m_entryPointPresentFlag;                           //!< Flag for indicating the presence of entry points
  int               m_qpBDOffset[MAX_NUM_CHANNEL_TYPE];
  int               m_internalMinusInputBitDepth[MAX_NUM_CHANNEL_TYPE]; //  max(0, internal bitdepth - input bitdepth);                                          }

  bool              m_sbtmvpEnabledFlag;
  bool              m_bdofEnabledFlag;
  bool              m_fpelMmvdEnabledFlag;
  bool              m_BdofControlPresentInPhFlag;
  bool              m_DmvrControlPresentInPhFlag;
  bool              m_ProfControlPresentInPhFlag;
  uint32_t          m_uiBitsForPOC;
  bool              m_pocMsbCycleFlag;
  uint32_t          m_pocMsbCycleLen;
  int               m_numExtraPHBytes;
  int               m_numExtraSHBytes;
#if 0
  std::vector<bool> m_extraPHBitPresentFlag;
  std::vector<bool> m_extraSHBitPresentFlag;
#endif
  uint32_t          m_numLongTermRefPicSPS;
  uint32_t          m_ltRefPicPocLsbSps[MAX_NUM_LONG_TERM_REF_PICS];
  bool              m_usedByCurrPicLtSPSFlag[MAX_NUM_LONG_TERM_REF_PICS];
  uint32_t          m_log2MaxTbSize;
  bool              m_useWeightPred;                     //!< Use of Weighting Prediction (P_SLICE)
  bool              m_useWeightedBiPred;                 //!< Use of Weighting Bi-Prediction (B_SLICE)

  bool              m_saoEnabledFlag;

  bool              m_bTemporalIdNestingFlag; // temporal_id_nesting_flag

  bool              m_scalingListEnabledFlag;
  bool              m_depQuantEnabledFlag;            //!< dependent quantization enabled flag
  bool              m_signDataHidingEnabledFlag;      //!< sign data hiding enabled flag
  bool              m_virtualBoundariesEnabledFlag;   //!< Enable virtual boundaries tool
  bool              m_virtualBoundariesPresentFlag;   //!< disable loop filtering across virtual boundaries
  unsigned          m_numVerVirtualBoundaries;                         //!< number of vertical virtual boundaries
  unsigned          m_numHorVirtualBoundaries;                         //!< number of horizontal virtual boundaries
  unsigned          m_virtualBoundariesPosX[3];                        //!< horizontal position of each vertical virtual boundary
  unsigned          m_virtualBoundariesPosY[3];                        //!< vertical position of each horizontal virtual boundary
  uint32_t          m_uiMaxDecPicBuffering[MAX_TLAYER];
  uint32_t          m_uiMaxLatencyIncreasePlus1[MAX_TLAYER];


  bool              m_generalHrdParametersPresentFlag;
  //GeneralHrdParams m_generalHrdParams;
  //OlsHrdParams     m_olsHrdParams[MAX_TLAYER];

  bool              m_fieldSeqFlag;
  bool              m_vuiParametersPresentFlag;
  unsigned          m_vuiPayloadSize;
  //VUI               m_vuiParameters;

  //SPSRExt           m_spsRangeExtension;

  //static const int  m_winUnitX[NUM_CHROMA_FORMAT];
  //static const int  m_winUnitY[NUM_CHROMA_FORMAT];
  //ProfileTierLevel  m_profileTierLevel;

  bool              m_alfEnabledFlag;
  bool              m_ccalfEnabledFlag;
  bool              m_wrapAroundEnabledFlag;
  unsigned          m_IBCFlag;
  bool              m_useColorTrans;
  unsigned          m_PLTMode;

  bool              m_lmcsEnabled;
  bool              m_AMVREnabledFlag;
  bool              m_LMChroma;
  bool              m_horCollocatedChromaFlag;
  bool              m_verCollocatedChromaFlag;
  bool              m_MTS;
  bool              m_IntraMTS;                   // 18
  bool              m_InterMTS;                   // 19
  bool              m_LFNST;
  bool              m_SMVD;
  bool              m_Affine;
  bool              m_AffineType;
  bool              m_PROF;
  bool              m_bcw;                        //
  bool              m_ciip;
  bool              m_Geo;
//#if LUMA_ADAPTIVE_DEBLOCKING_FILTER_QP_OFFSET
//  bool              m_LadfEnabled;
//  int               m_LadfNumIntervals;
//  int               m_LadfQpOffset[MAX_LADF_INTERVALS];
//  int               m_LadfIntervalLowerBound[MAX_LADF_INTERVALS];
//#endif
  bool              m_MRL;
  bool              m_MIP;
  //ChromaQpMappingTable m_chromaQpMappingTable;
  bool m_GDREnabledFlag;
  bool              m_SubLayerCbpParametersPresentFlag;

  bool              m_rprEnabledFlag;
  bool              m_resChangeInClvsEnabledFlag;
  bool              m_interLayerPresentFlag;

  uint32_t          m_log2ParallelMergeLevelMinus2;
  bool              m_ppsValidFlag[64];
  Size              m_scalingWindowSizeInPPS[64];
  uint32_t          m_maxNumMergeCand;
  uint32_t          m_maxNumAffineMergeCand;
  uint32_t          m_maxNumIBCMergeCand;
  uint32_t          m_maxNumGeoCand;
  bool              m_scalingMatrixAlternativeColourSpaceDisabledFlag;
  bool              m_scalingMatrixDesignatedColourSpaceFlag;

  bool m_disableScalingMatrixForLfnstBlks;

} SPS;

typedef struct SubPic_s
{
  uint32_t         m_subPicID;                                  //!< ID of subpicture
  uint32_t         m_subPicIdx;                                 //!< Index of subpicture
  uint32_t         m_numCTUsInSubPic;                           //!< number of CTUs contained in this sub-picture
  uint32_t         m_subPicCtuTopLeftX;                         //!< horizontal position of top left CTU of the subpicture in unit of CTU
  uint32_t         m_subPicCtuTopLeftY;                         //!< vertical position of top left CTU of the subpicture in unit of CTU
  uint32_t         m_subPicWidth;                               //!< the width of subpicture in units of CTU
  uint32_t         m_subPicHeight;                              //!< the height of subpicture in units of CTU
  uint32_t         m_subPicWidthInLumaSample;                   //!< the width of subpicture in units of luma sample
  uint32_t         m_subPicHeightInLumaSample;                  //!< the height of subpicture in units of luma sample
  uint32_t         m_firstCtuInSubPic;                          //!< the raster scan index of the first CTU in a subpicture
  uint32_t         m_lastCtuInSubPic;                           //!< the raster scan index of the last CTU in a subpicture
  uint32_t         m_subPicLeft;                                //!< the position of left boundary
  uint32_t         m_subPicRight;                               //!< the position of right boundary
  uint32_t         m_subPicTop;                                 //!< the position of top boundary
  uint32_t         m_subPicBottom;                              //!< the position of bottom boundary
  //std::vector<uint32_t> m_ctuAddrInSubPic;                      //!< raster scan addresses of all the CTUs in the slice

  bool             m_treatedAsPicFlag;                          //!< whether the subpicture is treated as a picture in the decoding process excluding in-loop filtering operations
  bool             m_loopFilterAcrossSubPicEnabledFlag;         //!< whether in-loop filtering operations may be performed across the boundaries of the subpicture
  uint32_t         m_numSlicesInSubPic;                         //!< Number of slices contained in this subpicture
} SubPic;

#ifdef AML
typedef struct {
  int first;
  int second;
} ScaleRatio;
#endif

typedef struct Slice_s
{

  //  Bitstream writing
  bool                       m_saoEnabledFlag[MAX_NUM_CHANNEL_TYPE];
  int                        m_iPOC;
  int                        m_iLastIDR;
  int                        m_prevGDRInSameLayerPOC;  //< the previous GDR in the same layer
  int                        m_iAssociatedIRAP;
  NalUnitType                m_iAssociatedIRAPType;
  int                        m_prevGDRSubpicPOC;
  int                        m_prevIRAPSubpicPOC;
  NalUnitType                m_prevIRAPSubpicType;
  bool                       m_enableDRAPSEI;
  bool                       m_useLTforDRAP;
  bool                       m_isDRAP;
  int                        m_latestDRAPPOC;
  bool                       m_enableEdrapSEI;
  int                        m_edrapRapId;
  bool                       m_useLTforEdrap;
  int                        m_edrapNumRefRapPics;
  //std::vector<int>           m_edrapRefRapIds;
  int                        m_latestEDRAPPOC;
  bool                       m_latestEdrapLeadingPicDecodableFlag;
  ReferencePictureList        m_RPL0;            //< RPL for L0 when present in slice header
  ReferencePictureList        m_RPL1;            //< RPL for L1 when present in slice header
  int                         m_rpl0Idx;              //< index of used RPL in the SPS or -1 for local RPL in the slice header
  int                         m_rpl1Idx;              //< index of used RPL in the SPS or -1 for local RPL in the slice header
  NalUnitType                m_eNalUnitType;         ///< Nal unit type for the slice
  bool                       m_pictureHeaderInSliceHeader;
  uint32_t                   m_nuhLayerId;           ///< Nal unit layer id
  enum SliceType                  m_eSliceType;
  bool                       m_noOutputOfPriorPicsFlag;           //!< no output of prior pictures flag
  int                        m_iSliceQp;
  int                        m_iSliceQpBase;
  bool                       m_ChromaQpAdjEnabled;
  bool                       m_lmcsEnabledFlag;
  bool                       m_explicitScalingListUsed;
  bool                       m_deblockingFilterDisable;
  bool                       m_deblockingFilterOverrideFlag;      //< offsets for deblocking filter inherit from PPS
  int                        m_deblockingFilterBetaOffsetDiv2;    //< beta offset for deblocking filter
  int                        m_deblockingFilterTcOffsetDiv2;      //< tc offset for deblocking filter
  int                        m_deblockingFilterCbBetaOffsetDiv2;  //< beta offset for deblocking filter
  int                        m_deblockingFilterCbTcOffsetDiv2;    //< tc offset for deblocking filter
  int                        m_deblockingFilterCrBetaOffsetDiv2;  //< beta offset for deblocking filter
  int                        m_deblockingFilterCrTcOffsetDiv2;    //< tc offset for deblocking filter
  bool                       m_depQuantEnabledFlag;               //!< dependent quantization enabled flag
  int                        m_riceBaseLevelValue;    //< baseLevel value for abs_remainder
  bool                       m_signDataHidingEnabledFlag;         //!< sign data hiding enabled flag
  bool                       m_tsResidualCodingDisabledFlag;
  int                        m_list1IdxToList0Idx[MAX_NUM_REF];
  int                        m_aiNumRefIdx   [NUM_REF_PIC_LIST_01];    //  for multiple reference of current slice
  bool                       m_pendingRasInit;

  bool                       m_bCheckLDC;

  bool                       m_biDirPred;
  int                        m_symRefIdx[2];

  //  Data
  int                        m_iSliceQpDelta;
  int                        m_iSliceChromaQpDelta[MAX_NUM_COMPONENT+1];
  Picture*                   m_apcRefPicList [NUM_REF_PIC_LIST_01][MAX_NUM_REF+1];
  int                        m_aiRefPOCList  [NUM_REF_PIC_LIST_01][MAX_NUM_REF+1];
  bool                       m_bIsUsedAsLongTerm[NUM_REF_PIC_LIST_01][MAX_NUM_REF+1];
  int                        m_iDepth;
  Picture*                   m_scaledRefPicList[NUM_REF_PIC_LIST_01][MAX_NUM_REF + 1];
  Picture*                   m_savedRefPicList[NUM_REF_PIC_LIST_01][MAX_NUM_REF + 1];
  ScaleRatio                 m_scalingRatio[NUM_REF_PIC_LIST_01][MAX_NUM_REF_PICS]; // std::pair<int, int>


  // access channel
  VPS*                 m_pcVPS;
  SPS*                 m_pcSPS;
  PPS*                 m_pcPPS;
  Picture*                   m_pcPic;
  PicHeader*           m_pcPicHeader;    //!< pointer to picture header structure
  bool                       m_colFromL0Flag;  // collocated picture from List0 flag


  uint32_t                   m_colRefIdx;
  //double                     m_lambdas[MAX_NUM_COMPONENT];

  //bool                       m_abEqualRef  [NUM_REF_PIC_LIST_01][MAX_NUM_REF][MAX_NUM_REF];
  uint32_t                   m_uiTLayer;
  bool                       m_bTLayerSwitchingFlag;

  SliceMap                   m_sliceMap;                     //!< list of CTUs in current slice - raster scan CTU addresses
  uint32_t                   m_independentSliceIdx;
  bool                       m_nextSlice;
  uint32_t                   m_sliceBits;
  bool                       m_bFinalized;


  bool                       m_bTestWeightPred;
  bool                       m_bTestWeightBiPred;
  //WPScalingParam             m_weightPredTable[NUM_REF_PIC_LIST_01][MAX_NUM_REF][MAX_NUM_COMPONENT]; // [REF_PIC_LIST_0 or REF_PIC_LIST_1][refIdx][0:Y, 1:U, 2:V]
  //WPACDCParam                m_weightACDCParam[MAX_NUM_COMPONENT];
  //ClpRngs                    m_clpRngs;
  //std::vector<uint32_t>      m_substreamSizes;
  uint32_t                   m_numEntryPoints;
  uint32_t                   m_numSubstream;

  bool                       m_cabacInitFlag;

  uint32_t                   m_sliceSubPicId;


  enum SliceType                  m_encCABACTableIdx;           // Used to transmit table selection across slices.

  //clock_t                    m_iProcessingStartTime;
  //double                     m_dProcessingTime;

  int                        m_rpPicOrderCntVal;
  //APS*                       m_alfApss[ALF_CTB_MAX_NUM_APS];
  bool                       m_alfEnabledFlag[MAX_NUM_COMPONENT];
  int                        m_numAlfApsIdsLuma;
  //std::vector<int>           m_alfApsIdsLuma;
  int                        m_alfApsIdChroma;
  bool                       m_ccAlfCbEnabledFlag;
  bool                       m_ccAlfCrEnabledFlag;
  int                        m_ccAlfCbApsId;
  int                        m_ccAlfCrApsId;
  bool                       m_disableSATDForRd;
  bool                       m_isLossless;
  int                        m_tsrc_index;
  unsigned                   m_riceBit[8];
} Slice;

typedef struct CodingStructure_s
{
  //UnitArea         area;

  Picture         *picture;
  struct CodingStructure_s *parent;
  struct CodingStructure_s *bestCS;
  Slice           *slice;

  //UnitScale        unitScale[MAX_NUM_COMPONENT];

  //int         baseQP;
  //int         prevQP[MAX_NUM_CHANNEL_TYPE];
  //int         currQP[MAX_NUM_CHANNEL_TYPE];
  //int         chromaQpAdj;
  SPS *sps;
  PPS *pps;
  PicHeader *picHeader;
  //APS*       alfApss[ALF_CTB_MAX_NUM_APS];
  //APS *      lmcsAps;
  //APS *      scalinglistAps;
  VPS *vps;
  //const PreCalcValues* pcv;
} CodingStructure;


#define PIC_LIST_SIZE 32

#ifdef AML
typedef struct PicList_s {
  Picture * pic[PIC_LIST_SIZE];
  //int pos;
} PicList;


#define MAX_VECTOR_SIZE 128
typedef struct ivector_s {
  int data[MAX_VECTOR_SIZE];
  int size;
} ivector;
#endif

typedef struct AccessUnitPicInfo_s
{
  NalUnitType     m_nalUnitType; ///< nal_unit_type
  uint32_t        m_temporalId;  ///< temporal_id
  uint32_t        m_nuhLayerId;  ///< nuh_layer_id
  int             m_POC;
} AccessUnitPicInfo;

  struct NalUnitInfo
  {
    NalUnitType     m_nalUnitType; ///< nal_unit_type
    uint32_t        m_nuhLayerId;  ///< nuh_layer_id
    uint32_t        m_firstCTUinSlice; /// the first CTU in slice, specified with raster scan order ctu address
    int             m_POC;             /// the picture order
  } ;

typedef struct DecLib_s {
  void *vvc_dec;
  void *hw;
  int                     m_iMaxRefPicNum;
  bool m_isFirstGeneralHrd;
  //GeneralHrdParams        m_prevGeneralHrdParams;

  int                     m_prevGDRInSameLayerPOC[MAX_VPS_LAYERS]; ///< POC number of the latest GDR picture
  int                     m_prevGDRInSameLayerRecoveryPOC[MAX_VPS_LAYERS]; ///< Recovery POC number of the latest GDR picture
  NalUnitType             m_associatedIRAPType[MAX_VPS_LAYERS]; ///< NAL unit type of the previous IRAP picture
  int                     m_pocCRA[MAX_VPS_LAYERS];            ///< POC number of the previous CRA picture
  int                     m_associatedIRAPDecodingOrderNumber[MAX_VPS_LAYERS]; ///< Decoding order number of the previous IRAP picture
  int                     m_decodingOrderCounter;
  int                     m_puCounter;
  bool                    m_seiInclusionFlag;
#ifndef REDUCE_SIZE
  int                     m_prevGDRSubpicPOC[MAX_VPS_LAYERS][MAX_NUM_SUB_PICS];
  int                     m_prevIRAPSubpicPOC[MAX_VPS_LAYERS][MAX_NUM_SUB_PICS];
  NalUnitType             m_prevIRAPSubpicType[MAX_VPS_LAYERS][MAX_NUM_SUB_PICS];
  int                     m_prevIRAPSubpicDecOrderNo[MAX_VPS_LAYERS][MAX_NUM_SUB_PICS];
#endif
  int                     m_pocRandomAccess;   ///< POC number of the random access point (the first IDR or CRA picture)
  int                     m_lastRasPoc;
  bool                    m_prevEOS[MAX_VPS_LAYERS];

  PicList                 m_cListPic;         //  Dynamic buffer
  //ParameterSetManager     m_parameterSetManager;  // storage for parameter sets
  PicHeader               m_picHeader;            // picture header
  Slice*                  m_apcSlicePilot;
#ifdef AML
  param_t                 *param;
  int                     decode_count;
#endif
#if 0
  SEIMessages             m_SEIs; ///< List of SEI messages that have been received before the first slice and between slices, excluding prefix SEIs...
  SEIScalabilityDimensionInfo* m_sdiSEIInFirstAU;
  SEIMultiviewAcquisitionInfo* m_maiSEIInFirstAU;


  // functional classes
  IntraPrediction         m_cIntraPred;
  InterPrediction         m_cInterPred;
  TrQuant                 m_cTrQuant;
  DecSlice                m_cSliceDecoder;
  TrQuant                 m_cTrQuantScalingList;
  DecCu                   m_cCuDecoder;
  HLSyntaxReader          m_HLSReader;
  CABACDecoder            m_CABACDecoder;
  SEIReader               m_seiReader;
#if JVET_S0257_DUMP_360SEI_MESSAGE
  SeiCfgFileDump          m_seiCfgDump;
#endif
  DeblockingFilter        m_deblockingFilter;
  SampleAdaptiveOffset    m_cSAO;
  AdaptiveLoopFilter      m_cALF;
  Reshape                 m_cReshaper;                        ///< reshaper class
  HRD                     m_HRD;
  // decoder side RD cost computation
  RdCost                  m_cRdCost;                      ///< RD cost computation class
#if JVET_J0090_MEMORY_BANDWIDTH_MEASURE
  CacheModel              m_cacheModel;
#endif
#endif
  //bool isRandomAccessSkipPicture(int& iSkipFrame, int& iPOCLastDisplay, bool mixedNaluInPicFlag, uint32_t layerId);
  Picture*                m_pcPic;
  uint32_t                m_uiSliceSegmentIdx;
  uint32_t                m_prevLayerID;
  int                     m_prevPOC;
  int                     m_prevPicPOC;
  int                     m_prevTid0POC;
  bool                    m_bFirstSliceInPicture;
  bool                    m_firstPictureInSequence;
  //SEIColourTransformApply m_colourTranfParams;
  //PelStorage              m_invColourTransfBuf;
  bool                    m_firstSliceInSequence[MAX_VPS_LAYERS];
  bool                    m_firstSliceInBitstream;
  bool                    m_isFirstAuInCvs;
  bool                    m_accessUnitEos[MAX_VPS_LAYERS];
  bool                    m_prevSliceSkipped;
  int                     m_skippedPOC;
  uint32_t                m_skippedLayerID;
  int                     m_lastPOCNoOutputPriorPics;
  bool                    m_isNoOutputPriorPics;
  bool                    m_lastNoOutputBeforeRecoveryFlag[MAX_VPS_LAYERS];    //value of variable NoOutputBeforeRecoveryFlag of the associated CRA/GDR pic
  int                     m_sliceLmcsApsId;         //value of LmcsApsId, constraint is same id for all slices in one picture
  //std::ostream           *m_pDecodedSEIOutputStream;
  uint32_t                m_audIrapOrGdrAuFlag;
//#if JVET_S0257_DUMP_360SEI_MESSAGE
  //std::string             m_decoded360SeiDumpFileName;
//#endif

  int                     m_decodedPictureHashSEIEnabled;  ///< Checksum(3)/CRC(2)/MD5(1)/disable(0) acting on decoded picture hash SEI message
  uint32_t                m_numberOfChecksumErrorsDetected;

  bool                    m_warningMessageSkipPicture;

  //std::list<InputNALUnit*> m_prefixSEINALUs; /// Buffered up prefix SEI NAL Units.
  int                     m_debugPOC;
  int                     m_debugCTU;
/*
  struct AccessUnitInfo
  {
    NalUnitType     m_nalUnitType; ///< nal_unit_type
    uint32_t        m_temporalId;  ///< temporal_id
    uint32_t        m_nuhLayerId;  ///< nuh_layer_id
  };*/
  cvector m_accessUnitNals; //std::vector<AccessUnitInfo>
 /* struct AccessUnitPicInfo
  {
    NalUnitType     m_nalUnitType; ///< nal_unit_type
    uint32_t        m_temporalId;  ///< temporal_id
    uint32_t        m_nuhLayerId;  ///< nuh_layer_id
    int             m_POC;
  };*/
  cvector m_accessUnitPicInfo; //std::vector<AccessUnitPicInfo>
  cvector m_firstAccessUnitPicInfo; //std::vector<AccessUnitPicInfo>
  cvector m_nalUnitInfo[MAX_VPS_LAYERS]; //std::vector<NalUnitInfo>
  cvector m_accessUnitApsNals; //std::vector<int>
  cvector m_accessUnitSeiTids;//std::vector<int>
  cvector m_accessUnitNoOutputPriorPicFlags;  //std::vector<bool>

  // NAL unit type, layer ID, and SEI payloadType
  cvector m_accessUnitSeiPayLoadTypes; //std::vector<std::tuple<NalUnitType, int, SEI::PayloadType>>

  cvector m_pictureUnitNals; //std::vector<NalUnitType>
  #define LIST_PIC_SEI_NAL_SIZE 32
  NALUnit *m_pictureSeiNalus[LIST_PIC_SEI_NAL_SIZE]; //std::list<InputNALUnit*> m_pictureSeiNalus;
  //std::list<InputNALUnit*> m_suffixApsNalus;
  OPI*                    m_opi;
  bool                    m_mTidExternalSet;
  bool                    m_mTidOpiSet;
  bool                    m_tOlsIdxTidExternalSet;
  bool                    m_tOlsIdxTidOpiSet;
  VPS*                    m_vps;
  int                     m_maxDecSubPicIdx;
  int                     m_maxDecSliceAddrInSubPic;
  int                     m_clsVPSid;

  int                     m_targetSubPicIdx;

  //DCI*                    m_dci;
  //ParameterSetMap<APS>*   m_apsMapEnc;
#if GDR_LEAK_TEST
  int                     m_gdrPocRandomAccess;
#endif // GDR_LEAK_TEST
#ifdef AML
  NALUnit a_nalu;

  VPS *a_cur_vps;
  PPS *a_cur_pps;
  SPS *a_cur_sps;
#endif
} DecLib;

#ifdef AML
#define REF_ENTRY_NUM 31
typedef struct ref_set_ {
    unsigned char LtrpInSliceHeaderFlag;
    unsigned numIlrp;
    unsigned numLtrp;
    unsigned numStrp;
    unsigned numRefPic;
    unsigned ref_entry[REF_ENTRY_NUM];
} ref_set_t;
#endif


#ifdef USE_FULL_REF_LIST_BUFFER
#define REF_SET_NUM 64  // -1(64) to 63 , 65 used
typedef struct RPL_set_{
  ref_set_t RPL0_set[REF_SET_NUM];
  ref_set_t RPL1_set[REF_SET_NUM];
} sps_RPL_set_t;
#else
#define REF_SET_NUM 72  // -1(64) to 63 , 65 used
#endif
typedef struct DecApp_s {
  /*DecAppCfg.h*/
  int           m_iSkipFrame;                           ///< counter for frames prior to the random access point to skip
  int           m_outputBitDepth[MAX_NUM_CHANNEL_TYPE]; ///< bit depth used for writing output
  //InputColourSpaceConversion m_outputColourSpaceConvert;
  int           m_targetOlsIdx;                       ///< target output layer set
  cvector       m_targetOutputLayerIdSet;     //std::vector<int>     ///< set of LayerIds to be outputted
  int           m_iMaxTemporalLayer;                  ///< maximum temporal layer to be decoded
  bool          m_mTidExternalSet;                    ///< maximum temporal layer set externally
  bool          m_tOlsIdxTidExternalSet;              ///< target output layer set index externally set
  int           m_decodedPictureHashSEIEnabled;       ///< Checksum(3)/CRC(2)/MD5(1)/disable(0) acting on decoded picture hash SEI message
  bool          m_decodedNoDisplaySEIEnabled;         ///< Enable(true)/disable(false) writing only pictures that get displayed based on the no display SEI message
  cvector       m_targetDecLayerIdSet;          //std::vector<int>   ///< set of LayerIds to be included in the sub-bitstream extraction process.

  bool          m_bClipOutputVideoToRec709Range;      ///< If true, clip the output video to the Rec 709 range on saving.
  bool          m_packedYUVMode;                      ///< If true, output 10-bit and 12-bit YUV data as 5-byte and 3-byte (respectively) packed YUV data
  int           m_statMode;                           ///< Config statistic mode (0 - bit stat, 1 - tool stat, 3 - both)
  bool          m_mctsCheck;

  int          m_upscaledOutput;                     ////< Output upscaled (2), decoded but in full resolution buffer (1) or decoded cropped (0, default) picture for RPR.
  int           m_targetSubPicIdx;                    ///< Specify which subpicture shall be write to output, using subpicture index
#if GDR_LEAK_TEST
  int           m_gdrPocRandomAccess;                   ///<
#endif // GDR_LEAK_TEST


  DecLib          m_cDecLib;                     ///< decoder class
  // for output control
  int             m_iPOCLastDisplay;              ///< last POC in display order

  bool            m_newCLVS[MAX_NUM_LAYER_IDS];   ///< used to record a new CLVSS

  //SEIAnnotatedRegions::AnnotatedRegionHeader                 m_arHeader; ///< AR header
  //std::map<uint32_t, SEIAnnotatedRegions::AnnotatedRegionObject> m_arObjects; ///< AR object pool
  //std::map<uint32_t, std::string>                                m_arLabels; ///< AR label pool

#ifdef AML
  //int cur_ref_index;
#ifdef USE_FULL_REF_LIST_BUFFER
  sps_RPL_set_t sps_RPL_set[16];
  ref_set_t slice_RPL0_set;
  ref_set_t slice_RPL1_set;
#else
  ref_set_t  RPL0_set[REF_SET_NUM];
  ref_set_t  RPL1_set[REF_SET_NUM];
#endif
#define CHROMAQPMAP_SIZE 76
  uint32_t chromaQPmap[CHROMAQPMAP_SIZE];
#endif
} DecApp;

void print_vvc_picture_list(DecLib *p_declib, PicList *rcListPic, unsigned char * mark);
Slice *h266_get_col_slice(DecApp *p_app, Slice* slice);

#endif
