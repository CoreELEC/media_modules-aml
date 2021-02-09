#ifndef MEDIA_SYNC_HEAD_HH
#define MEDIA_SYNC_HEAD_HH

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/amlogic/cpu_version.h>


#define MIN_UPDATETIME_THRESHOLD_US 50000
typedef enum {
	MEDIA_SYNC_VMASTER = 0,
	MEDIA_SYNC_AMASTER = 1,
	MEDIA_SYNC_PCRMASTER = 2,
	MEDIA_SYNC_MODE_MAX = 255,
}sync_mode;

typedef struct speed{
	u32 mNumerator;
	u32 mDenominator;
}mediasync_speed;

typedef struct instance{
	s32 mSyncInsId;
	s32 mDemuxId;
	s32 mPcrPid;
	s32 mPaused;
	s32 mRef;
	s32 mSyncMode;
	s64 mLastStc;
	s64 mLastRealTime;
	s64 mLastMediaTime;
	s64 mTrackMediaTime;
	mediasync_speed mSpeed;
	u64 last_system;
	u64 last_pcr;
}mediasync_ins;

long mediasync_ins_alloc(s32 sDemuxId,
			s32 sPcrPid,
			s32 *sSyncInsId,
			mediasync_ins **pIns);

long mediasync_ins_delete(s32 sSyncInsId);
long mediasync_ins_binder(s32 sSyncInsId,
			mediasync_ins **pIns);
long mediasync_ins_unbinder(s32 sSyncInsId);
long mediasync_ins_update_mediatime(s32 sSyncInsId,
					s64 lMediaTime,
					s64 lSystemTime, bool forceUpdate);
long mediasync_ins_set_mediatime_speed(s32 sSyncInsId, mediasync_speed fSpeed);
long mediasync_ins_set_paused(s32 sSyncInsId, s32 sPaused);
long mediasync_ins_get_paused(s32 sSyncInsId, s32* spPaused);
long mediasync_ins_get_trackmediatime(s32 sSyncInsId, s64* lpTrackMediaTime);
long mediasync_ins_set_syncmode(s32 sSyncInsId, s32 sSyncMode);
long mediasync_ins_get_syncmode(s32 sSyncInsId, s32 *sSyncMode);
long mediasync_ins_get_mediatime_speed(s32 sSyncInsId, mediasync_speed *fpSpeed);
long mediasync_ins_get_anchor_time(s32 sSyncInsId,
												s64* lpMediaTime,
												s64* lpSTCTime,
												s64* lpSystemTime);
long mediasync_ins_get_systemtime(s32 sSyncInsId,
				s64* lpSTC,
				s64* lpSystemTime);
long mediasync_ins_get_nextvsync_systemtime(s32 sSyncInsId, s64* lpSystemTime);
#endif


