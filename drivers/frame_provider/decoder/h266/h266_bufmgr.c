#include "h266_global.h"
#define MODIFY_CODE

#define fill_n(a, n, v) \
	{ \
		int fill_i; \
		for (fill_i = 0;fill_i < n;fill_i++) \
			a[fill_i]=v; \
	}

void cvector_init(cvector *v, unsigned int typeSize)
{
	v->_data = NULL;
	v->_size = 0;
	v->_typeSize = typeSize;
	v->_extra = 0;
	v->extra = 10;
}

void cvector_allocate(cvector* v, unsigned int additionalSize)
{
#if 0
	void* newData = realloc(v->_data, v->_typeSize * (v->_size + additionalSize));
	if (newData == NULL)
		free(v->_data);
	else
		v->_extra = additionalSize;
	v->_data = newData;
#else
	void *newData = malloc(v->_typeSize * (v->_size + additionalSize));
	if (newData == NULL)
		free(v->_data);
	else {
		if (v->_data) {
		  memcpy(newData, v->_data, v->_typeSize * v->_size);
		  free(v->_data);
		}
		v->_extra = additionalSize;
	}
	v->_data = newData;
#endif
}

void cvector_deallocate(cvector* v, unsigned int substractSize)
{
	void* newData;
	if (v->_size - substractSize < 1) {
		free(v->_data);
		v->_size = 0;
		v->_extra = 0;
		v->_data = NULL;

		return;
	}
#if 0
	newData = realloc(v->_data, v->_typeSize * (v->_size - substractSize + v->extra));

	if (newData == NULL)
		free(v->_data);
#else
	newData = malloc(v->_typeSize * (v->_size - substractSize + v->extra));
	if (newData == NULL)
		free(v->_data);
	else {
		memcpy(newData, v->_data, v->_typeSize * (v->_size - substractSize));
		free(v->_data);
	}
#endif
	v->_data = newData;
	v->_size -= substractSize;
}

void cvector_clear(cvector* v)
{
	cvector_deallocate(v, v->_size);
}

void cvector_push(cvector* v, void* element)
{
	if (v->_extra < 1)
		cvector_allocate(v, v->extra);
	memcpy(v->_size * v->_typeSize + (unsigned char*)v->_data, element, v->_typeSize);
	v->_size++;
	v->_extra--;
}
/*
void cvector_pushi(cvector* v, long int number)
{
	cvector_push(v, &number);
}

void cvector_pop(cvector* v)
{
	if (v->_size < 1)
		return;
	cvector_deallocate(v, 1);
	v->_extra++;
}
*/
void* cvector_get(const cvector* v, unsigned int index) {
	if (v->_size < 1 || v->_size <= index)
		return NULL;
	return (void*)((unsigned char*)v->_data + index * v->_typeSize);
}

int cvector_size(cvector* v)
{
	return v->_size;
}
/*
void cvector_insert(cvector* v, unsigned int index, void* element) {
  if (v->_extra < 1)
    cvector_allocate(v, v->extra);

  memmove((unsigned char*)v->_data + index * v->_typeSize + v->_typeSize, (unsigned char*)v->_data + index * v->_typeSize, (v->_size - index) * v->_typeSize);
  memcpy((unsigned char*)v->_data + index * v->_typeSize, element, v->_typeSize);
}

void cvector_inserti(cvector* v, unsigned int index, long int number) {
  cvector_insert(v, index, &number);
}

void cvector_insertf(cvector* v, unsigned int index, float number) {
  cvector_insert(v, index, &number);
}

void cvector_insertd(cvector* v, unsigned int index, double number) {
  cvector_insert(v, index, &number);
}*/
#if 0
void* cvector_remove(cvector* v, unsigned int index) {

}
#endif
bool PicList_empty(PicList *p_list)
{
	int i;
	for (i = 0; i < PIC_LIST_SIZE; i++) {
		if (p_list->pic[i])
			return 0;
	}
	return 1;
}

void PicList_clear(PicList *p_list)
{
	int i;
	for (i = 0; i < PIC_LIST_SIZE; i++)
		p_list->pic[i] = NULL;
}

int PicList_size(PicList *p_list)
{
	int i;
	for (i = 0; i < PIC_LIST_SIZE; i++) {
	if (p_list->pic[i] == NULL)
		break;
	}
	return i;
}

int PicList_push(PicList *p_list, Picture *new_pic)
{
	int i;
	for (i = 0; i < PIC_LIST_SIZE; i++) {
		if (p_list->pic[i] == NULL) {
			p_list->pic[i] = new_pic;
			break;
		}
	}
	return 0;
}

void PicList_remove_null(PicList *p_list)
{

}

void sortPicList(PicList *p_list)
{
  //picList.sort([](Picture *const &a, Picture *const &b) {
  //  return a->poc < b->poc || (a->poc == b->poc && a->layerId < b->layerId);
  //});
	int i, j;
	for (i = 0; i < PIC_LIST_SIZE; i++) {
		if (p_list->pic[i] == NULL)
			break;
		for (j = i + 1; j < PIC_LIST_SIZE; j++) {
		if (p_list->pic[j] == NULL)
			break;
		if (p_list->pic[i]->poc > p_list->pic[j]->poc ||
		(p_list->pic[i]->poc == p_list->pic[j]->poc &&
		p_list->pic[i]->layerId > p_list->pic[j]->layerId)) {
				Picture * tmp_pic = p_list->pic[i];
				p_list->pic[i] = p_list->pic[j];
				p_list->pic[j] = tmp_pic;
			}
		}
	}
}

Picture *new_picture(void)
{
	Picture* pic = malloc(sizeof(Picture));
	memset(pic, 0, sizeof(Picture));
	pic->temporalId           = MAX_UINT; //std::numeric_limits<uint32_t>::max();
	pic->edrapRapId           = -1;

	/*for ( int i = 0; i < MAX_NUM_CHANNEL_TYPE; i++ ) {
		pic->m_prevQP[i] = -1;
	}*/
	pic->layerId = NOT_VALID;
	pic->numSlices = 1;
#ifdef AML
	pic->poc = 0x7fffffff;
#endif
	return pic;
}

void free_picture(Picture *pic)
{
#ifdef AML
	if (pic->buf_cfg) {
		put_mv_buf(pic->buf_cfg);
		pic->buf_cfg->referenced = 0;
		//  pic_buf_cfg_free(pic->buf_cfg);
		pic->buf_cfg = NULL;
	}
#endif
	free(pic);
}

bool getWindowEnabledFlag(const Window *win) { return win->m_enabledFlag;                          }
static int  getWindowLeftOffset(const Window *win)  { return win->m_enabledFlag ? win->m_winLeftOffset : 0;    }
void setWindowLeftOffset(Window *win, int val)   { win->m_winLeftOffset = val; win->m_enabledFlag |=  (val!=0);   }
static int  getWindowRightOffset(const Window *win) { return win->m_enabledFlag ? win->m_winRightOffset : 0;   }
void setWindowRightOffset(Window *win, int val)  { win->m_winRightOffset = val; win->m_enabledFlag |= (val!=0);  }
static int  getWindowTopOffset(const Window *win) { return win->m_enabledFlag ? win->m_winTopOffset : 0;     }
void setWindowTopOffset(Window *win, int val)    { win->m_winTopOffset = val; win->m_enabledFlag |= (val!=0);    }
static int  getWindowBottomOffset(const Window *win) { return win->m_enabledFlag ? win->m_winBottomOffset: 0;   }
void setWindowBottomOffset(Window *win, int val) { win->m_winBottomOffset = val; win->m_enabledFlag |= (val!=0); }

static void setWindow(Window *win, int offsetLeft, int offsetRight, int offsetTop, int offsetBottom)
{
	win->m_enabledFlag     = (offsetLeft || offsetRight || offsetTop || offsetBottom);
	win->m_winLeftOffset   = offsetLeft;
	win->m_winRightOffset  = offsetRight;
	win->m_winTopOffset    = offsetTop;
	win->m_winBottomOffset = offsetBottom;
}

#ifdef AML
SPS *new_sps(void)
{
	int i, ch;
	SPS *sps = malloc(sizeof(SPS));
	memset(sps, 0, sizeof(SPS));
	sps->m_chromaFormatIdc            = CHROMA_420;
	sps->m_uiMaxTLayers               =   1;
	sps->m_ptlDpbHrdParamsPresentFlag  = 1;
	// Structure
	sps->m_maxWidthInLumaSamples      = 352;
	sps->m_maxHeightInLumaSamples     = 288;
	sps->m_numSubPics = 1;
	sps->m_subPicIdLen = 16;
	sps->m_log2MinCodingBlockSize     =   2;
	//sps->m_maxMTTHierarchyDepth{ MAX_BT_DEPTH, MAX_BT_DEPTH_INTER, MAX_BT_DEPTH_C }
	sps->m_uiMaxCUWidth               =  32;
	sps->m_uiMaxCUHeight              =  32;
	sps->m_allRplEntriesHasSameSignFlag  =  true ;
	// Tool list
	sps->m_log2MaxTransformSkipBlockSize  = 2;
	sps->m_uiBitsForPOC               =   8;
	sps->m_pocMsbCycleLen             =  1 ;

	sps->m_log2MaxTbSize              =   6;

	//sps->m_vuiParameters             ()
	sps->m_horCollocatedChromaFlag    =  true ;
	sps->m_GDREnabledFlag             =  true ;
	sps->m_SubLayerCbpParametersPresentFlag  =  true ;
	sps->m_maxNumMergeCand = MRG_MAX_NUM_CANDS;
	sps->m_maxNumAffineMergeCand = AFFINE_MRG_MAX_NUM_CANDS;
	sps->m_maxNumIBCMergeCand = IBC_MRG_MAX_NUM_CANDS;
	sps->m_scalingMatrixDesignatedColourSpaceFlag =  true ;
	sps->m_disableScalingMatrixForLfnstBlks =  true;
	for (ch = 0; ch < MAX_NUM_CHANNEL_TYPE; ch++) {
		sps->m_bitDepths.recon[ch] = 8;
	}

	for (i = 0; i < MAX_TLAYER; i++ ) {
		sps->m_uiMaxDecPicBuffering[i] = 1;
	}

	return sps;
}

PPS *new_pps(void)
{
	PPS *pps = malloc(sizeof(PPS));
	memset(pps, 0, sizeof(PPS));

	pps->m_numRefIdxL0DefaultActive         = 1;
	pps->m_numRefIdxL1DefaultActive         = 1;
	pps->m_numSubPics                       = 1;
	pps->m_subPicIdLen                      = 16;
	pps->m_noPicPartitionFlag               = 1;
	pps->m_numTileCols                      = 1;
	pps->m_numTileRows                      = 1;
	pps->m_rectSliceFlag                    = 1;
	pps->m_numSlicesInPic                   = 1;
	pps->m_loopFilterAcrossTilesEnabledFlag = 1;
	pps->m_picWidthInLumaSamples            = 352;
	pps->m_picHeightInLumaSamples           =  288;

	return pps;
}

VPS *new_vps(void)
{
	int i,j;
	VPS *vps = malloc(sizeof(VPS));
	memset(vps, 0, sizeof(VPS));

	vps->m_maxLayers = 1;
	vps->m_vpsMaxSubLayers = 7;
	vps->m_vpsDefaultPtlDpbHrdMaxTidFlag  = true;
	vps->m_vpsAllIndependentLayersFlag = true;
	vps->m_vpsEachLayerIsAnOlsFlag  = 1;
	vps->m_vpsNumOutputLayerSets  = 1;
	vps->m_vpsNumPtls  = 1;
	vps->m_totalNumOLSs = 1;
	for (i = 0; i < MAX_VPS_LAYERS; i++) {
		vps->m_vpsIndependentLayerFlag[i] = true;
		for (j = 0; j < MAX_VPS_LAYERS; j++) {
			vps->m_directRefLayerIdx[i][j] = MAX_VPS_LAYERS;
			vps->m_interLayerRefIdx[i][i] = NOT_VALID;
		}
	}
	for (i = 0; i < MAX_NUM_OLSS; i++) {
		if (i == 0) {
			vps->m_ptPresentFlag[i] = 1;
		}
		vps->m_ptlMaxTemporalId[i] = vps->m_vpsMaxSubLayers - 1;
		vps->m_hrdMaxTid[i] = vps->m_vpsMaxSubLayers - 1;
	}

	cvector_init(&vps->m_olsDpbPicSize, sizeof(Size));
	cvector_init(&vps->m_olsDpbParamsIdx, sizeof(int));
	cvector_init(&vps->m_outputLayerIdInOls, sizeof(int));
	cvector_init(&vps->m_numSubLayersInLayerInOLS, sizeof(int));

	cvector_init(&vps->m_multiLayerOlsIdxToOlsIdx, sizeof(int));
	// m_multiLayerOlsIdxToOlsIdx[n] is the OLSidx of the n-th multi-layer OLS.
	//cvector_init(&vps->m_olsHrdParams, sizeof(OlsHrdParams));
	cvector_init(&vps->m_dpbParameters, sizeof(DpbParameters));
	cvector_init(&vps->m_dpbMaxTemporalId, sizeof(int));
	cvector_init(&vps->m_targetOutputLayerIdSet, sizeof(int));
	cvector_init(&vps->m_targetLayerIdSet, sizeof(int));
	cvector_init(&vps->m_numOutputLayersInOls, sizeof(int));
	cvector_init(&vps->m_numLayersInOls, sizeof(int));
	cvector_init(&vps->m_layerIdInOls, sizeof(int));
	cvector_init(&vps->m_olsDpbChromaFormatIdc, sizeof(int));
	cvector_init(&vps->m_olsDpbBitDepthMinus8, sizeof(int));

	return vps;
}

#endif

const char* nalUnitTypeToString(NalUnitType type)
{
	switch (type)
	{
		case NAL_UNIT_CODED_SLICE_TRAIL:      return "TRAIL";
		case NAL_UNIT_CODED_SLICE_STSA:       return "STSA";
		case NAL_UNIT_CODED_SLICE_RADL:       return "RADL";
		case NAL_UNIT_CODED_SLICE_RASL:       return "RASL";
		case NAL_UNIT_CODED_SLICE_IDR_W_RADL: return "IDR_W_RADL";
		case NAL_UNIT_CODED_SLICE_IDR_N_LP:   return "IDR_N_LP";
		case NAL_UNIT_CODED_SLICE_CRA:        return "CRA";
		case NAL_UNIT_CODED_SLICE_GDR:        return "GDR";
		case NAL_UNIT_OPI:                    return "OPI";
		case NAL_UNIT_DCI:                    return "DCI";
		case NAL_UNIT_VPS:                    return "VPS";
		case NAL_UNIT_SPS:                    return "SPS";
		case NAL_UNIT_PPS:                    return "PPS";
		case NAL_UNIT_PREFIX_APS:             return "Prefix APS";
		case NAL_UNIT_SUFFIX_APS:             return "Suffix APS";
		case NAL_UNIT_PH:                     return "PH";
		case NAL_UNIT_ACCESS_UNIT_DELIMITER:  return "AUD";
		case NAL_UNIT_EOS:                    return "EOS";
		case NAL_UNIT_EOB:                    return "EOB";
		case NAL_UNIT_PREFIX_SEI:             return "Prefix SEI";
		case NAL_UNIT_SUFFIX_SEI:             return "Suffix SEI";
		case NAL_UNIT_FD:                     return "FD";
		default:                              return "UNK";
	}
}




DecApp mcDecApp;

Slice slice_pool[128];
int slice_count = 0;
Slice * get_slice_from_pool(void)
{
	return &slice_pool[slice_count++];
}

void initSlice(Slice *slice)
{
	uint32_t component;
	uint32_t i;
	for (i = 0; i < NUM_REF_PIC_LIST_01; i++) {
		slice->m_aiNumRefIdx[i]      = 0;
	}
	slice->m_colFromL0Flag = true;
	slice->m_colRefIdx = 0;
	slice->m_lmcsEnabledFlag = 0;
	slice->m_explicitScalingListUsed = 0;
	//initEqualRef();

	slice->m_noOutputOfPriorPicsFlag = 0;

	slice->m_bCheckLDC = false;

	slice->m_biDirPred = false;
	slice->m_symRefIdx[0] = -1;
	slice->m_symRefIdx[1] = -1;

	for (component = 0; component < MAX_NUM_COMPONENT; component++) {
		slice->m_iSliceChromaQpDelta[component] = 0;
	}
	slice->m_iSliceChromaQpDelta[JOINT_CbCr] = 0;

	slice->m_bFinalized=false;

	//slice->m_substreamSizes.clear();
	slice->m_cabacInitFlag        = false;
	slice->m_enableDRAPSEI        = false;
	slice->m_useLTforDRAP         = false;
	slice->m_isDRAP               = false;
	slice->m_latestDRAPPOC        = MAX_INT;
	slice->m_edrapRapId           = 0;
	slice->m_enableEdrapSEI       = false;
	slice->m_edrapRapId           = 0;
	slice->m_useLTforEdrap        = false;
	slice->m_edrapNumRefRapPics   = 0;
	//slice->m_edrapRefRapIds.resize(0);
	slice->m_latestEDRAPPOC       = MAX_INT;
	slice->m_latestEdrapLeadingPicDecodableFlag = false;
	//resetAlfEnabledFlag();
	//m_ccAlfFilterParam.reset();
	//m_ccAlfCbEnabledFlag = 0;
	//m_ccAlfCrEnabledFlag = 0;
	//m_ccAlfCbApsId = -1;
	//m_ccAlfCrApsId = -1;
	slice->m_nuhLayerId = 0;
}

Slice *new_slice(void)
{
	int idx;
	//int iDir, iRefIdx1, iRefIdx2;
	//Slice *p_slice = get_slice_from_pool();
	Slice *p_slice = malloc(sizeof(Slice)); //to do: to free memory
	memset(p_slice, 0, sizeof(Slice));
	p_slice->m_prevGDRInSameLayerPOC         =  -MAX_INT;
	p_slice->m_iAssociatedIRAPType           =  NAL_UNIT_INVALID;
	p_slice->m_prevGDRSubpicPOC              =  -MAX_INT;
	p_slice->m_prevIRAPSubpicPOC             =  -MAX_INT;
	p_slice->m_prevIRAPSubpicType            =  NAL_UNIT_INVALID;
	p_slice->m_rpl0Idx                       =  -1;
	p_slice->m_rpl1Idx                       =  -1;
	p_slice->m_eNalUnitType                  =  NAL_UNIT_CODED_SLICE_IDR_W_RADL;
	p_slice->m_eSliceType                    =  I_SLICE;
	p_slice->m_colFromL0Flag                 =  true;
	//p_slice->m_substreamSizes())
	p_slice->m_encCABACTableIdx              = I_SLICE;

#if 0
	//initEqualRef();
	for (iDir = 0; iDir < NUM_REF_PIC_LIST_01; iDir++) {
		for (iRefIdx1 = 0; iRefIdx1 < MAX_NUM_REF; iRefIdx1++) {
			for (iRefIdx2 = iRefIdx1; iRefIdx2 < MAX_NUM_REF; iRefIdx2++) {
				p_slice->m_abEqualRef[iDir][iRefIdx1][iRefIdx2] =
				p_slice->m_abEqualRef[iDir][iRefIdx2][iRefIdx1] = (iRefIdx1 == iRefIdx2? true : false);
			}
		}
	}
	//
#endif
	for (idx = 0; idx < MAX_NUM_REF; idx++ ) {
		p_slice->m_list1IdxToList0Idx[idx] = -1;
	}
#if 0
	resetWpScaling();
	initWpAcDcParam();

	m_ccAlfFilterParam.reset();
	resetAlfEnabledFlag();
	resetCcAlCbfEnabledFlag();
	resetCcAlCrfEnabledFlag();

	m_sliceMap.initSliceMap();
#endif
	return p_slice;
}

void initPicHeader(PicHeader *head)
{
#ifdef AML
	memset(head, 0, sizeof(PicHeader));
#endif
	head->m_valid                                         = 0;
	head->m_nonReferencePictureFlag                       = 0;
	head->m_gdrPicFlag                                    = 0;
	head->m_recoveryPocCnt                                = -1;
	head->m_spsId                                         = -1;
	head->m_ppsId                                         = -1;
	head->m_pocMsbPresentFlag                             = 0;
	head->m_pocMsbVal                                     = 0;
	head->m_virtualBoundariesEnabledFlag                  = 0;
	head->m_virtualBoundariesPresentFlag                  = 0;
	head->m_numVerVirtualBoundaries                       = 0;
	head->m_numHorVirtualBoundaries                       = 0;
	head->m_picOutputFlag                                 = true;
	head->m_rpl0Idx                                       = 0;
	head->m_rpl1Idx                                       = 0;
	head->m_splitConsOverrideFlag                         = 0;
	head->m_cuQpDeltaSubdivIntra                          = 0;
	head->m_cuQpDeltaSubdivInter                          = 0;
	head->m_cuChromaQpOffsetSubdivIntra                   = 0;
	head->m_cuChromaQpOffsetSubdivInter                   = 0;
	head->m_enableTMVPFlag                                = true;
	head->m_picColFromL0Flag                              = true;
	head->m_mvdL1ZeroFlag                                 = 0;
	head->m_maxNumAffineMergeCand                         = AFFINE_MRG_MAX_NUM_CANDS;
	head->m_disFracMMVD                                   = 0;
	head->m_bdofDisabledFlag                              = 0;
	head->m_dmvrDisabledFlag                              = 0;
	head->m_profDisabledFlag                              = 0;
	head->m_jointCbCrSignFlag                             = 0;
	head->m_qpDelta                                       = 0;
	head->m_numAlfApsIdsLuma                              = 0;
	head->m_alfApsIdChroma                                = 0;
	head->m_deblockingFilterOverrideFlag                  = 0;
	head->m_deblockingFilterDisable                       = 0;
	head->m_deblockingFilterBetaOffsetDiv2                = 0;
	head->m_deblockingFilterTcOffsetDiv2                  = 0;
	head->m_deblockingFilterCbBetaOffsetDiv2              = 0;
	head->m_deblockingFilterCbTcOffsetDiv2                = 0;
	head->m_deblockingFilterCrBetaOffsetDiv2              = 0;
	head->m_deblockingFilterCrTcOffsetDiv2                = 0;
	head->m_lmcsEnabledFlag                               = 0;
	head->m_lmcsApsId                                     = -1;
	//head->m_lmcsAps                                       = nullptr;
	head->m_lmcsChromaResidualScaleFlag                   = 0;
	head->m_explicitScalingListEnabledFlag                = 0;
	head->m_scalingListApsId                              = -1;
	//head->m_scalingListAps                                = nullptr;
	head->m_numL0Weights                                  = 0;
	head->m_numL1Weights                                  = 0;
#ifdef TO_DO
	memset(m_virtualBoundariesPosX,                   0,    sizeof(m_virtualBoundariesPosX));
	memset(m_virtualBoundariesPosY,                   0,    sizeof(m_virtualBoundariesPosY));
	memset(m_saoEnabledFlag,                          0,    sizeof(m_saoEnabledFlag));
	memset(m_alfEnabledFlag,                          0,    sizeof(m_alfEnabledFlag));
	memset(m_minQT,                                   0,    sizeof(m_minQT));
	memset(m_maxMTTHierarchyDepth,                    0,    sizeof(m_maxMTTHierarchyDepth));
	memset(m_maxBTSize,                               0,    sizeof(m_maxBTSize));
	memset(m_maxTTSize,                               0,    sizeof(m_maxTTSize));
	m_RPL0.setNumberOfActivePictures(0);
	m_RPL0.setNumberOfShorttermPictures(0);
	m_RPL0.setNumberOfLongtermPictures(0);
	m_RPL0.setLtrpInSliceHeaderFlag(0);

	m_RPL1.setNumberOfActivePictures(0);
	m_RPL1.setNumberOfShorttermPictures(0);
	m_RPL1.setNumberOfLongtermPictures(0);
	m_RPL1.setLtrpInSliceHeaderFlag(0);
	m_alfApsIdsLuma.resize(0);
#endif
#if GDR_ENABLED
	head->m_inGdrInterval      = false;
	head->m_lastGdrIntervalPoc = -1;
#endif
}

void xCreateDecLib(DecApp * p_app)
{
	DecLib *p_declib = &p_app->m_cDecLib;
	//initROM();

	// create decoder class
	//m_cDecLib.create();
	p_declib->m_apcSlicePilot = new_slice();
	p_declib->m_uiSliceSegmentIdx = 0;
	//

	// initialize decoder class
	//m_cDecLib.init(
	//#if JVET_J0090_MEMORY_BANDWIDTH_MEASURE
	//  m_cacheCfgFile
	//#endif
	//);
	//p_declib->m_cSliceDecoder.init(m_CABACDecoder, m_cCuDecoder );
	//#if JVET_J0090_MEMORY_BANDWIDTH_MEASURE
	//m_cacheModel.create( cacheCfgFileName );
	//m_cacheModel.clear( );
	//m_cInterPred.cacheAssign( &m_cacheModel );
	//#endif

	p_declib->m_decodedPictureHashSEIEnabled = p_app->m_decodedPictureHashSEIEnabled;

#if 0
	if (!m_outputDecodedSEIMessagesFilename.empty())
	{
	std::ostream &os=m_seiMessageFileStream.is_open() ? m_seiMessageFileStream : std::cout;
	m_cDecLib.setDecodedSEIMessageOutputStream(&os);
	}
#if JVET_S0257_DUMP_360SEI_MESSAGE
	if (!m_outputDecoded360SEIMessagesFilename.empty())
	{
	m_cDecLib.setDecoded360SEIMessageFileName(m_outputDecoded360SEIMessagesFilename);
	}
#endif
#endif
	p_declib->m_targetSubPicIdx = p_app->m_targetSubPicIdx;
	//m_cDecLib.initScalingList();
#if GDR_LEAK_TEST
	p_declib->m_gdrPocRandomAccess = p_app->m_gdrPocRandomAccess;
#endif // GDR_LEAK_TEST
}

void init_dec(DecApp * p_app)
	{
	int i;
	DecLib *p_declib = &p_app->m_cDecLib;
	//DecAppCfg
	memset(p_app, 0, sizeof(DecApp));
	//p_app->m_outputColourSpaceConvert = IPCOLOURSPACE_UNCHANGED;
	p_app->m_iMaxTemporalLayer = -1;
	//p_app->m_colourRemapSEIFileName()
	//p_app->m_SEICTIFileName()
	//p_app->m_annotatedRegionsSEIFileName()
	//p_app->m_targetDecLayerIdSet()
	//p_app->m_outputDecodedSEIMessagesFilename()
	//#if JVET_S0257_DUMP_360SEI_MESSAGE
	//m_outputDecoded360SEIMessagesFilename()
	//#endif

	//DecApp
	p_app->m_iPOCLastDisplay = -MAX_INT;
	for (i = 0; i < MAX_NUM_LAYER_IDS; i++) {
		p_app->m_newCLVS[i] = true;
	}
	for (i = 0; i < MAX_NUM_LAYER_IDS; i++) {
		p_app->m_newCLVS[i] = true;
	}

	//DecLib
	p_declib->m_isFirstGeneralHrd = true;
	//m_prevGeneralHrdParams()
	p_declib->m_pocRandomAccess = MAX_INT;
	p_declib->m_lastRasPoc = MAX_INT;
	//p_declib->m_cListPic()
	//p_declib->m_parameterSetManager()
	//p_declib->m_SEIs()
	//p_declib->m_cIntraPred()
	//p_declib->m_cInterPred()
	//p_declib->m_cTrQuant()
	//p_declib->m_cSliceDecoder()
	//p_declib->m_cTrQuantScalingList()
	//p_declib->m_cCuDecoder()
	//p_declib->m_HLSReader()
	//p_declib->m_seiReader()
	//p_declib->m_deblockingFilter()
	//p_declib->m_cSAO()
	//p_declib->m_cReshaper()
	//#if JVET_J0090_MEMORY_BANDWIDTH_MEASURE
	//p_declib->m_cacheModel()
	//#endif
	p_declib->m_prevLayerID = MAX_INT;
	p_declib->m_prevPOC = MAX_INT;
	p_declib->m_prevPicPOC = MAX_INT;
	p_declib->m_bFirstSliceInPicture = true;
	p_declib->m_firstPictureInSequence = true;
	//p_declib->m_colourTranfParams()
	p_declib->m_firstSliceInBitstream = true;
	p_declib->m_isFirstAuInCvs =  true;
	p_declib->m_skippedPOC = MAX_INT;
	p_declib->m_skippedLayerID = MAX_INT;
	p_declib->m_lastPOCNoOutputPriorPics = -1;
	p_declib->m_sliceLmcsApsId = -1;
	//#if JVET_S0257_DUMP_360SEI_MESSAGE
	//p_declib->m_decoded360SeiDumpFileName()
	//#endif
	//p_declib->m_prefixSEINALUs()
	p_declib->m_debugPOC =  -1;
	p_declib->m_debugCTU =  -1;
	p_declib->m_maxDecSliceAddrInSubPic = -1;
	//#if ENABLE_SIMD_OPT_BUFFER
	//g_pelBufOP.initPelBufOpsX86();
	//#endif
	fill_n(p_declib->m_prevGDRInSameLayerPOC, MAX_VPS_LAYERS, -MAX_INT);
	fill_n(p_declib->m_prevGDRInSameLayerRecoveryPOC, MAX_VPS_LAYERS, -MAX_INT);
	fill_n(p_declib->m_firstSliceInSequence, MAX_VPS_LAYERS, true);
	fill_n(p_declib->m_pocCRA, MAX_VPS_LAYERS, -MAX_INT);
	for (i = 0; i < MAX_VPS_LAYERS; i++) {
		p_declib->m_associatedIRAPType[i] = NAL_UNIT_INVALID;
#ifndef REDUCE_SIZE
		fill_n(p_declib->m_prevGDRSubpicPOC[i], MAX_NUM_SUB_PICS, -MAX_INT);
		fill_n(p_declib->m_prevIRAPSubpicPOC[i], MAX_NUM_SUB_PICS, -MAX_INT);
		fill_n(p_declib->m_prevIRAPSubpicType[i], MAX_NUM_SUB_PICS, NAL_UNIT_INVALID);
#endif
	}

	cvector_init(&p_declib->m_accessUnitPicInfo, sizeof(AccessUnitPicInfo));
	cvector_init(&p_declib->m_firstAccessUnitPicInfo, sizeof(AccessUnitPicInfo));

	for (i = 0; i < MAX_VPS_LAYERS; i++)
		cvector_init(&p_declib->m_nalUnitInfo[i], sizeof(struct NalUnitInfo));
}


bool isNewPicture(DecLib *p_declib, param_t* params)
{
	bool ret = false;
	bool finished = false;

	// cannot be a new picture if there haven't been any slices yet
	if (p_declib->m_bFirstSliceInPicture) { //getFirstSliceInPicture()
		return false;
	}

	// save stream position for backup
	//#if RExt__DECODER_DEBUG_STATISTICS
	//CodingStatistics::CodingStatisticsData* backupStats = new CodingStatistics::CodingStatisticsData(CodingStatistics::GetStatistics());
	//std::streampos location = bitstreamFile->tellg() - std::streampos(bytestream->GetNumBufferedBytes());
	//#else
	//std::streampos location = bitstreamFile->tellg();
	//#endif

	// look ahead until picture start location is determined
	while (!finished) { // && !!(*bitstreamFile))
		//AnnexBStats stats = AnnexBStats();
		NALUnit nalu;
		//byteStreamNALUnit(*bytestream, nalu.getBitstream().getFifo(), stats);
		//if (nalu.getBitstream().getFifo().empty())
		//{
		//  msg( ERROR, "Warning: Attempt to decode an empty NAL unit\n");
		//}
		//else
		{
			// get next NAL unit type
			//read(nalu);
			switch (nalu.m_nalUnitType) {
				// NUT that indicate the start of a new picture
				case NAL_UNIT_ACCESS_UNIT_DELIMITER:
				case NAL_UNIT_OPI:
				case NAL_UNIT_DCI:
				case NAL_UNIT_VPS:
				case NAL_UNIT_SPS:
				case NAL_UNIT_PPS:
				case NAL_UNIT_PH:
				ret = true;
				finished = true;
				break;

				// NUT that may be the start of a new picture - check first bit in slice header
				case NAL_UNIT_CODED_SLICE_TRAIL:
				case NAL_UNIT_CODED_SLICE_STSA:
				case NAL_UNIT_CODED_SLICE_RASL:
				case NAL_UNIT_CODED_SLICE_RADL:
				case NAL_UNIT_RESERVED_VCL_4:
				case NAL_UNIT_RESERVED_VCL_5:
				case NAL_UNIT_RESERVED_VCL_6:
				case NAL_UNIT_CODED_SLICE_IDR_W_RADL:
				case NAL_UNIT_CODED_SLICE_IDR_N_LP:
				case NAL_UNIT_CODED_SLICE_CRA:
				case NAL_UNIT_CODED_SLICE_GDR:
				case NAL_UNIT_RESERVED_IRAP_VCL_11:
#ifdef TO_DO
				//ret = checkPictureHeaderInSliceHeaderFlag(nalu);
#else
				ret = true;
#endif
				finished = true;
				break;

				// NUT that are not the start of a new picture
				case NAL_UNIT_EOS:
				case NAL_UNIT_EOB:
				case NAL_UNIT_SUFFIX_APS:
				case NAL_UNIT_SUFFIX_SEI:
				case NAL_UNIT_FD:
				ret = false;
				finished = true;
				break;

				// NUT that might indicate the start of a new picture - keep looking
				case NAL_UNIT_PREFIX_APS:
				case NAL_UNIT_PREFIX_SEI:
				case NAL_UNIT_RESERVED_NVCL_26:
				case NAL_UNIT_RESERVED_NVCL_27:
				case NAL_UNIT_UNSPECIFIED_28:
				case NAL_UNIT_UNSPECIFIED_29:
				case NAL_UNIT_UNSPECIFIED_30:
				case NAL_UNIT_UNSPECIFIED_31:
				default:
				break;
			}
		}
	}
#if 0
	// restore previous stream location - minus 3 due to the need for the annexB parser to read three extra bytes
#if RExt__DECODER_DEBUG_BIT_STATISTICS
	bitstreamFile->clear();
	bitstreamFile->seekg(location);
	bytestream->reset();
	CodingStatistics::SetStatistics(*backupStats);
	delete backupStats;
#else
	bitstreamFile->clear();
	bitstreamFile->seekg(location-std::streamoff(3));
	bytestream->reset();
#endif
#endif
	// return TRUE if next NAL unit is the start of a new picture
	return ret;
}


