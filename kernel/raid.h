#define RAID_ACTIVE 772024
#define DISK_BROKEN 773024
#define NO_RAID -1
#define LOCKED -69

enum func {RW, SPEC};

struct raid_header {
    int state;
    enum RAID_TYPE type;
    int disk_broken;
    int max_accessed;
};