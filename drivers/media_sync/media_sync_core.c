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
#include "media_sync_core.h"

#define MAX_INSTANCE_NUM 10
mediasync_ins* vMediaSyncInsList[MAX_INSTANCE_NUM] = {0};

extern int demux_get_stc(int demux_device_index, int index,
                  u64 *stc, unsigned int *base);
extern int demux_get_pcr(int demux_device_index, int index, u64 *pcr);

static u64 get_llabs(s64 value){
	u64 llvalue;
	if (value > 0) {
		return value;
	} else {
		llvalue = (u64)(0-value);
		return llvalue;
	}
}

static u64 get_stc_time_us(s32 sSyncInsId, u64 *systemtime)
{
    /*mediasync_ins* pInstance = NULL;
	u64 stc;
	unsigned int base;
      s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index];
	demux_get_stc(pInstance->mDemuxId, 0, &stc, &base);*/
	mediasync_ins* pInstance = NULL;
	int ret;
	u64 stc;
	u64 timeus;
	u64 pcr;
	s64 pcr_diff;
	s64 time_diff;
	s32 index = sSyncInsId;
	struct timespec64 ts_monotonic;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;
	pInstance = vMediaSyncInsList[index];
	ktime_get_ts64(&ts_monotonic);
	timeus = ts_monotonic.tv_sec * 1000000LL + ts_monotonic.tv_nsec / 1000LL;
	*systemtime = timeus;
	if (pInstance->mDemuxId < 0)
		return timeus;

	ret = demux_get_pcr(pInstance->mDemuxId, 0, &pcr);
	if (ret != 0) {
		stc = timeus;
	} else {
		if (pInstance->last_pcr == 0) {
			stc = timeus;
			pInstance->last_pcr = pcr * 100 / 9;
			pInstance->last_system = timeus;
		} else {
			pcr_diff = pcr * 100 / 9 - pInstance->last_pcr;
			time_diff = timeus - pInstance->last_system;
			if (time_diff && (get_llabs(pcr_diff) / time_diff
					    > 100)) {
				pInstance->last_pcr = pcr * 100 / 9;
				pInstance->last_system = timeus;
				stc = timeus;
			} else {
				if (time_diff)
					stc = pInstance->last_system + pcr_diff;
				else
					stc = timeus;

				pInstance->last_pcr = pcr * 100 / 9;
				pInstance->last_system = stc;
			}
		}
	}
	pr_debug("get_stc_time_us stc:%lld pcr:%lld system_time:%lld\n", stc,  pcr * 100 / 9,  timeus);
	return stc;
}

/*static s64 get_system_time_us(void) {
	s64 TimeUs;
	struct timespec64 ts_monotonic;
	ktime_get_ts64(&ts_monotonic);
	TimeUs = ts_monotonic.tv_sec * 1000000LL + ts_monotonic.tv_nsec / 1000LL;
	pr_debug("get_system_time_us %lld\n", TimeUs);
	return TimeUs;
}*/

long mediasync_ins_alloc(s32 sDemuxId,
			s32 sPcrPid,
			s32 *sSyncInsId,
			mediasync_ins **pIns){
	s32 index = 0;
	mediasync_ins* pInstance = NULL;
	pInstance = kzalloc(sizeof(mediasync_ins), GFP_KERNEL);
	if (pInstance == NULL) {
		return -1;
	}

	for (index = 0; index < MAX_INSTANCE_NUM - 1; index++) {
		if (vMediaSyncInsList[index] == NULL) {
			vMediaSyncInsList[index] = pInstance;
			pInstance->mSyncInsId = index;
			*sSyncInsId = index;
			pr_info("mediasync_ins_alloc index:%d\n", index);
			break;
		}
	}

	if (index == MAX_INSTANCE_NUM) {
		kzfree(pInstance);
		return -1;
	}

	pInstance->mDemuxId = sDemuxId;
	pInstance->mPcrPid = sPcrPid;
	*pIns = pInstance;
	return 0;
}


long mediasync_ins_delete(s32 sSyncInsId) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index];
	if (pInstance == NULL)
		return -1;

	kzfree(pInstance);
	vMediaSyncInsList[index] = NULL;
	return 0;
}

long mediasync_ins_binder(s32 sSyncInsId,
			mediasync_ins **pIns) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index];
	if (pInstance == NULL)
		return -1;

	pInstance->mRef++;
	*pIns = pInstance;
	return 0;
}

long mediasync_ins_unbinder(s32 sSyncInsId) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index];
	if (pInstance == NULL)
		return -1;

	pInstance->mRef--;

	if (pInstance->mRef <= 0)
		mediasync_ins_delete(sSyncInsId);

	return 0;
}

