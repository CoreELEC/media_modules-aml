/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/syscalls.h>
#include <linux/times.h>
#include <linux/time.h>
#include <linux/time64.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include "pts_server_core.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
#include <linux/moduleparam.h>
#include <linux/amlogic/gki_module.h>
#endif


#define MAX_INSTANCE_NUM (40)
#define MAX_EXPIRED_COUNT (300)
#define MAX_CHECKOUT_RETRY_COUNT (10)
#define MAX_CHECKOUT_MIN_PTS_THRESHOLD (5)
#define MAX_CHECKOUT_BOUND_VALUE (5)
#define MAX_DYNAMIC_INSTANCE_NUM (10)

#define DEFAULT_FREE_LIST_COUNT (500)
#define DEFAULT_LOOKUP_THRESHOLD (2500)
#define DEFAULT_DOULBCHECK_THRESHOLD (5)
#define DEFAULT_REWIND_PTS_THRESHOLD (5000000)
#define DEFAULT_REWIND_INDEX_THRESHOLD (120)
/* Upper bound for user-provided free list count to avoid excessive vmalloc */
#define MAX_FREE_LIST_COUNT (10000)

/* Debug log levels */
#define PTS_SERVER_DEBUG_LEVEL_0   0   /* Critical logs only */
#define PTS_SERVER_DEBUG_LEVEL_1   1   /* Important events */
#define PTS_SERVER_DEBUG_LEVEL_2   2   /* Normal debug information */
#define PTS_SERVER_DEBUG_LEVEL_3   3   /* Detailed debug information */
#define PTS_SERVER_DEBUG_LEVEL_4   4   /* Verbose debugging */
#define PTS_SERVER_DEBUG_LEVEL_5   5   /* Maximum details / trace */


static u32 ptsserver_debuglevel = PTS_SERVER_DEBUG_LEVEL_0;  /* Default log level */

#define PTS_LOG(level,type,number,fmt, args...)            \
    do {                                                   \
        if ((level) <= ptsserver_debuglevel)               \
            printk(KERN_DEBUG "[PtsServ][%c:%d] " fmt,     \
                   type,number,##args);                    \
    } while (0)



#define pts_pr_vinfo(number,fmt,args...) pr_info("ptsserv_v:[%d] " fmt, number,##args)
#define pts_pr_ainfo(number,fmt,args...) pr_info("ptsserv_a:[%d] " fmt, number,##args)



PtsServerManage vPtsServerInsList[MAX_INSTANCE_NUM];

static u64 get_llabs(s64 value){
	u64 llvalue;
	if (value > 0) {
		return value;
	} else {
		llvalue = (u64)(0-value);
		return llvalue;
	}
}

static long get_index_from_ptsserver_id(s32 pServerInsId)
{
	long index = -1;
	long temp = -1;

	for (temp = 0; temp < MAX_INSTANCE_NUM; temp++) {
		mutex_lock(&(vPtsServerInsList[temp].mListLock));
		if (vPtsServerInsList[temp].pInstance != NULL) {
			if (pServerInsId == vPtsServerInsList[temp].pInstance->mPtsServerInsId) {
				index = temp;
				mutex_unlock(&(vPtsServerInsList[temp].mListLock));
				break;
			}
		}
		mutex_unlock(&(vPtsServerInsList[temp].mListLock));
	}

	return index;
}

long ptsserver_ins_init_syncinfo(ptsserver_ins* pInstance,ptsserver_alloc_para* allocParm) {

	s32 index = 0;
	s32 pts_server_id = pInstance->mPtsServerInsId;
	pts_node* ptn = NULL;
	u32 alloc_count;

	if (pInstance == NULL) {
		return -1;
	}

	if (allocParm != NULL) {
		pInstance->mMaxCount = allocParm->mMaxCount; //500
		pInstance->mLookupThreshold = allocParm->mLookupThreshold; //2500
		pInstance->kDoubleCheckThreshold = allocParm->kDoubleCheckThreshold; //5
	} else {
		pInstance->mMaxCount = DEFAULT_FREE_LIST_COUNT;
		pInstance->mLookupThreshold = DEFAULT_LOOKUP_THRESHOLD;
		pInstance->kDoubleCheckThreshold = DEFAULT_DOULBCHECK_THRESHOLD;
	}

	/* initialize alloc_count after mMaxCount has been set and validate it */
	alloc_count = pInstance->mMaxCount;
	if (alloc_count == 0 || alloc_count > MAX_FREE_LIST_COUNT)
		alloc_count = DEFAULT_FREE_LIST_COUNT;

	pInstance->mPtsCheckinStarted = 0;
	pInstance->mPtsCheckoutStarted = 0;
	pInstance->mPtsCheckoutFailCount = 0;
	pInstance->mFrameDuration = 0;
	pInstance->mFrameDuration64 = 0;
	pInstance->mFirstCheckinPts = 0;
	pInstance->mFirstCheckinOffset = 0;
	pInstance->mFirstCheckinSize = 0;
	pInstance->mLastCheckinPts = 0;
	pInstance->mLastCheckinOffset = 0;
	pInstance->mLastCheckinSize = 0;
	pInstance->mAlignmentOffset = 0;
	pInstance->mLastCheckoutPts = 0;
	pInstance->mLastPeekPts = 0;
	pInstance->mLastCheckoutOffset = 0;
	pInstance->mFirstCheckinPts64 = 0;
	pInstance->mLastCheckinPts64 = 0;
	pInstance->mLastCheckoutPts64 = 0;
	pInstance->mLastPeekPts64 = 0;
	pInstance->mDoubleCheckFrameDuration = 0;
	pInstance->mDoubleCheckFrameDuration64 = 0;
	pInstance->mDoubleCheckFrameDurationCount = 0;
	pInstance->mLastDoubleCheckoutPts = 0;
	pInstance->mLastDoubleCheckoutPts64 = 0;
	pInstance->mDecoderDuration = 0;
	pInstance->mListSize = 0;
	pInstance->mLastCheckoutCurOffset = 0;
	pInstance->mLastCheckinPiecePts = 0;
	pInstance->mLastCheckinPiecePts64 = 0;
	pInstance->mLastCheckinPieceOffset = 0;
	pInstance->mLastCheckinPieceSize = 0;
	pInstance->mLastCheckoutIndex = 0;
	pInstance->mLastCheckinDurationCount = 0;
	pInstance->mLastCheckoutDurationCount = 0;
	pInstance->mLastShotBound = 0;
	pInstance->mTrickMode = 0;
	pInstance->setC2Mode = false;
	pInstance->mOffsetMode = 0;
	pInstance->mStickyWrapFlag = false;
	pInstance->mLastDropIndex = 0;
	pInstance->mLastIndex = 0;
	pInstance->mLastCheckoutPts90k = 0;
	pInstance->mFirstCheckinPts90k = 0;
	pInstance->mLastCheckinPts90k = 0;
	pInstance->mAudioOffsetMargin = 0;

	mutex_init(&pInstance->mPtsListLock);

	INIT_LIST_HEAD(&pInstance->pts_list);
	INIT_LIST_HEAD(&pInstance->pts_free_list);

	if (alloc_count > (SIZE_MAX / sizeof(struct ptsnode))) {
		alloc_count = DEFAULT_FREE_LIST_COUNT;
		pInstance->mMaxCount = alloc_count;
	}

	pInstance->all_free_ptn = vmalloc(alloc_count * sizeof(struct ptsnode));
	if (pInstance->all_free_ptn == NULL) {
		return -1;
	}

	for (index = 0; index < alloc_count; index++) {
		ptn = &pInstance->all_free_ptn[index];
		PTS_LOG(PTS_SERVER_DEBUG_LEVEL_5,'C',pts_server_id,
			"serverId:%d ptn[%d]\n",pts_server_id,index);
		list_add_tail(&ptn->node, &pInstance ->pts_free_list);
	}

	return 0;
}


long ptsserver_init(void) {
	int i = 0;
	for (i = 0;i < MAX_INSTANCE_NUM;i++) {
		vPtsServerInsList[i].pInstance = NULL;
		mutex_init(&(vPtsServerInsList[i].mListLock));
	}
	return 0;
}


long ptsserver_ins_alloc(s32 *pServerInsId,
							ptsserver_ins **pIns,
							ptsserver_alloc_para* allocParm) {

	s32 index = 0;
	ptsserver_ins* pInstance = NULL;
	pInstance = kzalloc(sizeof(ptsserver_ins), GFP_KERNEL);
	if (pInstance == NULL) {
		return -1;
	}
	for (index = 0; index < MAX_DYNAMIC_INSTANCE_NUM - 1; index++) {
		mutex_lock(&vPtsServerInsList[index].mListLock);
		if (vPtsServerInsList[index].pInstance == NULL) {
			// vPtsServerInsList[index].pInstance = pInstance;
			pInstance->mPtsServerInsId = index;
			pInstance->mRef++;
			*pServerInsId = index;
			if (ptsserver_ins_init_syncinfo(pInstance,allocParm) != 0) {
				pr_info("ptsserv: init syncinfo fail\n");
				vPtsServerInsList[index].pInstance = NULL;
				kfree(pInstance);
				mutex_unlock(&vPtsServerInsList[index].mListLock);
				return -1;
			}
			vPtsServerInsList[index].pInstance = pInstance;
			mutex_unlock(&vPtsServerInsList[index].mListLock);
			break;
		}
		mutex_unlock(&vPtsServerInsList[index].mListLock);
	}
	if (index == MAX_DYNAMIC_INSTANCE_NUM) {
		// memset(pInstance, 0, sizeof(ptsserver_ins));
		kfree(pInstance);;
		return -1;
	}
	*pIns = pInstance;
	pr_info("ptsserv: ins alloc ok\n");
	return 0;
}
EXPORT_SYMBOL(ptsserver_ins_alloc);

void ptsserver_set_mode(s32 pServerInsId, bool set_mode) {
	ptsserver_ins* pInstance = NULL;
	PtsServerManage* vPtsServerIns = NULL;
	s32 index = pServerInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return;
	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);

	pInstance = vPtsServerIns->pInstance;
	if (pInstance == NULL) {
		return;
	}
	pInstance->setC2Mode = set_mode;
	// If set trickmode we set mOffsetMode default
	if (set_mode) {
		pInstance->mOffsetMode = 1;
	}

	mutex_unlock(&vPtsServerIns->mListLock);
	return;
}
EXPORT_SYMBOL(ptsserver_set_mode);

