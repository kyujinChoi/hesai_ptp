#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <linux/net_tstamp.h> 


bool isHwTimestampEnabled(const std::string &interface)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return false;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ);

    struct ethtool_ts_info info;
    memset(&info, 0, sizeof(info));
    info.cmd = ETHTOOL_GET_TS_INFO;
    ifr.ifr_data = reinterpret_cast<char *>(&info);

    if (ioctl(sock, SIOCETHTOOL, &ifr) < 0)
    {
        perror("ioctl");
        close(sock);
        return false;
    }

    close(sock);
    // 하드웨어 타임스탬프가 사용 가능한지 확인
    return (info.so_timestamping & SOF_TIMESTAMPING_TX_HARDWARE) ||
           (info.so_timestamping & SOF_TIMESTAMPING_RX_HARDWARE);
}
std::vector<std::string> getPTPcandidates()
{
    std::vector<std::string> candidates;
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        exit(0);
    }

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == nullptr)
        {
            continue;
        }

        if (ifa->ifa_addr->sa_family == AF_PACKET)
        {
            std::string iface_name(ifa->ifa_name);
            if (isHwTimestampEnabled(iface_name))
            {
                candidates.push_back(iface_name);
                std::cout << "Interface with HW Timestamp: " << iface_name << std::endl;
            }
        }
    }

    freeifaddrs(ifaddr);
    return candidates;
}
int runPTP(std::string interface, std::string fn)
{
    std::string pkillptp4l = "sudo pkill -9 -f ptp4l";
    int result = system(pkillptp4l.c_str());
    std::string runPTP = "sudo ptp4l -f " + fn + " -m -l 5 -i " + interface;
    result = system(runPTP.c_str());
    return result;
}
int main(int argc, char **argv)
{
    if (geteuid() != 0) {
        std::cerr << "This program must be run as root (sudo). terminate" << std::endl;
        return 1;
    }
    std::string fn;
    std::vector<std::string> interfaces = getPTPcandidates();
    if(interfaces.size() > 1)
    {
        std::cout << "There are more than one interface with HW Timestamp" << std::endl;
        for(int i = 0; i < interfaces.size(); i++)
            std::cout << interfaces[i] << std::endl;
        std::cout << "terminate\n";
        exit(0);
    }
    
    if(argc > 1)
        fn.assign(argv[1]);
    else
        fn.assign("../default.conf");

    runPTP(interfaces[0], fn);
    return 0;
}