long mediasync_ins_update_mediatime(s32 sSyncInsId,
				s64 lMediaTime,
				s64 lSystemTime, bool forceUpdate) {
	mediasync_ins* pInstance = NULL;
	u64 current_stc = 0;
	s64 current_systemtime = 0;
	s64 diff_system_time = 0;
	s64 diff_mediatime = 0;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index];
	if (pInstance == NULL)
		return -1;

	current_stc = get_stc_time_us(sSyncInsId, &current_systemtime);
	//pInstance->mSyncMode = MEDIA_SYNC_PCRMASTER;

	if (pInstance->mSyncMode == MEDIA_SYNC_PCRMASTER) {
		if (lSystemTime == 0) {
			if (current_stc != 0) {
				diff_system_time = current_stc - pInstance->mLastStc;
				diff_mediatime = lMediaTime - pInstance->mLastMediaTime;
			} else {
				diff_system_time = current_systemtime - pInstance->mLastRealTime;
				diff_mediatime = lMediaTime - pInstance->mLastMediaTime;
			}
			if (diff_mediatime < 0
				|| ((diff_mediatime > 0)
				&& (get_llabs(diff_system_time - diff_mediatime) > MIN_UPDATETIME_THRESHOLD_US ))) {
				pr_info("MEDIA_SYNC_PCRMASTER update time system diff:%lld media diff:%lld current:%lld\n",
											diff_system_time,
											diff_mediatime,
											current_systemtime);
				pInstance->mLastMediaTime = lMediaTime;
				pInstance->mLastRealTime = current_systemtime;
				pInstance->mLastStc = current_stc;
			}
		} else {
			if (current_stc != 0) {
				diff_system_time = current_stc + lSystemTime - current_systemtime - pInstance->mLastStc;
				diff_mediatime = lMediaTime - pInstance->mLastMediaTime;
			} else {
				diff_system_time = lSystemTime - pInstance->mLastRealTime;
				diff_mediatime = lMediaTime - pInstance->mLastMediaTime;
			}

			if (diff_mediatime < 0
				|| ((diff_mediatime > 0)
				&& (get_llabs(diff_system_time - diff_mediatime) > MIN_UPDATETIME_THRESHOLD_US ))) {
				pr_info("MEDIA_SYNC_PCRMASTER update time stc diff:%lld media diff:%lld lSystemTime:%lld lMediaTime:%lld\n",
											diff_system_time,
											diff_mediatime,
											lSystemTime,
											lMediaTime);
				pInstance->mLastMediaTime = lMediaTime;
				pInstance->mLastRealTime = lSystemTime;
				pInstance->mLastStc = current_stc + lSystemTime - current_systemtime;
			}
		}
	} else {
		if (lSystemTime == 0) {
			pInstance->mLastMediaTime = lMediaTime;
			pInstance->mLastRealTime = current_systemtime;
			pInstance->mLastStc = current_stc;
		} else {
			pInstance->mLastMediaTime = lMediaTime;
			pInstance->mLastRealTime = lSystemTime;
			pInstance->mLastStc = current_stc + lSystemTime - current_systemtime;
		}
	}
	pInstance->mTrackMediaTime = lMediaTime;
	return 0;
}

long mediasync_ins_set_mediatime_speed(s32 sSyncInsId,
					mediasync_speed fSpeed) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index];
	if (pInstance == NULL)
		return -1;

	pInstance->mSpeed.mNumerator = fSpeed.mNumerator;
	pInstance->mSpeed.mDenominator = fSpeed.mDenominator;
	return 0;
}

long mediasync_ins_set_paused(s32 sSyncInsId, s32 sPaused) {

	mediasync_ins* pInstance = NULL;
	u64 current_stc = 0;
	s64 current_systemtime = 0;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index];
	if (pInstance == NULL)
		return -1;

	if ((sPaused != 0 && sPaused != 1)
		|| (sPaused == pInstance->mPaused))
		return -1;

	current_stc = get_stc_time_us(sSyncInsId, &current_systemtime);

	pInstance->mPaused = sPaused;
	pInstance->mLastRealTime = current_systemtime;
	pInstance->mLastStc = current_stc;

	return 0;
}

long mediasync_ins_get_paused(s32 sSyncInsId, s32* spPaused) {

	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index];
	if (pInstance == NULL)
		return -1;
	*spPaused = pInstance->mPaused ;
	return 0;
}

long mediasync_ins_set_syncmode(s32 sSyncInsId, s32 sSyncMode){
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index];
	if (pInstance == NULL)
		return -1;

	pInstance->mSyncMode = sSyncMode;
	return 0;
}

long mediasync_ins_get_syncmode(s32 sSyncInsId, s32 *sSyncMode) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index];
	if (pInstance == NULL)
		return -1;
	*sSyncMode = pInstance->mSyncMode;
	return 0;
}

long mediasync_ins_get_mediatime_speed(s32 sSyncInsId, mediasync_speed *fpSpeed) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index];
	if (pInstance == NULL)
		return -1;
	fpSpeed->mNumerator = pInstance->mSpeed.mNumerator;
	fpSpeed->mDenominator = pInstance->mSpeed.mDenominator;
	return 0;
}

long mediasync_ins_get_anchor_time(s32 sSyncInsId,
												s64* lpMediaTime,
												s64* lpSTCTime,
												s64* lpSystemTime) {
	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;

	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index];
	if (pInstance == NULL)
		return -1;

    *lpMediaTime = pInstance->mLastMediaTime;
	*lpSTCTime = pInstance->mLastStc;
	*lpSystemTime = pInstance->mLastRealTime;
	return 0;
}

long mediasync_ins_get_systemtime(s32 sSyncInsId, s64* lpSTC, s64* lpSystemTime){
	mediasync_ins* pInstance = NULL;
	u64 current_stc = 0;
	s64 current_systemtime = 0;
	s32 index = sSyncInsId;

	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index];
	if (pInstance == NULL)
		return -1;

	current_stc = get_stc_time_us(sSyncInsId, &current_systemtime);

	*lpSTC = current_stc;
	*lpSystemTime = current_systemtime;

	return 0;
}

long mediasync_ins_get_nextvsync_systemtime(s32 sSyncInsId, s64* lpSystemTime) {

	return 0;
}

long mediasync_ins_get_trackmediatime(s32 sSyncInsId, s64* lpTrackMediaTime) {

	mediasync_ins* pInstance = NULL;
	s32 index = sSyncInsId;
	if (index < 0 || index >= MAX_INSTANCE_NUM)
		return -1;

	pInstance = vMediaSyncInsList[index];
	if (pInstance == NULL)
		return -1;
	*lpTrackMediaTime = pInstance->mTrackMediaTime;
	return 0;
}