long ptsserver_set_first_checkin_offset(s32 pServerInsId,start_offset* mStartOffset) {
	ptsserver_ins* pInstance = NULL;
	PtsServerManage* vPtsServerIns = NULL;
	s32 index = pServerInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;
	vPtsServerIns = &(vPtsServerInsList[index]);

	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}
	pInstance->mLastCheckinOffset = mStartOffset->mBaseOffset;
	pInstance->mLastCheckinPieceOffset = mStartOffset->mBaseOffset;
	pInstance->mAlignmentOffset = mStartOffset->mAlignmentOffset;
	mutex_unlock(&vPtsServerIns->mListLock);
	pts_pr_vinfo(index,"mBaseOffset:%d mAlignOffset:%d\n",
						mStartOffset->mBaseOffset,
						mStartOffset->mAlignmentOffset);

	return 0;
}
EXPORT_SYMBOL(ptsserver_set_first_checkin_offset);

/*
 return number of recs if the offset is bigger
 Caching more than two frames is considered sufficient, and the decoder needs to run to reduce the number of runs.
*/
int ptsserver_check_rec_num_enough(s32 pServerInsId,u32 val)
{
	ptsserver_ins* pInstance = NULL;
	PtsServerManage* vPtsServerIns = NULL;
	s32 index = pServerInsId;
	int r = 1;
	if (index < 0 || index >= MAX_INSTANCE_NUM) {
		return 0;
	}
	vPtsServerIns = &(vPtsServerInsList[index]);

	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;

	/********rp < wp1 < wp2,only this case maybe rec enough**********/
	if ((val >= pInstance->last_offset[0]) &&
		(val - pInstance->last_offset[0] < 0x80000000)) {
		r = 0;
	} else if ((val >= pInstance->last_offset[1]) &&
		(val - pInstance->last_offset[1] < 0x80000000)) {
		r = 0;
	}

	if (ptsserver_debuglevel >= 4) {
		pts_pr_vinfo(index,"val:0x%x l_offset[0]:0x%x l_offset[1]:0x%x isEnough:%d \n",
			val,pInstance->last_offset[0],pInstance->last_offset[1],
			r);
	}
	mutex_unlock(&vPtsServerIns->mListLock);
	return r;
}
EXPORT_SYMBOL(ptsserver_check_rec_num_enough);

long ptsserver_checkin_pts_size(s32 pServerInsId,checkin_pts_size* mCheckinPtsSize,bool isOffset) {
	PtsServerManage* vPtsServerIns = NULL;
	ptsserver_ins* pInstance = NULL;
	pts_node* ptn = NULL;
	pts_node* ptn_cur = NULL;
	pts_node* ptn_tmp = NULL;
	pts_node* del_ptn = NULL;
	s32 index = pServerInsId;
	s32 level = 0;
	bool checkinHeadNode = false;
	bool checkinSpliceNode = false;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}
	if (pInstance->mListSize >= pInstance->mMaxCount) {
		list_for_each_entry_safe(ptn_cur, ptn_tmp, &pInstance->pts_list, node) {
			if (ptn_cur->index <= pInstance->mLastDropIndex) {
				del_ptn = ptn_cur;
				list_del(&del_ptn->node);
				list_add_tail(&del_ptn->node, &pInstance ->pts_free_list);
				pInstance->mLastDropIndex++;
				pInstance->mListSize--;
				if (ptsserver_debuglevel >= 1) {
					pts_pr_vinfo(index,"Checkin del i:%d, pts:0x%llx\n",
										del_ptn->index,
										del_ptn->pts_90k);
				}
			}
		}
		if (!del_ptn) {
			del_ptn = list_first_entry(&pInstance->pts_list, struct ptsnode, node);
			list_del(&del_ptn->node);
			list_add_tail(&del_ptn->node, &pInstance ->pts_free_list);	//queue empty buffer
			pInstance->mListSize--;
			if (ptsserver_debuglevel >= 1) {
				pts_pr_vinfo(index,"Checkin del i:%d, pts:0x%llx\n",
									del_ptn->index,
									del_ptn->pts_90k);
			}
		}
	}
	//record last two checkin offset
	pInstance->last_offset[pInstance->write_count % 2] = mCheckinPtsSize->size;
	pInstance->write_count++;
	//record every checkin offset in mLastCheckinPieceOffset
	if (isOffset) {
		pInstance->mLastCheckinPieceOffset = mCheckinPtsSize->size;
	} else {
		pInstance->mLastCheckinPieceOffset = pInstance->mLastCheckinPieceOffset + pInstance->mLastCheckinPieceSize;
	}

	if (mCheckinPtsSize->pts == 0) {//checkin piece node in splice mode
		if (pInstance->mListSize != 0) {
			ptn = list_last_entry(&pInstance->pts_list, struct ptsnode, node);

			if (ptn == NULL) {
				mutex_unlock(&vPtsServerIns->mListLock);
				return -1;
			}
		} else {
			//for piece checkin record
			//list is empty and head node not exist, just record info
			pInstance->mLastCheckinPieceSize = mCheckinPtsSize->size;
			pInstance->mLastCheckinPiecePts = mCheckinPtsSize->pts;
			pInstance->mLastCheckinPiecePts64 = mCheckinPtsSize->pts_64;
			mutex_unlock(&vPtsServerIns->mListLock);
			return 0;
		}
		checkinSpliceNode = true;
	}  else {//checkin head node
		checkinHeadNode = true;
		if (!list_empty(&pInstance->pts_free_list)) {	//dequeue empty buffer
			ptn = list_first_entry(&pInstance->pts_free_list, struct ptsnode, node);
			list_del(&ptn->node);
		}

		if (ptn != NULL) {
			if (isOffset) {
				ptn->offset = mCheckinPtsSize->size;
			} else {
				pInstance->mLastCheckinSize = pInstance->mLastCheckinPieceOffset - pInstance->mLastCheckinOffset;
				ptn->offset = pInstance->mLastCheckinOffset + pInstance->mLastCheckinSize;
			}
			ptn->pts_90k = mCheckinPtsSize->pts;
			ptn->pts_64 = mCheckinPtsSize->pts_64;
			ptn->expired_count = MAX_EXPIRED_COUNT;
			ptn->index = pInstance->mLastIndex++;
		} else {
			mutex_unlock(&vPtsServerIns->mListLock);
			return -1;
		}

		if (pInstance->setC2Mode) {
			list_for_each_entry_safe(ptn_cur,ptn_tmp,&pInstance->pts_list, node) {
				if (ptn_cur != NULL) {
					if (ptn_cur->pts_64 >= ptn->pts_64)
						break;
				}
			}
			list_add_tail(&ptn->node, &ptn_cur->node);
		} else {
			// sort pts_list by pts if pts invalid sort by offset,
			// so pts_list is amlost list by order
			list_for_each_entry_safe(ptn_cur, ptn_tmp, &pInstance->pts_list, node) {
				if (ptn_cur != NULL) {
					if (ptn->pts_64 != -1) {
						if (ptn->pts_64 <= ptn_cur->pts_64 &&
							get_llabs((s64)(ptn->pts_64 - ptn_cur->pts_64)) < DEFAULT_REWIND_PTS_THRESHOLD &&
							abs(ptn->index - ptn_cur->index) < DEFAULT_REWIND_INDEX_THRESHOLD)
							break;
					} else {
						if (ptn->offset <= ptn_cur->offset)
							break;
					}
				}
			}
			list_add_tail(&ptn->node, &ptn_cur->node);
		}

		if (ptn->pts_64 < pInstance->mLastCheckinPts64 &&
			get_llabs(ptn->pts_64 - pInstance->mLastCheckinPts64) > DEFAULT_REWIND_PTS_THRESHOLD) {
			ptn->duration_count = pInstance->mLastCheckinDurationCount++;
		} else {
			ptn->duration_count = pInstance->mLastCheckinDurationCount;
		}

		pInstance->mLastCheckinPts = ptn->pts_90k;
		pInstance->mLastCheckinPts64 = ptn->pts_64;

		pInstance->mListSize++;
	}

	if (!pInstance->mPtsCheckinStarted ||
		(ptsserver_debuglevel >= 1 && checkinHeadNode == true)) {
		level = ptn->offset - pInstance->mLastCheckoutCurOffset;
		pts_pr_vinfo(index,"[Head] Checkin%s LstCnt:%d Size(%d) %s:0x%x (%s:%d) pts(32:0x%llx 64:%lld) durCnt:%d\n",
							pInstance->mPtsCheckinStarted?"":"(First)",
							pInstance->mListSize,
							mCheckinPtsSize->size,
							ptn->offset < pInstance->mLastCheckinOffset?"rest offset":"offset",
							ptn->offset,
							level >= 5242880 ? ">5M level": "level",
							level,
							ptn->pts_90k,
							ptn->pts_64,
							ptn->duration_count);
	}

	if (!pInstance->mPtsCheckinStarted) {
		if (isOffset) {
			pInstance->mFirstCheckinOffset = mCheckinPtsSize->size;
		} else {
			pInstance->mFirstCheckinOffset = 0;
		}
		pInstance->mFirstCheckinSize = mCheckinPtsSize->size;
		pInstance->mFirstCheckinPts = mCheckinPtsSize->pts;
		pInstance->mFirstCheckinPts64 = mCheckinPtsSize->pts_64;
		pInstance->mPtsCheckinStarted = 1;
	}

	if (pInstance->mAlignmentOffset != 0) {
		pInstance->mLastCheckinOffset += pInstance->mAlignmentOffset;
		pInstance->mLastCheckinPieceOffset += pInstance->mAlignmentOffset;
		pInstance->mAlignmentOffset = 0;
	} else {
		pInstance->mLastCheckinOffset = ptn->offset;
	}

	//for piece checkin record
	pInstance->mLastCheckinPieceSize = mCheckinPtsSize->size;
	pInstance->mLastCheckinPiecePts = mCheckinPtsSize->pts;
	pInstance->mLastCheckinPiecePts64 = mCheckinPtsSize->pts_64;

	if (ptsserver_debuglevel >= 1 && checkinSpliceNode == true) {
		pts_pr_vinfo(index,"[Splice] Checkin Size(%d) offset:0x%x\n",
							pInstance->mLastCheckinPieceSize,
							pInstance->mLastCheckinPieceOffset);
	}

	mutex_unlock(&vPtsServerIns->mListLock);
	return 0;
}
EXPORT_SYMBOL(ptsserver_checkin_pts_size);

