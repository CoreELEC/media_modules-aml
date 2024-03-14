#ifndef VVC_GLOBAL_H_
#define VVC_GLOBAL_H_

#define LINUX
#define AML
//#define SIMULATION

//!!make sure VVC_BUFFER_NUM <= 31, from number of  HEVC_MPRED_COL_REF_CANVAS_xx_POC
//#define REF_MAXBUFFER            (31-1)
#define REF_MAXBUFFER            (24-1)
//#define REF_MAXBUFFER            16
#define VVC_BUFFER_NUM               (REF_MAXBUFFER + 1)
#define PIC_POOL_SIZE   VVC_BUFFER_NUM

#define Y_C                                0  /* Y luma */
#define U_C                                1  /* Cb Chroma */
#define V_C                                2  /* Cr Chroma */
#define N_C                                3  /* number of color component */

#define COM_ST_UNKNOWN                  (0)
#define COM_ST_I                        (1)
#define COM_ST_P                        (2)
#define COM_ST_B                        (3)

#define SLICE_I                            COM_ST_I
#define SLICE_P                            COM_ST_P
#define SLICE_B                            COM_ST_B

typedef struct _ALF_PARAM
{
    int alf_flag;
    int num_coeff;
    int filters_per_group;
//#if ALF_IMP
//    int dir_index;
//    int max_filter_num;
//#endif
    int component_id;
#ifdef AML
    int32_t filter_pattern[16];
    int32_t coeff_multi[16][9];
#else
    int *filter_pattern;
    int **coeff_multi;
#endif
} ALF_PARAM;
typedef ALF_PARAM ALFParam;

typedef struct _ALF_CORR_DATA
{
    double ***E_corr; //!< auto-correlation matrix
    double  **y_corr; //!< cross-correlation
    double   *pix_acc;
    int       component_id;
} ALF_CORR_DATA;

#define MAX_SLICE_NUM 800
typedef struct PIC_s {
    int index;
    int used;
    int new_picture;
    uint32_t mmu_alloc_flag;
    uint32_t lcu_size_log2;
    uint32_t header_adr;
    uint32_t header_dw_adr;
    uint32_t mc_y_adr;
    uint32_t mc_u_v_adr;
    uint32_t mc_canvas_y;
    uint32_t mc_canvas_u_v;
    uint32_t mpred_mv_wr_start_addr;
    u8 bg_flag;
    uint32_t refered_by_others;
    uint32_t is_output;
    int poc;
    uint32_t width;
    uint32_t height;
    uint8_t depth;
    uint8_t slice_type;
    uint8_t is_ref;
    uint8_t mv_wr_en;
    int vf_ref;
    int backend_ref;
    uint8_t inter_slice_allowed_flag;
    uint8_t has_inter_slice;
    int canvas_poc_list_size;
    int canvas_poc_list[PIC_POOL_SIZE];
    uint32_t canvas_lt_flag;
    /**/
    //int index;
    int scatter_alloc;
    int BUF_index;
    int mv_buf_index;
    //int POC;
    int decode_idx;
    //int slice_type;
    int RefNum_L0;
    int RefNum_L1;
    int num_reorder_pic;
    unsigned int stream_offset;
    unsigned char referenced;
    unsigned char decode_done;
    unsigned char error_mark;
    int slice_idx;
    int m_aiRefPOCList0[MAX_SLICE_NUM][16];
    int m_aiRefPOCList1[MAX_SLICE_NUM][16];
#ifdef SUPPORT_LONG_TERM_RPS
    unsigned char long_term_ref;
    unsigned char m_aiRefLTflgList0[MAX_SLICE_NUM][16];
    unsigned char m_aiRefLTflgList1[MAX_SLICE_NUM][16];
#endif
    /*buffer */
    //unsigned int header_adr;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
    unsigned char dv_enhance_exist;
#endif
    char *aux_data_buf;
    int aux_data_size;
    unsigned long cma_alloc_addr;
    struct page *alloc_pages;
    //unsigned int mpred_mv_wr_start_addr;
    int mv_size;
    //unsigned int mc_y_adr;
    //unsigned int mc_u_v_adr;

#ifdef SUPPORT_10BIT
    /*unsigned int comp_body_size;*/
    unsigned int dw_y_adr;
    unsigned int dw_u_v_adr;
#endif
#ifdef USE_NV21_EXTRA_BUF
    unsigned int ext_y_adr;     //mc_y_4bit_adr
    unsigned int ext_uv_adr;    //mc_u_v_4bit_adr
#endif
    u32 luma_size;
    u32 chroma_size;

    //int mc_canvas_y;
    //int mc_canvas_u_v;
    //int width;
    //int height;
    int crop_w;
    int crop_h;

    int y_canvas_index;
    int uv_canvas_index;
#ifdef MULTI_INSTANCE_SUPPORT
    struct canvas_config_s canvas_config[2];
#endif
#ifdef SUPPORT_10BIT
    int mem_saving_mode;
    u32 bit_depth_luma;
    u32 bit_depth_chroma;
#endif
#ifdef LOSLESS_COMPRESS_MODE
    unsigned int losless_comp_body_size;
#endif
#ifdef H265_10B_MMU_DW
    //u32 header_dw_adr;
#endif
    unsigned char pic_struct;
    //int vf_ref;

    u32 pts;
    u64 pts64;
    u64 timestamp;

    u32 aspect_ratio_idc;
    u32 sar_width;
    u32 sar_height;
    u32 double_write_mode;
    u32 video_signal_type;
    unsigned short conformance_window_flag;
    unsigned short conf_win_left_offset;
    unsigned short conf_win_right_offset;
    unsigned short conf_win_top_offset;
    unsigned short conf_win_bottom_offset;
    unsigned short chroma_format_idc;

    /* picture qos information*/
    int max_qp;
    int avg_qp;
    int min_qp;
    int max_skip;
    int avg_skip;
    int min_skip;
    int max_mv;
    int min_mv;
    int avg_mv;

    u32 hw_decode_time;
    u32 frame_size; // For frame base mode
    bool ip_mode;
    u32 hdr10p_data_size;
    char *hdr10p_data_buf;
    struct dma_fence *fence;
    bool show_frame;
    u32 sei_present_flag;
    struct hevc_state_s *hevc;
} vvc_frame_t;


