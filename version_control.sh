#!/bin/sh

MEDIA_MODULE_PATH=$(cd "$(dirname "$0")";pwd)
Major_V=$(cd ${MEDIA_MODULE_PATH}/; grep "Major_V" VERSION  | awk -F [=] '{print $2}')
Minor_V=$(cd ${MEDIA_MODULE_PATH}/; grep "Minor_V" VERSION  | awk -F [=] '{print $2}')
BASE_CHANGEID=$(cd ${MEDIA_MODULE_PATH}/; grep "^BaseChangeId" VERSION | awk -F [=] '{print $2}' | cut -c1-6)
MEDIAMODULE_CHANGEID=$(cd ${MEDIA_MODULE_PATH}; git log -1 ${MEDIA_MODULE_PATH} | grep "Change-Id: " | awk '{ print $2}' | cut -c1-6 | tail -1)
COMMIT_COUNT=$(cd ${MEDIA_MODULE_PATH}/; git log | grep "Change-Id: " | grep -n ${BASE_CHANGEID} | awk -F ":" '{printf "%d", $1-1}' )
MEDIAMODULE_COMMITID=$(cd ${MEDIA_MODULE_PATH}/; git log -1 --oneline ${MEDIA_MODULE_PATH} | awk '{print $1}' | cut -c1-6 | tail -1)
UCODE_VERSION_DETAIL=$(cd ${MEDIA_MODULE_PATH}/; ./firmware/checkmsg ./firmware/video_ucode.bin | grep "ver    :" | awk '{print $3}' | sed 's/v//g')
UCODE_VERSION=$(cd ${MEDIA_MODULE_PATH}/; ./firmware/checkmsg ./firmware/video_ucode.bin | grep "ver    :"  | awk -F '[v-]' '{print $3}' | awk -F [\.] '{printf "%d%02d%03d", $1,$2,$3}')

VERSION_CONTROL_CFLAGS="-DDECODER_VERSION=${Major_V}.${Minor_V}.${COMMIT_COUNT}-${MEDIAMODULE_CHANGEID}.${MEDIAMODULE_COMMITID}.${UCODE_VERSION}"
VERSION_CONTROL_CFLAGS="${VERSION_CONTROL_CFLAGS} -DUCODE_VERSION=${UCODE_VERSION_DETAIL}"
echo ${VERSION_CONTROL_CFLAGS}
