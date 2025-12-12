MODULE_NAME = decoder_common
obj-m	+=	${MODULE_NAME}.o
${MODULE_NAME}-objs	+=	utils.o vdec.o vdec_input.o amvdec.o
${MODULE_NAME}-objs	+=	decoder_mmu_box.o decoder_bmmu_box.o
${MODULE_NAME}-objs	+=	config_parser.o vdec_profile.o
${MODULE_NAME}-objs	+=	amstream_profile.o
${MODULE_NAME}-objs	+=	frame_check.o amlogic_fbc_hook.o
${MODULE_NAME}-objs	+=	vdec_v4l2_buffer_ops.o
${MODULE_NAME}-objs	+=	vdec_sync.o
${MODULE_NAME}-objs	+=	vdec_power_ctrl.o
${MODULE_NAME}-objs	+=	vdec_canvas_utils.o
${MODULE_NAME}-objs	+=	vdec_feature.o
${MODULE_NAME}-objs	+=	vdec_ge2d_utils.o
${MODULE_NAME}-objs	+=	decoder_dma_alloc.o
${MODULE_NAME}-objs	+=	decoder_report.o
${MODULE_NAME}-objs	+=	dma_buf_mgr.o
${MODULE_NAME}-objs	+=	debug/dos_axi_monitor.o
${MODULE_NAME}-objs	+=	vdec_res/vdec_res.o vdec_res/vdec_res_priv.o
${MODULE_NAME}-objs	+=	vdec_res/h264_res.o vdec_res/h265_res.o vdec_res/h266_res.o
${MODULE_NAME}-objs	+=	vdec_res/av1_res.o vdec_res/vp9_res.o vdec_res/avs_res.o
${MODULE_NAME}-objs	+=	vdec_res/mpeg12_res.o vdec_res/mpeg4_res.o vdec_res/mjpeg_res.o
${MODULE_NAME}-objs	+=	vdec_res/avs2_res.o vdec_res/avs3_res.o vdec_res/vc1_res.o
${MODULE_NAME}-objs	+=	vdec_dw.o
${MODULE_NAME}-objs	+=	sec_extractor.o

MODULE_NAME = amvdec_debug_port
obj-m	+=	${MODULE_NAME}.o
${MODULE_NAME}-objs	+=	vdec_debug_port.o

PR_FMT = "amlcom"
PR_FMT_DEFINE="-Dpr_fmt(fmt)=\"[$(PR_FMT)]:\" fmt"
ccflags-y += $(PR_FMT_DEFINE)
