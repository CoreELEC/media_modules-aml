// Must match the headers in the mediahal/mediaproxy directory
#define MAX_AFD_LEN (64)
enum media_message_type {
    MEDIA_VDEC_CONNECTED          = (1 << 0),  // 0001
    MEDIA_VIDEO_FRIST_OUT_FRAME_INFO   = (1 << 1),  // 0010
    MEDIA_VIDEO_INPUT_INFO       = (1 << 2),  // 0100
    MEDIA_VIDEO_OUTPUT_INFO      = (1 << 3),  // 1000
    MEDIA_VIDEO_STATISTIC_INFO   = (1 << 4)   // 10000
};

enum block_type {
    READ_DATA_BLOCK,
    READ_DATA_NON_BLOCK,
};

struct resolution {
    uint32_t width;
    uint32_t height;
};

struct HDRInfo {
    uint32_t color_primaries;
    uint32_t matrix_coeff;
};

struct videoFirstFrameInfo {
    uint32_t decoderId;   /* id of current instance */
    struct resolution res;
    struct HDRInfo hdr;
    char afd_data[MAX_AFD_LEN];
};

struct vdecConnectedInfo {
    uint32_t decoderId;
    bool vdecConnect;
};

struct videoInputInfo {
    uint32_t decoderId;
    uint64_t bitStreamId;
    uint64_t timestamp;
    uint32_t dataSize;
};

struct videoOutputInfo {
    uint32_t decoderId;
    uint64_t bitStreamId;
    uint64_t timestamp;
    uint32_t pictureBufferId;
};

struct videoStatisticsInfo {
    uint32_t decoderId;
    uint32_t decodedFrames;
    uint32_t errFrames;
    uint32_t dropFrames;
};

struct AmlVideoUserdata {
    uint32_t messageType;
    uint32_t dataSize;
    union {
        struct vdecConnectedInfo connInfo;
        struct videoFirstFrameInfo firstFrameInfo;
        struct videoInputInfo inputInfo;
        struct videoOutputInfo outputInfo;
        struct videoStatisticsInfo statisticsInfo;
    } data;
};