bool isNewAccessUnit(DecLib *p_declib, bool newPicture, param_t* params)
	{
	bool ret = false;
	//bool finished = false;

	// can only be the start of an AU if this is the start of a new picture
	if ( newPicture == false ) {
		return false;
	}
#if 0
	// save stream position for backup
#if RExt__DECODER_DEBUG_STATISTICS
	CodingStatistics::CodingStatisticsData* backupStats = new CodingStatistics::CodingStatisticsData(CodingStatistics::GetStatistics());
	std::streampos location = bitstreamFile->tellg() - std::streampos(bytestream->GetNumBufferedBytes());
#else
	std::streampos location = bitstreamFile->tellg();
#endif
#endif
#ifdef TO_DO
	// look ahead until access unit start location is determined
	while (!finished && !!(*bitstreamFile)) {
		AnnexBStats stats = AnnexBStats();
		InputNALUnit nalu;
		byteStreamNALUnit(*bytestream, nalu.getBitstream().getFifo(), stats);
		if (nalu.getBitstream().getFifo().empty()) {
			msg( ERROR, "Warning: Attempt to decode an empty NAL unit\n");
		}
		else
		{
			// get next NAL unit type
			read(nalu);
			switch (nalu.m_nalUnitType) {
				// AUD always indicates the start of a new access unit
				case NAL_UNIT_ACCESS_UNIT_DELIMITER:
				ret = true;
				finished = true;
				break;

				// slice types - check layer ID and POC
				case NAL_UNIT_CODED_SLICE_TRAIL:
				case NAL_UNIT_CODED_SLICE_STSA:
				case NAL_UNIT_CODED_SLICE_RASL:
				case NAL_UNIT_CODED_SLICE_RADL:
				case NAL_UNIT_CODED_SLICE_IDR_W_RADL:
				case NAL_UNIT_CODED_SLICE_IDR_N_LP:
				case NAL_UNIT_CODED_SLICE_CRA:
				case NAL_UNIT_CODED_SLICE_GDR:
				ret = isSliceNaluFirstInAU(p_declib, newPicture, &nalu);
				finished = true;
				break;

				// NUT that are not the start of a new access unit
				case NAL_UNIT_EOS:
				case NAL_UNIT_EOB:
				case NAL_UNIT_SUFFIX_APS:
				case NAL_UNIT_SUFFIX_SEI:
				case NAL_UNIT_FD:
				ret = false;
				finished = true;
				break;

				// all other NUT - keep looking to find first VCL
				default:
				break;
			}
		}
	}
#else
	ret = true;
#endif
#if 0
	// restore previous stream location
#if RExt__DECODER_DEBUG_BIT_STATISTICS
	bitstreamFile->clear();
	bitstreamFile->seekg(location);
	bytestream->reset();
	CodingStatistics::SetStatistics(*backupStats);
	delete backupStats;
#else
	bitstreamFile->clear();
	bitstreamFile->seekg(location);
	bytestream->reset();
#endif
#endif
	// return TRUE if next NAL unit is the start of a new picture
	return ret;
}

bool isSliceNaluFirstInAU(DecLib *p_declib, bool newPicture, NALUnit *nalu)
{
	// can only be the start of an AU if this is the start of a new picture
	if (newPicture == false) {
		return false;
	}

	// should only be called for slice NALU types
	if ( nalu->m_nalUnitType != NAL_UNIT_CODED_SLICE_TRAIL &&
	nalu->m_nalUnitType != NAL_UNIT_CODED_SLICE_STSA &&
	nalu->m_nalUnitType != NAL_UNIT_CODED_SLICE_RASL &&
	nalu->m_nalUnitType != NAL_UNIT_CODED_SLICE_RADL &&
	nalu->m_nalUnitType != NAL_UNIT_CODED_SLICE_IDR_W_RADL &&
	nalu->m_nalUnitType != NAL_UNIT_CODED_SLICE_IDR_N_LP &&
	nalu->m_nalUnitType != NAL_UNIT_CODED_SLICE_CRA &&
	nalu->m_nalUnitType != NAL_UNIT_CODED_SLICE_GDR) {
		return false;
	}

	// check for layer ID less than or equal to previous picture's layer ID
	if ( nalu->m_nuhLayerId <= p_declib->m_prevLayerID ) {
		return true;
	}
	// get slice POC
	p_declib->m_apcSlicePilot->m_pcPicHeader = &p_declib->m_picHeader;
	initSlice(p_declib->m_apcSlicePilot); //->initSlice();
	//p_declib->m_HLSReader.setBitstream( &nalu.getBitstream() );
#ifdef TO_DO
	p_declib->m_HLSReader.getSlicePoc(p_declib->m_apcSlicePilot, &p_declib->m_picHeader, &p_declib->m_parameterSetManager, p_declib->m_prevTid0POC);
#endif
	// check for different POC
	return (p_declib->m_apcSlicePilot->m_iPOC != p_declib->m_prevPOC);
}

bool getMixedNaluTypesInPicFlag(DecLib *p_declib)
{
#ifdef TO_DO
	if (p_declib->m_picHeader.isValid()) {
		return false;
	}

	PPS *pps = p_declib->m_parameterSetManager.getPPS(m_picHeader.getPPSId());
	//CHECK(pps == 0, "No PPS present");

	return pps->m_mixedNaluTypesInPicFlag;
#else
	return false;
#endif
}

void resetAccessUnitNals(DecLib *p_declib)
{
	cvector_clear(&p_declib->m_accessUnitNals);
}
void resetAccessUnitPicInfo(DecLib *p_declib)
{
	cvector_clear(&p_declib->m_accessUnitPicInfo);
}
void resetAccessUnitApsNals(DecLib *p_declib)
{
	cvector_clear(&p_declib->m_accessUnitApsNals);
}
void resetAccessUnitSeiTids(DecLib *p_declib)
{
	cvector_clear(&p_declib->m_accessUnitSeiTids);
}
void resetAudIrapOrGdrAuFlag(DecLib *p_declib)
{
	p_declib->m_audIrapOrGdrAuFlag = false;
}
void resetAccessUnitEos(DecLib *p_declib)
{
	memset(&p_declib->m_accessUnitEos, false, sizeof(p_declib->m_accessUnitEos));
}
void resetPictureUnitNals(DecLib *p_declib)
{
	cvector_clear(&p_declib->m_pictureUnitNals);
}

void resetAccessUnitSeiPayLoadTypes(DecLib *p_declib)
{
	cvector_clear(&p_declib->m_pictureUnitNals);
}

bool getGDRRecoveryPocReached(DecLib *p_declib)
{
	return ( p_declib->m_pcPic->poc >= p_declib->m_prevGDRInSameLayerRecoveryPOC[p_declib->m_pcPic->layerId] );
}

void resetAccessUnitNoOutputPriorPicFlags(DecLib *p_declib)
{
	cvector_clear(&p_declib->m_accessUnitNoOutputPriorPicFlags);
}

void deriveTargetOutputLayerSet(DecLib *p_declib, const int targetOlsIdx)
{
#ifdef TO_DO
	if (p_declib->m_vps != nullptr )
		p_declib->m_vps->deriveTargetOutputLayerSet(targetOlsIdx );
#endif
}
#if 0
int deriveTargetOLSIdx(VPS *vps)
{
	int lowestIdx = 0;
	int highestNumLayers = cvector_get(&vps->m_numLayersInOls, lowestIdx);
	int idx;
	if ((vps->m_numLayersInOls._size > 1 )) {
		for (idx = 1; idx < vps->m_numLayersInOls._size; idx++)
		{
			if (highestNumLayers == cvector_get(&vps->m_numLayersInOls, idx)) {
				if (cvector_get(&vps->m_numOutputLayersInOls, lowestIdx) < cvector_get(&vps->m_numOutputLayersInOls, idx)) {
				  lowestIdx       = idx;
				}
			} else if (highestNumLayers < cvector_get(&vps->m_numLayersInOls,idx)) {
				highestNumLayers = cvector_get(&vps->m_numLayersInOls,idx);
				lowestIdx       = idx;
			}
		}
	}
	return lowestIdx;
}
#endif
void copySliceInfo(Slice *pDst, Slice *pSrc, bool cpyAlmostAll)
{
	//CHECK(!pSrc, "Source is NULL");
	//int k;
	int i, j, e;
	uint32_t ch;
	uint32_t component;
	pDst->m_iPOC                 = pSrc->m_iPOC;
	pDst->m_eNalUnitType         = pSrc->m_eNalUnitType;
	pDst->m_eSliceType           = pSrc->m_eSliceType;
	pDst->m_iSliceQp             = pSrc->m_iSliceQp;
	pDst->m_iSliceQpBase         = pSrc->m_iSliceQpBase;
	pDst->m_ChromaQpAdjEnabled              = pSrc->m_ChromaQpAdjEnabled;
	pDst->m_deblockingFilterDisable         = pSrc->m_deblockingFilterDisable;
	pDst->m_deblockingFilterOverrideFlag    = pSrc->m_deblockingFilterOverrideFlag;
	pDst->m_deblockingFilterBetaOffsetDiv2  = pSrc->m_deblockingFilterBetaOffsetDiv2;
	pDst->m_deblockingFilterTcOffsetDiv2    = pSrc->m_deblockingFilterTcOffsetDiv2;
	pDst->m_deblockingFilterCbBetaOffsetDiv2  = pSrc->m_deblockingFilterCbBetaOffsetDiv2;
	pDst->m_deblockingFilterCbTcOffsetDiv2    = pSrc->m_deblockingFilterCbTcOffsetDiv2;
	pDst->m_deblockingFilterCrBetaOffsetDiv2  = pSrc->m_deblockingFilterCrBetaOffsetDiv2;
	pDst->m_deblockingFilterCrTcOffsetDiv2    = pSrc->m_deblockingFilterCrTcOffsetDiv2;
	pDst->m_depQuantEnabledFlag               = pSrc->m_depQuantEnabledFlag;
	pDst->m_signDataHidingEnabledFlag         = pSrc->m_signDataHidingEnabledFlag;
	pDst->m_tsResidualCodingDisabledFlag      = pSrc->m_tsResidualCodingDisabledFlag;
	pDst->m_tsrc_index                        = pSrc->m_tsrc_index;

	for (i = 0; i < MAX_TSRC_RICE; i++) {
		pDst->m_riceBit[i] = pSrc->m_riceBit[i];
	}

	for (i = 0; i < NUM_REF_PIC_LIST_01; i++) {
		pDst->m_aiNumRefIdx[i]     = pSrc->m_aiNumRefIdx[i];
	}

	for (i = 0; i < MAX_NUM_REF; i++) {
		pDst->m_list1IdxToList0Idx[i] = pSrc->m_list1IdxToList0Idx[i];
	}

	pDst->m_bCheckLDC            = pSrc->m_bCheckLDC;
	pDst->m_iSliceQpDelta        = pSrc->m_iSliceQpDelta;

	pDst->m_biDirPred = pSrc->m_biDirPred;
	pDst->m_symRefIdx[0] = pSrc->m_symRefIdx[0];
	pDst->m_symRefIdx[1] = pSrc->m_symRefIdx[1];

	for (component = 0; component < MAX_NUM_COMPONENT; component++) {
		pDst->m_iSliceChromaQpDelta[component] = pSrc->m_iSliceChromaQpDelta[component];
	}
	pDst->m_iSliceChromaQpDelta[JOINT_CbCr] = pSrc->m_iSliceChromaQpDelta[JOINT_CbCr];

	for (i = 0; i < NUM_REF_PIC_LIST_01; i++) {
		for (j = 0; j < MAX_NUM_REF; j++) {
			pDst->m_apcRefPicList[i][j]  = pSrc->m_apcRefPicList[i][j];
			pDst->m_aiRefPOCList[i][j]   = pSrc->m_aiRefPOCList[i][j];
			pDst->m_bIsUsedAsLongTerm[i][j] = pSrc->m_bIsUsedAsLongTerm[i][j];
		}
		pDst->m_bIsUsedAsLongTerm[i][MAX_NUM_REF] = pSrc->m_bIsUsedAsLongTerm[i][MAX_NUM_REF];
	}
	if (cpyAlmostAll) {
		pDst->m_iDepth = pSrc->m_iDepth;
	}

	// access channel
	if (cpyAlmostAll) {
		pDst->m_RPL0 = pSrc->m_RPL0;
	}
	if (cpyAlmostAll) {
		pDst->m_RPL1 = pSrc->m_RPL1;
	}
	pDst->m_iLastIDR             = pSrc->m_iLastIDR;

	if (cpyAlmostAll) {
		pDst->m_pcPic = pSrc->m_pcPic;
	}

	pDst->m_pcPicHeader          = pSrc->m_pcPicHeader;
	pDst->m_colFromL0Flag        = pSrc->m_colFromL0Flag;
	pDst->m_colRefIdx            = pSrc->m_colRefIdx;

	if (cpyAlmostAll) {
		//setLambdas(pSrc->getLambdas());
	}
#if 0
	for (i = 0; i < NUM_REF_PIC_LIST_01; i++) {
		for (j = 0; j < MAX_NUM_REF; j++) {
			for (k = 0; k < MAX_NUM_REF; k++) {
				pDst->m_abEqualRef[i][j][k] = pSrc->m_abEqualRef[i][j][k];
			}
		}
	}
#endif
	pDst->m_uiTLayer                      = pSrc->m_uiTLayer;
	pDst->m_bTLayerSwitchingFlag          = pSrc->m_bTLayerSwitchingFlag;

	pDst->m_sliceMap                      = pSrc->m_sliceMap;
	pDst->m_independentSliceIdx           = pSrc->m_independentSliceIdx;
	pDst->m_nextSlice                     = pSrc->m_nextSlice;
	//pDst->m_clpRngs                       = pSrc->m_clpRngs;
	pDst->m_lmcsEnabledFlag               = pSrc->m_lmcsEnabledFlag;
	pDst->m_explicitScalingListUsed       = pSrc->m_explicitScalingListUsed;

	pDst->m_pendingRasInit                = pSrc->m_pendingRasInit;

	for (e = 0; e < NUM_REF_PIC_LIST_01; e++) {
		uint32_t n;
		for (n = 0; n < MAX_NUM_REF; n++) {
			//memcpy(pDst->m_weightPredTable[e][n], pSrc->m_weightPredTable[e][n], sizeof(WPScalingParam)*MAX_NUM_COMPONENT );
		}
	}

	for (ch = 0 ; ch < MAX_NUM_CHANNEL_TYPE; ch++) {
		pDst->m_saoEnabledFlag[ch] = pSrc->m_saoEnabledFlag[ch];
	}

	pDst->m_cabacInitFlag                 = pSrc->m_cabacInitFlag;
	//memcpy(pDst->m_alfApss, pSrc->m_alfApss, sizeof(m_alfApss)); // this might be quite unsafe
	//memcpy( pDst->m_alfEnabledFlag, pSrc->m_alfEnabledFlag, sizeof(m_alfEnabledFlag));
	//pDst->m_numAlfApsIdsLuma              = pSrc->m_numAlfApsIdsLuma;
	//pDst->m_alfApsIdsLuma                 = pSrc->m_alfApsIdsLuma;
	//pDst->m_alfApsIdChroma                = pSrc->m_alfApsIdChroma;
	pDst->m_disableSATDForRd              = pSrc->m_disableSATDForRd;
	pDst->m_isLossless = pSrc->m_isLossless;

	if (cpyAlmostAll) {
		pDst->m_encCABACTableIdx = pSrc->m_encCABACTableIdx;
	}
	for (i = 0; i < NUM_REF_PIC_LIST_01; i ++ ) {
		for (j = 0; j < MAX_NUM_REF_PICS; j ++ ) {
		//pDst->m_scalingRatio[i][j]          = pSrc->m_scalingRatio[i][j];
		}
	}
	//pDst->m_ccAlfFilterParam               = pSrc->m_ccAlfFilterParam;
	//pDst->m_ccAlfFilterControl[0]          = pSrc->m_ccAlfFilterControl[0];
	//pDst->m_ccAlfFilterControl[1]          = pSrc->m_ccAlfFilterControl[1];
	//pDst->m_ccAlfCbEnabledFlag             = pSrc->m_ccAlfCbEnabledFlag;
	//pDst->m_ccAlfCrEnabledFlag             = pSrc->m_ccAlfCrEnabledFlag;
	//pDst->m_ccAlfCbApsId                   = pSrc->m_ccAlfCbApsId;
	//pDst->m_ccAlfCrApsId                   = pSrc->m_ccAlfCrApsId;
}
#ifdef TO_DO
uint32_t getSubPicIdxFromSubPicId(PPS *pps, uint32_t subPicId )
{
	int i;
	for (i = 0; i < pps->m_numSubPics; i++) {
		if ((SubPic *)cvector_get(&pps->m_subPics, i)->m_subPicID == subPicId) {
			return i;
		}
	}
	return 0;
}
#endif

void checkNoOutputPriorPics (DecLib *p_declib, PicList* pcListPic)
{
	int i;
	if (!pcListPic || !p_declib->m_isNoOutputPriorPics) {
		return;
	}

	//PicList::iterator  iterPic   = pcListPic->begin();

	//while (iterPic != pcListPic->end())
	for (i = 0; i < PIC_LIST_SIZE; i++) {
		Picture* pcPicTmp = pcListPic->pic[i]; //*(iterPic);
		if (pcPicTmp == NULL)
			break;
		//Picture* pcPicTmp = *(iterPic++);
		if (p_declib->m_lastPOCNoOutputPriorPics != pcPicTmp->poc) {
			pcPicTmp->neededForOutput = false;
		}
	}
}

void xUpdatePreviousTid0POC(DecLib *p_declib, Slice *pSlice)
{
	if ( (pSlice->m_uiTLayer == 0) &&
		(pSlice->m_eNalUnitType != NAL_UNIT_CODED_SLICE_RASL) &&
		(pSlice->m_eNalUnitType != NAL_UNIT_CODED_SLICE_RADL) &&
		!pSlice->m_pcPicHeader->m_nonReferencePictureFlag ) {
		p_declib->m_prevTid0POC = pSlice->m_iPOC;
		hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "%s: %d (%d,%d,%d)\n",
		__func__, p_declib->m_prevTid0POC,
		pSlice->m_uiTLayer, pSlice->m_eNalUnitType,
		pSlice->m_pcPicHeader->m_nonReferencePictureFlag);
	} else {
		hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "%s (not update for %d): %d (%d,%d,%d)\n",
		__func__, pSlice->m_iPOC,
		p_declib->m_prevTid0POC, pSlice->m_uiTLayer,
		pSlice->m_eNalUnitType, pSlice->m_pcPicHeader->m_nonReferencePictureFlag);
	}
}

bool getRapPicFlag(Slice *slice)
{
	return (slice->m_eNalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL
		|| slice->m_eNalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP
		|| slice->m_eNalUnitType == NAL_UNIT_CODED_SLICE_CRA);
}

bool getIdrPicFlag(Slice *slice)
{
	return (slice->m_eNalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL ||
		slice->m_eNalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP);
}

bool isRandomAccessSkipPicture(DecApp *p_app, bool mixedNaluInPicFlag, uint32_t layerId )
{
	DecLib *p_declib = &p_app->m_cDecLib;
	hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "enter %s iSkipFrame=%d, m_pocRandomAccess %d, iPOCLastDisplay=%d\n",
		__func__,  p_app->m_iSkipFrame, p_declib->m_pocRandomAccess, p_app->m_iPOCLastDisplay);

	if ( (p_app->m_iSkipFrame > 0) &&
	(cvector_get(&p_declib->m_apcSlicePilot->m_sliceMap.m_ctuAddrInSlice,0) == 0 && layerId == 0) &&
	(p_declib->m_skippedPOC != MAX_INT) && (p_declib->m_skippedLayerID != MAX_INT)) {
		// When skipFrame count greater than 0, and current frame is not the first frame of sequence, decrement skipFrame count.
		// If skipFrame count is still greater than 0, the current frame will be skipped.
		p_app->m_iSkipFrame--;
	}

	if (p_app->m_iSkipFrame) {
		hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "%s iSkipFrame=%d, return true\n", __func__, p_app->m_iSkipFrame);
		p_app->m_iSkipFrame--;   // decrement the counter
		p_declib->m_maxDecSubPicIdx = 0;
		p_declib->m_maxDecSliceAddrInSubPic = -1;
		return true;
	} else if ( p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL ||
		p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP ) {
		p_declib->m_pocRandomAccess = -MAX_INT; // no need to skip the reordered pictures in IDR, they are decodable.
	} else if (p_declib->m_pocRandomAccess == MAX_INT) { // start of random access point, m_pocRandomAccess has not been set yet.
		if (p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_CRA ||
			p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_GDR ) {
			// set the POC random access since we need to skip the reordered pictures in the case of CRA/CRANT/BLA/BLANT.
			p_declib->m_pocRandomAccess = p_declib->m_apcSlicePilot->m_iPOC;
		} else {
			hevc_print(p_declib->hw, H266_DEBUG_DETAIL,
				"%s iSkipFrame=%d, return true due to start of random access point\n",
				__func__, p_app->m_iSkipFrame);
			if (!p_declib->m_warningMessageSkipPicture) {
				hevc_print(p_declib->hw, 0,
					"Warning: This is not a valid random access point and the data is discarded until the first CRA or GDR picture\n");
				p_declib->m_warningMessageSkipPicture = true;
			}
			p_app->m_iSkipFrame--;
			p_declib->m_maxDecSubPicIdx = 0;
			p_declib->m_maxDecSliceAddrInSubPic = -1;
			return true;
		}
	} else if (p_declib->m_apcSlicePilot->m_iPOC < p_declib->m_pocRandomAccess &&  // skip the reordered pictures, if necessary
	(p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_RASL || mixedNaluInPicFlag)) {
		hevc_print(p_declib->hw, H266_DEBUG_DETAIL,
			"%s iSkipFrame=%d, return true due to reordered pictures (%d, %d, %d, %d)\n",
			__func__, p_app->m_iSkipFrame,
			p_declib->m_apcSlicePilot->m_iPOC,
			p_declib->m_pocRandomAccess,
			p_declib->m_apcSlicePilot->m_eNalUnitType, mixedNaluInPicFlag);
		p_app->m_iPOCLastDisplay++;
		p_app->m_iSkipFrame--;
		p_declib->m_maxDecSubPicIdx = 0;
		p_declib->m_maxDecSliceAddrInSubPic = -1;
		return true;
	}
	//printk("%s iSkipFrame=%d, return false (%d, %d, %d, %d)\n", __func__, p_app->m_iSkipFrame,
	//  p_declib->m_apcSlicePilot->m_iPOC, p_declib->m_pocRandomAccess, p_declib->m_apcSlicePilot->m_eNalUnitType, mixedNaluInPicFlag);
	// if we reach here, then the picture is not skipped.
	hevc_print(p_declib->hw, H266_DEBUG_DETAIL,
		"exit %s iSkipFrame=%d, m_pocRandomAccess %d, iPOCLastDisplay=%d\n",
		__func__,  p_app->m_iSkipFrame, p_declib->m_pocRandomAccess, p_app->m_iPOCLastDisplay);
	return false;
}

#ifdef AML
void update_rpl(ReferencePictureList *pRPL, ref_set_t *ref_set)
{
	//ref_entry_info - {LtrpInSliceHeaderFlag,numIlrp[4:0],numLtrp[4:0],numStrp[4:0],numRefPic[15:0]}
	//ref_entry_[0-30] - {isLongTerm, isInterLayerRefPic, ilrp_idx(Ilrp)/poc_lsb_lt(Ltrp)/deltaValue(Strp)[29:0]}
	int i;
	pRPL->m_numberOfShorttermPictures = ref_set->numStrp;
	pRPL->m_numberOfLongtermPictures = ref_set->numLtrp;
	pRPL->m_numberOfInterLayerPictures = ref_set->numIlrp;
	for (i = 0; i < pRPL->m_numberOfShorttermPictures +
		pRPL->m_numberOfLongtermPictures + pRPL->m_numberOfInterLayerPictures; i++) {

		pRPL->m_isLongtermRefPic[i] = (ref_set->ref_entry[i] >> 31) & 0x1;
		if (pRPL->m_isLongtermRefPic[i]) {
			pRPL->m_deltaPocMSBPresentFlag[i] = (ref_set->ref_entry[i] >> 30) & 0x1;
			pRPL->m_isInterLayerRefPic[i] = 0;
			pRPL->m_refPicIdentifier[i] = (ref_set->ref_entry[i])&0xffff;
			pRPL->m_deltaPOCMSBCycleLT[i] = (ref_set->ref_entry[i]>>16)&0x3fff;
		} else {
			pRPL->m_deltaPocMSBPresentFlag[i] = 0;
			pRPL->m_isInterLayerRefPic[i] = (ref_set->ref_entry[i] >> 30) & 0x1;
			if (pRPL->m_isInterLayerRefPic[i]) {
				pRPL->m_refPicIdentifier[i] = 0;
				pRPL->m_interLayerRefPicIdx[i] = (((int)ref_set->ref_entry[i])<<2)>>2;
			} else {
				pRPL->m_refPicIdentifier[i] = (((int)ref_set->ref_entry[i])<<2)>>2;
				pRPL->m_interLayerRefPicIdx[i] = 0;
			}
			pRPL->m_deltaPOCMSBCycleLT[i] = 0;
		}
	}
}
#endif

void applyReferencePictureListBasedMarking(Slice *slice, PicList *rcListPic,
	const ReferencePictureList *pRPL0, const ReferencePictureList *pRPL1, const int layerId, PPS *pps )
{
	int i, ii, isReference;
	int isAvailable, pocCycle, curPoc, refPoc;
	//checkLeadingPictureRestrictions(rcListPic, pps);

	bool isNeedToCheck = (slice->m_eNalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP ||
		slice->m_eNalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL) && !pps->m_mixedNaluTypesInPicFlag ? false : true;

	// mark long-term reference pictures in List0
	for (i = 0; i < pRPL0->m_numberOfShorttermPictures + pRPL0->m_numberOfLongtermPictures + pRPL0->m_numberOfInterLayerPictures; i++ ) {
		if (!pRPL0->m_isLongtermRefPic[i] || pRPL0->m_isInterLayerRefPic[i] ) {
			continue;
		}

		isAvailable = 0;
		//PicList::iterator iterPic = rcListPic.begin();
		//while (iterPic != rcListPic.end())
		//{
		//  Picture* rpcPic = *(iterPic++);
		for (ii = 0; ii < PIC_LIST_SIZE; ii++) {
			Picture* rpcPic = rcListPic->pic[ii];
			if (rpcPic == NULL)
				break;
			if (!rpcPic->referenced) {
				continue;
			}
			pocCycle = 1 << (rpcPic->cs->sps->m_uiBitsForPOC);
			curPoc = rpcPic->poc;
			refPoc = pRPL0->m_refPicIdentifier[i] & (pocCycle - 1);
			if (pRPL0->m_deltaPocMSBPresentFlag[i]) {
				refPoc += slice->m_iPOC - pRPL0->m_deltaPOCMSBCycleLT[i] * pocCycle - (slice->m_iPOC & (pocCycle - 1));
			} else {
				curPoc = curPoc & (pocCycle - 1);
			}
			if (rpcPic->longTerm && curPoc == refPoc && rpcPic->referenced) {
				isAvailable = 1;
				break;
			}
		}
		// if there was no such long-term check the short terms
		if (!isAvailable) {
		//iterPic = rcListPic.begin();
		//while (iterPic != rcListPic.end())
		//{
		//  Picture* rpcPic = *(iterPic++);
			for (ii = 0; ii < PIC_LIST_SIZE; ii++) {
				Picture* rpcPic = rcListPic->pic[ii];
				if (rpcPic == NULL)
				break;
				if (!rpcPic->referenced) {
					continue;
				}
				pocCycle = 1 << (rpcPic->cs->sps->m_uiBitsForPOC);
				curPoc = rpcPic->poc;
				refPoc = pRPL0->m_refPicIdentifier[i] & (pocCycle - 1);
				if (pRPL0->m_deltaPocMSBPresentFlag[i]) {
					refPoc += slice->m_iPOC - pRPL0->m_deltaPOCMSBCycleLT[i] * pocCycle - (slice->m_iPOC & (pocCycle - 1));
				} else {
					curPoc = curPoc & (pocCycle - 1);
				}
				if (!rpcPic->longTerm && curPoc == refPoc && rpcPic->referenced) {
					isAvailable = 1;
					rpcPic->longTerm = true;
					break;
				}
			}
		}
	}

	// mark long-term reference pictures in List1
	for (i = 0; i < pRPL1->m_numberOfShorttermPictures + pRPL1->m_numberOfLongtermPictures + pRPL1->m_numberOfInterLayerPictures; i++) {
		if (!pRPL1->m_isLongtermRefPic[i] || pRPL1->m_isInterLayerRefPic[i]) {
			continue;
		}

		isAvailable = 0;
		//PicList::iterator iterPic = rcListPic.begin();
		//while (iterPic != rcListPic.end())
		//{
		//  Picture* rpcPic = *(iterPic++);
		for (ii = 0; ii < PIC_LIST_SIZE; ii++) {
			Picture* rpcPic = rcListPic->pic[ii];
			if (rpcPic == NULL)
				break;
			if (!rpcPic->referenced) {
				continue;
			}
			pocCycle = 1 << (rpcPic->cs->sps->m_uiBitsForPOC);
			curPoc = rpcPic->poc;
			refPoc = pRPL1->m_refPicIdentifier[i] & (pocCycle - 1);
			if (pRPL1->m_deltaPocMSBPresentFlag[i]) {
				refPoc += slice->m_iPOC - pRPL1->m_deltaPOCMSBCycleLT[i] * pocCycle - (slice->m_iPOC & (pocCycle - 1));
			} else {
				curPoc = curPoc & (pocCycle - 1);
			}
			if (rpcPic->longTerm && curPoc == refPoc && rpcPic->referenced) {
				isAvailable = 1;
				break;
			}
		}
		// if there was no such long-term check the short terms
		if (!isAvailable) {
			//iterPic = rcListPic.begin();
			//while (iterPic != rcListPic.end())
			//{
			//  Picture* rpcPic = *(iterPic++);
			for (ii = 0; ii < PIC_LIST_SIZE; ii++) {
				Picture* rpcPic = rcListPic->pic[ii];
				if (rpcPic == NULL)
				break;
				if (!rpcPic->referenced) {
					continue;
				}
				pocCycle = 1 << (rpcPic->cs->sps->m_uiBitsForPOC);
				curPoc = rpcPic->poc;
				refPoc = pRPL1->m_refPicIdentifier[i] & (pocCycle - 1);
				if (pRPL1->m_deltaPocMSBPresentFlag[i]) {
					refPoc += slice->m_iPOC - pRPL1->m_deltaPOCMSBCycleLT[i] * pocCycle - (slice->m_iPOC & (pocCycle - 1));
				} else {
					curPoc = curPoc & (pocCycle - 1);
				}
				if (!rpcPic->longTerm && curPoc == refPoc && rpcPic->referenced)
				{
					isAvailable = 1;
					rpcPic->longTerm = true;
					break;
				}
			}
		}
	}

	// loop through all pictures in the reference picture buffer
	//PicList::iterator iterPic = rcListPic.begin();
	//while (iterPic != rcListPic.end())
	//{
	//  Picture* pcPic = *(iterPic++);
	for (ii = 0; ii < PIC_LIST_SIZE; ii++) {
		Picture* pcPic = rcListPic->pic[ii];
		if (pcPic == NULL)
			break;

		if (!pcPic->referenced) {
			continue;
		}

		isReference = 0;
		// loop through all pictures in the Reference Picture Set
		// to see if the picture should be kept as reference picture
		for (i = 0; isNeedToCheck && !isReference && i < pRPL0->m_numberOfShorttermPictures +
			pRPL0->m_numberOfLongtermPictures + pRPL0->m_numberOfInterLayerPictures; i++ ) {
			if (pRPL0->m_isInterLayerRefPic[i]) {
				// Diagonal inter-layer prediction is not allowed
				CHECK( pRPL0->m_refPicIdentifier[i], "ILRP identifier should be 0" );

				if ( pcPic->poc == slice->m_iPOC ) {
					isReference = 1;
					pcPic->longTerm = true;
				}
			} else if (pcPic->layerId == layerId) {
				if (!(pRPL0->m_isLongtermRefPic[i]))
				{
					if (pcPic->poc == slice->m_iPOC + pRPL0->m_refPicIdentifier[i]) {
						isReference = 1;
						pcPic->longTerm = false;
					}
				} else {
					pocCycle = 1 << (pcPic->cs->sps->m_uiBitsForPOC);
					curPoc = pcPic->poc;
					refPoc = pRPL0->m_refPicIdentifier[i] & (pocCycle - 1);
					if (pRPL0->m_deltaPocMSBPresentFlag[i]) {
						refPoc += slice->m_iPOC - pRPL0->m_deltaPOCMSBCycleLT[i] * pocCycle - (slice->m_iPOC & (pocCycle - 1));
					} else {
						curPoc = curPoc & (pocCycle - 1);
					}
					if (pcPic->longTerm && curPoc == refPoc)
					{
						isReference = 1;
						pcPic->longTerm = true;
					}
				}
			}
		}

		for (i = 0; isNeedToCheck && !isReference && i < pRPL1->m_numberOfShorttermPictures +
			pRPL1->m_numberOfLongtermPictures + pRPL1->m_numberOfInterLayerPictures; i++) {
			if ( pRPL1->m_isInterLayerRefPic[i] ) {
				// Diagonal inter-layer prediction is not allowed
				CHECK( pRPL1->m_refPicIdentifier[i], "ILRP identifier should be 0" );

				if ( pcPic->poc ==slice-> m_iPOC ) {
					isReference = 1;
					pcPic->longTerm = true;
				}
			} else if ( pcPic->layerId == layerId ) {
				if (!(pRPL1->m_isLongtermRefPic[i])) {
					if (pcPic->poc == slice->m_iPOC + pRPL1->m_refPicIdentifier[i])
					{
						isReference = 1;
						pcPic->longTerm = false;
					}
				} else {
					pocCycle = 1 << (pcPic->cs->sps->m_uiBitsForPOC);
					curPoc = pcPic->poc;
					refPoc = pRPL1->m_refPicIdentifier[i] & (pocCycle - 1);
					if (pRPL1->m_deltaPocMSBPresentFlag[i]) {
						refPoc += slice->m_iPOC - pRPL1->m_deltaPOCMSBCycleLT[i] * pocCycle - (slice->m_iPOC & (pocCycle - 1));
					} else {
						curPoc = curPoc & (pocCycle - 1);
					}
					if (pcPic->longTerm && curPoc == refPoc) {
						isReference = 1;
						pcPic->longTerm = true;
					}
				}
			}
		}
		// mark the picture as "unused for reference" if it is not in
		// the Reference Picture List
		if ( pcPic->layerId == layerId && pcPic->poc != slice->m_iPOC && isReference == 0 ) {
			pcPic->referenced = false;
			pcPic->longTerm = false;
		}

		// sanity checks
		if (pcPic->referenced) {
			//check that pictures of higher temporal layers are not used
			CHECK(pcPic->usedByCurr && !(pcPic->temporalId <= this->getTLayer()), "Invalid state");
		}
	}
}