long ptsserver_checkout_pts_offset(s32 pServerInsId, checkout_pts_offset* mCheckoutPtsOffset) {
	PtsServerManage* vPtsServerIns = NULL;
	ptsserver_ins* pInstance = NULL;
	s32 index = pServerInsId;
	pts_node* ptn = NULL;
	pts_node* find_ptn = NULL;
	pts_node* fit_offset_ptn = NULL;
	pts_node* fit_pts_ptn = NULL;
	pts_node* ptn_tmp = NULL;

	u32 cur_offset = 0xFFFFFFFF & mCheckoutPtsOffset->offset;
	s32 cur_duration = (mCheckoutPtsOffset->offset >> 32) & 0x3FFFFFFF;

	u32 expected_offset_diff = 2500;
	u64 expected_pts = 0;
	s32 find_frame_num = -1;
	s32 find = 0;
	s32 invalid_mode = 0;
	u32 offsetAbs = 0;
	u32 FrameDur = 0;
	u64 FrameDur64 = 0;
	s32 find_index = -1;
	s32 fit_offset_number = -1;
	s32 fit_pts_number = -1;
	s32 must_have_fit = 0;
	s32 retryCount = MAX_CHECKOUT_RETRY_COUNT;
	s32 lastCheckoutPts = 0;
	s64 lastCheckoutPtsU64 = 0;
	s32 shot_bound = MAX_CHECKOUT_BOUND_VALUE;// show the credibility of the current pts
	s32 i = 0;

	if (index < 0 || index >= MAX_INSTANCE_NUM) {
		return -1;
	}
	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;

	if (pInstance == NULL) {
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}

	expected_offset_diff = pInstance->mLookupThreshold;
	expected_pts = pInstance->mLastCheckoutPts64;

	if (!pInstance->mPtsCheckoutStarted || ptsserver_debuglevel >= 1) {
		pts_pr_vinfo(index,"Checkout%s LstCnt:%d offset:0x%x dur:%d\n",
							pInstance->mPtsCheckoutStarted?"":"(First)",
							pInstance->mListSize,
							cur_offset,
							cur_duration);
	}

	// Normal Case: Pull pts from list if decoder send offset with value.
	// 1.stream mode with normal offset
	// 2.v4l2 pileline es mode, pts must checkout from checkin node
	if (cur_offset != 0xFFFFFFFF &&
		!list_empty(&pInstance->pts_list)) {
		find = 0;
		list_for_each_entry_safe(ptn,ptn_tmp,&pInstance->pts_list, node) {

			if (ptn != NULL) {
				offsetAbs = abs(cur_offset - ptn->offset);

				// Only if ptn node is deadly expired, we increase the drop count.
				if (ptn->offset == 0xFFFFFFFF || ptn->offset < cur_offset) {
					if ((ptn->pts_64 < pInstance->mLastCheckoutPts64) || (ptn->pts_90k == -1)) {
						ptn->expired_count --;
					}
				}

				// Print all ptn base info
				if (ptsserver_debuglevel > 3) {
					pts_pr_vinfo(index,"Checkout i:%d offset(diff:%d L:0x%x) pts:0x%llx expected_pts:0x%llu\n",
									i, offsetAbs, ptn->offset, ptn->pts_90k, expected_pts);
				}

				//record the first fit pts minimum ptn node to record,this node is the key one normally even without
				// check offset,this can double check if the key one right or not in some specially case.
				if (fit_pts_ptn == NULL && pInstance->mLastCheckoutPts64 > 0) {
					if (((ptn->pts_64 - pInstance->mLastCheckoutPts64) >= 10000) && ((ptn->pts_64 - pInstance->mLastCheckoutPts64) < 500000) &&
							offsetAbs < 50000) {
						fit_pts_ptn = ptn;
						fit_pts_number = i;
						if (ptsserver_debuglevel >= 1) {
							pts_pr_vinfo(index, "fit pts. i:%d\n",
									fit_pts_number);
						}
					}
				}

				// If shot the threshold, there must be one ptn node be selected.
				// So trust mLookupThreshold,if not shot in mLookupThreshold, ignore them all.
				// Otherwise mLookupThreshold is not right,need adjust it.
				if (offsetAbs <=  pInstance->mLookupThreshold) {

					// Record the minimum offset as anchors, if any case need to reset pts, use this node fit_offset_ptn.
					// If in mLookupThreshold,any ptn fit in offset, we mark must_have_fit,
					// which means we can pull one ptn in list for sure, no matter where it is.
					if ((offsetAbs <= expected_offset_diff && cur_offset > ptn->offset)) {
						fit_offset_ptn = ptn;
						fit_offset_number = i;
						expected_offset_diff = offsetAbs;
						// cause we consider fit pts as find one, so it is we can find key one even in mLookupThreshold,
						// But if in mLookupThreshold,we can pull one for sure so we mark must_have_fit to find ptn not in mLookupThreshold.
						must_have_fit = 1;
						if (ptsserver_debuglevel >= 1) {
							pts_pr_vinfo(index, "fit offset. i:%d\n", fit_offset_number);
						}
						// Deal with invalid pts == -1 case,
						// If first node is invalid case, set it as found node
						// If there is other node found, we can only compare offset, and set invalid flag
						if (ptn->pts_90k == -1) {
							pInstance->mOffsetMode = 1;
							find = 1;// Here to set find to avoid retryCount count wrong
							if (ptsserver_debuglevel >= 1) {
								pts_pr_vinfo(index, "invalid pts.\n");
							}
						}
					}

					// Compare pts as first Strategy, expect the first ptn in mLookupThreshold will be the key one
					// any node in mLookupThreshold has invalid pts we not compare pts, just conmare offset
					if (ptn->pts_64 >= pInstance->mLastCheckoutPts64 &&
											!pInstance->mOffsetMode &&
											cur_offset > ptn->offset) {
											if (ptn->pts_64 <= expected_pts ||
												expected_pts == pInstance->mLastCheckoutPts64) {
												find = 1;
												find_index = i;
												find_ptn = ptn;
												expected_pts = ptn->pts_64;
											}
					}
					if (cur_offset > ptn->offset || (find_frame_num >= 0)) {
						find_frame_num = 0;
					}
				}else if (find_frame_num >= 0) {
					find_frame_num ++;
				}

				// Drop Node Strategy
				// <Out of Buffer Size>
				// <expired count == 0>
				if ((/*cur_offset > ptn->offset && */offsetAbs > 251658240 || ptn->expired_count <= 0) &&
				    (find_ptn != ptn) && (ptn != fit_offset_ptn) && (fit_pts_ptn != ptn)) {
					// > 240M (15M*16)
					if (ptsserver_debuglevel >= 1) {
						pts_pr_vinfo(index,"Checkout del(%d) offset:0x%x pts:0x%llx, reason map:<%d %d>\n",
											i,
											ptn->offset,
											ptn->pts_90k,
											offsetAbs > 251658240,
											ptn->expired_count <= 0);
					}
					list_del(&ptn->node);
					list_add_tail(&ptn->node, &pInstance ->pts_free_list);
					pInstance->mListSize--;

				}

				if (find) {
					// Deal with the case pts rewind and insert the first node in pts_list,
					// and offset is also within correct threshhold.But the key on is at the end of pts_list,
					// and we can`t find within just in MAX_CHECKOUT_RETRY_COUNT,we need retry all list
					if (find_index == 0 && pInstance->mLastCheckoutIndex > 0) {
						retryCount = pInstance->mListSize;
					}
				}

				if (find_frame_num >= retryCount) {
					break;
				}
			}
			i++;
		}

		if (pInstance->mOffsetMode) {
			if (fit_offset_ptn) {
				find = 1;
				find_ptn = fit_offset_ptn;
				find_index = fit_offset_number;
				shot_bound --;
				pInstance->mStickyWrapFlag = false;
				if (find_ptn->pts_90k == -1) {
					invalid_mode = 1;
					if (ptsserver_debuglevel >= 1) {
						pts_pr_vinfo(index, "invalid pts. i:%d\n", find_index);
					}
				}
			}
		} else {
			// to see if the key one within offset the same as the minimum pts diff one,
			// if not fit, we choose to trust pts fit.
			if (must_have_fit && fit_offset_ptn) {
				find = 1;
				if (pInstance->mPtsCheckoutStarted
					&& fit_pts_ptn && find_ptn != fit_pts_ptn
					&& fit_pts_ptn->duration_count == fit_offset_ptn->duration_count
					&& !pInstance->mStickyWrapFlag) {
					if (fit_pts_ptn->pts_64 > fit_offset_ptn->pts_64 &&
						fit_offset_ptn->pts_64 - pInstance->mLastCheckoutPts64 > 10000) {
						find_ptn = fit_offset_ptn;
						find_index = fit_offset_number;
						shot_bound --;
						pInstance->mStickyWrapFlag = false;
					} else {
						find_ptn = fit_pts_ptn;
						find_index = fit_pts_number;
						shot_bound --;
						pInstance->mStickyWrapFlag = true;
					}
					if (ptsserver_debuglevel > 1) {
						pts_pr_vinfo(index, "use %s fit ptn. i:%d\n",
											pInstance->mStickyWrapFlag?"pts":"offset",
											find_index);
					}
					// if find by pts not found but can be found by offset, need reset by offset,usually seen in pts rewind case
				} else if ((fit_offset_ptn && !find_ptn) || !pInstance->mPtsCheckoutStarted) {
					find_ptn = fit_offset_ptn;
					find_index = fit_offset_number;
					shot_bound --;
					pInstance->mStickyWrapFlag = false;
					if (ptsserver_debuglevel >= 1) {
						pts_pr_vinfo(index, "Not pts fit, use offset. i:%d\n", find_index);
					}
				} else if (find_ptn) {
					pInstance->mStickyWrapFlag = false;
					if (ptsserver_debuglevel > 1) {
						pts_pr_vinfo(index, "Perfect case %d\n", find_index);
					}
				}
			}
		}

		if (!find && (pInstance->mTrickMode == 1 || !pInstance->mPtsCheckoutStarted)) {//i_only, not found pts, need retry!
			i = 0;
			find_frame_num = 0;
			expected_offset_diff = 10000;
			list_for_each_entry_safe(ptn,ptn_tmp,&pInstance->pts_list, node) {
				if (ptn != NULL) {
					offsetAbs = abs(cur_offset - ptn->offset);
				}
				if (offsetAbs <= expected_offset_diff && cur_offset > ptn->offset) {
					expected_offset_diff = offsetAbs;
					find = 1;
					find_index = i;
					find_ptn = ptn;
				} else if (find_frame_num > 5) {
					break;
				}
				if (find) {
					find_frame_num++;
				}
				i++;
			}
		}

		if (!find && pInstance->setC2Mode == 1) {
			if (cur_offset != 0xFFFFFFFF) {
				u32 min_offsetAbs = 0xFFFFFFFF;

				if (pInstance->mPtsCheckoutStarted) {
					mCheckoutPtsOffset->pts = pInstance->mLastCheckoutPts;
					mCheckoutPtsOffset->pts_64 = pInstance->mLastCheckoutPts64;
					min_offsetAbs = abs(cur_offset - pInstance->mLastCheckoutCurOffset);
				}

				list_for_each_entry_safe(ptn,ptn_tmp,&pInstance->pts_list, node) {
					if (ptn != NULL && (cur_offset > ptn->offset) && (ptn->offset >= pInstance->mLastCheckoutCurOffset)) {
						offsetAbs = abs(cur_offset - ptn->offset);

						if (offsetAbs < min_offsetAbs) {
							min_offsetAbs = offsetAbs;
							find_ptn = ptn;
							if (ptsserver_debuglevel >= 1)
								pts_pr_vinfo(index,"Checkout change to i:%d\n",i);
						}
					}
				}
				if (find_ptn) {
					mCheckoutPtsOffset->pts = find_ptn->pts_90k;
					mCheckoutPtsOffset->pts_64 = find_ptn->pts_64;
					cur_offset = find_ptn->offset;
				} else {
					if (!pInstance->mPtsCheckoutStarted && !list_empty(&pInstance->pts_list)) {
						find_ptn = list_first_entry(&pInstance->pts_list, struct ptsnode, node);
						mCheckoutPtsOffset->pts = find_ptn->pts_90k;
						mCheckoutPtsOffset->pts_64 = find_ptn->pts_64;
						cur_offset = find_ptn->offset;
					} else {
						cur_offset = pInstance->mLastCheckoutCurOffset;
					}
					if (ptsserver_debuglevel >= 1)
						pts_pr_vinfo(index,"select %s pts\n",
							cur_offset==pInstance->mLastCheckoutCurOffset?"l_Checkout":"first");
				}
				find = 1;
			}
		}

		pInstance->mLastCheckoutIndex = find_index;

		// Deal with all the key ptn
		if (find && find_ptn) {
			lastCheckoutPtsU64 = pInstance->mLastCheckoutPts64;
			if (!invalid_mode) {
				mCheckoutPtsOffset->pts = find_ptn->pts_90k;
				mCheckoutPtsOffset->pts_64 = find_ptn->pts_64;
				pInstance->mLastCheckoutDurationCount = find_ptn->duration_count;
				if (ptsserver_debuglevel >= 1 ||
					!pInstance->mPtsCheckoutStarted) {
					pts_pr_vinfo(index,
						"Checkout ok LstCnt:%d find:%d offset(diff:%d L:0x%x) pts(32:0x%llx 64:%lld) ptsdiff:%lld us score:%d\n",
									pInstance->mListSize,find_index,abs(cur_offset - find_ptn->offset),
									find_ptn->offset,find_ptn->pts_90k,find_ptn->pts_64,
									mCheckoutPtsOffset->pts_64 - lastCheckoutPtsU64,
									shot_bound);
				}
			}

			list_del(&find_ptn->node);
			list_add_tail(&find_ptn->node, &pInstance ->pts_free_list);
			pInstance->mListSize--;
		}
	}

	// Unnormal Case: Calculate pts with duration.
	// 1.cant find node in ptslist within threshold
	// 2.one package with multi-frame,this case has no offset
	// 3.pts == -1 case, pts has error value
	if (!find || invalid_mode) {
		lastCheckoutPts = pInstance->mLastCheckoutPts;
		lastCheckoutPtsU64 = pInstance->mLastCheckoutPts64;
		pInstance->mPtsCheckoutFailCount++;

		// If cur_offset smaller than last offset means sticky package from codec
		if (cur_offset < pInstance->mLastCheckoutCurOffset) {
			pInstance->mStickyWrapFlag = false;
		} else {
			if (ptsserver_debuglevel > 1) {
				pts_pr_vinfo(index, "possible discontinuity, reset with offset.\n");
			}
		}

		shot_bound = 3;
		if (invalid_mode && find_ptn) {
			pInstance->mLastCheckoutDurationCount = find_ptn->duration_count;
		}
		if (ptsserver_debuglevel >= 1 || pInstance->mPtsCheckoutFailCount % 30 == 0) {
			pts_pr_vinfo(index,"Checkout fail Cnt:%d level:%d StickFlag:%d\n",
				pInstance->mPtsCheckoutFailCount,pInstance->mLastCheckinOffset - cur_offset,
				pInstance->mStickyWrapFlag);
		}
		if (pInstance->mDecoderDuration != 0) {
			pInstance->mLastCheckoutPts = pInstance->mLastCheckoutPts + pInstance->mDecoderDuration * 90 / 96;
			pInstance->mLastCheckoutPts64 = pInstance->mLastCheckoutPts64 + pInstance->mDecoderDuration * 1000 / 96;
		} else {
			if (pInstance->mFrameDuration != 0) {
				pInstance->mLastCheckoutPts = pInstance->mLastCheckoutPts + pInstance->mFrameDuration;
			}
			if (pInstance->mFrameDuration64 != 0) {
				pInstance->mLastCheckoutPts64 = pInstance->mLastCheckoutPts64 + pInstance->mFrameDuration64;
			}
			if (pInstance->mPtsCheckoutStarted == 0) {
				pts_pr_vinfo(index,"First Checkout fail.\n");
				pInstance->mLastCheckoutPts = -1;
				pInstance->mLastCheckoutPts64 = -1;

			}
		}
		if (ptsserver_debuglevel >= 1) {
			pts_pr_vinfo(index,"Checkout fail calc by FrmDur:%lld DecDur:%d pts(32:%x 64:%lld) l_outpts:%x score:%d ptsdiff:%lld us\n",
								pInstance->mFrameDuration64,
								pInstance->mDecoderDuration,
								pInstance->mLastCheckoutPts,
								pInstance->mLastCheckoutPts64,
								lastCheckoutPts,
								shot_bound,
								pInstance->mLastCheckoutPts64 - lastCheckoutPtsU64);
		}
		mCheckoutPtsOffset->pts = pInstance->mLastCheckoutPts;
		mCheckoutPtsOffset->pts_64 = pInstance->mLastCheckoutPts64;
		pInstance->mLastCheckoutCurOffset = cur_offset;
		mutex_unlock(&vPtsServerIns->mListLock);
		return 0;
	}

	// Calculate duration if checkout first time
	if (pInstance->mPtsCheckoutStarted) {
		lastCheckoutPtsU64 = pInstance->mLastCheckoutPts64;
		if (pInstance->mFrameDuration == 0 && pInstance->mFrameDuration64 == 0) {

			pInstance->mFrameDuration = div_u64(mCheckoutPtsOffset->pts - pInstance->mLastCheckoutPts,
													pInstance->mPtsCheckoutFailCount + 1);

			pInstance->mFrameDuration64 = div_u64(mCheckoutPtsOffset->pts_64 - pInstance->mLastCheckoutPts64,
													pInstance->mPtsCheckoutFailCount + 1);

			if (pInstance->mFrameDuration < 0 ||
				pInstance->mFrameDuration > 9000) {
				pInstance->mFrameDuration = 0;
			}
			if (pInstance->mFrameDuration64 < 0 ||
				pInstance->mFrameDuration64 > 100000) {
				pInstance->mFrameDuration64 = 0;
			}
		} else {
			FrameDur = div_u64(mCheckoutPtsOffset->pts - pInstance->mLastDoubleCheckoutPts,
								pInstance->mPtsCheckoutFailCount + 1);

			FrameDur64 = div_u64(mCheckoutPtsOffset->pts_64 - pInstance->mLastDoubleCheckoutPts64,
									pInstance->mPtsCheckoutFailCount + 1);

			if (ptsserver_debuglevel > 1) {
				pts_pr_vinfo(index,"checkout l_DoubleOutPts(32:%d 64:%lld) FailCnt:%d\n",
									pInstance->mLastDoubleCheckoutPts,
									pInstance->mLastDoubleCheckoutPts64,
									pInstance->mPtsCheckoutFailCount);
			}

			if ((FrameDur == pInstance->mDoubleCheckFrameDuration) ||
				(FrameDur64 == pInstance->mDoubleCheckFrameDuration64)) {
				pInstance->mDoubleCheckFrameDurationCount ++;
			} else {
				pInstance->mDoubleCheckFrameDuration = FrameDur;
				pInstance->mDoubleCheckFrameDuration64 = FrameDur64;
				pInstance->mDoubleCheckFrameDurationCount = 0;
			}
			if (pInstance->mDoubleCheckFrameDurationCount > pInstance->kDoubleCheckThreshold) {
				pInstance->mFrameDuration = pInstance->mDoubleCheckFrameDuration;
				pInstance->mFrameDuration64 = pInstance->mDoubleCheckFrameDuration64;
				pInstance->mDoubleCheckFrameDurationCount = 0;
			}
		}
	} else {
		pInstance->mPtsCheckoutStarted = 1;
	}

	pInstance->mPtsCheckoutFailCount = 0;
	pInstance->mLastCheckoutOffset = mCheckoutPtsOffset->offset;
	pInstance->mLastCheckoutPts = mCheckoutPtsOffset->pts;
	pInstance->mLastCheckoutPts64 = mCheckoutPtsOffset->pts_64;
	pInstance->mLastPeekPts = mCheckoutPtsOffset->pts;
	pInstance->mLastPeekPts64 = mCheckoutPtsOffset->pts_64;
	pInstance->mLastDoubleCheckoutPts = mCheckoutPtsOffset->pts;
	pInstance->mLastDoubleCheckoutPts64 = mCheckoutPtsOffset->pts_64;
	pInstance->mDecoderDuration = cur_duration;
	pInstance->mLastCheckoutCurOffset = cur_offset;
	pInstance->mLastShotBound = shot_bound;
	mutex_unlock(&vPtsServerIns->mListLock);
	return 0;
}
EXPORT_SYMBOL(ptsserver_checkout_pts_offset);