#define free vfree
#define malloc vmalloc

/******************************************************************************
 * CONTEXT used for decoding process.
 *
 * All have to be stored are in this structure.
 *****************************************************************************/


#define PICTURE_START_CODE      0xB3
#define I_PICTURE_START_CODE    0xB5
#define PB_PICTURE_START_CODE   0xB6
#define SLICE_START_CODE_MIN    0x00
#define SLICE_START_CODE_MAX    0x8F
#define USER_DATA_START_CODE    0xB2
#define SEQUENCE_HEADER_CODE    0xB0
#define EXTENSION_START_CODE    0xB5
#define SEQUENCE_END_CODE       0xB1
#define VIDEO_EDIT_CODE         0xB7

enum ALFComponentID {
    ALF_Y = 0,
    ALF_Cb,
    ALF_Cr,
    NUM_ALF_COMPONENT
};
typedef struct {
    //int32_t typeb;
    //int32_t type;
    //int32_t tr;                                     //<! temporal reference, 8 bit,
    int32_t width;                   //!< Number of pels
    int32_t height;                  //!< Number of lines
    int32_t num_of_references;
    int32_t            pic_alf_on[NUM_ALF_COMPONENT];
    //int32_t is_field_sequence;
    //int32_t is_top_field;
    int number;
} ImageParameters;

struct inp_par {
    uint32_t sample_bit_depth;
    uint32_t alf_enable;
};


#ifdef FRONT_BACK_SUPPORT
//new dual
#define MAX_FB_IFBUF_NUM             3
typedef struct
{
    uint32_t buf_start;
    uint32_t buf_size;
    uint32_t buf_end;
} buff_t;

typedef struct {
    uint32_t mmu0_ptr;
    uint32_t mmu1_ptr;
    uint32_t scalelut_ptr;
    uint32_t vcpu_imem_ptr;
    uint32_t sys_imem_ptr;
    uint32_t lmem0_ptr;
    uint32_t lmem1_ptr;
    uint32_t parser_sao0_ptr;
    uint32_t parser_sao1_ptr;
    uint32_t mpred_imp0_ptr;
    uint32_t mpred_imp1_ptr;
    //
    uint32_t scalelut_ptr_pre;
} buff_ptr_t;
#endif