void finalInit(Picture *pic, VPS* vps, SPS *sps, PPS *pps, PicHeader *picHeader); //, APS** alfApss, APS* lmcsAps, APS* scalingListAps )

void allocateNewSlice(Picture *pic)
{
	//slices.push_back(new Slice);
	//Slice& slice = *slices.back();
	//memcpy(slice.getAlfAPSs(), cs->alfApss, sizeof(cs->alfApss));
	int i;
	Slice *slice = NULL;
	for (i = 0; i < SLICE_MAX_NUM; i++) {
		if (pic->slices[i] == NULL) {
			slice = new_slice();
			pic->slices[i] = slice;
			break;
		}
	}
	if (slice == NULL)
		return;
	slice->m_pcPPS = pic->cs->pps;
	slice->m_pcSPS = pic->cs->sps;
	slice->m_pcVPS = pic->cs->vps;
	//if (slices.size() >= 2)
	if (i >= 1) {
		copySliceInfo(slice, pic->slices[i-1], true); //pic->slices[slices.size()-2] );
		initSlice(slice);
	}
}
#if 0
int getMaxDecPicBuffering(VPS *vps, int temporalId)
{
	DpbParameters *dpb_param = (DpbParameters *)cvector_get(&vps->m_dpbParameters, cvector_get(&vps->m_olsDpbParamsIdx, vps->m_targetOlsIdx));

	return dpb_param->m_maxDecPicBuffering[temporalId];
}
#endif
void pic_create(Picture *pic, uint32_t width, uint32_t height, const unsigned _maxCUSize, const unsigned _margin, const int _layerId)
{
	pic->layerId = _layerId;
}

void pic_destroy(Picture *pic)
{

}

Picture* xGetNewPicBuffer(DecLib *p_declib, SPS *sps, PPS *pps, const uint32_t temporalLayer, const int layerId )
{
	int i;
	Picture * pcPic = nullptr;
	bool bBufferIsAvailable;
#ifdef MODIFY_CODE
	p_declib->m_iMaxRefPicNum = 16;
	//p_declib->m_iMaxRefPicNum = 6;
	//p_declib->m_iMaxRefPicNum = 1;
#else
	p_declib->m_iMaxRefPicNum = ( p_declib->m_vps == nullptr || cvector_get(&p_declib->m_vps->m_numLayersInOls, p_declib->m_vps->m_targetOlsIdx) == 1 ) ? sps->m_uiMaxDecPicBuffering[temporalLayer] : getMaxDecPicBuffering(p_declib->m_vps, temporalLayer);     // m_uiMaxDecPicBuffering has the space for the picture currently being decoded
#endif
	if (PicList_size(&p_declib->m_cListPic) < (uint32_t)p_declib->m_iMaxRefPicNum) {
		pcPic = new_picture();
#if 1 //def TO_DO
		//pcPic->create( sps.getChromaFormatIdc(), Size( pps.getPicWidthInLumaSamples(), pps.getPicHeightInLumaSamples() ), sps.getMaxCUWidth(), sps.getMaxCUWidth() + 16, true, layerId );
		pic_create(pcPic, pps->m_picWidthInLumaSamples, pps->m_picHeightInLumaSamples, sps->m_uiMaxCUWidth, sps->m_uiMaxCUWidth + 16, layerId);
#endif
		PicList_push(&p_declib->m_cListPic, pcPic); //.push_back( pcPic );

		return pcPic;
	}

	bBufferIsAvailable = false;
	//for (auto * p: m_cListPic)
	//{
	//  pcPic = p;  // workaround because range-based for-loops don't work with existing variables
	for (i = 0; i < PIC_LIST_SIZE; i++) {
		pcPic = p_declib->m_cListPic.pic[i];
		if (pcPic == NULL)
			break;

		if ( pcPic->reconstructed == false && ! pcPic->neededForOutput ) {
			pcPic->neededForOutput = false;
			bBufferIsAvailable = true;
			break;
		}

		if ( ! pcPic->referenced  && ! pcPic->neededForOutput ) {
			pcPic->neededForOutput = false;
			pcPic->reconstructed = false;
			bBufferIsAvailable = true;
			break;
		}
	}

	if ( ! bBufferIsAvailable ) {
	//There is no room for this picture, either because of faulty encoder or dropped NAL. Extend the buffer.
		p_declib->m_iMaxRefPicNum++;

		pcPic = new_picture();

		PicList_push(&p_declib->m_cListPic, pcPic); //p_declib->m_cListPic.push_back( pcPic );
#if 1 //def TO_DO
		//pcPic->create( sps.getChromaFormatIdc(), Size( pps.getPicWidthInLumaSamples(), pps.getPicHeightInLumaSamples() ), sps.getMaxCUWidth(), sps.getMaxCUWidth() + 16, true, layerId );
		pic_create(pcPic, pps->m_picWidthInLumaSamples, pps->m_picHeightInLumaSamples, sps->m_uiMaxCUWidth, sps->m_uiMaxCUWidth + 16, layerId);
#endif
	} else {
#ifdef AML
		if (pcPic->buf_cfg) {
			pic_buf_cfg_free(pcPic->buf_cfg);
			pcPic->buf_cfg = NULL;
		}
#endif
#ifdef MODIFY_CODE
		pic_create(pcPic, pps->m_picWidthInLumaSamples, pps->m_picHeightInLumaSamples, sps->m_uiMaxCUWidth, sps->m_uiMaxCUWidth + 16, layerId);
#else
		if ( !pcPic->Y().Size::operator == ( Size( pps.getPicWidthInLumaSamples(), pps.getPicHeightInLumaSamples() ) ) ||
			pps.pcv->maxCUWidth != sps.getMaxCUWidth() || pps.pcv->maxCUHeight != sps.getMaxCUHeight() || pcPic->layerId != layerId ) {
			pcPic->destroy();
			pcPic->create( sps.getChromaFormatIdc(), Size( pps.getPicWidthInLumaSamples(), pps.getPicHeightInLumaSamples() ), sps.getMaxCUWidth(), sps.getMaxCUWidth() + 16, true, layerId );
		}
#endif
	}

#ifdef TO_DO
	pcPic->setBorderExtension( false );
#endif
	pcPic->neededForOutput = false;
	pcPic->reconstructed = false;
	return pcPic;
}

Slice *swapSliceObject(Picture *pic, Slice * p, uint32_t i)
{
	Slice * pTmp;
	p->m_pcSPS = pic->cs->sps;
	p->m_pcPPS = pic->cs->pps;
	p->m_pcVPS = pic->cs->vps;
	//p->setAlfAPSs(pic->cs->alfApss);


	pTmp = pic->slices[i];
	pic->slices[i] = p;
	pTmp->m_pcSPS = 0;
	pTmp->m_pcPPS = 0;
	pTmp->m_pcVPS = 0;
	//memset(pTmp->getAlfAPSs(), 0, sizeof(*pTmp->getAlfAPSs())*ALF_CTB_MAX_NUM_APS);

	return pTmp;
}


void xActivateParameterSets(DecLib *p_declib, NALUnit *nalu )
{
	const int layerId = nalu->m_nuhLayerId;
	Slice *pSlice;
	PicHeader *picHeader;
	bool isField;
	bool isTopField;

	if (p_declib->m_bFirstSliceInPicture) {
#ifdef TO_DO
		APS** apss = m_parameterSetManager.getAPSs();
		memset(apss, 0, sizeof(*apss) * ALF_CTB_MAX_NUM_APS);
		const PPS *pps = m_parameterSetManager.getPPS(m_picHeader.getPPSId()); // this is a temporary PPS object. Do not store this value
		CHECK(pps == 0, "Referred to PPS not present");

		const SPS *sps = m_parameterSetManager.getSPS(pps->getSPSId());             // this is a temporary SPS object. Do not store this value
		CHECK(sps == 0, "Referred to SPS not present");

		const VPS *vps = m_parameterSetManager.getVPS( sps->getVPSId() );
		CHECK(vps == 0, "Referred to VPS not present");

		if ( nullptr != pps->pcv ) {
			delete m_parameterSetManager.getPPS( m_picHeader.getPPSId() )->pcv;
		}
		m_parameterSetManager.getPPS( m_picHeader.getPPSId() )->pcv = new PreCalcValues( *sps, *pps, false );
		m_parameterSetManager.clearSPSChangedFlag(sps->getSPSId());
		m_parameterSetManager.clearPPSChangedFlag(pps->getPPSId());

		if (false == m_parameterSetManager.activatePPS(m_picHeader.getPPSId(),m_apcSlicePilot->isIRAP())) {
			THROW("Parameter set activation failed!");
		}

		// update the stored VPS to the actually referred to VPS
		m_vps = m_parameterSetManager.getVPS(sps->getVPSId());
#endif
#ifdef AML
		PPS *pps = p_declib->a_cur_pps;
		SPS *sps = p_declib->a_cur_sps;
		VPS *vps = p_declib->a_cur_vps;
#endif
		if (sps->m_VPSId == 0) {
			//No VPS in bitstream: set defaults values of variables in VPS to the ones signalled in SPS
			p_declib->m_vps->m_vpsMaxSubLayers = sps->m_uiMaxTLayers;
			p_declib->m_vps->m_vpsLayerId[0] = sps->m_layerId;
#ifdef TO_DO
			m_vps->deriveOutputLayerSets();
#endif
		} else {
			//VPS in the bitstream: check that SPS and VPS signalling are compatible
			CHECK(sps->getMaxTLayers() > m_vps->getMaxSubLayers(), "The SPS signals more temporal sub-layers than allowed by the VPS");
		}

#ifdef TO_DO
		m_parameterSetManager.getApsMap()->clearActive();
		for (i = 0; i < ALF_CTB_MAX_NUM_APS; i++) {
			APS* aps = m_parameterSetManager.getAPS(i, ALF_APS);
				if (aps) {
					m_parameterSetManager.clearAPSChangedFlag(i, ALF_APS);
				}
		}
		APS* lmcsAPS = nullptr;
		APS* scalinglistAPS = nullptr;
		activateAPS(&m_picHeader, m_apcSlicePilot, m_parameterSetManager, apss, lmcsAPS, scalinglistAPS);
		xParsePrefixSEImessages();

#if RExt__HIGH_BIT_DEPTH_SUPPORT == 0
		if (sps->getSpsRangeExtension().getExtendedPrecisionProcessingFlag() ||
			sps->getBitDepth(CHANNEL_TYPE_LUMA)>12 || sps->getBitDepth(CHANNEL_TYPE_CHROMA) > 12 ) {
			THROW("High bit depth support must be enabled at compile-time in order to decode this bitstream\n");
		}
#endif
#endif

		applyReferencePictureListBasedMarking(p_declib->m_apcSlicePilot, &p_declib->m_cListPic, &p_declib->m_apcSlicePilot->m_RPL0, &p_declib->m_apcSlicePilot->m_RPL1, layerId, pps);
#ifdef AML
		if (get_dbg_flag(p_declib->hw) & H266_DEBUG_DETAIL)
			print_vvc_picture_list(p_declib, &p_declib->m_cListPic,
			"After applyReferencePictureListBasedMarking()");
#endif
		//  Get a new picture buffer. This will also set up m_pcPic, and therefore give us a SPS and PPS pointer that we can use.
		p_declib->m_pcPic = xGetNewPicBuffer(p_declib, sps, pps, p_declib->m_apcSlicePilot->m_uiTLayer, layerId );
#ifdef AML
		setWindow(&p_declib->m_pcPic->m_scalingWindow,
			(short)(p_declib->param->p.pps_scaling_win_left_offset),
			(short)(p_declib->param->p.pps_scaling_win_right_offset),
			(short)(p_declib->param->p.pps_scaling_win_top_offset),
			(short)(p_declib->param->p.pps_scaling_win_bottom_offset));
		if (get_dbg_flag(p_declib->hw) & H266_DEBUG_DETAIL)
			print_vvc_picture_list(p_declib, &p_declib->m_cListPic, "After xGetNewPicBuffer()");
#endif

#if GDR_ENABLED
		picHeader = malloc(sizeof(PicHeader));
		//initPicHeader(picHeader);
		memcpy(picHeader,  &p_declib->m_picHeader, sizeof(PicHeader));
		p_declib->m_apcSlicePilot->m_pcPicHeader = picHeader; //setPicHeader(picHeader);
		finalInit(p_declib->m_pcPic, vps, sps, pps, picHeader); //, apss, lmcsAPS, scalinglistAPS);
#else
		finalInit(p_declib->m_pcPic, vps, sps, pps, &p_declib->m_picHeader); //, apss, lmcsAPS, scalinglistAPS );
#endif
#ifdef TO_DO
		m_pcPic->createColourTransfProcessor(m_firstPictureInSequence, &m_colourTranfParams,
			&m_invColourTransfBuf, pps->getPicWidthInLumaSamples(), pps->getPicHeightInLumaSamples(),
			sps->getChromaFormatIdc(), sps->getBitDepth(CHANNEL_TYPE_LUMA));
#endif
		p_declib->m_firstPictureInSequence = false;
#ifdef TO_DO
		m_pcPic->createTempBuffers( m_pcPic->cs->pps->pcv->maxCUWidth );
		m_pcPic->cs->createCoeffs((bool)m_pcPic->cs->sps->getPLTMode());
#endif
		allocateNewSlice(p_declib->m_pcPic);
		// make the slice-pilot a real slice, and set up the slice-pilot for the next slice
		CHECK(m_pcPic->slices.size() != (m_uiSliceSegmentIdx + 1), "Invalid number of slices");

#if 1 //def TO_DO
		p_declib->m_apcSlicePilot = swapSliceObject(p_declib->m_pcPic, p_declib->m_apcSlicePilot, p_declib->m_uiSliceSegmentIdx);
#endif
		// we now have a real slice:
		pSlice = p_declib->m_pcPic->slices[p_declib->m_uiSliceSegmentIdx];

		// Update the PPS and SPS pointers with the ones of the picture.
		pps=pSlice->m_pcPPS;
		sps=pSlice->m_pcSPS;

		// fix Parameter Sets, now that we have the real slice
		p_declib->m_pcPic->cs->slice = pSlice;
		p_declib->m_pcPic->cs->sps   = sps;
		p_declib->m_pcPic->cs->pps   = pps;
		p_declib->m_pcPic->cs->vps = vps;
#ifdef TO_DO
		memcpy(m_pcPic->cs->alfApss, apss, sizeof(m_pcPic->cs->alfApss));
		m_pcPic->cs->lmcsAps = lmcsAPS;
		m_pcPic->cs->scalinglistAps = scalinglistAPS;
		p_declib->m_pcPic->cs->pcv   = pps->pcv;
#endif

#ifdef TO_DO
		// Initialise the various objects for the new set of settings
		const int maxDepth = floorLog2(sps->getMaxCUWidth()) - pps->pcv->minCUWidthLog2;
		const uint32_t  log2SaoOffsetScaleLuma   = (uint32_t) std::max(0, sps->getBitDepth(CHANNEL_TYPE_LUMA  ) - MAX_SAO_TRUNCATED_BITDEPTH);
		const uint32_t  log2SaoOffsetScaleChroma = (uint32_t) std::max(0, sps->getBitDepth(CHANNEL_TYPE_CHROMA) - MAX_SAO_TRUNCATED_BITDEPTH);
		m_cSAO.create( pps->getPicWidthInLumaSamples(), pps->getPicHeightInLumaSamples(),
		sps->getChromaFormatIdc(),
		sps->getMaxCUWidth(), sps->getMaxCUHeight(),
		maxDepth,
		log2SaoOffsetScaleLuma, log2SaoOffsetScaleChroma );
		m_deblockingFilter.create(maxDepth);
		m_cIntraPred.init( sps->getChromaFormatIdc(), sps->getBitDepth( CHANNEL_TYPE_LUMA ) );
		m_cInterPred.init( &m_cRdCost, sps->getChromaFormatIdc(), sps->getMaxCUHeight() );
		if (sps->getUseLmcs()) {
			m_cReshaper.createDec(sps->getBitDepth(CHANNEL_TYPE_LUMA));
		}
#endif

		isField = false;
		isTopField = false;
#ifdef TO_DO
		if (!m_SEIs.empty()) {
			// Check if any new Frame Field Info SEI has arrived
			SEIMessages frameFieldSEIs = getSeisByType(m_SEIs, SEI::FRAME_FIELD_INFO);
			if (frameFieldSEIs.size()>0) {
				SEIFrameFieldInfo* ff = (SEIFrameFieldInfo*) *(frameFieldSEIs.begin());
				isField    = ff->m_fieldPicFlag;
				isTopField = isField && (!ff->m_bottomFieldFlag);
			}
			SEIMessages inclusionSEIs = getSeisByType(m_SEIs, SEI::PARAMETER_SETS_INCLUSION_INDICATION);
			const SEIParameterSetsInclusionIndication* inclusion = (inclusionSEIs.size() > 0) ? (SEIParameterSetsInclusionIndication*)*(inclusionSEIs.begin()) : NULL;
			if (inclusion != NULL) {
				p_declib->m_seiInclusionFlag = inclusion->m_selfContainedClvsFlag;
			}
		}
		if (m_seiInclusionFlag) {
			checkParameterSetsInclusionSEIconstraints(nalu);
		}
#endif
		//Set Field/Frame coding mode
		p_declib->m_pcPic->fieldPic = isField;
		p_declib->m_pcPic->topField = isTopField;

#ifdef TO_DO
		// transfer any SEI messages that have been received to the picture
		m_pcPic->SEIs = m_SEIs;
		m_SEIs.clear();

		// Recursive structure
		m_cCuDecoder.init( &m_cTrQuant, &m_cIntraPred, &m_cInterPred );
		if (sps->getUseLmcs()) {
			m_cCuDecoder.initDecCuReshaper(&m_cReshaper, sps->getChromaFormatIdc());
		}
		m_cTrQuant.init(m_cTrQuantScalingList.getQuant(), sps->getMaxTbSize(), false, false, false, false);

		// RdCost
		m_cRdCost.setCostMode ( COST_STANDARD_LOSSY ); // not used in decoder side RdCost stuff -> set to default

		m_cSliceDecoder.create();

		if ( sps->getALFEnabledFlag() ) {
			const int maxDepth = floorLog2(sps->getMaxCUWidth()) - sps->getLog2MinCodingBlockSize();
			m_cALF.create( pps->getPicWidthInLumaSamples(), pps->getPicHeightInLumaSamples(),
			sps->getChromaFormatIdc(), sps->getMaxCUWidth(), sps->getMaxCUHeight(), maxDepth, sps->getBitDepths().recon);
		}
		pSlice->m_ccAlfFilterControl[0] = m_cALF.getCcAlfControlIdc(COMPONENT_Cb);
		pSlice->m_ccAlfFilterControl[1] = m_cALF.getCcAlfControlIdc(COMPONENT_Cr);
#endif
	} else {
		Slice *pSlice; // we now have a real slice.
		SPS *sps;
		PPS *pps;
		// make the slice-pilot a real slice, and set up the slice-pilot for the next slice
		allocateNewSlice(p_declib->m_pcPic);
		CHECK(m_pcPic->slices.size() != (size_t)(m_uiSliceSegmentIdx + 1), "Invalid number of slices");
#if 1 //def TO_DO
		p_declib->m_apcSlicePilot = swapSliceObject(p_declib->m_pcPic, p_declib->m_apcSlicePilot, p_declib->m_uiSliceSegmentIdx);
#endif
		pSlice = p_declib->m_pcPic->slices[p_declib->m_uiSliceSegmentIdx]; // we now have a real slice.

		sps = pSlice->m_pcSPS;
		pps = pSlice->m_pcPPS;
		//APS** apss = pSlice->getAlfAPSs();
		//APS *lmcsAPS = m_picHeader.getLmcsAPS();
		//APS *scalinglistAPS = m_picHeader.getScalingListAPS();

		// fix Parameter Sets, now that we have the real slice
		p_declib->m_pcPic->cs->slice = pSlice;
		p_declib->m_pcPic->cs->sps   = sps;
		p_declib->m_pcPic->cs->pps   = pps;
#ifdef TO_DO
		memcpy(m_pcPic->cs->alfApss, apss, sizeof(m_pcPic->cs->alfApss));
		m_pcPic->cs->lmcsAps = lmcsAPS;
		m_pcPic->cs->scalinglistAps = scalinglistAPS;
		p_declib->m_pcPic->cs->pcv   = pps->pcv;
		// check that the current active PPS has not changed...
		if (m_parameterSetManager.getSPSChangedFlag(sps->getSPSId()) ) {
			EXIT("Error - a new SPS has been decoded while processing a picture");
		}
		if (m_parameterSetManager.getPPSChangedFlag(pps->getPPSId()) ) {
			EXIT("Error - a new PPS has been decoded while processing a picture");
		}
		for (i = 0; i < ALF_CTB_MAX_NUM_APS; i++) {
			APS* aps = m_parameterSetManager.getAPS(i, ALF_APS);
			if (aps && m_parameterSetManager.getAPSChangedFlag(i, ALF_APS)) {
				EXIT("Error - a new APS has been decoded while processing a picture");
			}
		}

		if (lmcsAPS && m_parameterSetManager.getAPSChangedFlag(lmcsAPS->getAPSId(), LMCS_APS) ) {
			EXIT("Error - a new LMCS APS has been decoded while processing a picture");
		}
		if ( scalinglistAPS && m_parameterSetManager.getAPSChangedFlag( scalinglistAPS->getAPSId(), SCALING_LIST_APS ) ) {
			EXIT( "Error - a new SCALING LIST APS has been decoded while processing a picture" );
		}

		activateAPS(&m_picHeader, pSlice, m_parameterSetManager, apss, lmcsAPS, scalinglistAPS);

		m_pcPic->cs->lmcsAps = lmcsAPS;
		m_pcPic->cs->scalinglistAps = scalinglistAPS;

		xParsePrefixSEImessages();

		// Check if any new SEI has arrived
		if (!m_SEIs.empty()) {
			// Currently only decoding Unit SEI message occurring between VCL NALUs copied
			SEIMessages& picSEI = m_pcPic->SEIs;
			SEIMessages decodingUnitInfos = extractSeisByType( picSEI, SEI::DECODING_UNIT_INFO);
			picSEI.insert(picSEI.end(), decodingUnitInfos.begin(), decodingUnitInfos.end());
			deleteSEIs(m_SEIs);
		}
		if (m_seiInclusionFlag) {
			checkParameterSetsInclusionSEIconstraints(nalu);
		}
#endif
	}
#ifdef TO_DO
	xCheckParameterSetConstraints(layerId);
#endif
}

int getLayerIdInOls(Slice* slice, int i, int j)
{
	cvector *v = (cvector*)cvector_get(&slice->m_pcVPS->m_layerIdInOls, i);
	return (int64_t)cvector_get(v, j);
}

bool isIRAP(Slice *slice)
{
	return (slice->m_eNalUnitType >= NAL_UNIT_CODED_SLICE_IDR_W_RADL) && (slice->m_eNalUnitType <= NAL_UNIT_CODED_SLICE_CRA);
}

void xCreateLostPicture(DecLib *p_declib, int iLostPoc, const int layerId )
{
	//msg( INFO, "\ninserting lost poc : %d\n",iLostPoc);
	//Picture *cFillPic = xGetNewPicBuffer(p_declib, *( m_parameterSetManager.getFirstSPS() ), *( m_parameterSetManager.getFirstPPS() ), 0, layerId );
	Picture* cFillPic = xGetNewPicBuffer(p_declib, p_declib->a_cur_sps, p_declib->a_cur_pps, 0, layerId );
	int ii, closestPoc;
	hevc_print(p_declib->hw, H266_DEBUG_DETAIL,
		"\ninserting lost poc : %d\n",iLostPoc);

	//CHECK( !cFillPic->slices.size(), "No slices in picture" );
	cFillPic->slices[0] = new_slice();
	initSlice(cFillPic->slices[0]);

	//PicList::iterator iterPic = m_cListPic.begin();
	closestPoc = 1000000;
	//while ( iterPic != m_cListPic.end())
	//{
	//  Picture * rpcPic = *(iterPic++);
	for (ii = 0; ii < PIC_LIST_SIZE; ii++) {
		Picture* rpcPic = p_declib->m_cListPic.pic[ii];
		if (rpcPic == NULL)
		break;
		if (abs(rpcPic->poc - iLostPoc) < closestPoc && abs(rpcPic->poc -iLostPoc) != 0 && rpcPic->poc != p_declib->m_apcSlicePilot->m_iPOC) {
			closestPoc = abs(rpcPic->poc -iLostPoc);
		}
	}
	//iterPic = m_cListPic.begin();
	//while ( iterPic != m_cListPic.end())
	//{
	//  Picture *rpcPic = *(iterPic++);
	for (ii = 0; ii < PIC_LIST_SIZE; ii++) {
		Picture* rpcPic = p_declib->m_cListPic.pic[ii];
		if (rpcPic == NULL)
			break;
		if (abs(rpcPic->poc - iLostPoc) == closestPoc && rpcPic->poc != p_declib->m_apcSlicePilot->m_iPOC) {
			hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "copying picture %d to %d (%d)\n",rpcPic->poc ,iLostPoc,p_declib->m_apcSlicePilot->m_iPOC);
			//cFillPic->getRecoBuf().copyFrom( rpcPic->getRecoBuf() );
			break;
		}
	}

	//  for (int ctuRsAddr=0; ctuRsAddr<cFillPic->getNumberOfCtusInFrame(); ctuRsAddr++)  { cFillPic->getCtu(ctuRsAddr)->initCtu(cFillPic, ctuRsAddr); }
	cFillPic->referenced = true;
	cFillPic->slices[0]->m_iPOC=iLostPoc;
	xUpdatePreviousTid0POC(p_declib, cFillPic->slices[0]);
	cFillPic->reconstructed = true;
	cFillPic->neededForOutput = true;
	if (p_declib->m_pocRandomAccess == MAX_INT) {
	p_declib->m_pocRandomAccess = iLostPoc;
#ifdef AML
	cFillPic->buf_cfg = pic_buf_cfg_alloc(p_declib->hw);
#endif
	}
}

void xCreateUnavailablePicture(DecLib *p_declib, PPS *pps, const int iUnavailablePoc, const bool longTermFlag, const int temporalId, const int layerId, const bool interLayerRefPicFlag )
{
	//Picture* cFillPic = xGetNewPicBuffer(p_declib, *( m_parameterSetManager.getFirstSPS() ), *( m_parameterSetManager.getFirstPPS() ), 0, layerId );
	Picture* cFillPic = xGetNewPicBuffer(p_declib, p_declib->a_cur_sps, p_declib->a_cur_pps, 0, layerId );
	hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "Note: Inserting unavailable POC : %d\n", iUnavailablePoc);

	/*
	cFillPic->cs = new CodingStructure( g_globalUnitCache.cuCache, g_globalUnitCache.puCache, g_globalUnitCache.tuCache );
	cFillPic->cs->sps = m_parameterSetManager.getFirstSPS();
	cFillPic->cs->pps = m_parameterSetManager.getFirstPPS();
	cFillPic->cs->vps = m_parameterSetManager.getVPS(0);
	cFillPic->cs->create(cFillPic->cs->sps->getChromaFormatIdc(), Area(0, 0, cFillPic->cs->pps->getPicWidthInLumaSamples(), cFillPic->cs->pps->getPicHeightInLumaSamples()), true, (bool)(cFillPic->cs->sps->getPLTMode()));
	cFillPic->allocateNewSlice();
	*/
	cFillPic->cs = malloc(sizeof(CodingStructure)); //new CodingStructure( g_globalUnitCache.cuCache, g_globalUnitCache.puCache, g_globalUnitCache.tuCache );
	memset(cFillPic->cs, 0, sizeof(CodingStructure));
	cFillPic->cs->sps = p_declib->a_cur_sps;
	cFillPic->cs->pps = p_declib->a_cur_pps;
	cFillPic->cs->vps = p_declib->a_cur_vps;

	cFillPic->slices[0] = new_slice();
	initSlice(cFillPic->slices[0]);

	cFillPic->m_decodingOrderNumber = 0;
	cFillPic->subLayerNonReferencePictureDueToSTSA = false;
	cFillPic->unscaledPic = cFillPic;

#ifdef TO_DO
	uint32_t yFill = 1 << (m_parameterSetManager.getFirstSPS()->getBitDepth(CHANNEL_TYPE_LUMA) - 1);
	uint32_t cFill = 1 << (m_parameterSetManager.getFirstSPS()->getBitDepth(CHANNEL_TYPE_CHROMA) - 1);
	cFillPic->getRecoBuf().Y().fill(yFill);
	cFillPic->getRecoBuf().Cb().fill(cFill);
	cFillPic->getRecoBuf().Cr().fill(cFill);
#endif
	//  for (int ctuRsAddr=0; ctuRsAddr<cFillPic->getNumberOfCtusInFrame(); ctuRsAddr++)  { cFillPic->getCtu(ctuRsAddr)->initCtu(cFillPic, ctuRsAddr); }
	cFillPic->referenced = true;
	cFillPic->interLayerRefPicFlag = interLayerRefPicFlag;
	cFillPic->longTerm = longTermFlag;
	cFillPic->slices[0]->m_iPOC = iUnavailablePoc;
	cFillPic->poc = iUnavailablePoc;
	if ( (cFillPic->slices[0]->m_uiTLayer == 0) && (cFillPic->slices[0]->m_eNalUnitType != NAL_UNIT_CODED_SLICE_RASL) &&
		(cFillPic->slices[0]->m_eNalUnitType != NAL_UNIT_CODED_SLICE_RADL) ) {
		p_declib->m_prevTid0POC = cFillPic->slices[0]->m_iPOC;
	}

	cFillPic->reconstructed = true;
	cFillPic->neededForOutput = false;
	// picture header is not derived for generated reference picture
	cFillPic->slices[0]->m_pcPicHeader = nullptr;
	cFillPic->temporalId = temporalId;
	cFillPic->nonReferencePictureFlag = false;
	cFillPic->slices[0]->m_pcPPS = pps;

	if (p_declib->m_pocRandomAccess == MAX_INT) {
		p_declib->m_pocRandomAccess = iUnavailablePoc;
	}
#ifdef AML
	cFillPic->buf_cfg = pic_buf_cfg_alloc(p_declib->hw);
#endif
}

#ifdef TO_DO
void checkPicTypeAfterEos(DecLib *p_declib)
{
	int layerId = p_declib->m_pcPic->slices[0]->m_nuhLayerId;
	if (p_declib->m_prevEOS[layerId]) {
		bool isIrapOrGdrPu = !p_declib->m_pcPic->cs->pps->m_mixedNaluTypesInPicFlag &&
			( isIRAP(p_declib->m_pcPic->slices[0]) || p_declib->m_pcPic->slices[0]->m_eNalUnitType == NAL_UNIT_CODED_SLICE_GDR );
		//CHECK(!isIrapOrGdrPu, "when present, the next PU of a particular layer after an EOS NAL unit that belongs to the same layer shall be an IRAP or GDR PU");

		p_declib->m_prevEOS[layerId] = false;
	}
}
#endif

int getNumRefEntries(ReferencePictureList *ref_list)
{
	return ref_list->m_numberOfShorttermPictures + ref_list->m_numberOfLongtermPictures + ref_list->m_numberOfInterLayerPictures;
}