long ptsserver_peek_pts_offset(s32 pServerInsId,checkout_pts_offset* mCheckoutPtsOffset) {
	PtsServerManage* vPtsServerIns = NULL;
	ptsserver_ins* pInstance = NULL;
	s32 index = pServerInsId;
	pts_node* ptn = NULL;
	pts_node* find_ptn = NULL;
	pts_node* fit_offset_ptn = NULL;
	pts_node* ptn_tmp = NULL;

	u32 cur_offset = 0xFFFFFFFF & mCheckoutPtsOffset->offset;

	u32 expected_offset_diff = 2500;
	u64 expected_pts = 0;
	s32 find_frame_num = 0;
	s32 find = 0;
	u32 offsetAbs = 0;
	s32 fit_offset_number = -1;
	s32 must_have_fit = 0;
	s32 retryCount = MAX_CHECKOUT_RETRY_COUNT;

	s32 find_index = -1;
	s32 invalid_mode = 0;
	s32 i = 0;
	u32 mCalculateLastCheckoutPts = 0;
	u64 mCalculateLastCheckoutPts64 = 0;
	s32 shot_bound = MAX_CHECKOUT_BOUND_VALUE;// show the credibility of the current pts
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;
	if (pInstance == NULL) {
		pts_pr_vinfo(pServerInsId,"peek Ins is NULL.\n");
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}

	expected_offset_diff = pInstance->mLookupThreshold;
	expected_pts = pInstance->mLastPeekPts64;

	// Normal Case: Pull pts from list if decoder send offset with value.
	// 1.stream mode with normal offset
	// 2.v4l2 pileline es mode, pts must checkout from checkin node
	if (cur_offset != 0xFFFFFFFF &&
		!list_empty(&pInstance->pts_list)) {
		find_frame_num = 0;
		find = 0;
		list_for_each_entry_safe(ptn,ptn_tmp,&pInstance->pts_list, node) {

			if (ptn != NULL) {
				offsetAbs = abs(cur_offset - ptn->offset);

				// Print all ptn base info
				if (ptsserver_debuglevel > 3) {
					pts_pr_vinfo(index,"Peek i:%d offset(diff:%d L:0x%x) pts:0x%llx expired(cnt:%d pts:0x%llu)\n",
									i, offsetAbs, ptn->offset, ptn->pts_90k, ptn->expired_count, expected_pts);
				}

				// If shot the threshold, there must be one ptn node be selected.
				// So trust mLookupThreshold,if not shot in mLookupThreshold, ignore them all.
				// Otherwise mLookupThreshold is not right,need adjust it.
				if (offsetAbs <=  pInstance->mLookupThreshold) {

					// Record the minimum offset as anchors, if any case need to reset pts, use this node fit_offset_ptn.
					// If in mLookupThreshold,any ptn fit in offset, we mark must_have_fit,
					// which means we can pull one ptn in list for sure, no matter where it is.
					if ((offsetAbs <= expected_offset_diff && cur_offset > ptn->offset)) {
						fit_offset_ptn = ptn;
						fit_offset_number = i;
						expected_offset_diff = offsetAbs;
						// cause we consider fit pts as find one, so it is we can find key one even in mLookupThreshold,
						// But if in mLookupThreshold,we can pull one for sure so we mark must_have_fit to find ptn not in mLookupThreshold.
						must_have_fit = 1;
						if (ptsserver_debuglevel >= 3) {
							pts_pr_vinfo(index, "offset fit ptn. i:%d\n", fit_offset_number);
						}
						// Deal with invalid pts == -1 case,
						// If first node is invalid case, set it as found node
						// If there is other node found, we can only compare offset, and set invalid flag
						if (ptn->pts_90k == -1) {
							pInstance->mOffsetMode = 1;
							find = 1;// Here to set find to avoid retryCount count wrong
							// if (ptsserver_debuglevel >= 3) {
								// pts_pr_info(index, "Found invalid pts. in invalid mode\n");
							// }
						}
					}
				}

				if (find) {
					// Deal with the case pts rewind and insert the first node in pts_list,
					// and offset is also within correct threshhold.But the key on is at the end of pts_list,
					// and we can`t find within just in MAX_CHECKOUT_RETRY_COUNT,we need retry all list
					if (find_index == 0 && pInstance->mLastCheckoutIndex > 0) {
						retryCount = pInstance->mListSize;
					}
					find_frame_num++;
				}

				if (find_frame_num >= retryCount) {
					break;
				}
			}
			i++;
		}

		if (pInstance->mOffsetMode) {
			if (fit_offset_ptn) {
				find = 1;
				find_ptn = fit_offset_ptn;
				find_index = fit_offset_number;
				shot_bound --;
				if (find_ptn->pts_90k == -1) {
					invalid_mode = 1;
					if (ptsserver_debuglevel >= 3) {
						pts_pr_vinfo(index, "Peek invalid pts. i:%d\n", find_index);
					}
				}
			}
		} else {
			// to see if the key one within offset the same as the minimum pts diff one,
			// if not fit, we choose to trust pts fit.
			if (must_have_fit) {
				find = 1;
				if ((fit_offset_ptn && !find_ptn) || !pInstance->mPtsCheckoutStarted) {
					find_ptn = fit_offset_ptn;
					find_index = fit_offset_number;
					shot_bound --;
					if (ptsserver_debuglevel >= 3) {
						pts_pr_vinfo(index, "Peek Might occure discontinue case, need reset by offset. i:%d.\n", find_index);
					}
				}
			}
		}

		// Deal with all the key ptn
		if (find && find_ptn && !invalid_mode) {
			mCheckoutPtsOffset->pts = find_ptn->pts_90k;
			mCheckoutPtsOffset->pts_64 = find_ptn->pts_64;
			if (ptsserver_debuglevel >= 3) {
				pts_pr_vinfo(index,"Peek ok LstCnt:%d find:%d offset(diff:%d L:0x%x C:%x) pts(32:0x%llx 64:%lld) l_outpts:%x, dur_cnt:%d, score:%d\n",
									pInstance->mListSize,find_index,abs(cur_offset - find_ptn->offset),
									find_ptn->offset,cur_offset,find_ptn->pts_90k,find_ptn->pts_64, pInstance->mLastPeekPts, pInstance->mLastCheckoutDurationCount, shot_bound);
			}
		}
	}

	if (!find || invalid_mode) {
		if (pInstance->mDecoderDuration != 0) {
			mCalculateLastCheckoutPts = pInstance->mLastPeekPts + pInstance->mDecoderDuration;
			mCalculateLastCheckoutPts64 = pInstance->mLastPeekPts64 + div_u64(pInstance->mDecoderDuration * 1000,96);
		} else {
			if (pInstance->mFrameDuration != 0) {
				mCalculateLastCheckoutPts = pInstance->mLastPeekPts + pInstance->mFrameDuration;
			}
			if (pInstance->mFrameDuration64 != 0) {
				mCalculateLastCheckoutPts64 = pInstance->mLastPeekPts64 + pInstance->mFrameDuration64;
			}
		}

		mCheckoutPtsOffset->pts = mCalculateLastCheckoutPts;
		mCheckoutPtsOffset->pts_64 = mCalculateLastCheckoutPts64;
	}

	pInstance->mLastPeekPts = mCheckoutPtsOffset->pts;
	pInstance->mLastPeekPts64 = mCheckoutPtsOffset->pts_64;

	mutex_unlock(&vPtsServerIns->mListLock);

	return 0;
}
EXPORT_SYMBOL_GPL(ptsserver_peek_pts_offset);