typedef struct vvc_decoder {
    DecApp m_decApp;
    uint8_t init_hw_flag;
    struct inp_par   input;
    ImageParameters  img;
    //Video_Com_data  hc;
    //Video_Dec_data  hd;
    //union param_u param;
    //vvc_frame_t *fref[REF_MAXBUFFER];
#ifdef AML
    /*used for background
    when background_picture_output_flag is 0*/
    //vvc_frame_t *m_bg;
    /*current background picture, ether m_bg or fref[..]*/
    vvc_frame_t *f_bg;
#endif
    //outdata outprint;
    uint32_t cm_header_start;
    ALF_PARAM m_alfPictureParam[N_C];
    ALF_PARAM *p_alfPictureParam[N_C];
/*#ifdef FIX_CHROMA_FIELD_MV_BK_DIST*/
    int8_t bk_img_is_top_field;
/*#endif*/
#ifdef AML
    int32_t lcu_size;
    int vvc_monochrome;
    int32_t lcu_size_log2;
    int32_t lcu_x_num;
    int32_t lcu_y_num;
    int32_t lcu_total;
    uint32_t misc_flag0;
#endif
    u8 *ref_list_buf_v;
    union param_u param;;
    //DEC_CTX ctx;
    //DEC_STAT stat;
    vvc_frame_t  pic_pool[PIC_POOL_SIZE];
    //LibVCData libvc_data;

    uint32_t slice_addr;
    unsigned int dec_status;
    vvc_frame_t *cur_pic;
    vvc_frame_t *col_pic;
    u8 slice_type;
    u8 seq_change_flag;
    int decode_id;
/**/
#ifdef FRONT_BACK_SUPPORT
    uint8_t wait_working_buf;
    /*FB mgr*/
    uint8_t fb_wr_pos;
    uint8_t fb_rd_pos;
    buff_t fb_buf_mmu0;
    buff_t fb_buf_mmu1;
    buff_t fb_buf_scalelut;
    buff_t fb_buf_vcpu_imem;
    buff_t fb_buf_sys_imem;
    buff_t fb_buf_lmem0;
    buff_t fb_buf_lmem1;
    buff_t fb_buf_parser_sao0;
    buff_t fb_buf_parser_sao1;
    buff_t fb_buf_mpred_imp0;
    buff_t fb_buf_mpred_imp1;
    uint32_t frontend_decoded_count;
    uint32_t backend_decoded_count;
    buff_ptr_t fr;
    buff_ptr_t bk;
    buff_ptr_t next_bk[MAX_FB_IFBUF_NUM];
    vvc_frame_t* next_be_decode_pic[MAX_FB_IFBUF_NUM];
    /**/
#endif
} vvc_decoder_t;

static void pic_buf_cfg_free(vvc_frame_t *pic);
static vvc_frame_t * pic_buf_cfg_alloc(struct hevc_state_s *hevc);
static int prepare_display_buf(void* hw, struct PIC_s *pic);

#endif

#if 1 //def USE_OLD_CHIP
#define HEVC_MV_INFO   0x310d
#define HEVC_QP_INFO   0x3137
#define HEVC_SKIP_INFO 0x3136

#define AV1D_MPP_ORDERHINT_CFG                     0x3493

#if 0

#define VVC_MPP_REF0_POC_CFG                       0x34b0
#define VVC_MPP_REF1_POC_CFG                       0x34b1