Picture* xGetRefPic(Slice *slice, PicList *rcListPic, const int poc, const int layerId )
{
	// return a nullptr, if picture is not found
	Picture* refPic = nullptr;
	int i;
	//for ( auto &currPic : rcListPic )
	for (i = 0; i < PIC_LIST_SIZE; i++) {
		Picture* currPic = rcListPic->pic[i];
		if (currPic == NULL)
			break;
		if ( currPic->poc == poc && currPic->layerId == layerId ) {
			refPic = currPic;
			break;
		}
	}
	return  refPic;
}

Picture* xGetLongTermRefPicCandidate(Slice *slice, PicList *rcListPic, const int poc, const bool pocHasMsb, const int layerId )
{
	// return a nullptr, if picture is not found (might be a short-term or a long-term)
	Picture*  refPic = nullptr;
	const int pocCycle = 1 << slice->m_pcSPS->m_uiBitsForPOC;

	const int refPoc = pocHasMsb ? poc : (poc & (pocCycle - 1));
	int i;
	//for ( auto &currPic : rcListPic )
	for (i = 0; i < PIC_LIST_SIZE; i++) {
		Picture* currPic = rcListPic->pic[i];
		if (currPic == NULL)
			break;
		if ( currPic->poc != slice->m_iPOC && currPic->referenced && currPic->layerId == layerId ) {
			int currPicPoc = pocHasMsb ? currPic->poc : (currPic->poc & (pocCycle - 1));
			if (refPoc == currPicPoc) {
				refPic = currPic;
				break;
			}
		}
	}

	return refPic;
}


void print_ref_pic_list(DecLib *p_declib, Slice *slice)
{
	int ii;
	hevc_print(p_declib->hw, H266_DEBUG_DETAIL,
		"cur poc %d, slice adr %d, slice type %d, layer %d\n",
		slice->m_iPOC, slice->m_sliceMap.m_sliceID, slice->m_eSliceType, slice->m_pcPic->layerId);
	//if (slice->m_iPOC == 76)
	//  printk("to debug");
	hevc_print(p_declib->hw, H266_DEBUG_DETAIL,
		"list0 (%d): ", slice->m_aiNumRefIdx[REF_PIC_LIST_0]);

	for (ii = 0; ii < slice->m_aiNumRefIdx[REF_PIC_LIST_0]; ii++) {
		if (slice->m_apcRefPicList[REF_PIC_LIST_0][ii])
			hevc_print_cont(p_declib->hw, H266_DEBUG_DETAIL, "%d%s",
			slice->m_apcRefPicList[REF_PIC_LIST_0][ii]->poc, slice->m_bIsUsedAsLongTerm[REF_PIC_LIST_0][ii]?"(LT) ":" ");
		else
			hevc_print_cont(p_declib->hw, H266_DEBUG_DETAIL, "NULL");
	}

	hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "\n");

	hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "list1 (%d): ",
		slice->m_aiNumRefIdx[REF_PIC_LIST_1]);

	for (ii = 0; ii < slice->m_aiNumRefIdx[REF_PIC_LIST_1]; ii++) {
		if (slice->m_apcRefPicList[REF_PIC_LIST_1][ii])
			hevc_print_cont(p_declib->hw, H266_DEBUG_DETAIL, "%d%s",
			slice->m_apcRefPicList[REF_PIC_LIST_1][ii]->poc, slice->m_bIsUsedAsLongTerm[REF_PIC_LIST_1][ii]?"(LT) ":" ");
		else
			hevc_print_cont(p_declib->hw, H266_DEBUG_DETAIL, "NULL");
	}
	hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "\n");
}

void constructRefPicList(DecLib *p_declib, Slice *slice, PicList *rcListPic)
{
	Picture*  pcRefPic = NULL;
	uint32_t numOfActiveRef = 0;
	int layerIdx, ii, i;
	//memset(slice->m_bIsUsedAsLongTerm, 0, sizeof(slice->m_bIsUsedAsLongTerm));
	for (i = 0; i < NUM_REF_PIC_LIST_01; i++) {
		for (ii = 0; ii < (MAX_NUM_REF + 1); ii++)
			slice->m_bIsUsedAsLongTerm[i][ii] = 0;
	}
	if (slice->m_eSliceType == I_SLICE) {
		for (i = 0; i < NUM_REF_PIC_LIST_01; i++) {
			slice->m_aiNumRefIdx[i] = 0;
			for (ii = 0; ii < (MAX_NUM_REF + 1); ii++)
				slice->m_apcRefPicList[i][ii] = NULL;
		}
		//memset(slice->m_apcRefPicList, 0, sizeof(slice->m_apcRefPicList));
		//memset(slice->m_aiNumRefIdx, 0, sizeof(slice->m_aiNumRefIdx));
		return;
	}

	//construct L0
	numOfActiveRef = slice->m_aiNumRefIdx[REF_PIC_LIST_0];
	layerIdx = slice->m_pcPic->cs->vps == nullptr ? 0 : slice->m_pcPic->cs->vps->m_generalLayerIdx[slice->m_pcPic->layerId];
	hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "%s for L0:\n", __func__);
	for (ii = 0; ii < getNumRefEntries(&slice->m_RPL0); ii++) {
		if (slice->m_RPL0.m_isInterLayerRefPic[ii]) {
		//CHECK( m_RPL0.m_interLayerRefPicIdx[ii] == NOT_VALID, "Wrong ILRP index" );

		int refLayerId = slice->m_pcPic->cs->vps->m_vpsLayerId[slice->m_pcPic->cs->vps->m_directRefLayerIdx[layerIdx][slice->m_RPL0.m_interLayerRefPicIdx[ii] ] ];

		hevc_print(p_declib->hw, 0, "%d=> interlayer ref_layer %d ref poc %d\n", ii, refLayerId, slice->m_iPOC);
		hevc_print(p_declib->hw, 0, "Error, interlayer reference not supported!!\n");
		return; //exit(0);
		pcRefPic = xGetRefPic(slice, rcListPic, slice->m_iPOC, refLayerId );
		pcRefPic->longTerm = true;
		} else if (!slice->m_RPL0.m_isLongtermRefPic[ii]) {
			hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "%d=> ST ref_layer %d ref poc %d (%d,%d)\n", ii, slice->m_pcPic->layerId, slice->m_iPOC + slice->m_RPL0.m_refPicIdentifier[ii],
			slice->m_iPOC, slice->m_RPL0.m_refPicIdentifier[ii]);
			pcRefPic = xGetRefPic(slice, rcListPic, slice->m_iPOC + slice->m_RPL0.m_refPicIdentifier[ii], slice->m_pcPic->layerId);
			pcRefPic->longTerm = false;
		} else {
			int pocBits = slice->m_pcSPS->m_uiBitsForPOC;
			int pocMask = (1 << pocBits) - 1;
			int ltrpPoc = slice->m_RPL0.m_refPicIdentifier[ii] & pocMask;
			if  (slice->m_RPL0.m_deltaPocMSBPresentFlag[ii]) {
				ltrpPoc += slice->m_iPOC - slice->m_RPL0.m_deltaPOCMSBCycleLT[ii] * (pocMask + 1) - (slice->m_iPOC & pocMask);
			}
			hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "%d=> LT ref_layer %d ref poc %d\n", ii, slice->m_pcPic->layerId, ltrpPoc);
			pcRefPic = xGetLongTermRefPicCandidate(slice, rcListPic, ltrpPoc, slice->m_RPL0.m_deltaPocMSBPresentFlag[ii], slice->m_pcPic->layerId );
			pcRefPic->longTerm = true;
		}
		if (ii < numOfActiveRef) {
#ifdef TO_DO
			pcRefPic->extendPicBorder( slice->m_pcPPS );
#endif
			slice->m_apcRefPicList[REF_PIC_LIST_0][ii] = pcRefPic;
			slice->m_bIsUsedAsLongTerm[REF_PIC_LIST_0][ii] = pcRefPic->longTerm;
		}
	}

	//construct L1
	numOfActiveRef = slice->m_aiNumRefIdx[REF_PIC_LIST_1];
	hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "%s for L1:\n", __func__);
	for (ii = 0; ii < getNumRefEntries(&slice->m_RPL1); ii++) {
		if ( slice->m_RPL1.m_isInterLayerRefPic[ii] ) {
			//CHECK( m_RPL1.m_interLayerRefPicIdx[ii] == NOT_VALID, "Wrong ILRP index" );

			int refLayerId = slice->m_pcPic->cs->vps->m_vpsLayerId[slice->m_pcPic->cs->vps->m_directRefLayerIdx[layerIdx][slice->m_RPL1.m_interLayerRefPicIdx[ii] ] ];

			hevc_print(p_declib->hw, 0, "%d=> interlayer ref_layer %d ref poc %d\n", ii, refLayerId, slice->m_iPOC);
			hevc_print(p_declib->hw, 0, "Error, interlayer reference not supported!!\n");
			return; //exit(0);
			pcRefPic = xGetRefPic(slice, rcListPic, slice->m_iPOC, refLayerId );
			pcRefPic->longTerm = true;
		} else if (!slice->m_RPL1.m_isLongtermRefPic[ii]) {
			hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "%d=> ST ref_layer %d ref poc %d (%d,%d)\n", ii, slice->m_pcPic->layerId, slice->m_iPOC + slice->m_RPL1.m_refPicIdentifier[ii],
			slice->m_iPOC, slice->m_RPL1.m_refPicIdentifier[ii]);
			pcRefPic = xGetRefPic(slice, rcListPic, slice->m_iPOC + slice->m_RPL1.m_refPicIdentifier[ii], slice->m_pcPic->layerId);
			pcRefPic->longTerm = false;
		} else {
			int pocBits = slice->m_pcSPS->m_uiBitsForPOC;
			int pocMask = (1 << pocBits) - 1;
			int ltrpPoc = slice->m_RPL1.m_refPicIdentifier[ii] & pocMask;
			if (slice->m_RPL1.m_deltaPocMSBPresentFlag[ii]) {
				ltrpPoc += slice->m_iPOC - slice->m_RPL1.m_deltaPOCMSBCycleLT[ii] * (pocMask + 1) - (slice->m_iPOC & pocMask);
			}
			hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "%d=> LT ref_layer %d ref poc %d\n", ii, slice->m_pcPic->layerId, ltrpPoc);
			pcRefPic = xGetLongTermRefPicCandidate(slice, rcListPic, ltrpPoc, slice->m_RPL1.m_deltaPocMSBPresentFlag[ii], slice->m_pcPic->layerId );
			pcRefPic->longTerm = true;
		}
		if (ii < numOfActiveRef) {
#ifdef TO_DO
			pcRefPic->extendPicBorder( slice->m_pcPPS );
#endif
			slice->m_apcRefPicList[REF_PIC_LIST_1][ii] = pcRefPic;
			slice->m_bIsUsedAsLongTerm[REF_PIC_LIST_1][ii] = pcRefPic->longTerm;
		}
	}
}

void clearSliceBuffer(Picture *pic)
{
#ifdef TO_DO
	uint32_t i;
	for (i = 0; i < SLICE_MAX_NUM; i++) {
		Slice *slice = pic->slice[i];
		if (slice == NULL)
			break;

		//delete slices[i];
	}
	//slices.clear();
#endif
}

void finalInit(Picture *pic, VPS* vps, SPS *sps, PPS *pps, PicHeader *picHeader) //, APS** alfApss, APS* lmcsAps, APS* scalingListAps )
{
#ifdef TO_DO
	for ( auto &sei : SEIs ) {
		delete sei;
	}
	SEIs.clear();
	clearSliceBuffer(pic);

	const ChromaFormat chromaFormatIDC = sps.getChromaFormatIdc();
	const int          iWidth = pps.getPicWidthInLumaSamples();
	const int          iHeight = pps.getPicHeightInLumaSamples();
#else
	clearSliceBuffer(pic);
#endif
	if ( pic->cs ) {
#ifdef TO_DO
		pic->cs->initStructData();
#endif
	} else {
		pic->cs = malloc(sizeof(CodingStructure)); //new CodingStructure( g_globalUnitCache.cuCache, g_globalUnitCache.puCache, g_globalUnitCache.tuCache );
		memset(pic->cs, 0, sizeof(CodingStructure));
		pic->cs->sps = sps;
		//cs->create(chromaFormatIDC, Area(0, 0, iWidth, iHeight), true, (bool)sps.getPLTMode());
	}
	pic->cs->vps = vps;
	pic->cs->picture = pic;
	pic->cs->slice   = nullptr;  // the slices for this picture have not been set at this point. update cs->slice after swapSliceObject()
	pic->cs->pps     = pps;
	picHeader->m_spsId = sps->m_SPSId;
	picHeader->m_ppsId = pps->m_PPSId;
	pic->cs->picHeader = picHeader;
#ifdef TO_DO
	memcpy(cs->alfApss, alfApss, sizeof(cs->alfApss));
	cs->lmcsAps = lmcsAps;
	cs->scalinglistAps = scalingListAps;
	cs->pcv     = pps.pcv;
	m_conformanceWindow = pps.getConformanceWindow();
	m_scalingWindow = pps.getScalingWindow();
#endif
	pic->mixedNaluTypesInPicFlag = pps->m_mixedNaluTypesInPicFlag;
	pic->nonReferencePictureFlag = picHeader->m_nonReferencePictureFlag;
#ifdef TO_DO
	if (pic->m_spliceIdx == NULL) {
		pic->m_ctuNums = pic->cs->pcv->sizeInCtus;
		pic->m_spliceIdx = new int[m_ctuNums];
		memset(pic->m_spliceIdx, 0, m_ctuNums * sizeof(int));
	}
#endif
}

void freeScaledRefPicList(Slice *slice, Picture *scaledRefPic[] )
{
	int i;

	if (slice->m_eSliceType == I_SLICE) {
		return;
	}
	for (i = 0; i < MAX_NUM_REF; i++) {
		if ( scaledRefPic[i] != nullptr ) {
			free_picture(scaledRefPic[i]); //->destroy();
			scaledRefPic[i] = nullptr;
		}
	}
}

const int SPS_m_winUnitX[]={1,2,2,1};
const int SPS_m_winUnitY[]={1,2,1,1};

static bool isRefScaled(Picture *pic, const PPS* pps )
{
	if (pic->unscaledPic->buf_cfg == NULL) {
		hevc_print(NULL, 0, "Error %s, pic->unscaledPic->buf_cfg is NULL\n", __func__);
		return 0;
	}
	return  pic->unscaledPic->buf_cfg->width != pps->m_picWidthInLumaSamples ||
		pic->unscaledPic->buf_cfg->height != pps->m_picHeightInLumaSamples ||
		getWindowLeftOffset(&pic->m_scalingWindow) != getWindowLeftOffset(&pps->m_scalingWindow)  ||
		getWindowRightOffset(&pic->m_scalingWindow)  != getWindowRightOffset(&pps->m_scalingWindow) ||
		getWindowTopOffset(&pic->m_scalingWindow)    != getWindowTopOffset(&pps->m_scalingWindow)   ||
		getWindowBottomOffset(&pic->m_scalingWindow) != getWindowBottomOffset(&pps->m_scalingWindow);
}

bool getRprScaling( const SPS* sps, const PPS* curPPS, Picture* refPic, int* xScale, int* yScale )
{
	const int subWidthC  = SPS_m_winUnitX[sps->m_chromaFormatIdc];
	const int subHeightC = SPS_m_winUnitY[sps->m_chromaFormatIdc];

	const Window *curScalingWindow = &curPPS->m_scalingWindow; //getScalingWindow();

	const int curLeftOffset   = subWidthC * getWindowLeftOffset(curScalingWindow);
	const int curRightOffset  = subWidthC * getWindowRightOffset(curScalingWindow);
	const int curTopOffset    = subHeightC * getWindowTopOffset(curScalingWindow);
	const int curBottomOffset = subHeightC * getWindowBottomOffset(curScalingWindow);

	// Note: 64-bit integers are used for sizes such as to avoid possible overflows in corner cases
	const int64_t curPicScalWinWidth  = curPPS->m_picWidthInLumaSamples - (curLeftOffset + curRightOffset);
	const int64_t curPicScalWinHeight = curPPS->m_picHeightInLumaSamples - (curTopOffset + curBottomOffset);

	Window *refScalingWindow = &refPic->m_scalingWindow; //getScalingWindow();

	const int refLeftOffset   = subWidthC * getWindowLeftOffset(refScalingWindow);
	const int refRightOffset  = subWidthC * getWindowRightOffset(refScalingWindow);
	const int refTopOffset    = subHeightC * getWindowTopOffset(refScalingWindow);
	const int refBottomOffset = subHeightC * getWindowBottomOffset(refScalingWindow);

	int64_t refPicScalWinWidth;
	int64_t refPicScalWinHeight;
	if (refPic->buf_cfg == NULL) {
		hevc_print(NULL, 0, "Error %s, refPic->buf_cfg is NULL\n", __func__);
		return 0;
	}
	refPicScalWinWidth  = refPic->buf_cfg->width - (refLeftOffset + refRightOffset);
	refPicScalWinHeight = refPic->buf_cfg->height - (refTopOffset + refBottomOffset);
#if 0
	CHECK(curPicScalWinWidth * 2 < refPicScalWinWidth,
	"curPicScalWinWidth * 2 shall be greater than or equal to refPicScalWinWidth");
	CHECK(curPicScalWinHeight * 2 < refPicScalWinHeight,
	"curPicScalWinHeight * 2 shall be greater than or equal to refPicScalWinHeight");
	CHECK(curPicScalWinWidth > refPicScalWinWidth * 8,
	"curPicScalWinWidth shall be less than or equal to refPicScalWinWidth * 8");
	CHECK(curPicScalWinHeight > refPicScalWinHeight * 8,
	"curPicScalWinHeight shall be less than or equal to refPicScalWinHeight * 8");
#endif
	*xScale = (int) (div64_u64(((refPicScalWinWidth << SCALE_RATIO_BITS) + (curPicScalWinWidth >> 1)), curPicScalWinWidth));
	*yScale = (int) (div64_u64(((refPicScalWinHeight << SCALE_RATIO_BITS) + (curPicScalWinHeight >> 1)), curPicScalWinHeight));

#if 0
	const int maxPicWidth  = sps->getMaxPicWidthInLumaSamples();    // sps_pic_width_max_in_luma_samples
	const int maxPicHeight = sps->getMaxPicHeightInLumaSamples();   // sps_pic_height_max_in_luma_samples
	const int curPicWidth  = curPPS->getPicWidthInLumaSamples();    // pps_pic_width_in_luma_samples
	const int curPicHeight = curPPS->getPicHeightInLumaSamples();   // pps_pic_height_in_luma_samples

	const int picSizeIncrement = std::max((int) 8, (1 << sps->getLog2MinCodingBlockSize()));   // Max(8, MinCbSizeY)

	CHECK((curPicScalWinWidth * maxPicWidth) < refPicScalWinWidth * (curPicWidth - picSizeIncrement),
	"(curPicScalWinWidth * maxPicWidth) should be greater than or equal to refPicScalWinWidth * (curPicWidth - "
	"picSizeIncrement))");
	CHECK((curPicScalWinHeight * maxPicHeight) < refPicScalWinHeight * (curPicHeight - picSizeIncrement),
	"(curPicScalWinHeight * maxPicHeight) should be greater than or equal to refPicScalWinHeight * (curPicHeight - "
	"picSizeIncrement))");

	CHECK(curLeftOffset < -curPicWidth * 15, "The value of SubWidthC * pps_scaling_win_left_offset shall be greater "
	                       "than or equal to -pps_pic_width_in_luma_samples * 15");
	CHECK(curLeftOffset >= curPicWidth,
	"The value of SubWidthC * pps_scaling_win_left_offset shall be less than pps_pic_width_in_luma_samples");
	CHECK(curRightOffset < -curPicWidth * 15, "The value of SubWidthC * pps_scaling_win_right_offset shall be greater "
	                        "than or equal to -pps_pic_width_in_luma_samples * 15");
	CHECK(curRightOffset >= curPicWidth,
	"The value of SubWidthC * pps_scaling_win_right_offset shall be less than pps_pic_width_in_luma_samples");

	CHECK(curTopOffset < -curPicHeight * 15, "The value of SubHeightC * pps_scaling_win_top_offset shall be greater "
	                       "than or equal to -pps_pic_height_in_luma_samples * 15");
	CHECK(curTopOffset >= curPicHeight,
	"The value of SubHeightC * pps_scaling_win_top_offset shall be less than pps_pic_height_in_luma_samples");
	CHECK(curBottomOffset < (-curPicHeight) * 15, "The value of SubHeightC * pps_scaling_win_bottom_offset shall be "
	                            "greater than or equal to -pps_pic_height_in_luma_samples * 15");
	CHECK(curBottomOffset >= curPicHeight,
	"The value of SubHeightC * pps_scaling_win_bottom_offset shall be less than pps_pic_height_in_luma_samples");

	CHECK(curLeftOffset + curRightOffset < -curPicWidth * 15,
	"The value of SubWidthC * ( pps_scaling_win_left_offset + pps_scaling_win_right_offset ) shall be greater than "
	"or equal to -pps_pic_width_in_luma_samples * 15");
	CHECK(curLeftOffset + curRightOffset >= curPicWidth,
	"The value of SubWidthC * ( pps_scaling_win_left_offset + pps_scaling_win_right_offset ) shall be less than "
	"pps_pic_width_in_luma_samples");
	CHECK(curTopOffset + curBottomOffset < -curPicHeight * 15,
	"The value of SubHeightC * ( pps_scaling_win_top_offset + pps_scaling_win_bottom_offset ) shall be greater "
	"than or equal to -pps_pic_height_in_luma_samples * 15");
	CHECK(curTopOffset + curBottomOffset >= curPicHeight,
	"The value of SubHeightC * ( pps_scaling_win_top_offset + pps_scaling_win_bottom_offset ) shall be less than "
	"pps_pic_height_in_luma_samples");

#endif
	return isRefScaled(refPic, curPPS );
}

void scaleRefPicList(DecLib *p_declib, Slice *slice, Picture *scaledRefPic[ ], PicHeader *picHeader, //APS** apss, APS* lmcsAps, APS* scalingListAps,
    const bool isDecoder )
{
	int i;
	int refList;
	int rIdx;
	SPS* sps = slice->m_pcSPS; //getSPS();
	PPS* pps = slice->m_pcPPS; //getPPS();

	bool refPicIsSameRes = false;

	// this is needed for IBC
	slice->m_pcPic->unscaledPic = slice->m_pcPic;

	if ( slice->m_eSliceType == I_SLICE ) {
		return;
	}
	freeScaledRefPicList(slice, scaledRefPic );

	for (refList = 0; refList < NUM_REF_PIC_LIST_01; refList++) {
		if ( refList == 1 && slice->m_eSliceType != B_SLICE ) {
			continue;
		}

		for (rIdx = 0; rIdx < slice->m_aiNumRefIdx[refList]; rIdx++) {
			// if rescaling is needed, otherwise just reuse the original picture pointer; it is needed for motion field, otherwise motion field requires a copy as well
			// reference resampling for the whole picture is not applied at decoder

			int xScale=0, yScale=0;
			getRprScaling( sps, pps, slice->m_apcRefPicList[refList][rIdx], &xScale, &yScale );
			hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "%s refList %d rIdx %d: xScale %d yScale %d\n", __func__, refList, rIdx, xScale, yScale);
			slice->m_scalingRatio[refList][rIdx].first = xScale; // std::pair<int, int>( xScale, yScale );
			slice->m_scalingRatio[refList][rIdx].second = yScale;
			CHECK( m_apcRefPicList[refList][rIdx]->unscaledPic == nullptr, "unscaledPic is not properly set" );
			if (isRefScaled(slice->m_apcRefPicList[refList][rIdx], pps) == false ) {
				refPicIsSameRes = true;
			}
			//if ( slice->m_scalingRatio[refList][rIdx] == SCALE_1X || isDecoder )
			if ( (slice->m_scalingRatio[refList][rIdx].first == (1 << SCALE_RATIO_BITS) && slice->m_scalingRatio[refList][rIdx].second == (1 << SCALE_RATIO_BITS))
			|| isDecoder ) {
				slice->m_scaledRefPicList[refList][rIdx] = slice->m_apcRefPicList[refList][rIdx];
			} else {
				int poc = slice->m_apcRefPicList[refList][rIdx]->poc;
				int layerId = slice->m_apcRefPicList[refList][rIdx]->layerId;

				// check whether the reference picture has already been scaled
				for (i = 0; i < MAX_NUM_REF; i++) {
					if ( scaledRefPic[i] != nullptr && scaledRefPic[i]->poc == poc && scaledRefPic[i]->layerId == layerId ) {
						break;
					}
				}

				if (i == MAX_NUM_REF) {
					int j;
					// search for unused Picture structure in scaledRefPic
					for (j = 0; j < MAX_NUM_REF; j++) {
						if ( scaledRefPic[j] == nullptr ) {
							break;
						}
					}

					CHECK( j >= MAX_NUM_REF, "scaledRefPic can not hold all reference pictures!" );

					if ( j >= MAX_NUM_REF ) {
						j = 0;
					}

					if ( scaledRefPic[j] == nullptr ) {
						scaledRefPic[j] = new_picture();

						scaledRefPic[j]->m_bIsBorderExtended = false;
						scaledRefPic[j]->reconstructed = false;
						scaledRefPic[j]->referenced = true;

						finalInit(scaledRefPic[j], slice->m_pcPic->cs->vps, sps, pps, picHeader); //, apss, lmcsAps, scalingListAps );

						scaledRefPic[j]->poc = NOT_VALID;

						//scaledRefPic[j]->create( sps->getChromaFormatIdc(), Size( pps->getPicWidthInLumaSamples(), pps->getPicHeightInLumaSamples() ), sps->getMaxCUWidth(), sps->getMaxCUWidth() + 16, isDecoder, layerId );
					}

					scaledRefPic[j]->poc = poc;
					scaledRefPic[j]->longTerm = slice->m_apcRefPicList[refList][rIdx]->longTerm;
#ifdef TO_DO

					// rescale the reference picture
					const bool downsampling = slice->m_apcRefPicList[refList][rIdx]->getRecoBuf().Y().width >= scaledRefPic[j]->getRecoBuf().Y().width && slice->m_apcRefPicList[refList][rIdx]->getRecoBuf().Y().height >= scaledRefPic[j]->getRecoBuf().Y().height;
					Picture::rescalePicture( slice->m_scalingRatio[refList][rIdx],
					   slice->m_apcRefPicList[refList][rIdx]->getRecoBuf(), slice->m_apcRefPicList[refList][rIdx]->slices[0]->m_pcPPS->getScalingWindow(),
					   scaledRefPic[j]->getRecoBuf(), pps->getScalingWindow(),
					   sps->getChromaFormatIdc(), sps->getBitDepths(), true, downsampling,
					   sps->getHorCollocatedChromaFlag(), sps->getVerCollocatedChromaFlag() );
#endif
					scaledRefPic[j]->unscaledPic = slice->m_apcRefPicList[refList][rIdx];
#ifdef TO_DO
					scaledRefPic[j]->extendPicBorder( getPPS() );
#endif
					slice->m_scaledRefPicList[refList][rIdx] = scaledRefPic[j];
				} else {
					slice->m_scaledRefPicList[refList][rIdx] = scaledRefPic[i];
				}
			}
		}
	}

	// make the scaled reference picture list as the default reference picture list
	for (refList = 0; refList < NUM_REF_PIC_LIST_01; refList++) {
		if (refList == 1 && slice->m_eSliceType != B_SLICE ) {
			continue;
		}

		for (rIdx = 0; rIdx < slice->m_aiNumRefIdx[refList]; rIdx++) {
			slice->m_savedRefPicList[refList][rIdx] = slice->m_apcRefPicList[refList][rIdx];
			slice->m_apcRefPicList[refList][rIdx] = slice->m_scaledRefPicList[refList][rIdx];

			// allow the access of the unscaled version in xPredInterBlk()
			slice->m_apcRefPicList[refList][rIdx]->unscaledPic = slice->m_savedRefPicList[refList][rIdx];
		}
	}

	//Make sure that TMVP is disabled when there are no reference pictures with the same resolution
	if (!refPicIsSameRes) {
		CHECK(getPicHeader()->getEnableTMVPFlag() != 0, "TMVP cannot be enabled in pictures that have no reference pictures with the same resolution")
	}
}

ScaleRatio *getScalingRatio(Slice *slice, const enum RefPicList refPicList, const int refIdx )
{
	CHECK( refIdx < 0, "Invalid reference index" );
	return &slice->m_scalingRatio[refPicList][refIdx];
}

void setRefPOCList(DecLib *p_declib, Slice *slice)
{
	int iDir;
	hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "%s:", __func__);
	for (iDir = 0; iDir < NUM_REF_PIC_LIST_01; iDir++) {
		int iNumRefIdx;
		hevc_print_cont(p_declib->hw, H266_DEBUG_DETAIL, "list%d (", iDir);

		for (iNumRefIdx = 0; iNumRefIdx < slice->m_aiNumRefIdx[iDir]; iNumRefIdx++) {
			slice->m_aiRefPOCList[iDir][iNumRefIdx] = slice->m_apcRefPicList[iDir][iNumRefIdx]->poc;
			hevc_print_cont(p_declib->hw, H266_DEBUG_DETAIL, "%d ", slice->m_aiRefPOCList[iDir][iNumRefIdx]);
		}
		hevc_print_cont(p_declib->hw, H266_DEBUG_DETAIL, ") " );
	}
	hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "\n");
}

void setBiDirPred(DecLib *p_declib, Slice *slice, bool b, int refIdx0, int refIdx1)
{
	hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "setBiDirPred : m_biDirPred=%d, m_symRefIdx0=%d, m_symRefIdx1=%d\n", b, refIdx0, refIdx1);
	slice->m_biDirPred = b;
	slice->m_symRefIdx[0] = refIdx0;
	slice->m_symRefIdx[1] = refIdx1;
}

int checkThatAllRefPicsAreAvailable(DecLib *p_declib, Slice *slice, PicList *rcListPic, const ReferencePictureList *pRPL, int rplIdx, bool printErrors, int *refPicIndex, int numActiveRefPics)
{
	int i, ii;
	Picture* rpcPic;
	int isAvailable = 0;
	int notPresentPoc = 0;
	int numberOfPictures = numActiveRefPics;
	*refPicIndex = 0;

	if (slice->m_eNalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL)
		return 0; //Assume that all pic in the DPB will be flushed anyway so no need to check.

	//Check long term ref pics
	for (ii = 0; pRPL->m_numberOfLongtermPictures > 0 && ii < numberOfPictures; ii++) {
		if ( !pRPL->m_isLongtermRefPic[ii] || pRPL->m_isInterLayerRefPic[ii] ) {
			continue;
		}

		notPresentPoc = pRPL->m_refPicIdentifier[ii];
		isAvailable = 0;
		//PicList::iterator iterPic = rcListPic.begin();
		//while (iterPic != rcListPic.end())
		//{
		//  rpcPic = *(iterPic++);
		for (i = 0; i < PIC_LIST_SIZE; i++) {
			int pocCycle, curPoc, refPoc;
			rpcPic = rcListPic->pic[i];
			if (rpcPic == NULL)
				break;
			pocCycle = 1 << (rpcPic->cs->sps->m_uiBitsForPOC);
			curPoc = rpcPic->poc;
			refPoc = pRPL->m_refPicIdentifier[ii] & (pocCycle - 1);
			if (pRPL->m_deltaPocMSBPresentFlag[ii]) {
				refPoc += slice->m_iPOC - pRPL->m_deltaPOCMSBCycleLT[ii] * pocCycle - ( slice->m_iPOC & (pocCycle - 1));
			} else {
				curPoc = curPoc & (pocCycle - 1);
			}
			if (rpcPic->longTerm && curPoc == refPoc && rpcPic->referenced) {
				isAvailable = 1;
				break;
			}
		}
		// if there was no such long-term check the short terms
		if (!isAvailable) {
			//iterPic = rcListPic.begin();
			//while (iterPic != rcListPic.end())
			//{
			//  rpcPic = *(iterPic++);
			for (i = 0; i < PIC_LIST_SIZE; i++) {
				int pocCycle, curPoc, refPoc;
				rpcPic = rcListPic->pic[i];
				if (rpcPic == NULL)
					break;
				pocCycle = 1 << (rpcPic->cs->sps->m_uiBitsForPOC);
				curPoc = rpcPic->poc;
				refPoc = pRPL->m_refPicIdentifier[ii] & (pocCycle - 1);
				if (pRPL->m_deltaPocMSBPresentFlag[ii]) {
					refPoc +=  slice->m_iPOC - pRPL->m_deltaPOCMSBCycleLT[ii] * pocCycle - ( slice->m_iPOC & (pocCycle - 1));
				} else {
					curPoc = curPoc & (pocCycle - 1);
				}
				if (!rpcPic->longTerm && curPoc == refPoc && rpcPic->referenced) {
					isAvailable = 1;
					rpcPic->longTerm = true;
					break;
				}
			}
		}
		if (!isAvailable) {
			if (printErrors) {
				hevc_print(p_declib->hw, 0,
					"E.r.r.o.r.: Current picture: %d Long-term reference picture with POC = %3d seems to have been removed or not correctly decoded.\n",
					slice->m_iPOC, notPresentPoc);
			}
			*refPicIndex = ii;
			return notPresentPoc;
		}
	}
	//report that a picture is lost if it is in the Reference Picture List but not in the DPB

	isAvailable = 0;
	//Check short term ref pics
	for (ii = 0; ii < numberOfPictures; ii++) {
		if (pRPL->m_isLongtermRefPic[ii]) {
			continue;
		}

		notPresentPoc =  slice->m_iPOC + pRPL->m_refPicIdentifier[ii];
		isAvailable = 0;
		//PicList::iterator iterPic = rcListPic.begin();
		//while (iterPic != rcListPic.end())
		//{
		//  rpcPic = *(iterPic++);
		for (i = 0; i < PIC_LIST_SIZE; i++) {
			rpcPic = rcListPic->pic[i];
			if (rpcPic == NULL)
				break;
			if (!rpcPic->longTerm && rpcPic->poc ==  slice->m_iPOC + pRPL->m_refPicIdentifier[ii] && rpcPic->referenced) {
				isAvailable = 1;
				break;
			}
		}
		//report that a picture is lost if it is in the Reference Picture List but not in the DPB
		if (isAvailable == 0 && pRPL->m_numberOfShorttermPictures > 0) {
			if (printErrors) {
				hevc_print(p_declib->hw, 0,
					"E.r.r.o.r.: Current picture: %d Short-term reference picture with POC = %3d seems to have been removed or not correctly decoded.\n",
					slice->m_iPOC, notPresentPoc);
			}
			*refPicIndex = ii;
			return notPresentPoc;
		}
	}
	return 0;
}