long ptsserver_get_last_checkin_pts(s32 pServerInsId,last_checkin_pts* mLastCheckinPts) {
	PtsServerManage* vPtsServerIns = NULL;
	ptsserver_ins* pInstance = NULL;
	s32 index = pServerInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}
	mLastCheckinPts->mLastCheckinPts = pInstance->mLastCheckinPts;
	mLastCheckinPts->mLastCheckinPts64 = pInstance->mLastCheckinPts64;
	mutex_unlock(&vPtsServerIns->mListLock);

	return 0;
}

long ptsserver_get_last_checkout_pts(s32 pServerInsId,last_checkout_pts* mLastCheckOutPts) {

	PtsServerManage* vPtsServerIns = NULL;
	ptsserver_ins* pInstance = NULL;
	s32 index = pServerInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}
	mLastCheckOutPts->mLastCheckoutPts = pInstance->mLastCheckoutPts;
	mLastCheckOutPts->mLastCheckoutPts64 = pInstance->mLastCheckoutPts64;
	mutex_unlock(&vPtsServerIns->mListLock);
	return 0;
}

long ptsserver_ins_release(s32 pServerInsId) {
	PtsServerManage* vPtsServerIns = NULL;
	ptsserver_ins* pInstance = NULL;
	s32 index = get_index_from_ptsserver_id(pServerInsId);
	pts_node *ptn = NULL;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;
	if (pInstance == NULL) {
		pr_info("ptsserv: Ins is null");
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}

	pInstance->mRef--;
	if (pInstance->mRef > 0) {
		mutex_unlock(&vPtsServerIns->mListLock);
		ptsserver_ins_reset(pServerInsId);
		return 0;
	}
	pr_info("ptsserv: release index:%d mRef:%d",index,pInstance->mRef);
	while (!list_empty(&pInstance->pts_free_list)) {
		ptn = list_entry(pInstance->pts_free_list.next,
						struct ptsnode, node);
		if (ptn != NULL) {
			list_del(&ptn->node);
			ptn = NULL;
		}
	}
	while (!list_empty(&pInstance->pts_list)) {
		ptn = list_entry(pInstance->pts_list.next,
						struct ptsnode, node);
		if (ptn != NULL) {
			list_del(&ptn->node);
			ptn = NULL;
		}
	}
	if (pInstance->all_free_ptn) {
		vfree(pInstance->all_free_ptn);
		pInstance->all_free_ptn = NULL;
	}

	memset(pInstance, 0, sizeof(ptsserver_ins));
	kfree(pInstance);
	pInstance = NULL;
	vPtsServerInsList[index].pInstance = NULL;
	mutex_unlock(&vPtsServerIns->mListLock);
	return 0;
}
EXPORT_SYMBOL(ptsserver_ins_release);

