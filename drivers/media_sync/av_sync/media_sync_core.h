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
} sync_mode;

typedef enum {
	MEDIASYNC_INIT = 0,
	MEDIASYNC_AUDIO_ARRIVED,
	MEDIASYNC_VIDEO_ARRIVED,
	MEDIASYNC_AV_ARRIVED,
	MEDIASYNC_AV_SYNCED,
	MEDIASYNC_RUNNING,
	MEDIASYNC_VIDEO_LOST_SYNC,
	MEDIASYNC_AUDIO_LOST_SYNC,
	MEDIASYNC_EXIT,
} avsync_state;

typedef enum {
	UNKNOWN_CLOCK = 0,
	AUDIO_CLOCK,
	VIDEO_CLOCK,
	PCR_CLOCK,
	REF_CLOCK,
} mediasync_clocktype;

typedef enum {
	GET_UPDATE_INFO = 0,
	SET_VIDEO_FRAME_ADVANCE = 500,
} mediasync_control_cmd;

typedef struct m_control {
	u32 cmd;
	u32 size;
	u32 reserved[2];
	union {
		s32 value;
		s64 value64;
		ulong ptr;
	};
} mediasync_control;


typedef struct speed{
	u32 mNumerator;
	u32 mDenominator;
} mediasync_speed;

typedef struct mediasync_lock{
	struct mutex m_mutex;
	int Is_init;
} mediasync_lock;

typedef struct frameinfo{
	int64_t framePts;
	int64_t frameSystemTime;
} mediasync_frameinfo;

typedef struct video_packets_info{
	int packetsSize;
	int64_t packetsPts;
} mediasync_video_packets_info;

typedef struct audio_packets_info{
	int packetsSize;
	int duration;
	int isworkingchannel;
	int isneedupdate;
	int64_t packetsPts;
} mediasync_audio_packets_info;

typedef struct discontinue_frame_info{
	int64_t discontinuePtsBefore;
	int64_t discontinuePtsAfter;
	int isDiscontinue;
} mediasync_discontinue_frame_info;


typedef struct avsync_state_cur_time_us{
	avsync_state state;
	int64_t setStateCurTimeUs;
} mediasync_avsync_state_cur_time_us;

typedef struct syncinfo {
	avsync_state state;
	int64_t setStateCurTimeUs;
	mediasync_frameinfo firstAframeInfo;
	mediasync_frameinfo firstVframeInfo;
	mediasync_frameinfo firstDmxPcrInfo;
	mediasync_frameinfo refClockInfo;
	mediasync_frameinfo curAudioInfo;
	mediasync_frameinfo curVideoInfo;
	mediasync_frameinfo curDmxPcrInfo;
	mediasync_frameinfo queueAudioInfo;
	mediasync_frameinfo queueVideoInfo;
	mediasync_frameinfo firstAudioPacketsInfo;
	mediasync_frameinfo firstVideoPacketsInfo;
	mediasync_frameinfo pauseVideoInfo;
	mediasync_frameinfo pauseAudioInfo;
	mediasync_video_packets_info videoPacketsInfo;
	mediasync_audio_packets_info audioPacketsInfo;
} mediasync_syncinfo;

typedef struct audioinfo{
	int cacheSize;
	int cacheDuration;
} mediasync_audioinfo;

typedef struct videoinfo{
	int cacheSize;
	int specialSizeCount;
	int cacheDuration;
} mediasync_videoinfo;

typedef struct audio_format{
	int samplerate;
	int datawidth;
	int channels;
	int format;
} mediasync_audio_format;

typedef enum
{
	TS_DEMOD = 0,                          // TS Data input from demod
	TS_MEMORY = 1,                         // TS Data input from memory
	ES_MEMORY = 2,                         // ES Data input from memory
} aml_Source_Type;

typedef enum {
    CLOCK_PROVIDER_NONE = 0,
    CLOCK_PROVIDER_DISCONTINUE,
    CLOCK_PROVIDER_NORMAL,
    CLOCK_PROVIDER_LOST,
    CLOCK_PROVIDER_RECOVERING,
} mediasync_clockprovider_state;