#ifdef AML
static int calcPOC(DecLib *p_declib) //refer to c-model HLSyntaxReader::parseSliceHeader()
{
	param_t *param = p_declib->param;
	int ret_poc=0;
	// picture order count
	//uiCode = picHeader->getPocLsb();
	int iPOClsb = param->p.PocLsb;
	int bitforpoc = (param->p.sps_flag_0 >> 6) & 0x1f;
	int iMaxPOClsb = 1 << bitforpoc;
	int iPOCmsb;
	if (getIdrPicFlag(p_declib->m_apcSlicePilot)) {
		//if (picHeader->getPocMsbPresentFlag())
		if (param->p.PocMsb & 0x1) { //PocMsbPresentFlag
			iPOCmsb = param->p.PocMsbVal*iMaxPOClsb;
		} else {
			iPOCmsb = 0;
		}
		ret_poc = iPOCmsb + iPOClsb;
	} else {
		int iPrevPOC = p_declib->m_prevTid0POC;
		int iPrevPOClsb = iPrevPOC & (iMaxPOClsb - 1);
		int iPrevPOCmsb = iPrevPOC - iPrevPOClsb;
		//if (picHeader->getPocMsbPresentFlag())
		if (param->p.PocMsb & 0x1) { //PocMsbPresentFlag
			iPOCmsb = param->p.PocMsbVal*iMaxPOClsb;
		} else {
			if ((iPOClsb < iPrevPOClsb) && ((iPrevPOClsb - iPOClsb) >= (iMaxPOClsb / 2))) {
				iPOCmsb = iPrevPOCmsb + iMaxPOClsb;
			} else if ((iPOClsb > iPrevPOClsb) && ((iPOClsb - iPrevPOClsb) > (iMaxPOClsb / 2))) {
				iPOCmsb = iPrevPOCmsb - iMaxPOClsb;
			} else {
				iPOCmsb = iPrevPOCmsb;
			}
		}
		ret_poc = iPOCmsb + iPOClsb;
	}
	return ret_poc;
}
#endif

bool xDecodeSlice(DecApp *p_app, NALUnit *nalu)
{
	DecLib *p_declib = &p_app->m_cDecLib;
	Picture* scaledRefPic[MAX_NUM_REF] = {};
#ifdef AML
	param_t *param = p_declib->param;
	int rpl0_index = param->p.RPLidx & 0xff;
	int rpl1_index = (param->p.RPLidx >> 8) & 0xff;
#endif
	struct NalUnitInfo naluInfo;
	AccessUnitPicInfo picInfo;
	uint32_t uiIndependentSliceIdx = 0;
	PPS *pps;
	SPS *sps;
	VPS *vps;
	Slice* pcSlice;
	p_declib->m_apcSlicePilot->m_pcPicHeader = &p_declib->m_picHeader;

	initSlice(p_declib->m_apcSlicePilot); // the slice pilot is an object to prepare for a new slice
	    // it is not associated with picture, sps or pps structures.
	PRINT_LINE();

	if (p_declib->m_bFirstSliceInPicture) {
		p_declib->m_uiSliceSegmentIdx = 0;
	} else {
		//CHECK(nalu.m_nalUnitType != m_pcPic->slices[m_uiSliceSegmentIdx - 1]->m_eNalUnitType && !m_pcPic->cs->pps->m_mixedNaluTypesInPicFlag, "If pps_mixed_nalu_types_in_pic_flag is equal to 0, the value of NAL unit type shall be the same for all coded slice NAL units of a picture");
		copySliceInfo(p_declib->m_apcSlicePilot, p_declib->m_pcPic->slices[p_declib->m_uiSliceSegmentIdx-1], true);
	}
	PRINT_LINE();

	p_declib->m_apcSlicePilot->m_eNalUnitType = nalu->m_nalUnitType;
	p_declib->m_apcSlicePilot->m_nuhLayerId = nalu->m_nuhLayerId;
	p_declib->m_apcSlicePilot->m_uiTLayer = nalu->m_temporalId;
#ifdef AML
	//refer to VLCReader.cpp line 3934
	p_declib->m_apcSlicePilot->m_sliceMap.m_sliceID = param->p.sliceAddr;
	if (param->p.PocLsb == 14)
		hevc_print(p_declib->hw, H266_DEBUG_DETAIL,
		"poc is %d, sps_seq_parameter_set_id id %d\n",
		param->p.PocLsb, param->p.sps_seq_parameter_set_id);
#ifdef USE_FULL_REF_LIST_BUFFER
	if (rpl0_index < 64)
		update_rpl(&p_declib->m_apcSlicePilot->m_RPL0, &p_app->sps_RPL_set[param->p.sps_seq_parameter_set_id].RPL0_set[rpl0_index]);
	else
		update_rpl(&p_declib->m_apcSlicePilot->m_RPL0, &p_app->slice_RPL0_set);

	if (rpl1_index < 64)
		update_rpl(&p_declib->m_apcSlicePilot->m_RPL1, &p_app->sps_RPL_set[param->p.sps_seq_parameter_set_id].RPL1_set[rpl1_index]);
	else
		update_rpl(&p_declib->m_apcSlicePilot->m_RPL1, &p_app->slice_RPL1_set);
#else
	update_rpl(&p_declib->m_apcSlicePilot->m_RPL0, &p_app->RPL0_set[rpl0_index]);
	update_rpl(&p_declib->m_apcSlicePilot->m_RPL1, &p_app->RPL1_set[rpl1_index]);
#endif
	if ((param->p.slice_type != I_SLICE && getNumRefEntries(&p_declib->m_apcSlicePilot->m_RPL0) > 1) ||
	(param->p.slice_type == B_SLICE && getNumRefEntries(&p_declib->m_apcSlicePilot->m_RPL1) > 1)) {
		p_declib->m_apcSlicePilot->m_aiNumRefIdx[0] = param->p.NumRefIdx & 0x3f;
		p_declib->m_apcSlicePilot->m_aiNumRefIdx[1] = (param->p.NumRefIdx >> 6) & 0x3f;

		/*aml add*/
		p_declib->m_apcSlicePilot->m_aiNumRefIdx[0] =
		(param->p.slice_type == I_SLICE) ? 0 : p_declib->m_apcSlicePilot->m_aiNumRefIdx[0];
		p_declib->m_apcSlicePilot->m_aiNumRefIdx[1] =
		(param->p.slice_type == B_SLICE) ? p_declib->m_apcSlicePilot->m_aiNumRefIdx[1] : 0;
		/**/

	} else {
		p_declib->m_apcSlicePilot->m_aiNumRefIdx[0] = (param->p.slice_type == I_SLICE) ? 0 : 1;
		p_declib->m_apcSlicePilot->m_aiNumRefIdx[1] = (param->p.slice_type == B_SLICE) ? 1 : 0;
	}
	p_declib->m_apcSlicePilot->m_eSliceType = param->p.slice_type;
	//p_declib->m_apcSlicePilot->m_iPOC = param->p.PocLsb;
	p_declib->m_apcSlicePilot->m_iPOC = calcPOC(p_declib);
	p_declib->m_apcSlicePilot->m_colFromL0Flag = (param->p.slice_decoding_flags_0 >> 1) & 0x1;
	if (p_declib->m_apcSlicePilot->m_eSliceType != I_SLICE &&
	((p_declib->m_apcSlicePilot->m_colFromL0Flag == 1 && p_declib->m_apcSlicePilot->m_aiNumRefIdx[REF_PIC_LIST_0] > 1)||
	(p_declib->m_apcSlicePilot->m_colFromL0Flag == 0 && p_declib->m_apcSlicePilot->m_aiNumRefIdx[REF_PIC_LIST_1] > 1))) {
		/*refer to VLCReader.cpp line 4039*/
		p_declib->m_apcSlicePilot->m_colRefIdx = (param->p.slice_ph_decoding_flags_1 & 0x1f);
	} else {
		p_declib->m_apcSlicePilot->m_colRefIdx = 0;
	}

#if 0 //def MODIFY_CODE
	//force code, to remove...
	if (p_declib->decode_count == 1)
	p_declib->m_apcSlicePilot->m_bCheckLDC = 1;
#endif
#else
	for (auto& naluTemporalId : m_accessUnitNals) {
		if (naluTemporalId.m_nalUnitType != NAL_UNIT_OPI &&
		naluTemporalId.m_nalUnitType != NAL_UNIT_DCI
		&& naluTemporalId.m_nalUnitType != NAL_UNIT_VPS
		&& naluTemporalId.m_nalUnitType != NAL_UNIT_SPS
		&& naluTemporalId.m_nalUnitType != NAL_UNIT_EOS
		&& naluTemporalId.m_nalUnitType != NAL_UNIT_EOB) {
		CHECK( naluTemporalId.m_temporalId < nalu.m_temporalId,
			"TemporalId shall be greater than or equal to the TemporalId of the layer access unit containing the NAL unit" );
		}
	}

	if (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_GDR) {
		CHECK(nalu.m_temporalId != 0, "Current GDR picture has TemporalId not equal to 0");
	}

	m_HLSReader.setBitstream( &nalu.getBitstream() );
	m_apcSlicePilot->m_ccAlfFilterParam = m_cALF.getCcAlfFilterParam();
	m_HLSReader.parseSliceHeader( m_apcSlicePilot, &m_picHeader, &m_parameterSetManager, m_prevTid0POC, m_prevPicPOC);
#endif
	if (p_declib->m_picHeader.m_gdrOrIrapPicFlag && p_declib->m_bFirstSliceInPicture) {
		cvector_push(&p_declib->m_accessUnitNoOutputPriorPicFlags, &p_declib->m_apcSlicePilot->m_noOutputOfPriorPicsFlag);
	}

	// Only care about recovery POC if it is the first coded GDR picture in the layer
	if (p_declib->m_picHeader.m_gdrPicFlag && p_declib->m_prevGDRInSameLayerPOC[nalu->m_nuhLayerId] == -MAX_INT) {
		p_declib->m_prevGDRInSameLayerRecoveryPOC[nalu->m_nuhLayerId] =
			p_declib->m_apcSlicePilot->m_iPOC + p_declib->m_picHeader.m_recoveryPocCnt;
	}
#ifdef TO_DO
	PPS *pps = m_parameterSetManager.getPPS(m_picHeader.m_ppsId);
	CHECK(pps == 0, "No PPS present");
	SPS *sps = m_parameterSetManager.getSPS(pps->getSPSId());
	CHECK(sps == 0, "No SPS present");
	VPS *vps = m_parameterSetManager.getVPS(sps->getVPSId());
#endif
#ifdef AML
	pps = p_declib->a_cur_pps;
	sps = p_declib->a_cur_sps;
	vps = p_declib->a_cur_vps;
#endif
	//if (nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_STSA && vps != nullptr && (vps->getIndependentLayerFlag(vps->m_generalLayerIdx[nalu->m_nuhLayerId]) == 1))
	//{
	//  CHECK(nalu.m_temporalId == 0, "TemporalID of STSA picture shall not be zero in independent layers");
	//}
	PRINT_LINE();
#ifdef TO_DO
	int currSubPicIdx = getSubPicIdxFromSubPicId(pps,  p_declib->m_apcSlicePilot->m_sliceSubPicId );
	int currSliceAddr = m_apcSlicePilot->m_sliceMap.m_sliceID;
	for (int sp = 0; sp < currSubPicIdx; sp++) {
		currSliceAddr -= pps->m_subPics[sp].m_numSlicesInSubPic;
	}
	//CHECK( currSubPicIdx < m_maxDecSubPicIdx, "Error in the order of coded slice NAL units of subpictures" );
	//CHECK( currSubPicIdx == m_maxDecSubPicIdx && currSliceAddr <= m_maxDecSliceAddrInSubPic, "Error in the order of coded slice NAL units within a subpicture" );
	if ( currSubPicIdx == m_maxDecSubPicIdx ) {
		m_maxDecSliceAddrInSubPic = currSliceAddr;
	}
	if ( currSubPicIdx > m_maxDecSubPicIdx ) {
		m_maxDecSubPicIdx = currSubPicIdx;
		m_maxDecSliceAddrInSubPic = currSliceAddr;
	}
#endif
	//if ((sps->getVPSId() == 0) && (m_prevLayerID != MAX_INT))
	//{
	//  CHECK(m_prevLayerID != nalu.m_nuhLayerId, "All VCL NAL unit in the CVS shall have the same value of nuh_layer_id "
	//                                            "when sps_video_parameter_set_id is equal to 0");
	//}
	//CHECK((sps->getVPSId() > 0) && (vps == 0), "Invalid VPS");

	//if ((sps->getProfileTierLevel()->getMultiLayerEnabledFlag() == 0) && (m_prevLayerID != MAX_INT))
	//{
	//  CHECK(m_prevLayerID != nalu.m_nuhLayerId, "All slices in OlsInScope shall have the same value of nuh_layer_id when ptl_multilayer_enabled_flag is equal to 0" );
	//}
	PRINT_LINE();

	if ( vps != nullptr
#if 1 //ndef MODIFY_CODE
	&& !vps->m_vpsIndependentLayerFlag[vps->m_generalLayerIdx[nalu->m_nuhLayerId]]
#endif
	) {
		int i;
		bool pocIsSet = false;
		for (i = 0; i < cvector_size(&p_declib->m_accessUnitPicInfo); i++) {
			int iRefIdx;
			AccessUnitPicInfo *auNALit=(AccessUnitPicInfo *)cvector_get(&p_declib->m_accessUnitPicInfo, i);
			for (iRefIdx = 0; iRefIdx < p_declib->m_apcSlicePilot->m_aiNumRefIdx[REF_PIC_LIST_0] && !pocIsSet; iRefIdx++) {
				if (p_declib->m_apcSlicePilot->m_apcRefPicList[REF_PIC_LIST_0][iRefIdx] &&
					p_declib->m_apcSlicePilot->m_apcRefPicList[REF_PIC_LIST_0][iRefIdx]->poc == (*auNALit).m_POC) {
					p_declib->m_apcSlicePilot->m_iPOC=p_declib->m_apcSlicePilot->m_apcRefPicList[REF_PIC_LIST_0][iRefIdx]->poc;
					pocIsSet = true;
				}
			}
			for (iRefIdx = 0; iRefIdx < p_declib->m_apcSlicePilot->m_aiNumRefIdx[REF_PIC_LIST_1] && !pocIsSet; iRefIdx++) {
				if (p_declib->m_apcSlicePilot->m_apcRefPicList[REF_PIC_LIST_1][iRefIdx] &&
					p_declib->m_apcSlicePilot->m_apcRefPicList[REF_PIC_LIST_1][iRefIdx]->poc == (*auNALit).m_POC) {
					p_declib->m_apcSlicePilot->m_iPOC = p_declib->m_apcSlicePilot->m_apcRefPicList[REF_PIC_LIST_1][iRefIdx]->poc;
					pocIsSet = true;
				}
			}
		}
	}
	PRINT_LINE();

	// update independent slice index
	if (!p_declib->m_bFirstSliceInPicture) {
		uiIndependentSliceIdx = p_declib->m_pcPic->slices[p_declib->m_uiSliceSegmentIdx-1]->m_independentSliceIdx;
		uiIndependentSliceIdx++;
	}
	p_declib->m_apcSlicePilot->m_independentSliceIdx=uiIndependentSliceIdx;
	PRINT_LINE();

	//#if K0149_BLOCK_STATISTICS
	//  writeBlockStatisticsHeader(sps);
	//#endif

	//DTRACE_UPDATE( g_trace_ctx, std::make_pair( "poc", m_apcSlicePilot->m_iPOC ) );

	if ((p_declib->m_bFirstSliceInPicture ||
	p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_CRA ||
	p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_GDR) &&
	p_declib->m_isNoOutputPriorPics) {
		checkNoOutputPriorPics(p_declib, &p_declib->m_cListPic);
		p_declib->m_isNoOutputPriorPics = false;
	}
	PRINT_LINE();

	xUpdatePreviousTid0POC(p_declib, p_declib->m_apcSlicePilot);
#ifdef TO_DO
	m_apcSlicePilot->setPrevGDRInSameLayerPOC(m_prevGDRInSameLayerPOC[nalu.m_nuhLayerId]);
	m_apcSlicePilot->setAssociatedIRAPPOC(m_pocCRA[nalu.m_nuhLayerId]);
	m_apcSlicePilot->setAssociatedIRAPType(m_associatedIRAPType[nalu.m_nuhLayerId]);
#endif
	if (getRapPicFlag(p_declib->m_apcSlicePilot) || p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_GDR ) {
		// Derive NoOutputBeforeRecoveryFlag
		if ( !pps->m_mixedNaluTypesInPicFlag ) {
			if ( p_declib->m_firstSliceInSequence[nalu->m_nuhLayerId] ) {
				p_declib->m_picHeader.m_noOutputBeforeRecoveryFlag = true;
			} else if ( getIdrPicFlag(p_declib->m_apcSlicePilot) ) {
				p_declib->m_picHeader.m_noOutputBeforeRecoveryFlag = true;
			} else if ( p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_CRA ) {
				p_declib->m_picHeader.m_noOutputBeforeRecoveryFlag = p_declib->m_picHeader.m_handleCraAsCvsStartFlag;
			} else if ( p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_GDR ) {
				p_declib->m_picHeader.m_noOutputBeforeRecoveryFlag = p_declib->m_picHeader.m_handleGdrAsCvsStartFlag;
			}
		} else {
			p_declib->m_picHeader.m_noOutputBeforeRecoveryFlag = false;
		}

		//the inference for NoOutputOfPriorPicsFlag
		if ( !p_declib->m_firstSliceInBitstream && p_declib->m_picHeader.m_noOutputBeforeRecoveryFlag ) {
			p_declib->m_apcSlicePilot->m_noOutputOfPriorPicsFlag = true;
		} else {
			p_declib->m_apcSlicePilot->m_noOutputOfPriorPicsFlag = false;
		}

		if (p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_CRA ||
			p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_GDR) {
			p_declib->m_lastNoOutputBeforeRecoveryFlag[nalu->m_nuhLayerId] = p_declib->m_picHeader.m_noOutputBeforeRecoveryFlag;
		}

		if (p_declib->m_apcSlicePilot->m_noOutputOfPriorPicsFlag) {
			p_declib->m_lastPOCNoOutputPriorPics = p_declib->m_apcSlicePilot->m_iPOC;
			p_declib->m_isNoOutputPriorPics = true;
		} else {
			p_declib->m_isNoOutputPriorPics = false;
		}
	}
	PRINT_LINE();

	//For inference of PicOutputFlag
	if ( !pps->m_mixedNaluTypesInPicFlag && ( p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_RASL ) ) {
		if ( p_declib->m_lastNoOutputBeforeRecoveryFlag[nalu->m_nuhLayerId] ) {
			p_declib->m_picHeader.m_picOutputFlag = false;
		}
	}
#ifdef TO_DO

	{
		PPS *pps = m_parameterSetManager.getPPS(p_declib->m_picHeader.getPPSId());
		CHECK(pps == 0, "No PPS present");
		SPS *sps = m_parameterSetManager.getSPS(pps->getSPSId());
		CHECK(sps == 0, "No SPS present");
		if (sps->getVPSId() > 0) {
			VPS *vps = m_parameterSetManager.getVPS(sps->getVPSId());
			CHECK(vps == 0, "No VPS present");
			bool isCurLayerNotOutput = true;
			for (int i = 0; i < vps->getNumLayersInOls(vps->m_targetOlsIdx); i++) {
				if ( vps->getLayerIdInOls(vps->m_targetOlsIdx, i) == nalu.m_nuhLayerId ) {
					isCurLayerNotOutput = false;
					break;
				}
			}

			if (isCurLayerNotOutput) {
				p_declib->m_picHeader.m_picOutputFlag = false;
			}
		}
	}
#endif
	PRINT_LINE();

	//Reset POC MSB when CRA or GDR has NoOutputBeforeRecoveryFlag equal to 1
	if (!pps->m_mixedNaluTypesInPicFlag &&
		(p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_CRA ||
		p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_GDR) &&
		p_declib->m_lastNoOutputBeforeRecoveryFlag[nalu->m_nuhLayerId]) {
		int iMaxPOClsb = 1 << sps->m_uiBitsForPOC;
		p_declib->m_apcSlicePilot->m_iPOC= p_declib->m_apcSlicePilot->m_iPOC & (iMaxPOClsb - 1);
		xUpdatePreviousTid0POC(p_declib, p_declib->m_apcSlicePilot);
	}

	picInfo.m_nalUnitType = nalu->m_nalUnitType;
	picInfo.m_nuhLayerId  = nalu->m_nuhLayerId;
	picInfo.m_temporalId  = nalu->m_temporalId;
	picInfo.m_POC         = p_declib->m_apcSlicePilot->m_iPOC;
	cvector_push(&p_declib->m_accessUnitPicInfo, &picInfo);

	// Skip pictures due to random access
	PRINT_LINE();

	if (isRandomAccessSkipPicture(p_app, pps->m_mixedNaluTypesInPicFlag, nalu->m_nuhLayerId)) {
		p_declib->m_prevSliceSkipped = true;
		p_declib->m_skippedPOC = p_declib->m_apcSlicePilot->m_iPOC;
		p_declib->m_skippedLayerID = nalu->m_nuhLayerId;

		// reset variables for bitstream conformance tests
		resetAccessUnitNals(p_declib);
		resetAccessUnitApsNals(p_declib);
		resetAccessUnitPicInfo(p_declib);
		resetPictureUnitNals(p_declib);
		p_declib->m_maxDecSubPicIdx = 0;
		p_declib->m_maxDecSliceAddrInSubPic = -1;
		hevc_print(p_declib->hw, H266_DEBUG_DETAIL,
			"isRandomAccessSkipPicture, %s return false\n", __func__);
		return false;
	}
	// Skip TFD pictures associated with BLA/BLANT pictures
	PRINT_LINE();

	// clear previous slice skipped flag
	p_declib->m_prevSliceSkipped = false;

	//we should only get a different poc for a new picture (with CTU address == 0)
	if (p_declib->m_apcSlicePilot->m_iPOC != p_declib->m_prevPOC &&
		!p_declib->m_firstSliceInSequence[nalu->m_nuhLayerId] &&
		(cvector_get(&p_declib->m_apcSlicePilot->m_sliceMap.m_ctuAddrInSlice,0) != 0)) {
		hevc_print(p_declib->hw, 0, "Warning, the first slice of a picture might have been lost!\n");
	}
	p_declib->m_prevLayerID = nalu->m_nuhLayerId;
	PRINT_LINE();
#ifdef TO_DO
	// leave when a new picture is found
	if (cvector_get(&p_declib->m_apcSlicePilot->m_sliceMap.m_ctuAddrInSlice, 0) == 0 && !p_declib->m_bFirstSliceInPicture) {
		if (p_declib->m_prevPOC >= p_declib->m_pocRandomAccess) {
			//DTRACE_UPDATE( g_trace_ctx, std::make_pair( "final", 0 ) );
			p_declib->m_prevPOC = p_declib->m_apcSlicePilot->m_iPOC;
			return true;
		}
		p_declib->m_prevPOC = p_declib->m_apcSlicePilot->m_iPOC;
	} else {
	//DTRACE_UPDATE( g_trace_ctx, std::make_pair( "final", 1 ) );
	}
#endif
#if 1 //def TO_DO
	//detect lost reference picture and insert copy of earlier frame.
	{
		int lostPoc;
		int refPicIndex;
		while ((lostPoc = checkThatAllRefPicsAreAvailable(p_declib, p_declib->m_apcSlicePilot,
		&p_declib->m_cListPic, &p_declib->m_apcSlicePilot->m_RPL0, 0, true,
		&refPicIndex, p_declib->m_apcSlicePilot->m_aiNumRefIdx[REF_PIC_LIST_0])) > 0)
		{
			if ( !pps->m_mixedNaluTypesInPicFlag && (
			( ( p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL ||
				p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP ) &&
				( sps->m_idrRefParamList || pps->m_rplInfoInPhFlag ) ) ||
			( ( p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_GDR ||
			p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_CRA ) &&
			p_declib->m_picHeader.m_noOutputBeforeRecoveryFlag ) ) ) {

				if (p_declib->m_apcSlicePilot->m_RPL0.m_isInterLayerRefPic[refPicIndex] == 0) {
					xCreateUnavailablePicture(p_declib, pps, lostPoc,
						p_declib->m_apcSlicePilot->m_RPL0.m_isLongtermRefPic[refPicIndex],
						p_declib->m_apcSlicePilot->m_uiTLayer,
						p_declib->m_apcSlicePilot->m_nuhLayerId,
						p_declib->m_apcSlicePilot->m_RPL0.m_isInterLayerRefPic[refPicIndex] );
				}
			} else {
				xCreateLostPicture(p_declib, lostPoc - 1, p_declib->m_apcSlicePilot->m_pcPic->layerId );
			}
		}
		while ((lostPoc = checkThatAllRefPicsAreAvailable(p_declib, p_declib->m_apcSlicePilot,
			&p_declib->m_cListPic, &p_declib->m_apcSlicePilot->m_RPL1, 0, true,
			&refPicIndex, p_declib->m_apcSlicePilot->m_aiNumRefIdx[REF_PIC_LIST_1])) > 0)
		{
			if ( !pps->m_mixedNaluTypesInPicFlag && (
			( ( p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL ||
				p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP ) &&
				( sps->m_idrRefParamList || pps->m_rplInfoInPhFlag ) ) ||
			( ( p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_GDR ||
			p_declib->m_apcSlicePilot->m_eNalUnitType == NAL_UNIT_CODED_SLICE_CRA ) &&
			p_declib->m_picHeader.m_noOutputBeforeRecoveryFlag ) ) ) {

				if (p_declib->m_apcSlicePilot->m_RPL1.m_isInterLayerRefPic[refPicIndex] == 0) {
					xCreateUnavailablePicture(p_declib, p_declib->m_apcSlicePilot->m_pcPPS, lostPoc - 1,
						p_declib->m_apcSlicePilot->m_RPL1.m_isLongtermRefPic[refPicIndex],
						p_declib->m_apcSlicePilot->m_pcPic->temporalId,
						p_declib->m_apcSlicePilot->m_pcPic->layerId,
						p_declib->m_apcSlicePilot->m_RPL1.m_isInterLayerRefPic[refPicIndex] );
				}
			} else {
				xCreateLostPicture(p_declib, lostPoc - 1, p_declib->m_apcSlicePilot->m_pcPic->layerId );
			}
		}
	}
#endif
	p_declib->m_prevPOC = p_declib->m_apcSlicePilot->m_iPOC;

	if (p_declib->m_bFirstSliceInPicture) {
#ifdef TO_DO
		xUpdateRasInit(p_declib->m_apcSlicePilot);
#endif
	}
	PRINT_LINE();

	// actual decoding starts here
	xActivateParameterSets(p_declib, nalu);

	p_declib->m_firstSliceInSequence[nalu->m_nuhLayerId] = false;
	p_declib->m_firstSliceInBitstream  = false;

	pcSlice = p_declib->m_pcPic->slices[p_declib->m_uiSliceSegmentIdx];
	p_declib->m_pcPic->numSlices = p_declib->m_uiSliceSegmentIdx + 1;
	pcSlice->m_pcPic = p_declib->m_pcPic;
	p_declib->m_pcPic->poc         = pcSlice->m_iPOC;
	p_declib->m_pcPic->referenced  = true;
	p_declib->m_pcPic->temporalId  = nalu->m_temporalId;
	p_declib->m_pcPic->layerId     = nalu->m_nuhLayerId;
	p_declib->m_pcPic->subLayerNonReferencePictureDueToSTSA = false;

#ifdef TO_DO
	if (pcSlice->m_pcSPS->getSpsRangeExtension().getRrcRiceExtensionEnableFlag()) {
		int bitDepth = pcSlice->m_pcSPS->getBitDepth(CHANNEL_TYPE_LUMA);
		int baseLevel = (bitDepth > 12) ? (pcSlice->m_eSliceType == I_SLICE ? 5 : 2 * 5 ) : (pcSlice->m_eSliceType == I_SLICE ? 2 * 5 : 3 * 5);
		pcSlice->setRiceBaseLevel(baseLevel);
	} else {
		pcSlice->setRiceBaseLevel(4);
	}

	if (pcSlice->m_pcSPS->getProfileTierLevel()->getConstraintInfo()->getNoApsConstraintFlag()) {
		bool flag = pcSlice->m_pcSPS->getCCALFEnabledFlag() || pcSlice->m_pcPicHeader->getNumAlfApsIdsLuma() || pcSlice->m_pcPicHeader->getAlfEnabledFlag(COMPONENT_Cb) || pcSlice->m_pcPicHeader->getAlfEnabledFlag(COMPONENT_Cr);
		CHECK(flag, "When no_aps_constraint_flag is equal to 1, the values of ph_num_alf_aps_ids_luma, sh_num_alf_aps_ids_luma, ph_alf_cb_flag, ph_alf_cr_flag, sh_alf_cb_flag, sh_alf_cr_flag, and sps_ccalf_enabled_flag shall all be equal to 0")
	}
#endif
	if ( pcSlice->m_nuhLayerId != pcSlice->m_pcSPS->m_layerId ) {
		int i;
		//CHECK( pcSlice->m_pcSPS->m_layerId > pcSlice->m_nuhLayerId, "Layer Id of SPS cannot be greater than layer Id of VCL NAL unit the refer to it" );
		//CHECK( pcSlice->m_pcSPS->getVPSId() == 0, "VPSId of the referred SPS cannot be 0 when layer Id of SPS and layer Id of current slice are different" );
		for (i = 0; i < pcSlice->m_pcVPS->m_vpsNumOutputLayerSets; i++ ) {
			bool isCurrLayerInOls = false;
			bool isRefLayerInOls = false;
			int j = (int64_t)cvector_get(&pcSlice->m_pcVPS->m_numLayersInOls,i) - 1;
			for (; j >= 0; j--) {
				if ( getLayerIdInOls(pcSlice, i, j) == pcSlice->m_nuhLayerId ) {
					isCurrLayerInOls = true;
				}
				if ( getLayerIdInOls(pcSlice, i, j) == pcSlice->m_pcSPS->m_layerId ) {
					isRefLayerInOls = true;
				}
			}
			//CHECK( isCurrLayerInOls && !isRefLayerInOls, "When VCL NAl unit in layer A refers to SPS in layer B, all OLS that contains layer A shall also contains layer B" );
		}
	}
	if ( pcSlice->m_nuhLayerId != pcSlice->m_pcPPS->m_layerId ) {
		int i;
		//CHECK( pcSlice->m_pcPPS->m_layerId > pcSlice->m_nuhLayerId, "Layer Id of PPS cannot be greater than layer Id of VCL NAL unit the refer to it" );
		//CHECK( pcSlice->m_pcSPS->getVPSId() == 0, "VPSId of the referred SPS cannot be 0 when layer Id of PPS and layer Id of current slice are different" );
		for (i = 0; i < pcSlice->m_pcVPS->m_vpsNumOutputLayerSets; i++ ) {
			bool isCurrLayerInOls = false;
			bool isRefLayerInOls = false;

			int j = (int64_t)cvector_get(&pcSlice->m_pcVPS->m_numLayersInOls,i) - 1;
			for (; j >= 0; j--) {
				if ( getLayerIdInOls(pcSlice, i, j) == pcSlice->m_nuhLayerId ) {
					isCurrLayerInOls = true;
				}
				if ( getLayerIdInOls(pcSlice, i, j) == pcSlice->m_pcPPS->m_layerId ) {
					isRefLayerInOls = true;
				}
			}
			//CHECK( isCurrLayerInOls && !isRefLayerInOls, "When VCL NAl unit in layer A refers to PPS in layer B, all OLS that contains layer A shall also contains layer B" );
		}
	}
	if (p_declib->m_bFirstSliceInPicture) {
		p_declib->m_pcPic->m_decodingOrderNumber = p_declib->m_decodingOrderCounter;
		p_declib->m_decodingOrderCounter++;
		p_declib->m_pcPic->m_pictureType = nalu->m_nalUnitType;
#ifdef TO_DO
		checkPicTypeAfterEos(p_declib);
		// store sub-picture numbers, sizes, and locations with a picture
		pcSlice->m_pcPic->subPictures.clear();

		for (int subPicIdx = 0; subPicIdx < sps->getNumSubPics(); subPicIdx++) {
			pcSlice->m_pcPic->subPictures.push_back( pps->m_subPics=[subPicIdx] );
		}
#endif
		pcSlice->m_pcPic->numSlices = pps->m_numSlicesInPic;
#ifdef TO_DO
		pcSlice->m_pcPic->sliceSubpicIdx.clear();
#endif
	}
#ifdef TO_DO
	pcSlice->m_pcPic->sliceSubpicIdx.push_back(getSubPicIdxFromSubPicId(pps, pcSlice->m_sliceSubPicId));
	pcSlice->checkCRA(&pcSlice->m_RPL0, &pcSlice->m_RPL1, m_pocCRA[nalu.m_nuhLayerId], m_cListPic);
#endif
	constructRefPicList(p_declib, pcSlice, &p_declib->m_cListPic);
#ifdef AML
	print_ref_pic_list(p_declib, pcSlice);
#endif
	PRINT_LINE();

#ifdef TO_DO
	pcSlice->setPrevGDRSubpicPOC(m_prevGDRSubpicPOC[nalu.m_nuhLayerId][currSubPicIdx]);
	pcSlice->setPrevIRAPSubpicPOC(m_prevIRAPSubpicPOC[nalu.m_nuhLayerId][currSubPicIdx]);
	pcSlice->setPrevIRAPSubpicType(m_prevIRAPSubpicType[nalu.m_nuhLayerId][currSubPicIdx]);
	pcSlice->checkSubpicTypeConstraints(m_cListPic, &pcSlice->m_RPL0, &pcSlice->m_RPL1, m_prevIRAPSubpicDecOrderNo[nalu.m_nuhLayerId][currSubPicIdx]);
	pcSlice->checkRPL(&pcSlice->m_RPL0, &pcSlice->m_RPL1, m_associatedIRAPDecodingOrderNumber[nalu.m_nuhLayerId], m_cListPic);
	pcSlice->checkSTSA(m_cListPic);
	if (m_pcPic->cs->vps && !m_pcPic->cs->vps->getIndependentLayerFlag(m_pcPic->cs->vps->m_generalLayerIdx[nalu->m_nuhLayerId]) &&
		m_pcPic->cs->pps->getNumSubPics() > 1) {
		CU::checkConformanceILRP(pcSlice);
	}

#endif
	scaleRefPicList(p_declib, pcSlice, scaledRefPic, p_declib->m_pcPic->cs->picHeader, //m_parameterSetManager.getAPSs(),p_declib->m_picHeader.m_lmcsAps, p_declib->m_picHeader.m_scalingListAps,
	true );

	if (!(pcSlice->m_eSliceType == I_SLICE)) {
		bool bLowDelay = true;
		int  iCurrPOC  = pcSlice->m_iPOC;
		int iRefIdx = 0;

		for (iRefIdx = 0; iRefIdx < pcSlice->m_aiNumRefIdx[REF_PIC_LIST_0] && bLowDelay; iRefIdx++) {
			if ( pcSlice->m_apcRefPicList[REF_PIC_LIST_0][iRefIdx]->poc > iCurrPOC ) {
				bLowDelay = false;
			}
		}
		if (pcSlice->m_eSliceType == B_SLICE) {
			for (iRefIdx = 0; iRefIdx < pcSlice->m_aiNumRefIdx[REF_PIC_LIST_1] && bLowDelay; iRefIdx++) {
				if ( pcSlice->m_apcRefPicList[REF_PIC_LIST_1][iRefIdx]->poc > iCurrPOC ) {
					bLowDelay = false;
				}
			}
		}

#if 1 //def TO_DO
		pcSlice->m_bCheckLDC = bLowDelay;
#endif
	}
	PRINT_LINE();

	if (pcSlice->m_pcSPS->m_SMVD && pcSlice->m_bCheckLDC == false
#if 1 //ndef MODIFY_CODE
	&& pcSlice->m_pcPicHeader->m_mvdL1ZeroFlag == false
#endif
	) {
		int currPOC = pcSlice->m_iPOC;

		int forwardPOC = currPOC;
		int backwardPOC = currPOC;
		int ref = 0;
		int refIdx0 = -1;
		int refIdx1 = -1;

		// search nearest forward POC in List 0
		for (ref = 0; ref < pcSlice->m_aiNumRefIdx[ REF_PIC_LIST_0 ]; ref++) {
			int poc = pcSlice->m_apcRefPicList[REF_PIC_LIST_0][ref]->poc;
			const bool isRefLongTerm = pcSlice->m_apcRefPicList[REF_PIC_LIST_0][ref]->longTerm;
			if ( poc < currPOC && (poc > forwardPOC || refIdx0 == -1) && !isRefLongTerm ) {
				forwardPOC = poc;
				refIdx0 = ref;
			}
		}

		// search nearest backward POC in List 1
		for (ref = 0; ref < pcSlice->m_aiNumRefIdx[ REF_PIC_LIST_1 ]; ref++) {
			int poc = pcSlice->m_apcRefPicList[REF_PIC_LIST_1][ref]->poc;
			const bool isRefLongTerm = pcSlice->m_apcRefPicList[REF_PIC_LIST_1][ref]->longTerm;
			if ( poc > currPOC && (poc < backwardPOC || refIdx1 == -1) && !isRefLongTerm ) {
				backwardPOC = poc;
				refIdx1 = ref;
			}
		}

		if (!(forwardPOC < currPOC && backwardPOC > currPOC) ) {
			forwardPOC = currPOC;
			backwardPOC = currPOC;
			refIdx0 = -1;
			refIdx1 = -1;

			// search nearest backward POC in List 0
			for (ref = 0; ref < pcSlice->m_aiNumRefIdx[ REF_PIC_LIST_0 ]; ref++) {
				int poc = pcSlice->m_apcRefPicList[REF_PIC_LIST_0][ref]->poc;
				const bool isRefLongTerm = pcSlice->m_apcRefPicList[REF_PIC_LIST_0][ref]->longTerm;
				if ( poc > currPOC && (poc < backwardPOC || refIdx0 == -1) && !isRefLongTerm ) {
					backwardPOC = poc;
					refIdx0 = ref;
				}
			}

			// search nearest forward POC in List 1
			for (ref = 0; ref < pcSlice->m_aiNumRefIdx[ REF_PIC_LIST_1 ]; ref++) {
				int poc = pcSlice->m_apcRefPicList[REF_PIC_LIST_1][ref]->poc;
				const bool isRefLongTerm = pcSlice->m_apcRefPicList[REF_PIC_LIST_1][ref]->longTerm;
				if ( poc < currPOC && (poc > forwardPOC || refIdx1 == -1) && !isRefLongTerm ) {
				forwardPOC = poc;
				refIdx1 = ref;
				}
			}
		}
		if ( forwardPOC < currPOC && backwardPOC > currPOC ) {
			setBiDirPred(p_declib, pcSlice, true, refIdx0, refIdx1);
		} else {
			setBiDirPred(p_declib, pcSlice, false, -1, -1);
		}
	}
	else {
		setBiDirPred(p_declib, pcSlice, false, -1, -1);
	}
	PRINT_LINE();

	//---------------
	setRefPOCList(p_declib, pcSlice);

	naluInfo.m_nalUnitType = nalu->m_nalUnitType;
	naluInfo.m_nuhLayerId = nalu->m_nuhLayerId;
	naluInfo.m_firstCTUinSlice = (uint64_t)cvector_get(&pcSlice->m_sliceMap.m_ctuAddrInSlice, 0);
	naluInfo.m_POC = pcSlice->m_iPOC;
	//xCheckMixedNalUnit(pcSlice, sps, nalu);
	cvector_push(&p_declib->m_nalUnitInfo[naluInfo.m_nuhLayerId], &naluInfo);
#ifdef TO_DO
	SEIMessages drapSEIs = getSeisByType(m_pcPic->SEIs, SEI::DEPENDENT_RAP_INDICATION );
	if (!drapSEIs.empty()) {
		msg( NOTICE, "Dependent RAP indication SEI decoded\n");
		pcSlice->setDRAP(true);
		pcSlice->setLatestDRAPPOC(pcSlice->m_iPOC);
	}
	pcSlice->checkConformanceForDRAP(nalu.m_temporalId);
	if (pcSlice->m_eSliceType == I_SLICE)
		pcSlice->m_pcPic->setEdrapRapId(0);
	SEIMessages edrapSEIs = getSeisByType(m_pcPic->SEIs, SEI::EXTENDED_DRAP_INDICATION );
	if (!edrapSEIs.empty()) {
		int i;
		msg( NOTICE, "Extended DRAP indication SEI decoded\n");
		SEIExtendedDrapIndication *seiEdrap = (SEIExtendedDrapIndication *)edrapSEIs.front();
		pcSlice->setEdrapRapId(seiEdrap->m_edrapIndicationRapIdMinus1 + 1);
		pcSlice->m_pcPic->setEdrapRapId(seiEdrap->m_edrapIndicationRapIdMinus1 + 1);
		pcSlice->setEdrapNumRefRapPics(seiEdrap->m_edrapIndicationNumRefRapPicsMinus1 + 1);
		for (i = 0; i < pcSlice->getEdrapNumRefRapPics(); i++) {
			pcSlice->addEdrapRefRapIds(seiEdrap->m_edrapIndicationRefRapId[i]);
		}
		pcSlice->setLatestEDRAPPOC(pcSlice->m_iPOC);
	}
	pcSlice->checkConformanceForEDRAP(nalu.m_temporalId);

	Quant *quant = m_cTrQuant.getQuant();

	if (pcSlice->getExplicitScalingListUsed()) {
		APS* scalingListAPS = pcSlice->m_pcPicHeader->getScalingListAPS();
		if ( pcSlice->m_nuhLayerId != scalingListAPS->m_layerId ) {
			int i;
			//CHECK( scalingListAPS->m_layerId > pcSlice->m_nuhLayerId, "Layer Id of APS cannot be greater than layer Id of VCL NAL unit the refer to it" );
			//CHECK( pcSlice->m_pcSPS->getVPSId() == 0, "VPSId of the referred SPS cannot be 0 when layer Id of APS and layer Id of current slice are different" );
			for (i = 0; i < pcSlice->m_pcVPS->m_vpsNumOutputLayerSets; i++) {
				bool isCurrLayerInOls = false;
				bool isRefLayerInOls = false;
				for (int j = cvector_get(&pcSlice->m_pcVPS->m_numLayersInOls,i) - 1; j >= 0; j--) {

					if (getLayerIdInOls(pcSlice, i, j) == pcSlice->m_nuhLayerId) {
						isCurrLayerInOls = true;
					}
					if (getLayerIdInOls(pcSlice, i, j) == scalingListAPS->m_layerId) {
						isRefLayerInOls = true;
					}
				}
				//CHECK( isCurrLayerInOls && !isRefLayerInOls, "When VCL NAl unit in layer A refers to APS in layer B, all OLS that contains layer A shall also contains layer B" );
			}
		}
		ScalingList scalingList = scalingListAPS->getScalingList();
		quant->setScalingListDec(scalingList);
		quant->setUseScalingList(true);
	} else {
		quant->setUseScalingList( false );
	}

	if (pcSlice->m_pcSPS->getUseLmcs()) {
		if (m_bFirstSliceInPicture)
			m_sliceLmcsApsId = -1;
		if (pcSlice->getLmcsEnabledFlag()) {
			APS* lmcsAPS = pcSlice->m_pcPicHeader->getLmcsAPS();
			if (m_sliceLmcsApsId == -1) {
				m_sliceLmcsApsId = lmcsAPS->getAPSId();
			} else {
				CHECK(lmcsAPS->getAPSId() != m_sliceLmcsApsId, "same APS ID shall be used for all slices in one picture");
			}
			if ( pcSlice->m_nuhLayerId != lmcsAPS->m_layerId ) {
				int i;
				CHECK( lmcsAPS->m_layerId > pcSlice->m_nuhLayerId, "Layer Id of APS cannot be greater than layer Id of VCL NAL unit the refer to it" );
				CHECK( pcSlice->m_pcSPS->getVPSId() == 0, "VPSId of the referred SPS cannot be 0 when layer Id of APS and layer Id of current slice are different" );
				for (i = 0; i < pcSlice->m_pcVPS->m_vpsNumOutputLayerSets; i++ ) {
					bool isCurrLayerInOls = false;
					bool isRefLayerInOls = false;
					for ( int j = cvector_get(&pcSlice->m_pcVPS->m_numLayersInOls,i) - 1; j >= 0; j-- ) {
						if ( getLayerIdInOls(pcSlice, i, j) == pcSlice->m_nuhLayerId ) {
							isCurrLayerInOls = true;
						}
						if ( getLayerIdInOls(pcSlice, i, j) == lmcsAPS->m_layerId ) {
							isRefLayerInOls = true;
						}
					}
					CHECK( isCurrLayerInOls && !isRefLayerInOls, "When VCL NAl unit in layer A refers to APS in layer B, all OLS that contains layer A shall also contains layer B" );
				}
			}
			SliceReshapeInfo& sInfo = lmcsAPS->getReshaperAPSInfo();
			SliceReshapeInfo& tInfo = m_cReshaper.getSliceReshaperInfo();
			tInfo.reshaperModelMaxBinIdx = sInfo.reshaperModelMaxBinIdx;
			tInfo.reshaperModelMinBinIdx = sInfo.reshaperModelMinBinIdx;
			memcpy(tInfo.reshaperModelBinCWDelta, sInfo.reshaperModelBinCWDelta, sizeof(int)*(PIC_CODE_CW_BINS));
			tInfo.maxNbitsNeededDeltaCW = sInfo.maxNbitsNeededDeltaCW;
			tInfo.chrResScalingOffset = sInfo.chrResScalingOffset;
			tInfo.setUseSliceReshaper(pcSlice->getLmcsEnabledFlag());
			tInfo.setSliceReshapeChromaAdj(pcSlice->m_pcPicHeader->getLmcsChromaResidualScaleFlag());
			tInfo.setSliceReshapeModelPresentFlag(true);
		} else {
			SliceReshapeInfo& tInfo = m_cReshaper.getSliceReshaperInfo();
			tInfo.setUseSliceReshaper(false);
			tInfo.setSliceReshapeChromaAdj(false);
			tInfo.setSliceReshapeModelPresentFlag(false);
		}
		if (pcSlice->getLmcsEnabledFlag()) {
			m_cReshaper.constructReshaper();
		} else {
			m_cReshaper.setReshapeFlag(false);
		}
		if ((pcSlice->getSliceType() == I_SLICE) && m_cReshaper.getSliceReshaperInfo().getUseSliceReshaper()) {
			m_cReshaper.setCTUFlag(false);
			m_cReshaper.setRecReshaped(true);
		} else {
			if (m_cReshaper.getSliceReshaperInfo().getUseSliceReshaper()) {
				m_cReshaper.setCTUFlag(true);
				m_cReshaper.setRecReshaped(true);
			} else {
				m_cReshaper.setCTUFlag(false);
				m_cReshaper.setRecReshaped(false);
			}
		}
		m_cReshaper.setVPDULoc(-1, -1);
	} else {
		m_cReshaper.setCTUFlag(false);
		m_cReshaper.setRecReshaped(false);
	}

#if GDR_LEAK_TEST
	if (m_gdrPocRandomAccess == pcSlice->m_iPOC) {
		int e;
		for (e = 0; e < 2; e++) {
			int ridx;
			for (ridx = 0; ridx < pcSlice->m_aiNumRefIdx[(RefPicList)e]; ridx++) {
				Picture *pic = pcSlice->m_apcRefPicList[(RefPicList)e][ridx];
				if (pic) {
					CodingStructure& cs = *pic->cs;
					cs.getRecoBuf().Y().fill(0 * 4); // for 8-bit sequence
					cs.getRecoBuf().Cb().fill(0 * 4);
					cs.getRecoBuf().Cr().fill(0 * 4);
					cs.getMotionBuf().memset(0);    // clear MV storage
				}
			}
		}
	}
#endif // GDR_LEAK_TEST
	//  Decode a picture
	m_cSliceDecoder.decompressSlice( pcSlice, &( nalu.getBitstream() ), ( m_pcPic->poc == getDebugPOC() ? getDebugCTU() : -1 ) );
	//TO_DO
#endif
	p_declib->m_bFirstSliceInPicture = false;
	p_declib->m_uiSliceSegmentIdx++;
	PRINT_LINE();

	freeScaledRefPicList(pcSlice, scaledRefPic );
	PRINT_LINE();
	return false;

}