long ptsserver_set_trick_mode(s32 pServerInsId, s32 mode) {
	PtsServerManage* vPtsServerIns = NULL;
	ptsserver_ins* pInstance = NULL;
	s32 index = pServerInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;
	pts_pr_vinfo(pServerInsId,"set trick mode:%d\n", mode);
	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}
	pInstance->mTrickMode = mode;
	// If set trickmode we set mOffsetMode default
	pInstance->mOffsetMode = mode;
	mutex_unlock(&vPtsServerIns->mListLock);
	return 0;
}

long ptsserver_checkin_apts_size(s32 pServerInsId,checkin_apts_size* mCheckinPtsSize) {
	PtsServerManage* vPtsServerIns = NULL;
	ptsserver_ins* pInstance = NULL;
	pts_node* ptn = NULL;
	pts_node* del_ptn = NULL;
	s32 index = get_index_from_ptsserver_id(pServerInsId);
	//s32 level = 0;

	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}

	if (pInstance->mListSize >= pInstance->mMaxCount) {
		del_ptn = list_first_entry(&pInstance->pts_list, pts_node, node);
		list_del(&del_ptn->node);
		list_add_tail(&del_ptn->node, &pInstance->pts_free_list);	//queue empty buffer
		if (ptsserver_debuglevel >= 1) {
			pts_pr_ainfo(index,"Checkin del offset:%d pts:0x%llx\n",
							del_ptn->offset,
							del_ptn->pts_90k);
		}
		pInstance->mListSize--;
	}

	if (!list_empty(&pInstance->pts_free_list)) {	//dequeue empty buffer
		ptn = list_first_entry(&pInstance->pts_free_list, struct ptsnode, node);
		list_del(&ptn->node);
		if (ptn != NULL) {
			ptn->offset = mCheckinPtsSize->offset;
			ptn->pts_90k = mCheckinPtsSize->pts_90k;
			ptn->pts_64 = mCheckinPtsSize->pts_64;
		}
	}

	if (ptn == NULL) {
		// if (ptsserver_debuglevel >= 1) {
		// 	pts_pr_ainfo(index,"ptn is null\n");
		// }
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}

	list_add_tail(&ptn->node, &pInstance->pts_list);

	pInstance->mListSize++;

	if (!pInstance->mPtsCheckinStarted || ptsserver_debuglevel >= 1) {
		//level = ptn->offset - pInstance->mLastCheckoutCurOffset;
		pts_pr_ainfo(index,"Checkin%s LstCnt:%d %s:0x%x pts(32:0x%llx 64:%lld)\n",
							pInstance->mPtsCheckinStarted?"":"(First)",
							pInstance->mListSize,
							ptn->offset < pInstance->mLastCheckinOffset?"rest offset":"offset",
							ptn->offset,
							ptn->pts_90k,
							ptn->pts_64);
	}

	if (!pInstance->mPtsCheckinStarted) {
		pInstance->mFirstCheckinOffset = mCheckinPtsSize->offset;
		pInstance->mFirstCheckinSize = mCheckinPtsSize->size;
		pInstance->mFirstCheckinPts90k = mCheckinPtsSize->pts_90k;
		pInstance->mFirstCheckinPts64 = mCheckinPtsSize->pts_64;
		pInstance->mPtsCheckinStarted = 1;
	}

	if (pInstance->mAlignmentOffset != 0) {
		pInstance->mLastCheckinOffset = pInstance->mAlignmentOffset;
		pInstance->mAlignmentOffset = 0;
	} else {
		pInstance->mLastCheckinOffset = ptn->offset;
	}

	pInstance->mLastCheckinSize = mCheckinPtsSize->size;
	pInstance->mLastCheckinPts90k = mCheckinPtsSize->pts_90k;
	pInstance->mLastCheckinPts64 = mCheckinPtsSize->pts_64;

	mutex_unlock(&vPtsServerIns->mListLock);

	return 0;
}
EXPORT_SYMBOL(ptsserver_checkin_apts_size);

