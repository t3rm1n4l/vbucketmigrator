#include <vector>
#include <map>

typedef struct _vbcount {
    uint32_t rcvd;
    uint32_t sent;
}VbCount;

class VbStats {

public:
    void init_stats(std::vector<uint16_t> &buckets);
    void update_sent_stats(uint16_t vb);
    void update_rcvd_stats(uint16_t vb);
    std::string get_stats_str();
    static VbStats * instance();

private:
    VbStats(){}
    VbStats(VbStats const &){}
    VbStats& operator =(VbStats const&){}

    std::string dest;
    std::map<uint16_t, VbCount> vbstats;

    static VbStats* stats;
};


void * stats_thread (void *arg);