#if 0
bool decode(DecLib *p_declib, NALUnit *nalu, int iTargetOlsIdx)
	{
	bool ret;
	// ignore all NAL units of layers > 0
	if ( (nalu->m_nalUnitType != NAL_UNIT_SUFFIX_APS       &&
		nalu->m_nalUnitType != NAL_UNIT_EOS              &&
		nalu->m_nalUnitType != NAL_UNIT_EOB              &&
		nalu->m_nalUnitType != NAL_UNIT_SUFFIX_SEI       &&
		nalu->m_nalUnitType != NAL_UNIT_FD               &&
		nalu->m_nalUnitType != NAL_UNIT_RESERVED_NVCL_27 &&
		nalu->m_nalUnitType != NAL_UNIT_UNSPECIFIED_30   &&
		nalu->m_nalUnitType != NAL_UNIT_UNSPECIFIED_31)  ||
		!p_declib->m_prevSliceSkipped )
	{
#ifdef TO_DO
		AccessUnitInfo auInfo;
		auInfo.m_nalUnitType = nalu.m_nalUnitType;
		auInfo.m_nuhLayerId = nalu.m_nuhLayerId;
		auInfo.m_temporalId = nalu.m_temporalId;
		m_accessUnitNals.push_back(auInfo);
		m_pictureUnitNals.push_back( nalu.m_nalUnitType );
#endif
	}
	switch (nalu->m_nalUnitType)
	{
		case NAL_UNIT_VPS:
			//xDecodeVPS( nalu );
			if (p_declib->m_tOlsIdxTidExternalSet) {
				p_declib->m_vps->m_targetOlsIdx = iTargetOlsIdx;
			} else if (p_declib->m_tOlsIdxTidOpiSet) {
				p_declib->m_vps->m_targetOlsIdx = p_declib->m_opi->m_opiolsidx;
			} else {
				p_declib->m_vps->m_targetOlsIdx = deriveTargetOLSIdx(p_declib->m_vps);
			}
			return false;
		case NAL_UNIT_OPI:
			//xDecodeOPI( nalu );
			return false;
		case NAL_UNIT_DCI:
			//xDecodeDCI( nalu );
			return false;
		case NAL_UNIT_SPS:
			//xDecodeSPS( nalu );
			return false;

		case NAL_UNIT_PPS:
			//xDecodePPS( nalu );
			return false;

		case NAL_UNIT_PH:
			//xDecodePicHeader(nalu);
			return !p_declib->m_bFirstSliceInPicture;

		case NAL_UNIT_PREFIX_APS:
			//xDecodeAPS(nalu);
			return false;

		case NAL_UNIT_SUFFIX_APS:
#ifdef TO_DO
			if ( p_declib->m_prevSliceSkipped ) {
				xDecodeAPS(nalu);
			} else {
				m_suffixApsNalus.push_back(new InputNALUnit(nalu));
			}
#endif
			return false;

		case NAL_UNIT_PREFIX_SEI:
			// Buffer up prefix SEI messages until SPS of associated VCL is known.
#ifdef TO_DO
			m_prefixSEINALUs.push_back(new InputNALUnit(nalu));
			m_pictureSeiNalus.push_back(new InputNALUnit(nalu));
#endif
			return false;

		case NAL_UNIT_SUFFIX_SEI:
#ifdef TO_DO
			if (m_pcPic)
			{
				if ( m_prevSliceSkipped ) {
					msg( NOTICE, "Note: received suffix SEI but current picture is skipped.\n");
					return false;
				}
				m_pictureSeiNalus.push_back(new InputNALUnit(nalu));
				m_accessUnitSeiTids.push_back(nalu.m_temporalId);
				const SPS *sps = m_parameterSetManager.getActiveSPS();
				const VPS *vps = m_parameterSetManager.getVPS(sps->getVPSId());
				m_seiReader.parseSEImessage( &(nalu.getBitstream()), m_pcPic->SEIs, nalu.m_nalUnitType, nalu.m_nuhLayerId, nalu.m_temporalId, vps, sps, m_HRD, m_pDecodedSEIOutputStream );
#if JVET_S0257_DUMP_360SEI_MESSAGE
				m_seiCfgDump.write360SeiDump(m_decoded360SeiDumpFileName, m_pcPic->SEIs, sps);
#endif
				m_accessUnitSeiPayLoadTypes.push_back(std::tuple<NalUnitType, int, SEI::PayloadType>(nalu.m_nalUnitType, nalu.m_nuhLayerId, m_pcPic->SEIs.back()->payloadType()));
			} else {
				msg( NOTICE, "Note: received suffix SEI but no picture currently active.\n");
			}
#endif
			return false;

		case NAL_UNIT_CODED_SLICE_TRAIL:
		case NAL_UNIT_CODED_SLICE_STSA:
		case NAL_UNIT_CODED_SLICE_IDR_W_RADL:
		case NAL_UNIT_CODED_SLICE_IDR_N_LP:
		case NAL_UNIT_CODED_SLICE_CRA:
		case NAL_UNIT_CODED_SLICE_GDR:
		case NAL_UNIT_CODED_SLICE_RADL:
		case NAL_UNIT_CODED_SLICE_RASL:
			ret = xDecodeSlice(p_declib, &nalu);
			return ret;

		case NAL_UNIT_EOS:
#ifdef TO_DO
			m_associatedIRAPType[nalu.m_nuhLayerId] = NAL_UNIT_INVALID;
			m_pocCRA[nalu.m_nuhLayerId] = -MAX_INT;
			m_prevGDRInSameLayerPOC[nalu.m_nuhLayerId] = -MAX_INT;
			m_prevGDRInSameLayerRecoveryPOC[nalu.m_nuhLayerId] = -MAX_INT;
			std::fill_n(m_prevGDRSubpicPOC[nalu.m_nuhLayerId], MAX_NUM_SUB_PICS, -MAX_INT);
			std::fill_n(m_prevIRAPSubpicPOC[nalu.m_nuhLayerId], MAX_NUM_SUB_PICS, -MAX_INT);
			memset(m_prevIRAPSubpicDecOrderNo[nalu.m_nuhLayerId], 0, sizeof(int)*MAX_NUM_SUB_PICS);
			std::fill_n(m_prevIRAPSubpicType[nalu.m_nuhLayerId], MAX_NUM_SUB_PICS, NAL_UNIT_INVALID);
			m_pocRandomAccess = MAX_INT;
			m_prevLayerID = MAX_INT;
			m_prevPOC = -MAX_INT;
			m_prevSliceSkipped = false;
			m_skippedPOC = 0;
			m_accessUnitEos[nalu.m_nuhLayerId] = true;
			m_prevEOS[nalu.m_nuhLayerId] = true;
#endif
			return false;

		case NAL_UNIT_ACCESS_UNIT_DELIMITER:
		{
#ifdef TO_DO
			AUDReader audReader;
			uint32_t picType;
			audReader.parseAccessUnitDelimiter(&(nalu.getBitstream()), m_audIrapOrGdrAuFlag, picType);
#endif
			return !p_declib->m_bFirstSliceInPicture;
		}

		case NAL_UNIT_EOB:
			return false;

		case NAL_UNIT_FD:
		{
#ifdef TO_DO
			FDReader fdReader;
			uint32_t fdSize;
			fdReader.parseFillerData(&(nalu.getBitstream()), fdSize);
			msg( NOTICE, "Note: found NAL_UNIT_FD with %u bytes payload.\n", fdSize);
#endif
			return false;
		}

		case NAL_UNIT_RESERVED_IRAP_VCL_11:
#ifdef TO_DO
			msg( NOTICE, "Note: found reserved VCL NAL unit.\n");
			xParsePrefixSEIsForUnknownVCLNal();
#endif
			return false;
		case NAL_UNIT_RESERVED_VCL_4:
		case NAL_UNIT_RESERVED_VCL_5:
		case NAL_UNIT_RESERVED_VCL_6:
		case NAL_UNIT_RESERVED_NVCL_26:
		case NAL_UNIT_RESERVED_NVCL_27:
			//msg( NOTICE, "Note: found reserved NAL unit.\n");
			return false;
		case NAL_UNIT_UNSPECIFIED_28:
		case NAL_UNIT_UNSPECIFIED_29:
		case NAL_UNIT_UNSPECIFIED_30:
		case NAL_UNIT_UNSPECIFIED_31:
			//msg( NOTICE, "Note: found unspecified NAL unit.\n");
			return false;
		default:
			//THROW( "Invalid NAL unit type" );
			break;
	}
	return false;
}
#endif

bool xIsNaluWithinTargetDecLayerIdSet(DecApp *p_app, const NALUnit* nalu)
{
#ifdef TO_DO
	if ( !p_app->m_targetDecLayerIdSet.size() ) { // By default, the set is empty, meaning all LayerIds are allowed
		return true;
	}

	return std::find( m_targetDecLayerIdSet.begin(), m_targetDecLayerIdSet.end(), nalu->m_nuhLayerId ) != m_targetDecLayerIdSet.end();
#else
	return true;
#endif
}

//Picture * m_cListPic[PIC_LIST_SIZE];
void xFlushOutput(DecApp *p_app, PicList* pcListPic, const int layerId)
{
	int i;
	DecLib *p_declib = &p_app->m_cDecLib;
	Picture* pcPic;
	if (!pcListPic || PicList_empty(pcListPic)) {
		return;
	}
	//PicList::iterator iterPic   = pcListPic->begin();

	//iterPic   = pcListPic->begin();
	pcPic = pcListPic->pic[0]; //*(iterPic);

	if (pcPic->fieldPic ) { //Field Decoding
		//PicList::iterator endPic   = pcListPic->end();
		//endPic--;
		Picture *pcPicTop, *pcPicBottom = NULL;
		//while (iterPic != endPic)
		for (i = 0; i < PIC_LIST_SIZE; i += 2) {
			pcPicTop = pcListPic->pic[i]; //*(iterPic);
			if (pcPicTop == NULL)
				break;
			//iterPic++;
			pcPicBottom = pcListPic->pic[i+1]; //*(iterPic);

			if ( pcPicTop->layerId != layerId && layerId != NOT_VALID ) {
				continue;
			}

			if ( pcPicTop->neededForOutput && pcPicBottom->neededForOutput && !(pcPicTop->poc%2) && (pcPicBottom->poc == pcPicTop->poc+1) ) {
				// write to file
#ifdef TO_DO
				if ( !m_reconFileName.empty() ) {
					const Window &conf = pcPicTop->cs->pps->getConformanceWindow();
					const bool    isTff   = pcPicTop->topField;

					m_cVideoIOYuvReconFile[pcPicTop->layerId].write( pcPicTop->getRecoBuf(), pcPicBottom->getRecoBuf(),
						m_outputColourSpaceConvert,
						false, // TODO: m_packedYUVMode,
						conf.getWindowLeftOffset() * SPS::getWinUnitX( pcPicTop->cs->sps->getChromaFormatIdc() ),
						conf.getWindowRightOffset() * SPS::getWinUnitX( pcPicTop->cs->sps->getChromaFormatIdc() ),
						conf.getWindowTopOffset() * SPS::getWinUnitY( pcPicTop->cs->sps->getChromaFormatIdc() ),
						conf.getWindowBottomOffset() * SPS::getWinUnitY( pcPicTop->cs->sps->getChromaFormatIdc() ),
						NUM_CHROMA_FORMAT, isTff );
				}
				writeLineToOutputLog(pcPicTop);
				writeLineToOutputLog(pcPicBottom);
#endif
				// update POC of display order
				p_app->m_iPOCLastDisplay = pcPicBottom->poc;

				// erase non-referenced picture in the reference picture list after display
				if ( ! pcPicTop->referenced && pcPicTop->reconstructed ) {
					pcPicTop->reconstructed = false;
				}
				if ( ! pcPicBottom->referenced && pcPicBottom->reconstructed ) {
					pcPicBottom->reconstructed = false;
				}
				pcPicTop->neededForOutput = false;
				pcPicBottom->neededForOutput = false;
				if (pcPicTop->buf_cfg)
					pcPicTop->buf_cfg->pic_struct = 1;
				if (pcPicBottom->buf_cfg)
					pcPicBottom->buf_cfg->pic_struct = 2;
				prepare_display_buf(p_declib->hw, pcPicTop->buf_cfg);
				prepare_display_buf(p_declib->hw, pcPicBottom->buf_cfg);

				if (pcPicTop) {
					//pcPicTop->destroy();
					//delete pcPicTop;
					free_picture(pcPicTop);
					pcPicTop = NULL;
				}
			}
		}
		if (pcPicBottom) {
			//pcPicBottom->destroy();
			//delete pcPicBottom;
			free_picture(pcPicBottom);
			pcPicBottom = NULL;
		}
	} else { //Frame decoding
		//while (iterPic != pcListPic->end())
		for (i = 0; i < PIC_LIST_SIZE; i++) {
			pcPic = pcListPic->pic[i]; //*(iterPic);
			if (pcPic == NULL)
				break;
			if ( pcPic->layerId != layerId && layerId != NOT_VALID ) {
				//iterPic++;
				continue;
			}

			if (pcPic->neededForOutput) {
				// write to file
#ifdef TO_DO
				if (!m_reconFileName.empty()) {
					const Window &conf = pcPic->getConformanceWindow();
					const SPS* sps = pcPic->cs->sps;
					ChromaFormat chromaFormatIDC = sps->getChromaFormatIdc();
					if ( m_upscaledOutput ) {
						m_cVideoIOYuvReconFile[pcPic->layerId].writeUpscaledPicture( *sps, *pcPic->cs->pps, pcPic->getRecoBuf(), m_outputColourSpaceConvert, m_packedYUVMode, m_upscaledOutput, NUM_CHROMA_FORMAT, m_bClipOutputVideoToRec709Range );
					} else {
						m_cVideoIOYuvReconFile[pcPic->layerId].write( pcPic->getRecoBuf().get( COMPONENT_Y ).width, pcPic->getRecoBuf().get( COMPONENT_Y ).height, pcPic->getRecoBuf(),
							m_outputColourSpaceConvert,
							m_packedYUVMode,
							conf.getWindowLeftOffset() * SPS::getWinUnitX( chromaFormatIDC ),
							conf.getWindowRightOffset() * SPS::getWinUnitX( chromaFormatIDC ),
							conf.getWindowTopOffset() * SPS::getWinUnitY( chromaFormatIDC ),
							conf.getWindowBottomOffset() * SPS::getWinUnitY( chromaFormatIDC ),
							NUM_CHROMA_FORMAT, m_bClipOutputVideoToRec709Range );
					}
				}
				// Perform CTI on decoded frame and write to output CTI file
				if (!m_SEICTIFileName.empty()) {
					const Window& conf = pcPic->getConformanceWindow();
					const SPS* sps = pcPic->cs->sps;
					ChromaFormat chromaFormatIDC = sps->getChromaFormatIdc();
					if (m_upscaledOutput) {
						m_cVideoIOYuvSEICTIFile[pcPic->layerId].writeUpscaledPicture(*sps, *pcPic->cs->pps, pcPic->getDisplayBuf(), m_outputColourSpaceConvert, m_packedYUVMode, m_upscaledOutput, NUM_CHROMA_FORMAT, m_bClipOutputVideoToRec709Range);
					} else {
						m_cVideoIOYuvSEICTIFile[pcPic->layerId].write(pcPic->getRecoBuf().get(COMPONENT_Y).width, pcPic->getRecoBuf().get(COMPONENT_Y).height, pcPic->getDisplayBuf(),
						m_outputColourSpaceConvert, m_packedYUVMode,
						conf.getWindowLeftOffset() * SPS::getWinUnitX(chromaFormatIDC),
						conf.getWindowRightOffset() * SPS::getWinUnitX(chromaFormatIDC),
						conf.getWindowTopOffset() * SPS::getWinUnitY(chromaFormatIDC),
						conf.getWindowBottomOffset() * SPS::getWinUnitY(chromaFormatIDC),
						NUM_CHROMA_FORMAT, m_bClipOutputVideoToRec709Range);
					}
				}
				writeLineToOutputLog(pcPic);
#endif
				// update POC of display order
				p_app->m_iPOCLastDisplay = pcPic->poc;

				// erase non-referenced picture in the reference picture list after display
				if (!pcPic->referenced && pcPic->reconstructed) {
					pcPic->reconstructed = false;
				}
				pcPic->neededForOutput = false;
				prepare_display_buf(p_declib->hw, pcPic->buf_cfg);
			}
			if (pcPic != NULL) {
				//pcPic->destroy();
				//delete pcPic;
				free_picture(pcPic);
				pcPic = NULL;
				//*iterPic = nullptr;
				pcListPic->pic[i] = NULL;
			}
			//iterPic++;
		}
	}

	if ( layerId != NOT_VALID ) {
		PicList_remove_null(pcListPic); //pcListPic->remove_if ([](Picture* p) { return p == nullptr; });
	} else
		PicList_clear(pcListPic); //->clear();
	p_app->m_iPOCLastDisplay = -MAX_INT;
}