long ptsserver_checkout_apts_offset(s32 pServerInsId,checkout_apts_offset* mCheckoutPtsOffset) {
	PtsServerManage* vPtsServerIns = NULL;
	ptsserver_ins* pInstance = NULL;
	s32 index = get_index_from_ptsserver_id(pServerInsId);
	pts_node* ptn = NULL;
	pts_node* find_ptn = NULL;
	pts_node* ptn_tmp = NULL;

	u32 cur_offset = 0xFFFFFFFF & mCheckoutPtsOffset->offset;

	u32 offsetDiff = 1024;
	u32 lookup_threshold = 0;
	s32 find_frame_num = 0;
	s32 find = 0;
	s32 offsetAbs = 0;
	s32 number = -1;
	s32 i = 0;

	if (index < 0 || index >= MAX_INSTANCE_NUM) {
		return -1;
	}
	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;

	if (pInstance == NULL) {
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}

	if (pInstance->mAudioOffsetMargin == 0) {
		lookup_threshold = pInstance->mLookupThreshold;
		offsetDiff = pInstance->mLookupThreshold;
	} else {
		lookup_threshold = pInstance->mAudioOffsetMargin;
		offsetDiff = pInstance->mAudioOffsetMargin;
	}

	if (!pInstance->mPtsCheckoutStarted || ptsserver_debuglevel >= 1) {
		pts_pr_ainfo(index,"checkout%s LstCnt:%d offset:0x%x\n",
							pInstance->mPtsCheckoutStarted?"":"(First)",
							pInstance->mListSize,
							cur_offset);
	}

	if (cur_offset != 0xFFFFFFFF &&
		!list_empty(&pInstance->pts_list)) {
		find_frame_num = 0;
		find = 0;
		list_for_each_entry_safe(ptn,ptn_tmp,&pInstance->pts_list, node) {
			if (ptn != NULL) {
				offsetAbs = abs(cur_offset - ptn->offset);
				if (offsetAbs <= lookup_threshold) {
					if (offsetAbs <= offsetDiff) {
						offsetDiff = offsetAbs;
						find = 1;
						number = i;
						find_ptn = ptn;
					}
				} else if (offsetAbs > 251658240) {
					// > 240M (15M*16)
					list_del(&ptn->node);
					list_add_tail(&ptn->node, &pInstance->pts_free_list);
					pInstance->mListSize--;
				} else if (find_frame_num > 5) {
					break;
				}
				if (ptsserver_debuglevel > 2 || offsetAbs > 251658240) {
					pts_pr_ainfo(index,"Checkout %s:%d diff:%d offset:0x%x pts:0x%llx\n",
										offsetAbs>251658240?"del":"i",
										i,
										offsetAbs,
										ptn->offset,
										ptn->pts_90k);
				}
				if (find) {
					find_frame_num++;
				}
			}
			i++;
		}

		if (find) {
			mCheckoutPtsOffset->pts_90k = find_ptn->pts_90k;
			mCheckoutPtsOffset->pts_64 = find_ptn->pts_64;
			if (ptsserver_debuglevel >= 1 ||
				!pInstance->mPtsCheckoutStarted) {
				pts_pr_ainfo(index,"Checkout ok LstCnt:%d find:%d offset(diff:%d L:0x%x) pts(32:0x%llx 64:%lld)\n",
									pInstance->mListSize,number,offsetDiff,
									find_ptn->offset,find_ptn->pts_90k,find_ptn->pts_64);
			}
			list_del(&find_ptn->node);
			list_add_tail(&find_ptn->node, &pInstance->pts_free_list);
			pInstance->mListSize--;
		}
	}
	if (find == 1 && (s64)mCheckoutPtsOffset->pts_64 == -1) {
		find = 0; //invalid pts
	}
	if (!find) {
		//when first apts lookup failed, use first checkin apts instead.(Refer to tsync logic)
		if (!pInstance->mPtsCheckoutStarted) {
			pInstance->mPtsCheckoutStarted = 1;
			mCheckoutPtsOffset->pts_90k = pInstance->mFirstCheckinPts90k;
			mCheckoutPtsOffset->pts_64 = pInstance->mFirstCheckinPts64;
			pts_pr_ainfo(index,"Checkout fail, use first pts:0x%llx\n",
					mCheckoutPtsOffset->pts_90k);
		} else {
			pInstance->mPtsCheckoutFailCount++;
			mCheckoutPtsOffset->pts_90k = -1;
			mCheckoutPtsOffset->pts_64 = -1;
			pInstance->mLastCheckoutCurOffset = cur_offset;
			if (ptsserver_debuglevel >= 1 || pInstance->mPtsCheckoutFailCount % 30 == 0) {
				pts_pr_ainfo(index,"Checkout fail Cnt:%d level:%d pts:%lld\n",
					pInstance->mPtsCheckoutFailCount,pInstance->mLastCheckinOffset - cur_offset,
					mCheckoutPtsOffset->pts_64);
			}
			mutex_unlock(&vPtsServerIns->mListLock);
			return 0;
		}
	}

	if (!pInstance->mPtsCheckoutStarted) {
		pInstance->mPtsCheckoutStarted = 1;
	}

	pInstance->mPtsCheckoutFailCount = 0;
	pInstance->mLastCheckoutOffset = mCheckoutPtsOffset->offset;
	pInstance->mLastCheckoutPts64 = mCheckoutPtsOffset->pts_64;
	pInstance->mLastDoubleCheckoutPts64 = mCheckoutPtsOffset->pts_64;
	pInstance->mLastCheckoutPts90k = mCheckoutPtsOffset->pts_90k;
	pInstance->mLastCheckoutCurOffset = cur_offset;
	mutex_unlock(&vPtsServerIns->mListLock);
	return 0;
}
EXPORT_SYMBOL(ptsserver_checkout_apts_offset);