#define VVC_MPP_REF_IS_LONGTERM                    0x34b2
#define HEVC_MPRED_COL_REF_CANVAS_00_POC           0x326a
#define HEVC_MPRED_COL_REF_CANVAS_01_POC           0x326b
#define HEVC_MPRED_COL_REF_CANVAS_02_POC           0x326c
#define HEVC_MPRED_COL_REF_CANVAS_03_POC           0x326d
#define HEVC_MPRED_COL_REF_CANVAS_04_POC           0x326e
#define HEVC_MPRED_COL_REF_CANVAS_05_POC           0x326f
#define HEVC_MPRED_COL_REF_CANVAS_06_POC           0x3270
#define HEVC_MPRED_COL_REF_CANVAS_07_POC           0x3271
#define HEVC_MPRED_COL_REF_CANVAS_08_POC           0x3272
#define HEVC_MPRED_COL_REF_CANVAS_09_POC           0x3273
#define HEVC_MPRED_COL_REF_CANVAS_10_POC           0x3274
#define HEVC_MPRED_COL_REF_CANVAS_11_POC           0x3275
#define HEVC_MPRED_COL_REF_CANVAS_12_POC           0x3276
#define HEVC_MPRED_COL_REF_CANVAS_13_POC           0x3277
#define HEVC_MPRED_COL_REF_CANVAS_14_POC           0x3278
#define HEVC_MPRED_COL_REF_CANVAS_15_POC           0x3279
#define HEVC_MPRED_COL_REF_CANVAS_16_POC           0x327a
#define HEVC_MPRED_COL_REF_CANVAS_17_POC           0x327b
#define HEVC_MPRED_COL_REF_CANVAS_18_POC           0x327c
#define HEVC_MPRED_COL_REF_CANVAS_19_POC           0x327d
#define HEVC_MPRED_COL_REF_CANVAS_20_POC           0x327e
#define HEVC_MPRED_COL_REF_CANVAS_21_POC           0x327f
#define HEVC_MPRED_COL_REF_CANVAS_22_POC           0x3280
#define HEVC_MPRED_COL_REF_CANVAS_23_POC           0x3281
#define HEVC_MPRED_COL_REF_CANVAS_24_POC           0x3282
#define HEVC_MPRED_COL_REF_CANVAS_25_POC           0x3283
#define HEVC_MPRED_COL_REF_CANVAS_26_POC           0x3284
#define HEVC_MPRED_COL_REF_CANVAS_27_POC           0x3285
#define HEVC_MPRED_COL_REF_CANVAS_28_POC           0x3286
#define HEVC_MPRED_COL_REF_CANVAS_29_POC           0x3287
#define HEVC_MPRED_COL_REF_CANVAS_30_POC           0x3288
#define HEVC_MPRED_COL_REF_CANVAS_LT               0x3289

#define VVC_MPP_REF0_POC_CFG                       0x34b0
#define VVC_MPP_REF1_POC_CFG                       0x34b1
#define VVC_MPP_REF_IS_LONGTERM                    0x34b2
#define VVC_MPP_CHROMA_COLLOCATED_CFG              0x34b3
#define VVC_MPP_REF_WRAPAROUND_CFG                 0x34b4
#define VVC_MPP_SUBPIC_START                       0x34b5
#define VVC_MPP_SUBPIC_SIZE                        0x34b6
#define VVC_MPP_SCALING_WIN_OFFSET                 0x34b7
#define VVC_MPP_CURR_PROF_ENABLE                   0x34b8
#define VVC_MPP_CANVAS_ID_L0                       0x34b9
#define VVC_MPP_CANVAS_ID_L1                       0x34ba
#define VVC_MPP_MV_WRPTR                           0x34bb
#define VVC_MPP_SLICE_INFO                         0x34bc
#define VVC_MPP_AXI_CTL                            0x34bd
#define VVC_MPP_LCU_INFO                           0x34be
#define VVC_MPP_RPR_REFINFO                        0x34bf

#define HEVC_DBLK_VBVER                            0x3548
#define HEVC_DBLK_VBVER1                           0x3549
#define HEVC_DBLK_VBHOR                            0x354a
#define HEVC_DBLK_VBHOR1                           0x354b
#define HEVC_DBLK_SUBPIC                           0x354c
#define HEVC_DBLK_EOT                              0x354d
#endif

#define HEVC_DBLK_DBLK0                            0x3523
#define HEVC_DBLK_DBLK1                            0x3524
#define HEVC_DBLK_DBLK2                            0x3525
#define HEVC_DBLK_ALF0                             0x3544
#define AV1_GMC_PARAM_BUFF_ADDR                    0x316d
#define HEVC_SHIFT_LENGTH_PROTECT                  0x313a

#define HEVC_MPRED_CTRL4                           0x324c

#define HEVC_IQIT_QP_CHROMA_MAP_WADDR              0x373c
#define HEVC_IQIT_QP_CHROMA_MAP_RADDR              0x373d
#define HEVC_IQIT_QP_CHROMA_MAP_DATA               0x373e

#endif
