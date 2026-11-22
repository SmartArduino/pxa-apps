#pragma once

namespace tide {

struct HarborCharter {
    int port, rent, trust;
    Language language;
    Text authority, advantage;
};

inline constexpr std::array HarborCharters{
    HarborCharter{lisbon,180,0,Portuguese,"葡萄牙王国","塔霍河口离大西洋航线近，适合往返本土与马德拉。"},
    HarborCharter{madeira,150,0,Portuguese,"葡萄牙王国","丰沙尔靠近前往加那利和佛得角的航路，适合把备船留在岛上。"},
    HarborCharter{cadiz,200,1,Castilian,"卡斯蒂利亚王冠","湾内有泊地，往西去群岛，往东可过直布罗陀。"},
    HarborCharter{tunis,195,1,Arabic,"哈夫斯王朝","西地中海与东地中海之间的商港。谈妥泊位后可换船补给。"},
    HarborCharter{malindi,170,2,Swahili,"马林迪城邦","当地领航员熟悉季风。马林迪与蒙巴萨各有自己的商路和盟友。"},
    HarborCharter{calicut,260,2,Malayalam,"卡利卡特 · 扎莫林","胡椒与棉布在此集散。泊位由当地经手，来船仍要遵守港规。"},
};

inline constexpr const HarborCharter* harbor_charter(int port) {
    for (const auto& charter : HarborCharters)
        if (charter.port==port) return &charter;
    return nullptr;
}

inline constexpr Text HullNames[]{"轻帆艇","远洋卡拉维尔","三桅商船"};
inline constexpr int BerthVesselPrices[]{480,1050,2200};

// A signed lease has one laid-up vessel and may house sailors between voyages.
struct MerchantBerth {
    std::uint8_t port=0; // Port index plus one.
    std::uint8_t vessel=0; // Hull index plus one.
    std::uint8_t hull=0, guns=0, sailors=0;
};

} // namespace tide