void xWriteOutput(DecApp *p_app, PicList* pcListPic, uint32_t tId )
{
	DecLib *p_declib = &p_app->m_cDecLib;
	int i;
	int numPicsNotYetDisplayed = 0;
	int dpbFullness = 0;
	uint32_t maxNumReorderPicsHighestTid;
	uint32_t maxDecPicBufferingHighestTid;
	SPS* activeSPS;
	uint32_t maxNrSublayers;
	int idx = 0;
	Picture* pcPic; //*(iterPic);
	if (PicList_empty(pcListPic)) {
		return;
	}

	//PicList::iterator iterPic   = pcListPic->begin();
	activeSPS = pcListPic->pic[0]->cs->sps; // (pcListPic->front()->cs->sps);
	maxNrSublayers = activeSPS->m_uiMaxTLayers; //getMaxTLayers();

	//const VPS* referredVPS = pcListPic->pic[0]->cs->vps; //pcListPic->front()->cs->vps;
	//const int temporalId = ( p_app->m_iMaxTemporalLayer == -1 || p_app->m_iMaxTemporalLayer >= maxNrSublayers ) ? maxNrSublayers - 1 : p_app->m_iMaxTemporalLayer;
#ifdef MODIFY_CODE
	maxNumReorderPicsHighestTid = 6; //4; //0; //
	maxDecPicBufferingHighestTid = 8; //6; //1; //
#else
	if ( referredVPS == nullptr || cvector_get(&referredVPS->m_numLayersInOls,referredVPS->m_targetOlsIdx) == 1 ) {
		maxNumReorderPicsHighestTid = activeSPS->m_maxNumReorderPics[temporalId];
		maxDecPicBufferingHighestTid = activeSPS->m_uiMaxDecPicBuffering[temporalId];
	} else {
		DpbParameters *dpb_param;
		//maxNumReorderPicsHighestTid = referredVPS->m_dpbParameters[referredVPS->m_olsDpbParamsIdx[referredVPS->m_targetOlsIdx]].m_maxNumReorderPics[temporalId];
		//maxDecPicBufferingHighestTid = referredVPS->m_dpbParameters[referredVPS->m_olsDpbParamsIdx[referredVPS->m_targetOlsIdx]].m_maxDecPicBuffering[temporalId];
		dpb_param = (DpbParameters*)(cvector_get(&referredVPS->m_dpbParameters, cvector_get(&referredVPS->m_olsDpbParamsIdx,referredVPS->m_targetOlsIdx)));
		maxNumReorderPicsHighestTid = dpb_param->m_maxNumReorderPics[temporalId];
		maxDecPicBufferingHighestTid = dpb_param->m_maxDecPicBuffering[temporalId];
	}
#endif
	//while (iterPic != pcListPic->end())
	for (i = 0; i < PIC_LIST_SIZE; i++) {
		pcPic = pcListPic->pic[i]; //*(iterPic);
		if (pcPic == NULL)
			break;
		if (pcPic->neededForOutput && pcPic->poc >= p_app->m_iPOCLastDisplay) {
			numPicsNotYetDisplayed++;
			dpbFullness++;
		} else if (pcPic->referenced) {
			dpbFullness++;
		}
		//iterPic++;
	}

	//iterPic = pcListPic->begin();
	if (numPicsNotYetDisplayed>2) {
		idx++; //iterPic++;
	}

	pcPic = pcListPic->pic[idx]; //*(iterPic);
	if ( numPicsNotYetDisplayed > 2 && pcPic->fieldPic ) { //Field Decoding
		//PicList::iterator endPic   = pcListPic->end();
		//endPic--;
		//iterPic   = pcListPic->begin();
		//while (iterPic != endPic)
		//{
		//  Picture* pcPicTop = *(iterPic);
		//  iterPic++;
		//  Picture* pcPicBottom = *(iterPic);
		for (i = 0; i < PIC_LIST_SIZE; i += 2) {
			Picture* pcPicTop = pcListPic->pic[i]; //*(iterPic);
			Picture* pcPicBottom;
			if (pcPicTop == NULL)
				break;

			pcPicBottom = pcListPic->pic[i+1];
			if ( pcPicTop->neededForOutput && pcPicBottom->neededForOutput &&
			(numPicsNotYetDisplayed >  maxNumReorderPicsHighestTid || dpbFullness > maxDecPicBufferingHighestTid
#ifdef AML
			//|| pic_buf_available_num(p_app->m_cDecLib.vvc_dec) <= 2
#endif
			) &&
			(!(pcPicTop->poc%2) && pcPicBottom->poc == pcPicTop->poc+1) &&
			(pcPicTop->poc == p_app->m_iPOCLastDisplay+1 || p_app->m_iPOCLastDisplay < 0))
			{
				// write to file
				numPicsNotYetDisplayed = numPicsNotYetDisplayed-2;
#ifdef TO_DO
				if ( !m_reconFileName.empty() ) {
					const Window &conf = pcPicTop->cs->pps->getConformanceWindow();
					const bool isTff = pcPicTop->topField;

					bool display = true;

					if (display) {
						m_cVideoIOYuvReconFile[pcPicTop->layerId].write( pcPicTop->getRecoBuf(), pcPicBottom->getRecoBuf(),
						m_outputColourSpaceConvert,
						false, // TODO: m_packedYUVMode,
						conf.getWindowLeftOffset() * SPS::getWinUnitX( pcPicTop->cs->sps->getChromaFormatIdc() ),
						conf.getWindowRightOffset() * SPS::getWinUnitX( pcPicTop->cs->sps->getChromaFormatIdc() ),
						conf.getWindowTopOffset() * SPS::getWinUnitY( pcPicTop->cs->sps->getChromaFormatIdc() ),
						conf.getWindowBottomOffset() * SPS::getWinUnitY( pcPicTop->cs->sps->getChromaFormatIdc() ),
						NUM_CHROMA_FORMAT, isTff );
					}
				}
				writeLineToOutputLog(pcPicTop);
				writeLineToOutputLog(pcPicBottom);
#endif
				hevc_print(p_declib->hw, H266_DEBUG_DETAIL,
					"%s: top poc %d, bot poc %d\n",
					__func__, pcPicTop->poc, pcPicBottom->poc);

				// update POC of display order
				p_app->m_iPOCLastDisplay = pcPicBottom->poc; //getPOC();

				// erase non-referenced picture in the reference picture list after display
				if ( ! pcPicTop->referenced && pcPicTop->reconstructed ) {
					pcPicTop->reconstructed = false;
				}
				if ( ! pcPicBottom->referenced && pcPicBottom->reconstructed ) {
					pcPicBottom->reconstructed = false;
				}
				pcPicTop->neededForOutput = false;
				pcPicBottom->neededForOutput = false;
				if (pcPicTop->buf_cfg)
					pcPicTop->buf_cfg->pic_struct = 1;
				if (pcPicBottom->buf_cfg)
					pcPicBottom->buf_cfg->pic_struct = 2;
				prepare_display_buf(p_declib->hw, pcPicTop->buf_cfg);
				prepare_display_buf(p_declib->hw, pcPicBottom->buf_cfg);
			}
		}
	} else if ( !pcPic->fieldPic ) { //Frame Decoding
		//iterPic = pcListPic->begin();

		//while (iterPic != pcListPic->end())
		//{
		//  pcPic = *(iterPic);
		for (i = 0; i < PIC_LIST_SIZE; i++) {
			pcPic = pcListPic->pic[i]; //*(iterPic);
			if (pcPic == NULL)
				break;

			if (pcPic->neededForOutput && pcPic->poc >= p_app->m_iPOCLastDisplay &&
			(numPicsNotYetDisplayed >  maxNumReorderPicsHighestTid || dpbFullness > maxDecPicBufferingHighestTid
#ifdef AML
			//|| pic_buf_available_num(p_app->m_cDecLib.vvc_dec) <= 2
#endif
			)) {
				// write to file
				numPicsNotYetDisplayed--;
				if (!pcPic->referenced) {
					dpbFullness--;
				}

#ifdef TO_DO
				if (!m_reconFileName.empty()) {
					const Window &conf = pcPic->getConformanceWindow();
					const SPS* sps = pcPic->cs->sps;
					ChromaFormat chromaFormatIDC = sps->getChromaFormatIdc();
					if ( m_upscaledOutput ) {
						m_cVideoIOYuvReconFile[pcPic->layerId].writeUpscaledPicture( *sps, *pcPic->cs->pps, pcPic->getRecoBuf(), m_outputColourSpaceConvert, m_packedYUVMode, m_upscaledOutput, NUM_CHROMA_FORMAT, m_bClipOutputVideoToRec709Range );
					} else {
						m_cVideoIOYuvReconFile[pcPic->layerId].write( pcPic->getRecoBuf().get( COMPONENT_Y ).width, pcPic->getRecoBuf().get( COMPONENT_Y ).height, pcPic->getRecoBuf(),
						m_outputColourSpaceConvert,
						m_packedYUVMode,
						conf.getWindowLeftOffset() * SPS::getWinUnitX( chromaFormatIDC ),
						conf.getWindowRightOffset() * SPS::getWinUnitX( chromaFormatIDC ),
						conf.getWindowTopOffset() * SPS::getWinUnitY( chromaFormatIDC ),
						conf.getWindowBottomOffset() * SPS::getWinUnitY( chromaFormatIDC ),
						NUM_CHROMA_FORMAT, m_bClipOutputVideoToRec709Range );
					}
				}
				// Perform CTI on decoded frame and write to output CTI file
				if (!m_SEICTIFileName.empty()) {
					const Window& conf = pcPic->getConformanceWindow();
					const SPS* sps = pcPic->cs->sps;
					ChromaFormat chromaFormatIDC = sps->getChromaFormatIdc();
					if (m_upscaledOutput) {
						m_cVideoIOYuvSEICTIFile[pcPic->layerId].writeUpscaledPicture(*sps, *pcPic->cs->pps, pcPic->getDisplayBuf(), m_outputColourSpaceConvert, m_packedYUVMode, m_upscaledOutput, NUM_CHROMA_FORMAT, m_bClipOutputVideoToRec709Range);
					} else {
						m_cVideoIOYuvSEICTIFile[pcPic->layerId].write(pcPic->getRecoBuf().get(COMPONENT_Y).width, pcPic->getRecoBuf().get(COMPONENT_Y).height, pcPic->getDisplayBuf(),
						m_outputColourSpaceConvert, m_packedYUVMode,
						conf.getWindowLeftOffset() * SPS::getWinUnitX(chromaFormatIDC),
						conf.getWindowRightOffset() * SPS::getWinUnitX(chromaFormatIDC),
						conf.getWindowTopOffset() * SPS::getWinUnitY(chromaFormatIDC),
						conf.getWindowBottomOffset() * SPS::getWinUnitY(chromaFormatIDC),
						NUM_CHROMA_FORMAT, m_bClipOutputVideoToRec709Range);
					}
				}
				writeLineToOutputLog(pcPic);
#endif
				// update POC of display order
				hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "%s: poc %d\n", __func__, pcPic->poc);
				p_app->m_iPOCLastDisplay = pcPic->poc;

				// erase non-referenced picture in the reference picture list after display
				if (!pcPic->referenced && pcPic->reconstructed) {
					pcPic->reconstructed = false;
				}
				pcPic->neededForOutput = false;
				prepare_display_buf(p_declib->hw, pcPic->buf_cfg);
			}
			//iterPic++;
		}
	}
}

#define setOutputPicturePresentInStream() \
{ \
		if ( !outputPicturePresentInBitstream ) { \
			int i; \
			for (i = 0; i < PIC_LIST_SIZE; i++) { \
				Picture *pcPic = pcListPic->pic[i]; \
				if (pcPic == NULL) \
					break; \
				if (pcPic->neededForOutput) \
					outputPicturePresentInBitstream = true; \
			} \
		} \
}


void resetPictureSeiNalus(DecLib *p_declib)
{
#if 1
	int i;
	for (i = 0; i<LIST_PIC_SEI_NAL_SIZE; i++) {
		if (p_declib->m_pictureSeiNalus[i]) {
			free(p_declib->m_pictureSeiNalus[i]);
			p_declib->m_pictureSeiNalus[i] = NULL;
		}
	}
#else
	while (!m_pictureSeiNalus.empty())
	{
		delete m_pictureSeiNalus.front();
		m_pictureSeiNalus.pop_front();
	}
#endif
}

void updatePrevGDRInSameLayer(DecLib *p_declib)
{
	const NalUnitType pictureType = p_declib->m_pcPic->m_pictureType;

	if (pictureType == NAL_UNIT_CODED_SLICE_GDR && !p_declib->m_pcPic->cs->pps->m_mixedNaluTypesInPicFlag) {
		p_declib->m_prevGDRInSameLayerPOC[p_declib->m_pcPic->layerId] = p_declib->m_pcPic->poc;
	}
}

void updateAssociatedIRAP(DecLib *p_declib)
{
	const NalUnitType pictureType = p_declib->m_pcPic->m_pictureType;

	if ((pictureType == NAL_UNIT_CODED_SLICE_IDR_W_RADL ||
		pictureType == NAL_UNIT_CODED_SLICE_IDR_N_LP ||
		pictureType == NAL_UNIT_CODED_SLICE_CRA) && !p_declib->m_pcPic->cs->pps->m_mixedNaluTypesInPicFlag) {
		p_declib->m_associatedIRAPDecodingOrderNumber[p_declib->m_pcPic->layerId] = p_declib->m_pcPic->m_decodingOrderNumber;
		p_declib->m_pocCRA[p_declib->m_pcPic->layerId] = p_declib->m_pcPic->poc;
		p_declib->m_associatedIRAPType[p_declib->m_pcPic->layerId] = pictureType;
	}
}
#ifdef TO_DO
void updatePrevIRAPAndGDRSubpic(DecLib *p_declib)
{
	int j;
	for (j = 0; j < p_declib->m_uiSliceSegmentIdx; j++) {
		Slice* pcSlice = p_declib->m_pcPic->slices[j];
		const int subpicIdx = getSubPicIdxFromSubPicId(&pcSlice->m_pcPPS, pcSlice->m_sliceSubPicId);
		SubPic *sub_pic = cvector_get(&p_declib->m_pcPic->cs->pps->m_subPics, subpicIdx);
		if (cvector_get(&pcSlice->m_sliceMap.m_ctuAddrInSlice, 0) == sub_pic->m_firstCtuInSubPic) {
			const NalUnitType subpicType = pcSlice->m_eNalUnitType;
			if (subpicType == NAL_UNIT_CODED_SLICE_IDR_W_RADL ||
				subpicType == NAL_UNIT_CODED_SLICE_IDR_N_LP ||
				subpicType == NAL_UNIT_CODED_SLICE_CRA) {
				p_declib->m_prevIRAPSubpicPOC[p_declib->m_pcPic->layerId][subpicIdx] = p_declib->m_pcPic->poc;
				p_declib->m_prevIRAPSubpicType[p_declib->m_pcPic->layerId][subpicIdx] = subpicType;
				p_declib->m_prevIRAPSubpicDecOrderNo[p_declib->m_pcPic->layerId][subpicIdx] = p_declib->m_pcPic->m_decodingOrderNumber;
			} else if (subpicType == NAL_UNIT_CODED_SLICE_GDR) {
				p_declib->m_prevGDRSubpicPOC[p_declib->m_pcPic->layerId][subpicIdx] = p_declib->m_pcPic->poc;
			}
		}
	}
}
#endif
void finishPicture(DecLib *p_declib, int *poc, PicList **rpcListPic) //, enum MsgLevel msgl, bool associatedWithNewClvs)
{
	bool associatedWithNewClvs = false;
	char c;
	//#if RExt__DECODER_DEBUG_TOOL_STATISTICS
	//  CodingStatistics::StatTool& s = CodingStatistics::GetStatisticTool( STATS__TOOL_TOTAL_FRAME );
	//  s.count++;
	//  s.pixels = s.count * m_pcPic->Y().width * m_pcPic->Y().height;
	//#endif

	Slice*  pcSlice = p_declib->m_pcPic->cs->slice;
	VPS *vps;
	int iRefList;
	p_declib->m_prevPicPOC = pcSlice->m_iPOC;

	c = (pcSlice->m_eSliceType == I_SLICE ? 'I' : pcSlice->m_eSliceType == P_SLICE ? 'P' : 'B');
	if (!p_declib->m_pcPic->referenced) {
		c += 32;  // tolower
	}

	if (pcSlice->m_isDRAP) c = 'D';
	if (pcSlice->m_edrapRapId > 0) c = 'E';

	//-- For time output for each slice
	hevc_print(p_declib->hw, H266_DEBUG_DETAIL,
		"POC %4d LId: %2d TId: %1d ( %s, %c-SLICE, QP%3d ) ",
		pcSlice->m_iPOC, pcSlice->m_pcPic->layerId,
		pcSlice->m_uiTLayer,
		nalUnitTypeToString(pcSlice->m_eNalUnitType),
		c, pcSlice->m_iSliceQp );
	//printk("[DT %6.3f] ", pcSlice->m_dProcessingTime );

#if 1 //def TO_DO
	for (iRefList = 0; iRefList < 2; iRefList++) {
		int iRefIndex;
		hevc_print_cont(p_declib->hw, H266_DEBUG_DETAIL, "[L%d", iRefList);
		for (iRefIndex = 0; iRefIndex < pcSlice->m_aiNumRefIdx[iRefList]; iRefIndex++) {
			//const std::pair<int, int>& scaleRatio = pcSlice->getScalingRatio( RefPicList( iRefList ), iRefIndex );
			ScaleRatio *scaleRatio = getScalingRatio(pcSlice, iRefList, iRefIndex );

			if ( pcSlice->m_pcPicHeader->m_enableTMVPFlag &&
				pcSlice->m_colFromL0Flag == ((bool)(1 - iRefList)) && pcSlice->m_colRefIdx == iRefIndex ) {

				if ( scaleRatio->first != 1 << SCALE_RATIO_BITS || scaleRatio->second != 1 << SCALE_RATIO_BITS ) {
					//printk(" %dc(%1.2lfx, %1.2lfx)", pcSlice->m_aiRefPOCList[iRefList][iRefIndex], (double)( scaleRatio->first ) / ( 1 << SCALE_RATIO_BITS ), (double)( scaleRatio->second ) / ( 1 << SCALE_RATIO_BITS ) );
				} else {
					hevc_print_cont(p_declib->hw, H266_DEBUG_DETAIL, " %dc",
						pcSlice->m_aiRefPOCList[iRefList][iRefIndex] );
				}
			} else {
				if ( scaleRatio->first != 1 << SCALE_RATIO_BITS || scaleRatio->second != 1 << SCALE_RATIO_BITS ) {
					//printk(" %d(%1.2lfx, %1.2lfx)", pcSlice->m_aiRefPOCList[iRefList][iRefIndex], (double)( scaleRatio->first ) / ( 1 << SCALE_RATIO_BITS ), (double)( scaleRatio->second ) / ( 1 << SCALE_RATIO_BITS ) );
				} else {
					hevc_print_cont(p_declib->hw, H266_DEBUG_DETAIL,
						" %d", pcSlice->m_aiRefPOCList[iRefList][iRefIndex] );
				}
			}

			if ( pcSlice->m_aiRefPOCList[iRefList][iRefIndex] == pcSlice->m_iPOC ) {
				hevc_print_cont(p_declib->hw, H266_DEBUG_DETAIL, ".%d",
					pcSlice->m_apcRefPicList[iRefList][iRefIndex]->layerId );
			}
		}
		hevc_print_cont(p_declib->hw, H266_DEBUG_DETAIL, "] ");
	}
#endif
#ifdef TO_DO
	if (m_decodedPictureHashSEIEnabled) {
		SEIMessages pictureHashes = getSeisByType(m_pcPic->SEIs, SEI::DECODED_PICTURE_HASH );
		const SEIDecodedPictureHash *hash = ( pictureHashes.size() > 0 ) ? (SEIDecodedPictureHash*) *(pictureHashes.begin()) : NULL;
		if (pictureHashes.size() > 1) {
			msg( WARNING, "Warning: Got multiple decoded picture hash SEI messages. Using first.");
		}
		m_numberOfChecksumErrorsDetected += calcAndPrintHashStatus(((const Picture*) m_pcPic)->getRecoBuf(), hash, pcSlice->m_pcSPS->getBitDepths(), msgl);

		SEIMessages scalableNestingSeis = getSeisByType(m_pcPic->SEIs, SEI::SCALABLE_NESTING );
		for (auto seiIt : scalableNestingSeis) {
			SEIScalableNesting *nestingSei = dynamic_cast<SEIScalableNesting*>(seiIt);
			if (nestingSei->m_snSubpicFlag) {
				uint32_t subpicId = nestingSei->m_snSubpicId.front();
				SEIMessages nestedPictureHashes = getSeisByType(nestingSei->m_nestedSEIs, SEI::DECODED_PICTURE_HASH);
				for (auto decPicHash : nestedPictureHashes) {
					const SubPic& subpic = pcSlice->m_pcPPS->m_subPics[subpicId];
					const UnitArea area = UnitArea(pcSlice->m_pcSPS->getChromaFormatIdc(), Area(subpic.getSubPicLeft(), subpic.getSubPicTop(), subpic.getSubPicWidthInLumaSample(), subpic.getSubPicHeightInLumaSample()));
					PelUnitBuf recoBuf = m_pcPic->cs->getRecoBuf(area);
					m_numberOfChecksumErrorsDetected += calcAndPrintHashStatus(recoBuf, dynamic_cast<SEIDecodedPictureHash*>(decPicHash), pcSlice->m_pcSPS->getBitDepths(), msgl);
				}
			}
		}
	}
#endif
	hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "\n");

	//#if JVET_J0090_MEMORY_BANDWIDTH_MEASURE
	//    m_cacheModel.reportFrame();
	//    m_cacheModel.accumulateFrame();
	//    m_cacheModel.clear();
	//#endif

	p_declib->m_pcPic->neededForOutput = (pcSlice->m_pcPicHeader->m_picOutputFlag ? true : false);
	if (associatedWithNewClvs && p_declib->m_pcPic->neededForOutput) {
		if (!pcSlice->m_pcPPS->m_mixedNaluTypesInPicFlag && pcSlice->m_eNalUnitType == NAL_UNIT_CODED_SLICE_RASL) {
			p_declib->m_pcPic->neededForOutput = false;
		} else if (pcSlice->m_pcPPS->m_mixedNaluTypesInPicFlag) {
			bool isRaslPic = true;
			int i;
			for (i = 0; isRaslPic && i < p_declib->m_pcPic->numSlices; i++) {
				if (!(pcSlice->m_eNalUnitType == NAL_UNIT_CODED_SLICE_RASL ||
					pcSlice->m_eNalUnitType == NAL_UNIT_CODED_SLICE_RADL)) {
					isRaslPic = false;
				}
			}
			if (isRaslPic) {
				p_declib->m_pcPic->neededForOutput = false;
			}
		}
	}

	vps = pcSlice->m_pcVPS;
	if (vps != nullptr) {
		if (!vps->m_vpsEachLayerIsAnOlsFlag) {
			const int layerId        = pcSlice->m_nuhLayerId;
			const int generalLayerId = vps->m_generalLayerIdx[layerId];
			bool      layerIsOutput  = true;

			if (vps->m_vpsOlsModeIdc == 0) {
				layerIsOutput = generalLayerId == vps->m_targetOlsIdx;
			} else if (vps->m_vpsOlsModeIdc == 1) {
				layerIsOutput = generalLayerId <= vps->m_targetOlsIdx;
			} else if (vps->m_vpsOlsModeIdc == 2) {
				layerIsOutput = vps->m_vpsOlsOutputLayerFlag[vps->m_targetOlsIdx][generalLayerId];
			}
			if (!layerIsOutput) {
				p_declib->m_pcPic->neededForOutput = false;
			}
		}
	}
	p_declib->m_pcPic->reconstructed = true;
#ifdef TO_DO
	// process buffered suffix APS NALUs
	processSuffixApsNalus();
#endif
	sortPicList(&p_declib->m_cListPic); //Slice::sortPicList( m_cListPic ); // sorting for application output
	*poc                 = pcSlice->m_iPOC; //getPOC();
	*rpcListPic          = &p_declib->m_cListPic;
	p_declib->m_bFirstSliceInPicture  = true; // TODO: immer true? hier ist irgendwas faul
	p_declib->m_maxDecSubPicIdx = 0;
	p_declib->m_maxDecSliceAddrInSubPic = -1;

#ifdef TO_DO
	m_pcPic->destroyTempBuffers();
	m_pcPic->cs->destroyCoeffs();
	m_pcPic->cs->releaseIntermediateData();
#endif
#if GDR_ENABLED
	initPicHeader(&p_declib->m_picHeader); //m_picHeader.initPicHeader();
#else
	initPicHeader(p_declib->m_pcPic->cs->picHeader);//m_pcPic->cs->picHeader->initPicHeader();
#endif
	p_declib->m_puCounter++;
}

#if 0
uint32_t DecAppDecode(DecApp *p_app, param_t* params)
{
	uint8_t decode_end = false;
	DecLib *p_declib = &p_app->m_cDecLib;
	int skipFrameCounter;
	int                 poc;
	PicList* pcListPic = NULL;
	bool loopFiltered[MAX_VPS_LAYERS] = { false };
	bool bPicSkipped = false;
	bool isEosPresentInPu = false;
	bool isEosPresentInLastPu = false;
	bool outputPicturePresentInBitstream = false;
	bool gdrRecoveryPeriod[MAX_NUM_LAYER_IDS];
	bool prevPicSkipped = true;
	void *bitstreamFile = NULL;
	int i;
	for (i = 0; i < MAX_NUM_LAYER_IDS; i++)
		gdrRecoveryPeriod[i] = 0;

	xCreateDecLib(p_app);
	p_app->m_iPOCLastDisplay += p_app->m_iSkipFrame;      // set the last displayed POC correctly for skip forward.

	p_declib->m_mTidExternalSet = p_app->m_mTidExternalSet;
	p_declib->m_tOlsIdxTidExternalSet = p_app->m_tOlsIdxTidExternalSet;

	while (!decode_end) {
		NALUnit nalu;
		nalu.m_nalUnitType = NAL_UNIT_INVALID;
		// determine if next NAL unit will be the first one from a new picture
		bool bNewPicture = isNewPicture(p_declib, params);
		bool bNewAccessUnit = bNewPicture && isNewAccessUnit(p_declib, bNewPicture, params);
		if (!bNewPicture) {
#if 0
			AnnexBStats stats = AnnexBStats();

			// find next NAL unit in stream
			byteStreamNALUnit(bytestream, nalu.getBitstream().getFifo(), stats);
			if (nalu.getBitstream().getFifo().empty()) {
				/* this can happen if the following occur:
				*  - empty input file
				*  - two back-to-back start_code_prefixes
				*  - start_code_prefix immediately followed by EOF
				*/
				msg( ERROR, "Warning: Attempt to decode an empty NAL unit\n");
			} else
#endif
			{
				// read NAL unit header
				read(nalu);

				// flush output for first slice of an IDR picture
				if (p_declib->m_bFirstSliceInPicture &&
				(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL ||
				nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP)) {
					if (!getMixedNaluTypesInPicFlag(p_declib)) {
						p_app->m_newCLVS[nalu.m_nuhLayerId] = true;   // An IDR picture starts a new CLVS
						xFlushOutput(p_app, pcListPic, nalu.m_nuhLayerId);
					} else {
						p_app->m_newCLVS[nalu.m_nuhLayerId] = false;
					}
				} else if (p_declib->m_bFirstSliceInPicture && nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA && isEosPresentInLastPu) {
					// A CRA that is immediately preceded by an EOS is a CLVSS
					p_app->m_newCLVS[nalu.m_nuhLayerId] = true;
					xFlushOutput(p_app, pcListPic, nalu.m_nuhLayerId);
				} else if (p_declib->m_bFirstSliceInPicture && nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA && !isEosPresentInLastPu) {
					// A CRA that is not immediately precede by an EOS is not a CLVSS
					p_app->m_newCLVS[nalu.m_nuhLayerId] = false;
				} else if (p_declib->m_bFirstSliceInPicture && !isEosPresentInLastPu) {
					p_app->m_newCLVS[nalu.m_nuhLayerId] = false;
				}

				// parse NAL unit syntax if within target decoding layer
				if ( ( p_app->m_iMaxTemporalLayer < 0 || nalu.m_temporalId <= p_app->m_iMaxTemporalLayer ) && xIsNaluWithinTargetDecLayerIdSet(p_app, &nalu ) )
				{
					//CHECK(nalu.m_temporalId > p_app->m_iMaxTemporalLayer, "bitstream shall not include any NAL unit with TemporalId greater than HighestTid");
					//if (m_targetDecLayerIdSet.size())
					//{
					//  CHECK(std::find(m_targetDecLayerIdSet.begin(), m_targetDecLayerIdSet.end(), nalu.m_nuhLayerId) == m_targetDecLayerIdSet.end(), "bitstream shall not contain any other layers than included in the OLS with OlsIdx");
					//}
					if (bPicSkipped) {
						if ((nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_TRAIL) ||
							(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_STSA) ||
							(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_RASL) ||
							(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_RADL) ||
							(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL) ||
							(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP) ||
							(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA) ||
							(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_GDR)) {
							if (isSliceNaluFirstInAU(p_declib, true, &nalu)) {
								resetAccessUnitNals(p_declib);
								resetAccessUnitApsNals(p_declib);
								resetAccessUnitPicInfo(p_declib);
							}
							bPicSkipped = false;
						}
					}

					skipFrameCounter = p_app->m_iSkipFrame;
					decode(p_declib, &nalu, p_app->m_targetOlsIdx);

					if ( prevPicSkipped && nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_GDR ) {
						gdrRecoveryPeriod[nalu.m_nuhLayerId] = true;
					}

					if ( skipFrameCounter == 1 &&
					( nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_GDR  || nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA )) {
						skipFrameCounter--;
					}

					if ( p_app->m_iSkipFrame < skipFrameCounter  &&
					((nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_TRAIL) ||
					(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_STSA) ||
					(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_RASL) ||
					(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_RADL) ||
					(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL) ||
					(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP) ||
					(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA) ||
					(nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_GDR))) {
						if (isSliceNaluFirstInAU(p_declib, true, &nalu)) {
							//checkSeiInPictureUnit(p_declib);
							resetPictureSeiNalus(p_declib);
							//checkAPSInPictureUnit(p_declib);
							resetPictureUnitNals(p_declib);
							resetAccessUnitSeiTids(p_declib);
							//checkSEIInAccessUnit(p_declib);
							resetAccessUnitSeiPayLoadTypes(p_declib);
							resetAccessUnitNals(p_declib);
							resetAccessUnitApsNals(p_declib);
							resetAccessUnitPicInfo(p_declib);
						}
						bPicSkipped = true;
						p_app->m_iSkipFrame++;   // skipFrame count restore, the real decrement occur at the begin of next frame
					}

					if (nalu.m_nalUnitType == NAL_UNIT_OPI) {
						if (!p_declib->m_mTidExternalSet && p_declib->m_opi->m_htidinfopresentflag) {
							p_app->m_iMaxTemporalLayer = p_declib->m_opi->m_opihtidplus1-1;
						}
						p_declib->m_mTidOpiSet = p_declib->m_opi->m_htidinfopresentflag;
					}
					if (nalu.m_nalUnitType == NAL_UNIT_VPS) {
						deriveTargetOutputLayerSet(p_declib, p_declib->m_vps->m_targetOlsIdx);
						p_app->m_targetDecLayerIdSet = p_declib->m_vps->m_targetLayerIdSet;
						p_app->m_targetOutputLayerIdSet = p_declib->m_vps->m_targetOutputLayerIdSet;
					}
				} else {
					bPicSkipped = true;
				}
			}
			if (isSlice(nalu) && nalu.m_nalUnitType != NAL_UNIT_CODED_SLICE_RASL) {
				prevPicSkipped = bPicSkipped;
			}

			// once an EOS NAL unit appears in the current PU, mark the variable isEosPresentInPu as true
			if (nalu.m_nalUnitType == NAL_UNIT_EOS) {
				isEosPresentInPu = true;
				p_app->m_newCLVS[nalu.m_nuhLayerId] = true;  //The presence of EOS means that the next picture is the beginning of new CLVS
			}
			// within the current PU, only EOS and EOB are allowed to be sent after an EOS nal unit
			if (isEosPresentInPu) {
				//CHECK(nalu.m_nalUnitType != NAL_UNIT_EOS && nalu.m_nalUnitType != NAL_UNIT_EOB, "When an EOS NAL unit is present in a PU, it shall be the last NAL unit among all NAL units within the PU other than other EOS NAL units or an EOB NAL unit");
			}
		}


		if ((bNewPicture || !bitstreamFile || nalu.m_nalUnitType == NAL_UNIT_EOS) &&
			!p_declib->m_firstSliceInSequence[nalu.m_nuhLayerId] && !bPicSkipped) {
			if (!loopFiltered[nalu.m_nuhLayerId] || bitstreamFile) {
#ifndef BUFMGR_ONLY
				m_cDecLib.executeLoopFilters();
#endif
				finishPicture(p_declib, &poc, &pcListPic); //, INFO, p_app->m_newCLVS[nalu.m_nuhLayerId]);
			}
			loopFiltered[nalu.m_nuhLayerId] = (nalu.m_nalUnitType == NAL_UNIT_EOS);
			if (nalu.m_nalUnitType == NAL_UNIT_EOS) {
				p_declib->m_firstSliceInSequence[nalu.m_nuhLayerId] = true;
			}

			updateAssociatedIRAP(p_declib);
			updatePrevGDRInSameLayer(p_declib);
#ifdef TO_DO
			updatePrevIRAPAndGDRSubpic(p_declib);
#endif
			if ( gdrRecoveryPeriod[nalu.m_nuhLayerId] ) {
				if ( getGDRRecoveryPocReached(p_declib) ) {
					gdrRecoveryPeriod[nalu.m_nuhLayerId] = false;
				}
			}
		} else if ( (bNewPicture || !bitstreamFile || nalu.m_nalUnitType == NAL_UNIT_EOS ) &&
		p_declib->m_firstSliceInSequence[nalu.m_nuhLayerId]) {
			p_declib->m_bFirstSliceInPicture = true;
		}

		if ( pcListPic ) {

			if ( gdrRecoveryPeriod[nalu.m_nuhLayerId] ) { // Suppress YUV and OPL output during GDR recovery
#if 1
				int i;
				for (i = 0; i < PIC_LIST_SIZE; i++) {
					Picture *pcPic = pcListPic->pic[i];
					if (pcPic == NULL)
						break;
					if (pcPic->layerId == nalu.m_nuhLayerId) {
						pcPic->neededForOutput = false;
					}
				}
#else
				PicList::iterator iterPic = pcListPic->begin();
				while (iterPic != pcListPic->end()) {
					Picture *pcPic = *(iterPic++);
					if (pcPic->layerId == nalu.m_nuhLayerId) {
						pcPic->neededForOutput = false;
					}
				}
#endif
			}
#ifdef TO_DO
			if ( !m_reconFileName.empty() && !m_cVideoIOYuvReconFile[nalu.m_nuhLayerId].isOpen() )
			{
				const BitDepths &bitDepths=pcListPic->front()->cs->sps->getBitDepths(); // use bit depths of first reconstructed picture.
				for ( uint32_t channelType = 0; channelType < MAX_NUM_CHANNEL_TYPE; channelType++ ) {
					if ( m_outputBitDepth[channelType] == 0 ) {
						m_outputBitDepth[channelType] = bitDepths.recon[channelType];
					}
				}

				if (m_packedYUVMode && (m_outputBitDepth[CH_L] != 10 && m_outputBitDepth[CH_L] != 12)) {
					EXIT ("Invalid output bit-depth for packed YUV output, aborting\n");
				}

				std::string reconFileName = m_reconFileName;
				if ( m_reconFileName.compare( "/dev/null" ) && m_cDecLib.m_pcVPS != nullptr && m_cDecLib.m_pcVPS->getMaxLayers() > 1 && xIsNaluWithinTargetOutputLayerIdSet( &nalu ) )
				{
					size_t pos = reconFileName.find_last_of('.');
					std::string layerString = std::string(".layer") + std::to_string(nalu.m_nuhLayerId);
					if (pos != string::npos) {
						reconFileName.insert(pos, layerString);
					} else {
						reconFileName.append(layerString);
					}
				}
				if ( ( m_cDecLib.m_pcVPS != nullptr && ( m_cDecLib.m_pcVPS->getMaxLayers() == 1 || xIsNaluWithinTargetOutputLayerIdSet( &nalu ) ) ) || m_cDecLib.m_pcVPS == nullptr )
				{
					m_cVideoIOYuvReconFile[nalu.m_nuhLayerId].open( reconFileName, true, m_outputBitDepth, m_outputBitDepth, bitDepths.recon ); // write mode
				}
			}
			// update file bitdepth shift if recon bitdepth changed between sequences
			for ( uint32_t channelType = 0; channelType < MAX_NUM_CHANNEL_TYPE; channelType++ )
			{
				int reconBitdepth = pcListPic->front()->cs->sps->getBitDepth((ChannelType)channelType);
				int fileBitdepth  = m_cVideoIOYuvReconFile[nalu.m_nuhLayerId].getFileBitdepth(channelType);
				int bitdepthShift = m_cVideoIOYuvReconFile[nalu.m_nuhLayerId].getBitdepthShift(channelType);
				if ( fileBitdepth + bitdepthShift != reconBitdepth ) {
					m_cVideoIOYuvReconFile[nalu.m_nuhLayerId].setBitdepthShift(channelType, reconBitdepth - fileBitdepth);
				}
			}
			if (!m_SEICTIFileName.empty() && !m_cVideoIOYuvSEICTIFile[nalu.m_nuhLayerId].isOpen())
			{
				const BitDepths& bitDepths = pcListPic->front()->cs->sps->getBitDepths(); // use bit depths of first reconstructed picture.
				uint32_t channelType;
				for (channelType = 0; channelType < MAX_NUM_CHANNEL_TYPE; channelType++) {
					if (m_outputBitDepth[channelType] == 0) {
						m_outputBitDepth[channelType] = bitDepths.recon[channelType];
					}
				}

				if (m_packedYUVMode && (m_outputBitDepth[CH_L] != 10 && m_outputBitDepth[CH_L] != 12)) {
					EXIT("Invalid output bit-depth for packed YUV output, aborting\n");
				}

				std::string SEICTIFileName = m_SEICTIFileName;
				if (m_SEICTIFileName.compare("/dev/null") && m_cDecLib.m_pcVPS != nullptr &&
					m_cDecLib.m_pcVPS->getMaxLayers() > 1 && xIsNaluWithinTargetOutputLayerIdSet(&nalu)) {
					size_t pos = SEICTIFileName.find_last_of('.');
					if (pos != string::npos) {
						SEICTIFileName.insert(pos, std::to_string(nalu.m_nuhLayerId));
					} else {
						SEICTIFileName.append(std::to_string(nalu.m_nuhLayerId));
					}
				}
				if ((m_cDecLib.m_pcVPS != nullptr && (m_cDecLib.m_pcVPS->getMaxLayers() == 1 ||
					xIsNaluWithinTargetOutputLayerIdSet(&nalu))) || m_cDecLib.m_pcVPS == nullptr) {
					m_cVideoIOYuvSEICTIFile[nalu.m_nuhLayerId].open(SEICTIFileName, true, m_outputBitDepth, m_outputBitDepth, bitDepths.recon); // write mode
				}
			}
			if (!m_annotatedRegionsSEIFileName.empty()) {
				xOutputAnnotatedRegions(pcListPic);
			}
#endif
			// write reconstruction to file
			if ( bNewPicture ) {
				setOutputPicturePresentInStream();
				xWriteOutput(p_app, pcListPic, nalu.m_temporalId );
			}
			if (nalu.m_nalUnitType == NAL_UNIT_EOS) {
#ifdef TO_DO
				if (!m_annotatedRegionsSEIFileName.empty() && bNewPicture) {
					xOutputAnnotatedRegions(pcListPic);
				}
#endif
				setOutputPicturePresentInStream();
				xWriteOutput(p_app, pcListPic, nalu.m_temporalId );
				p_declib->m_bFirstSliceInPicture = false;
			}
			// write reconstruction to file -- for additional bumping as defined in C.5.2.3
			if (!bNewPicture && ((nalu.m_nalUnitType >= NAL_UNIT_CODED_SLICE_TRAIL && nalu.m_nalUnitType <= NAL_UNIT_RESERVED_IRAP_VCL_11)
			|| (nalu.m_nalUnitType >= NAL_UNIT_CODED_SLICE_IDR_W_RADL && nalu.m_nalUnitType <= NAL_UNIT_CODED_SLICE_GDR))) {
				setOutputPicturePresentInStream();
				xWriteOutput(p_app, pcListPic, nalu.m_temporalId );
			}
		}
		if ( bNewPicture ) {
			//checkSeiInPictureUnit(p_declib);
			resetPictureSeiNalus(p_declib);
			// reset the EOS present status for the next PU check
			isEosPresentInLastPu = isEosPresentInPu;
			isEosPresentInPu = false;
		}
		if (bNewPicture || !bitstreamFile || nalu.m_nalUnitType == NAL_UNIT_EOS) {
			//checkAPSInPictureUnit(p_declib);
			resetPictureUnitNals(p_declib);
		}
		if (bNewAccessUnit || !bitstreamFile) {
			//CheckNoOutputPriorPicFlagsInAccessUnit(p_declib);
			resetAccessUnitNoOutputPriorPicFlags(p_declib);
			//checkLayerIdIncludedInCvss(p_declib);
			resetAccessUnitEos(p_declib);
			resetAudIrapOrGdrAuFlag(p_declib);
		}
		if (bNewAccessUnit) {
			//checkTidLayerIdInAccessUnit(p_declib);
			resetAccessUnitSeiTids(p_declib);
			//checkSEIInAccessUnit(p_declib);
			resetAccessUnitSeiPayLoadTypes(p_declib);
			resetAccessUnitNals(p_declib);
			resetAccessUnitApsNals(p_declib);
			resetAccessUnitPicInfo(p_declib);
		}
	}
}
#endif