long ptsserver_static_ins_binder(s32 pServerInsId, ptsserver_ins** pIns, ptsserver_alloc_para* allocParm) {
	ptsserver_ins* pInstance = NULL;
	s32 index = -1;
	s32 temp_ins = -1;
	index = get_index_from_ptsserver_id(pServerInsId);

	if (index >= 0 && index < MAX_INSTANCE_NUM) {
		mutex_lock(&(vPtsServerInsList[index].mListLock));
		pInstance = vPtsServerInsList[index].pInstance;
		if (pInstance == NULL) {
			mutex_unlock(&(vPtsServerInsList[index].mListLock));
			return -1;
		}
		pInstance->mRef++;
		*pIns = pInstance;
		mutex_unlock(&(vPtsServerInsList[index].mListLock));
	} else if (index < 0) {
		pInstance = kzalloc(sizeof(ptsserver_ins), GFP_KERNEL);
		if (pInstance == NULL) {
			return -1;
		}

		for (temp_ins = MAX_DYNAMIC_INSTANCE_NUM; temp_ins < MAX_INSTANCE_NUM; temp_ins++) {
			mutex_lock(&(vPtsServerInsList[temp_ins].mListLock));
			if (vPtsServerInsList[temp_ins].pInstance == NULL) {
				vPtsServerInsList[temp_ins].pInstance = pInstance;
				pInstance->mPtsServerInsId = pServerInsId;
				index = temp_ins;
				pInstance->mRef++;
				ptsserver_ins_init_syncinfo(pInstance, allocParm);
				mutex_unlock(&(vPtsServerInsList[temp_ins].mListLock));
				break;
			}
			mutex_unlock(&(vPtsServerInsList[temp_ins].mListLock));
		}
		*pIns = pInstance;
	}
	pr_info("ptsserv: binder insId:%d index:%d\n", pServerInsId, index);

	return 0;
}
EXPORT_SYMBOL(ptsserver_static_ins_binder);

long ptsserver_get_list_size(s32 pServerInsId, u32* ListSize) {
	PtsServerManage* vPtsServerIns = NULL;
	ptsserver_ins* pInstance = NULL;
	s32 index = get_index_from_ptsserver_id(pServerInsId);

	if (index < 0 || index >= MAX_INSTANCE_NUM) {
		return -1;
	}
	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;

	if (pInstance == NULL) {
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}

	*ListSize = pInstance->mListSize;
	mutex_unlock(&vPtsServerIns->mListLock);

	return 0;
}

long ptsserver_set_audio_offset_margin(s32 pServerInsId, u32 offsetMargin) {
	PtsServerManage* vPtsServerIns = NULL;
	ptsserver_ins* pInstance = NULL;
	s32 index = get_index_from_ptsserver_id(pServerInsId);

	if (index < 0 || index >= MAX_INSTANCE_NUM) {
		return -1;
	}
	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;

	if (pInstance == NULL) {
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}
	pts_pr_ainfo(index,"set margin:%d\n insId:%d\n",
						offsetMargin, pServerInsId);

	pInstance->mAudioOffsetMargin = offsetMargin;
	mutex_unlock(&vPtsServerIns->mListLock);

	return 0;
}

long ptsserver_ins_reset(s32 pServerInsId) {
	PtsServerManage* vPtsServerIns = NULL;
	ptsserver_ins* pInstance = NULL;
	s32 index = get_index_from_ptsserver_id(pServerInsId);
	pts_node *ptn = NULL;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	vPtsServerIns = &(vPtsServerInsList[index]);
	mutex_lock(&vPtsServerIns->mListLock);
	pInstance = vPtsServerIns->pInstance;
	if (pInstance == NULL) {
		mutex_unlock(&vPtsServerIns->mListLock);
		return -1;
	}

	while (!list_empty(&pInstance->pts_list)) {
		ptn = list_entry(pInstance->pts_list.next,
						struct ptsnode, node);
		if (ptn != NULL) {
			list_del(&ptn->node);
			list_add_tail(&ptn->node, &pInstance->pts_free_list);
			pInstance->mListSize--;
		}
	}
	pts_pr_ainfo(index,"reset LstCnt:%d\n", pInstance->mListSize);

	pInstance->mPtsCheckinStarted = 0;
	pInstance->mPtsCheckoutStarted = 0;
	pInstance->mPtsCheckoutFailCount = 0;
	pInstance->mFrameDuration = 0;
	pInstance->mFrameDuration64 = 0;
	pInstance->mFirstCheckinPts = 0;
	pInstance->mFirstCheckinOffset = 0;
	pInstance->mFirstCheckinSize = 0;
	pInstance->mLastCheckinPts = 0;
	pInstance->mLastCheckinOffset = 0;
	pInstance->mLastCheckinSize = 0;
	pInstance->mAlignmentOffset = 0;
	pInstance->mLastCheckoutPts = 0;
	pInstance->mLastCheckoutOffset = 0;
	pInstance->mFirstCheckinPts64 = 0;
	pInstance->mLastCheckinPts64 = 0;
	pInstance->mLastCheckoutPts64 = 0;
	pInstance->mDoubleCheckFrameDuration = 0;
	pInstance->mDoubleCheckFrameDuration64 = 0;
	pInstance->mDoubleCheckFrameDurationCount = 0;
	pInstance->mLastDoubleCheckoutPts = 0;
	pInstance->mLastDoubleCheckoutPts64 = 0;
	pInstance->mDecoderDuration = 0;
	pInstance->mLastCheckoutCurOffset = 0;
	pInstance->mLastCheckinPiecePts = 0;
	pInstance->mLastCheckinPiecePts64 = 0;
	pInstance->mLastCheckinPieceOffset = 0;
	pInstance->mLastCheckinPieceSize = 0;
	pInstance->mLastCheckoutIndex = 0;
	pInstance->mLastCheckinDurationCount = 0;
	pInstance->mLastCheckoutDurationCount = 0;
	pInstance->mLastShotBound = 0;
	pInstance->mTrickMode = 0;
	pInstance->setC2Mode = false;
	pInstance->mOffsetMode = 0;
	pInstance->mStickyWrapFlag = false;
	pInstance->mLastCheckoutPts90k = 0;
	pInstance->mFirstCheckinPts90k = 0;
	pInstance->mLastCheckinPts90k = 0;
	pInstance->mListSize = 0;
	pInstance->mAudioOffsetMargin = 0;

	mutex_unlock(&vPtsServerIns->mListLock);
	pts_pr_ainfo(index,"reset ok\n");
	return 0;
}
EXPORT_SYMBOL(ptsserver_ins_reset);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
static struct param_entry ptsserver_params[] = {
	PARAM_INT(ptsserver_debuglevel),
	{ /* sentinel */ }
};

module_param_cb(debug_ptsserver, &key_value_param_ops, &ptsserver_params, 0644);

#else
module_param(ptsserver_debuglevel, uint, 0664);
MODULE_PARM_DESC(ptsserver_debuglevel, "\n pts server debug level\n");
#endif