typedef struct update_info{
	u32 mStcParmUpdateCount;
	u32 debugLevel;
	s64 mCurrentSystemtime;
	int mPauseResumeFlag;
	avsync_state mAvSyncState;
	int64_t mSetStateCurTimeUs;
	mediasync_clockprovider_state mSourceClockState;
	mediasync_audioinfo mAudioInfo;
	mediasync_videoinfo mVideoInfo;
	u32 isVideoFrameAdvance;
} mediasync_update_info;

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
	s64 mStartMediaTime;
	mediasync_speed mSpeed;
	mediasync_speed mPcrSlope;
	s32 mSyncModeChange;
	s64 mUpdateTimeThreshold;
	s32 mPlayerInstanceId;
	s32 mVideoSmoothTag;
	int mHasAudio;
	int mHasVideo;
	int mute_flag;
	int mStartThreshold;
	int mPtsAdjust;
	int mVideoWorkMode;
	int mFccEnable;
	int mPauseResumeFlag;
	int mAVRef;
	u32 mStcParmUpdateCount;
	u32 mAudioCacheUpdateCount;
	u32 mVideoCacheUpdateCount;
	u32 mGetAudioCacheUpdateCount;
	u32 mGetVideoCacheUpdateCount;
	u32 isVideoFrameAdvance;
	s64 mLastCheckSlopeSystemtime;
	s64 mLastCheckSlopeDemuxPts;
	u32 mLastCheckPcrSlope;
	mediasync_clocktype mSourceClockType;
	mediasync_clockprovider_state mSourceClockState;
	mediasync_audioinfo mAudioInfo;
	mediasync_videoinfo mVideoInfo;
	mediasync_syncinfo mSyncInfo;
	mediasync_discontinue_frame_info mVideoDiscontinueInfo;
	mediasync_discontinue_frame_info mAudioDiscontinueInfo;
	aml_Source_Type mSourceType;
	mediasync_audio_format mAudioFormat;
	char atrace_video[32];
	char atrace_audio[32];
	char atrace_pcrscr[32];
}mediasync_ins;

typedef struct Media_Sync_Manage {
	mediasync_ins* pInstance;
	struct mutex m_lock;
} MediaSyncManage;

long mediasync_init(void);
long mediasync_ins_alloc(s32 sDemuxId,
			s32 sPcrPid,
			s32 *sSyncInsId,
			mediasync_ins **pIns);

long mediasync_ins_delete(s32 sSyncInsId);
long mediasync_ins_binder(s32 sSyncInsId,
			mediasync_ins **pIns);