#ifdef AML
#ifdef USE_FULL_REF_LIST_BUFFER
void get_ref_set_(DecApp *p_app, unsigned char *ref_list_buf_v, ref_set_t *p_ref0_set, ref_set_t *p_ref1_set, int ref_set_num)
{
	int ii, i, j;
	DecLib *p_declib = &p_app->m_cDecLib;
	struct hevc_state_s *hevc = (struct hevc_state_s *)p_declib->hw;
	unsigned char *p = ref_list_buf_v;
	//ref_set_t *p_ref_set = &p_app->RPL0_set[0];
	ref_set_t *p_ref_set = p_ref0_set;
	/*
	.ref_list = {
	// ref_list STOREAREA
	// 64(num_ref_pic_lists) * 32(num_ref_entries) * 4(32-bits per entries) *2 (L0+L1) = 0x4000(16kBytes)
	// 32 ref_entries map :
	//   ref_entry_info - {LtrpInSliceHeaderFlag,numIlrp[4:0],numLtrp[4:0],numStrp[4:0],numRefPic[15:0]}
	//   ref_entry_[0-30] - {isLongTerm, isInterLayerRefPic, ilrp_idx(Ilrp)/poc_lsb_lt(Ltrp)/deltaValue(Strp)[29:0]}
	.buf_size = 0x4000,
	},
	00 03 00 03 3f ff ff f0 3f ff ff e0 3f ff ff e8
	*/

	for (i = 0; i < ref_set_num; i++) {
		for (ii = 0; ii < 2; ii++) {
			unsigned ref_entry_info = (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
			if (ii == 0)
				p_ref_set = &p_ref0_set[i];
			else
				p_ref_set = &p_ref1_set[i];
			p_ref_set->LtrpInSliceHeaderFlag = (ref_entry_info >> (16 + 15)) & 0x1;
			p_ref_set->numIlrp = (ref_entry_info >> (16 + 10)) & 0x1f;
			p_ref_set->numLtrp = (ref_entry_info >> (16 + 5)) & 0x1f;
			p_ref_set->numStrp = (ref_entry_info >> 16) & 0x1f;
			p_ref_set->numRefPic = ref_entry_info & 0xffff;
			hevc_print(hevc, H266_DEBUG_REF_LIST,
				"%d L%d: LtrpInSliceHeaderFlag %d numIlrp %d numLtrp %d numStrp %d numRefPic %d\n",
				i, ii,
				p_ref_set->LtrpInSliceHeaderFlag,
				p_ref_set->numIlrp,
				p_ref_set->numLtrp,
				p_ref_set->numStrp,
				p_ref_set->numRefPic);
			p += 4;
			for (j = 0; j < REF_ENTRY_NUM; j++) {
				p_ref_set->ref_entry[j] = (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
				if (get_dbg_flag(hevc) & H266_DEBUG_REF_LIST) {
					hevc_print_cont(hevc, 0, "%08x ", p_ref_set->ref_entry[j]);
					if (j == 16 || j == (REF_ENTRY_NUM-1))
						hevc_print_cont(hevc, 0, "\n");
				}
				p += 4;
			}
		}
		//hevc_print(hevc, H266_DEBUG_DETAIL, "\n");
	}
}

void get_ref_set(DecApp *p_app, unsigned char *ref_list_buf_v)
{
	int i;
	DecLib *p_declib = &p_app->m_cDecLib;
	for (i = 0; i < 16; i++) {
		hevc_print(p_declib->hw, H266_DEBUG_REF_LIST,
			"========== sps ref set %d ========\n", i);
		get_ref_set_(p_app, ref_list_buf_v+(i*16*1024),
			&p_app->sps_RPL_set[i].RPL0_set[0], &p_app->sps_RPL_set[i].RPL1_set[0], REF_SET_NUM);
	}
	hevc_print(p_declib->hw, H266_DEBUG_REF_LIST,
		"========== slice ref set %d ========\n", i);
	get_ref_set_(p_app, ref_list_buf_v+(16*16*1024),
		&p_app->slice_RPL0_set, &p_app->slice_RPL1_set, 1);
}


void get_ref_set_fast_(DecApp *p_app, unsigned char *ref_list_buf_v, ref_set_t *p_ref_set)
{
	int j;
	DecLib *p_declib = &p_app->m_cDecLib;
	struct hevc_state_s *hevc = (struct hevc_state_s *)p_declib->hw;
	unsigned char *p = ref_list_buf_v;

	/*
	.ref_list = {
	// ref_list STOREAREA
	// 64(num_ref_pic_lists) * 32(num_ref_entries) * 4(32-bits per entries) *2 (L0+L1) = 0x4000(16kBytes)
	// 32 ref_entries map :
	//   ref_entry_info - {LtrpInSliceHeaderFlag,numIlrp[4:0],numLtrp[4:0],numStrp[4:0],numRefPic[15:0]}
	//   ref_entry_[0-30] - {isLongTerm, isInterLayerRefPic, ilrp_idx(Ilrp)/poc_lsb_lt(Ltrp)/deltaValue(Strp)[29:0]}
	.buf_size = 0x4000,
	},
	00 03 00 03 3f ff ff f0 3f ff ff e0 3f ff ff e8
	*/

	unsigned ref_entry_info = (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];

	p_ref_set->LtrpInSliceHeaderFlag = (ref_entry_info >> (16 + 15)) & 0x1;
	p_ref_set->numIlrp = (ref_entry_info >> (16 + 10)) & 0x1f;
	p_ref_set->numLtrp = (ref_entry_info >> (16 + 5)) & 0x1f;
	p_ref_set->numStrp = (ref_entry_info >> 16) & 0x1f;
	p_ref_set->numRefPic = ref_entry_info & 0xffff;
	hevc_print(hevc, H266_DEBUG_REF_LIST, "adr %p: LtrpInSliceHeaderFlag %d numIlrp %d numLtrp %d numStrp %d numRefPic %d\n",
		p,
		p_ref_set->LtrpInSliceHeaderFlag,
		p_ref_set->numIlrp,
		p_ref_set->numLtrp,
		p_ref_set->numStrp,
		p_ref_set->numRefPic);
	p += 4;
	for (j = 0; j < REF_ENTRY_NUM; j++) {
		p_ref_set->ref_entry[j] = (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
		if (get_dbg_flag(hevc) & H266_DEBUG_REF_LIST) {
			hevc_print_cont(hevc, 0, "%08x ", p_ref_set->ref_entry[j]);
		if (j == 16 || j == (REF_ENTRY_NUM - 1))
			hevc_print_cont(hevc, 0, "\n");
		}
		p += 4;
	}
}

void get_ref_set_fast(DecApp *p_app, unsigned char *ref_list_buf_v, int sps_seq_parameter_set_id, int rpl0_index, int rpl1_index)
{
	int i = sps_seq_parameter_set_id;
	DecLib *p_declib = &p_app->m_cDecLib;
	if (rpl0_index < 64) {
		hevc_print(p_declib->hw, H266_DEBUG_REF_LIST, "========== sps ref set0 %d (%d) ========\n", i, rpl0_index);
		get_ref_set_fast_(p_app, ref_list_buf_v+(i*16*1024)+(rpl0_index*2)*(REF_ENTRY_NUM+1)*4, &p_app->sps_RPL_set[i].RPL0_set[rpl0_index]);
	} else {
		hevc_print(p_declib->hw, H266_DEBUG_REF_LIST, "========== slice ref set0 %d ========\n", i);
		get_ref_set_fast_(p_app, ref_list_buf_v+(16*16*1024), &p_app->slice_RPL0_set);
	}

	if (rpl1_index < 64) {
		hevc_print(p_declib->hw, H266_DEBUG_REF_LIST, "========== sps ref set1 %d (%d) ========\n", i, rpl1_index);
		get_ref_set_fast_(p_app, ref_list_buf_v+(i*16*1024)+((rpl1_index*2)+1)*(REF_ENTRY_NUM+1)*4, &p_app->sps_RPL_set[i].RPL1_set[rpl1_index]);
	} else {
		hevc_print(p_declib->hw, H266_DEBUG_REF_LIST, "========== slice ref set1 %d ========\n", i);
		get_ref_set_fast_(p_app, ref_list_buf_v+(16*16*1024)+(REF_ENTRY_NUM+1)*4, &p_app->slice_RPL1_set);
	}
}

#else
void get_ref_set(DecApp *p_app, unsigned char *ref_list_buf_v)
{
	int ii, i, j;
	DecLib *p_declib = &p_app->m_cDecLib;
	unsigned char *p = ref_list_buf_v;
	ref_set_t *p_ref_set = &p_app->RPL0_set[0];
	/*
	.ref_list = {
	// ref_list STOREAREA
	// 64(num_ref_pic_lists) * 32(num_ref_entries) * 4(32-bits per entries) *2 (L0+L1) = 0x4000(16kBytes)
	// 32 ref_entries map :
	//   ref_entry_info - {LtrpInSliceHeaderFlag,numIlrp[4:0],numLtrp[4:0],numStrp[4:0],numRefPic[15:0]}
	//   ref_entry_[0-30] - {isLongTerm, isInterLayerRefPic, ilrp_idx(Ilrp)/poc_lsb_lt(Ltrp)/deltaValue(Strp)[29:0]}
	.buf_size = 0x4000,
	},
	00 03 00 03 3f ff ff f0 3f ff ff e0 3f ff ff e8
	*/

	for (ii = 0; ii < 2; ii++) {
		for (i = 0; i < REF_SET_NUM; i++) {
			unsigned ref_entry_info = (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
			p_ref_set->LtrpInSliceHeaderFlag = (ref_entry_info >> (16 + 15)) & 0x1;
			p_ref_set->numIlrp = (ref_entry_info >> (16 + 10)) & 0x1f;
			p_ref_set->numLtrp = (ref_entry_info >> (16 + 5)) & 0x1f;
			p_ref_set->numStrp = (ref_entry_info >> 16) & 0x1f;
			p_ref_set->numRefPic = ref_entry_info & 0xffff;
			hevc_print(p_declib->hw, H266_DEBUG_DETAIL,
				"%d: LtrpInSliceHeaderFlag %d numIlrp %d numLtrp %d numStrp %d numRefPic %d\n",
				i,
				p_ref_set->LtrpInSliceHeaderFlag,
				p_ref_set->numIlrp,
				p_ref_set->numLtrp,
				p_ref_set->numStrp,
				p_ref_set->numRefPic);
			p += 4;
			for (j = 0; j < REF_ENTRY_NUM; j++) {
				p_ref_set->ref_entry[j] = (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
				if (0) {
					hevc_print_cont(p_declib->hw, H266_DEBUG_DETAIL, "%08x ", p_ref_set->ref_entry[j]);
					if (j == 16 || j == (REF_ENTRY_NUM-1))
						hevc_print_cont(p_declib->hw, H266_DEBUG_DETAIL, "\n");
				}
				p += 4;
			}
			p_ref_set++;
		}
		p_ref_set = &p_app->RPL1_set[0];
		hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "\n");
		hevc_print(p_declib->hw, H266_DEBUG_DETAIL, "size=0x%x\n", p-ref_list_buf_v);
	}
}
#endif

/*
Slice *h266_get_col_slice(DecApp *p_app, Slice* slice)
{ //c model UnitTools.cpp, getColocatedMVP()
	const Slice *pColSlice = nullptr;
	enum RefPicList e = slice->m_eSliceType == B_SLICE ? 1 - slice->m_colFromL0Flag : 0;
	const Picture *pColPic = slice->m_apcRefPicList[e][slice->m_colRefIdx]; //getRefPic()
	int mi_sliceIdx = slice->m_independentSliceIdx; //to change ...
	int i;

	if (!pColPic)
		return NULL;

	for (i = 0; i < SLICE_MAX_NUM; i++) // const auto s : pColPic->slices )
	{
		Slice *s = pColPic->slices[i];
		if (s == NULL)
			break;
		if ( s->m_independentSliceIdx == mi_sliceIdx) //mi.sliceIdx )
		{
			pColSlice = s;
			break;
		}
	}
	printk("%s: cur slice poc %d type %d colFromL0Flag %d colRefIdx %d ref list %d pColSlice %p\n",
	__func__, slice->m_iPOC, slice->m_eSliceType, slice->m_colFromL0Flag,
	slice->m_colRefIdx, e, pColSlice);
	return pColSlice;
}
*/

Picture *h266_get_col_picture(DecApp *p_app, Slice* slice)
{ //c model UnitTools.cpp, getColocatedMVP()
	DecLib *p_declib = &p_app->m_cDecLib;
	enum RefPicList e = slice->m_eSliceType == B_SLICE ? 1 - slice->m_colFromL0Flag : 0;
	Picture *pColPic = slice->m_apcRefPicList[e][slice->m_colRefIdx]; //getRefPic()
	if (pColPic)
		hevc_print(p_declib->hw, H266_DEBUG_DETAIL,
		"%s: cur slice poc %d type %d colFromL0Flag %d colRefIdx %d ref list %d pColPic poc = %d\n",
		__func__, slice->m_iPOC, slice->m_eSliceType, slice->m_colFromL0Flag,
		slice->m_colRefIdx, e, pColPic->poc);

	return pColPic;
}

bool h266_is_ref_scaled(Slice* slice, int refList, int rIdx)
{
	const PPS* pps = slice->m_pcPPS; //getPPS();
	return isRefScaled(slice->m_apcRefPicList[refList][rIdx], pps);
}

bool h266_is_long_term(struct vvc_decoder *hw, int poc)
{
	DecLib *p_declib = &hw->m_decApp.m_cDecLib;
	Picture *pcPic=NULL;
	int ii;
	for (ii = 0; ii < PIC_LIST_SIZE; ii++) {
		pcPic = p_declib->m_cListPic.pic[ii];
		if ((pcPic == NULL) || (pcPic->poc == poc))
			break;
	}
	if (pcPic)
		return pcPic->longTerm;
	else {
		hevc_print(p_declib->hw, 0, "Warning: %s, poc of %d not in the picture list\n", __func__, poc);
		return 0;
	}
}

void h266_init_decode(DecApp *p_app)
{
	//int i;
	DecLib *p_declib = &p_app->m_cDecLib;
	init_dec(p_app);

	xCreateDecLib(p_app);

	memset(&p_declib->a_nalu, 0, sizeof(NALUnit));
	p_declib->a_cur_vps = new_vps();
	p_declib->a_cur_sps = new_sps();
	p_declib->a_cur_pps = new_pps();
	p_declib->m_vps = p_declib->a_cur_vps;
}

int h266_bufmgr_code_process(DecApp *p_app, int start_code)
{
	DecLib *p_declib = &p_app->m_cDecLib;
	int ret = 0;
	if (start_code == SEQUENCE_END_CODE) {
		xFlushOutput(p_app, &p_declib->m_cListPic, p_declib->a_nalu.m_nuhLayerId);
		if (get_dbg_flag(p_declib->hw) & H266_DEBUG_DETAIL)
			print_vvc_picture_list(p_declib, &p_declib->m_cListPic, "After xFlushOutput() due to SEQUENCE_END_CODE");
		p_declib->m_pocRandomAccess = MAX_INT;
		p_declib->m_prevLayerID = MAX_INT;
		p_declib->m_prevPOC = -MAX_INT;
		p_declib->m_prevSliceSkipped = false;
		p_declib->m_skippedPOC = 0;
		p_declib->m_accessUnitEos[p_declib->a_nalu.m_nuhLayerId] = true;
		p_declib->m_prevEOS[p_declib->a_nalu.m_nuhLayerId] = true;
	}
	return ret;
}


int h266_bufmgr_process(DecApp *p_app, param_t *param, bool bNewPicture)
{
	int ret = 0;
	int skipFrameCounter;
	DecLib *p_declib = &p_app->m_cDecLib;
	//bool bNewPicture;
	bool isEosPresentInLastPu = false;
	p_declib->param = param;
	p_declib->a_nalu.m_nalUnitType = (param->p.nalu >> 3) & 0x1f;
	p_declib->a_nalu.m_nuhLayerId = (param->p.nalu >> 8) & 0x3f;

	if (p_declib->a_nalu.m_nuhLayerId > 0) {
		//printk("Error, m_nuhLayerId = %d, not supported!!\n", p_declib->a_nalu.m_nuhLayerId);
		//exit(0);
	}
	//update pps, vps, sps
#ifdef MODIFY_CODE
	//p_declib->m_bFirstSliceInPicture = (param->p.sliceAddr == 0);

	if ( (bNewPicture || p_declib->a_nalu.m_nalUnitType == NAL_UNIT_EOS ) &&
		p_declib->m_firstSliceInSequence[p_declib->a_nalu.m_nuhLayerId]) {
		p_declib->m_bFirstSliceInPicture = true;
	}

#else
	bNewPicture = isNewPicture(p_declib, params);
	if ( (bNewPicture || p_declib->a_nalu.m_nalUnitType == NAL_UNIT_EOS ) &&
		p_declib->m_firstSliceInSequence[p_declib->a_nalu.m_nuhLayerId]) {
		p_declib->m_bFirstSliceInPicture = true;
	}
#endif
	p_declib->a_cur_sps->m_uiMaxCUWidth = param->p.maxCUSize;
	p_declib->a_cur_sps->m_uiMaxCUHeight = param->p.maxCUSize;
	p_declib->a_cur_sps->m_SMVD = (param->p.sps_decoding_flags_2 >> 8) & 0x1;
	p_declib->a_cur_sps->m_chromaFormatIdc = param->p.sps_chroma_format_idc;
	p_declib->a_cur_sps->m_subPicIdLen = (param->p.sps_flag_1 >> 8) & 0xf;
	p_declib->a_cur_sps->m_uiBitsForPOC = (param->p.sps_flag_0 >> 6) & 0x1f;
	p_declib->a_cur_sps->m_pocMsbCycleLen = (param->p.sps_flag_0 >> 0) & 0x1f;

	setWindow(&p_declib->a_cur_pps->m_scalingWindow,
	(short)(param->p.pps_scaling_win_left_offset), (short)(param->p.pps_scaling_win_right_offset),
	(short)(param->p.pps_scaling_win_top_offset), (short)(param->p.pps_scaling_win_bottom_offset));
	p_declib->a_cur_pps->m_picWidthInLumaSamples = param->p.pic_width_in_luma_samples; //pic_width_in_luma_samples is pps_pic_width_in_luma_samples, PicWidthInLumaSamples is from sps
	p_declib->a_cur_pps->m_picHeightInLumaSamples = param->p.pic_height_in_luma_samples; //pic_height_in_luma_samples is pps_pic_height_in_luma_samples, PicHeightInLumaSamples is from sps
	p_declib->a_cur_pps->m_mixedNaluTypesInPicFlag = (param->p.pps_decoding_flags_0 >> 15) & 0x1;

	p_declib->m_picHeader.m_mvdL1ZeroFlag = (param->p.slice_ph_decoding_flags_2 >> 14) & 0x1;
	p_declib->m_picHeader.m_picOutputFlag = (param->p.slice_ph_decoding_flags_1 >> 11) & 0x1;
	p_declib->m_picHeader.m_nonReferencePictureFlag = (param->p.slice_ph_decoding_flags_0 >> 14) & 0x1;
	//update_rpl(&p_declib->m_apcSlicePilot->m_RPL0, &p_app->RPL0_set[param->p.RPLidx & 0xff]);
	//update_rpl(&p_declib->m_apcSlicePilot->m_RPL1, &p_app->RPL1_set[(param->p.RPLidx >> 8) & 0xff]);

	skipFrameCounter = p_app->m_iSkipFrame;

	if (p_declib->m_bFirstSliceInPicture &&
	(p_declib->a_nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL ||
	p_declib->a_nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP)) {
		//if (!getMixedNaluTypesInPicFlag(p_declib))
		if (p_declib->a_cur_pps->m_mixedNaluTypesInPicFlag == 0) {
			p_app->m_newCLVS[p_declib->a_nalu.m_nuhLayerId] = true;   // An IDR picture starts a new CLVS
			xFlushOutput(p_app, &p_declib->m_cListPic, p_declib->a_nalu.m_nuhLayerId);
			if (get_dbg_flag(p_declib->hw) & H266_DEBUG_DETAIL)
				print_vvc_picture_list(p_declib, &p_declib->m_cListPic,
					"After xFlushOutput() <getMixedNaluTypesInPicFlag is 0>");
		} else {
			p_app->m_newCLVS[p_declib->a_nalu.m_nuhLayerId] = false;
		}
	}
#if 1
	//to do ..
	else if (p_declib->m_bFirstSliceInPicture &&
		p_declib->a_nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA && isEosPresentInLastPu) {
		// A CRA that is immediately preceded by an EOS is a CLVSS
		p_app->m_newCLVS[p_declib->a_nalu.m_nuhLayerId] = true;
		xFlushOutput(p_app, &p_declib->m_cListPic, p_declib->a_nalu.m_nuhLayerId);
		if (get_dbg_flag(p_declib->hw) & H266_DEBUG_DETAIL)
			print_vvc_picture_list(p_declib, &p_declib->m_cListPic, "After xFlushOutput()");
	}
#endif
	else if (p_declib->m_bFirstSliceInPicture &&
		p_declib->a_nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA && !isEosPresentInLastPu) {
		// A CRA that is not immediately precede by an EOS is not a CLVSS
		p_app->m_newCLVS[p_declib->a_nalu.m_nuhLayerId] = false;
	} else if (p_declib->m_bFirstSliceInPicture && !isEosPresentInLastPu) {
		p_app->m_newCLVS[p_declib->a_nalu.m_nuhLayerId] = false;
	}

	hevc_print(p_declib->hw, H266_DEBUG_DETAIL,
		"[SKIP DEBUG 0] skipFrameCounter = %d, m_iSkipFrame = %d\n",
		skipFrameCounter, p_app->m_iSkipFrame);

	xDecodeSlice(p_app, &p_declib->a_nalu);

	hevc_print(p_declib->hw, H266_DEBUG_DETAIL,
		"[SKIP DEBUG 1] skipFrameCounter = %d, m_iSkipFrame = %d\n", skipFrameCounter, p_app->m_iSkipFrame);

	if ( skipFrameCounter == 1 && ( p_declib->a_nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_GDR ||
		p_declib->a_nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA )) {
		skipFrameCounter--;
	}

	hevc_print(p_declib->hw, H266_DEBUG_DETAIL,
		"[SKIP DEBUG 2] skipFrameCounter = %d, m_iSkipFrame = %d\n",
		skipFrameCounter, p_app->m_iSkipFrame);

	if ( p_app->m_iSkipFrame < skipFrameCounter  &&
	((p_declib->a_nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_TRAIL)
	|| (p_declib->a_nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_STSA)
	|| (p_declib->a_nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_RASL)
	|| (p_declib->a_nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_RADL)
	|| (p_declib->a_nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_W_RADL)
	|| (p_declib->a_nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR_N_LP)
	|| (p_declib->a_nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA)
	|| (p_declib->a_nalu.m_nalUnitType == NAL_UNIT_CODED_SLICE_GDR))) {
		if (isSliceNaluFirstInAU(p_declib, true, &p_declib->a_nalu)) {
			//m_cDecLib.checkSeiInPictureUnit();
			resetPictureSeiNalus(p_declib);
			//m_cDecLib.checkAPSInPictureUnit();
			resetPictureUnitNals(p_declib);
			resetAccessUnitSeiTids(p_declib);
			//m_cDecLib.checkSEIInAccessUnit();
			resetAccessUnitSeiPayLoadTypes(p_declib);
			resetAccessUnitNals(p_declib);
			resetAccessUnitApsNals(p_declib);
			resetAccessUnitPicInfo(p_declib);
		}
		//bPicSkipped = true;
		ret = 1;
		p_app->m_iSkipFrame++;   // skipFrame count restore, the real decrement occur at the begin of next frame
	} else {
		p_declib->decode_count++;
	}
	hevc_print(p_declib->hw, H266_DEBUG_DETAIL,
		"[SKIP DEBUG 3] skipFrameCounter = %d, m_iSkipFrame = %d\n",
		skipFrameCounter, p_app->m_iSkipFrame);
	return ret;
}

int h266_bufmgr_post_process(DecApp *p_app)
{
	DecLib *p_declib = &p_app->m_cDecLib;
	PicList* pcListPic = NULL;
	int                 poc;
	bool outputPicturePresentInBitstream = false;

	finishPicture(p_declib, &poc, &pcListPic); //, INFO, p_app->m_newCLVS[nalu.m_nuhLayerId]);
	if (get_dbg_flag(p_declib->hw) & H266_DEBUG_DETAIL)
		print_vvc_picture_list(p_declib, &p_declib->m_cListPic, "After finishPicture()");

	setOutputPicturePresentInStream();
	xWriteOutput(p_app, pcListPic, p_declib->a_nalu.m_temporalId );

	if (get_dbg_flag(p_declib->hw) & H266_DEBUG_DETAIL)
		print_vvc_picture_list(p_declib, &p_declib->m_cListPic, "After xWriteOutput()");

	return 0;
}

void print_vvc_picture_list(DecLib *p_declib, PicList *rcListPic, unsigned char * mark)
{
	int i;
	hevc_print(p_declib->hw, 0, "%s ---- picture list:\n", mark);
	for (i = 0; i < PIC_LIST_SIZE; i++) {
		Picture* rpcPic = rcListPic->pic[i];
		if (rpcPic == NULL)
			break;
		hevc_print(p_declib->hw, 0,
			"%d: canvas_index %d, poc %d, layerId %d, referenced %d longTerm %d reconstructed %d neededForOutput %d winoffset (%d,%d,%d,%d)\n",
			i, rpcPic->buf_cfg == NULL?-1:rpcPic->buf_cfg->index, rpcPic->poc, rpcPic->layerId, rpcPic->referenced, rpcPic->longTerm,
			rpcPic->reconstructed, rpcPic->neededForOutput,
			rpcPic->m_scalingWindow.m_winLeftOffset, rpcPic->m_scalingWindow.m_winRightOffset,
			rpcPic->m_scalingWindow.m_winTopOffset, rpcPic->m_scalingWindow.m_winBottomOffset);
	}
}

#endif