long mediasync_static_ins_binder(s32 sSyncInsId,
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
long mediasync_ins_set_updatetime_threshold(s32 sSyncInsId, s64 lTimeThreshold);
long mediasync_ins_get_updatetime_threshold(s32 sSyncInsId, s64* lpTimeThreshold);

long mediasync_ins_init_syncinfo(s32 sSyncInsId);
long mediasync_ins_set_clocktype(s32 sSyncInsId, mediasync_clocktype type);
long mediasync_ins_get_clocktype(s32 sSyncInsId, mediasync_clocktype* type);
long mediasync_ins_set_avsyncstate(s32 sSyncInsId, s32 state);
long mediasync_ins_get_avsyncstate(s32 sSyncInsId, s32* state);
long mediasync_ins_set_hasaudio(s32 sSyncInsId, int hasaudio);
long mediasync_ins_get_hasaudio(s32 sSyncInsId, int* hasaudio);
long mediasync_ins_set_hasvideo(s32 sSyncInsId, int hasvideo);
long mediasync_ins_get_hasvideo(s32 sSyncInsId, int* hasvideo);
long mediasync_ins_set_audioinfo(s32 sSyncInsId, mediasync_audioinfo info);
long mediasync_ins_get_audioinfo(s32 sSyncInsId, mediasync_audioinfo* info);
long mediasync_ins_set_videoinfo(s32 sSyncInsId, mediasync_videoinfo info);
long mediasync_ins_set_audiomute(s32 sSyncInsId, int mute_flag);
long mediasync_ins_get_audiomute(s32 sSyncInsId, int* mute_flag);
long mediasync_ins_get_videoinfo(s32 sSyncInsId, mediasync_videoinfo* info);
long mediasync_ins_set_firstaudioframeinfo(s32 sSyncInsId, mediasync_frameinfo info);
long mediasync_ins_get_firstaudioframeinfo(s32 sSyncInsId, mediasync_frameinfo* info);
long mediasync_ins_set_firstvideoframeinfo(s32 sSyncInsId, mediasync_frameinfo info);
long mediasync_ins_get_firstvideoframeinfo(s32 sSyncInsId, mediasync_frameinfo* info);
long mediasync_ins_set_firstdmxpcrinfo(s32 sSyncInsId, mediasync_frameinfo info);
long mediasync_ins_get_firstdmxpcrinfo(s32 sSyncInsId, mediasync_frameinfo* info);
long mediasync_ins_set_refclockinfo(s32 sSyncInsId, mediasync_frameinfo info);
long mediasync_ins_get_refclockinfo(s32 sSyncInsId, mediasync_frameinfo* info);
long mediasync_ins_set_curaudioframeinfo(s32 sSyncInsId, mediasync_frameinfo info);
long mediasync_ins_get_curaudioframeinfo(s32 sSyncInsId, mediasync_frameinfo* info);
long mediasync_ins_set_curvideoframeinfo(s32 sSyncInsId, mediasync_frameinfo info);
long mediasync_ins_get_curvideoframeinfo(s32 sSyncInsId, mediasync_frameinfo* info);
long mediasync_ins_set_curdmxpcrinfo(s32 sSyncInsId, mediasync_frameinfo info);
long mediasync_ins_get_curdmxpcrinfo(s32 sSyncInsId, mediasync_frameinfo* info);
long mediasync_ins_set_clockstate(s32 sSyncInsId, mediasync_clockprovider_state state);
long mediasync_ins_get_clockstate(s32 sSyncInsId, mediasync_clockprovider_state* state);
long mediasync_ins_set_startthreshold(s32 sSyncInsId, s32 threshold);
long mediasync_ins_get_startthreshold(s32 sSyncInsId, s32* threshold);
long mediasync_ins_set_ptsadjust(s32 sSyncInsId, s32 adjust_pts);
long mediasync_ins_get_ptsadjust(s32 sSyncInsId, s32* adjust_pts);
long mediasync_ins_set_videoworkmode(s32 sSyncInsId, s64 mode);
long mediasync_ins_get_videoworkmode(s32 sSyncInsId, s64* mode);
long mediasync_ins_set_fccenable(s32 sSyncInsId, s64 enable);
long mediasync_ins_get_fccenable(s32 sSyncInsId, s64* enable);
long mediasync_ins_set_source_type(s32 sSyncInsId, aml_Source_Type sourceType);
long mediasync_ins_get_source_type(s32 sSyncInsId, aml_Source_Type* sourceType);
long mediasync_ins_set_start_media_time(s32 sSyncInsId, s64 startime);
long mediasync_ins_get_start_media_time(s32 sSyncInsId, s64* starttime);
long mediasync_ins_set_audioformat(s32 sSyncInsId, mediasync_audio_format format);
long mediasync_ins_get_audioformat(s32 sSyncInsId, mediasync_audio_format* format);
long mediasync_ins_set_pauseresume(s32 sSyncInsId, int flag);
long mediasync_ins_get_pauseresume(s32 sSyncInsId, int* flag);
long mediasync_ins_set_pcrslope(s32 sSyncInsId, mediasync_speed pcrslope);
long mediasync_ins_get_pcrslope(s32 sSyncInsId, mediasync_speed *pcrslope);
long mediasync_ins_reset(s32 sSyncInsId);
long mediasync_ins_update_avref(s32 sSyncInsId, int flag);
long mediasync_ins_get_avref(s32 sSyncInsId, int *ref);
long mediasync_ins_set_queue_audio_info(s32 sSyncInsId, mediasync_frameinfo info);
long mediasync_ins_get_queue_audio_info(s32 sSyncInsId, mediasync_frameinfo* info);
long mediasync_ins_set_queue_video_info(s32 sSyncInsId, mediasync_frameinfo info);
long mediasync_ins_get_queue_video_info(s32 sSyncInsId, mediasync_frameinfo* info);
long mediasync_ins_set_audio_packets_info(s32 sSyncInsId, mediasync_audio_packets_info info);
long mediasync_ins_get_audio_cache_info(s32 sSyncInsId, mediasync_audioinfo* info);
long mediasync_ins_set_video_packets_info(s32 sSyncInsId, mediasync_video_packets_info info);
long mediasync_ins_get_video_cache_info(s32 sSyncInsId, mediasync_videoinfo* info);
long mediasync_ins_set_first_queue_audio_info(s32 sSyncInsId, mediasync_frameinfo info);
long mediasync_ins_get_first_queue_audio_info(s32 sSyncInsId, mediasync_frameinfo* info);
long mediasync_ins_set_first_queue_video_info(s32 sSyncInsId, mediasync_frameinfo info);
long mediasync_ins_get_first_queue_video_info(s32 sSyncInsId, mediasync_frameinfo* info);
long mediasync_ins_set_player_instance_id(s32 sSyncInsId, s32 PlayerInstanceId);
long mediasync_ins_get_player_instance_id(s32 sSyncInsId, s32* PlayerInstanceId);
long mediasync_ins_get_avsync_state_cur_time_us(s32 sSyncInsId, mediasync_avsync_state_cur_time_us* avsyncstate);
long mediasync_ins_set_pause_video_info(s32 sSyncInsId, mediasync_frameinfo info);
long mediasync_ins_get_pause_video_info(s32 sSyncInsId, mediasync_frameinfo* info);
long mediasync_ins_set_pause_audio_info(s32 sSyncInsId, mediasync_frameinfo info);
long mediasync_ins_get_pause_audio_info(s32 sSyncInsId, mediasync_frameinfo* info);
long mediasync_ins_ext_ctrls(s32 sSyncInsId, ulong arg, unsigned int is_compat_ptr);
s64 mediasync_ins_get_stc_time(mediasync_ins* pInstance,s64 CurTimeUs);
void mediasync_ins_check_pcr_slope(mediasync_ins* pInstance, mediasync_update_info* info);
long mediasync_ins_set_pcrslope_implementation(mediasync_ins* pInstance, mediasync_speed pcrslope);
long mediasync_ins_set_video_smooth_tag(s32 sSyncInsId, s32 sSmooth_tag);
long mediasync_ins_get_video_smooth_tag(s32 sSyncInsId, s32* spSmooth_tag);
long mediasync_ins_get_status(s32 sSyncInsId, char *buf);
long mediasync_ins_get_all_status(char *buf, int *size);
long mediasync_ins_get_status_by_tag(const char *buf, int size);


#endif
