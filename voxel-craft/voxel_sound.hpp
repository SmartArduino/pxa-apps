#pragma once
#include "voxel_items.hpp"
#include <cmath>
namespace voxel {
enum class SoundMaterial : std::uint8_t { foliage,earth,granular,snow,wood,stone,glass,wool };
inline constexpr SoundMaterial sound_material(std::uint8_t item)noexcept {
    switch(item){
    case kLeaves:case kBush:case kFlower:case kCactus:return SoundMaterial::foliage;
    case kGrass:case kDirt:return SoundMaterial::earth;
    case kSand:case kGravel:return SoundMaterial::granular;
    case kSnow:return SoundMaterial::snow;
    case kWood:case kPlank:case kTable:return SoundMaterial::wood;
    case kGlass:return SoundMaterial::glass;
    case kWool:return SoundMaterial::wool;
    default:return item>=kStick?SoundMaterial::wood:SoundMaterial::stone;
    }
}
// Two damped resonances and low-pass contact noise. Generate one Host PCM
// packet at a time; no resident sample bank, DSP cache or per-sample trig.
class ContactSound {
    struct Profile {float hz1,hz2,lowpass,noise,amplitude;};
    static constexpr Profile profiles[]={{220,880,.075f,.95f,1500},
        {110,290,.10f,.65f,3600},{300,670,.20f,.82f,2400},
        {180,420,.06f,.85f,2000},{240,520,.12f,.32f,3200},
        {620,1430,.16f,.35f,2800},{1820,2870,.14f,.05f,2100},
        {90,220,.04f,.85f,1800}};
    Profile profile_;std::uint32_t random_;unsigned position_=0,length_;
    float first_=0,first_old_=0,second_=0,second_old_=0,c1_,c2_,filter_=0,envelope_=1,gain_;
public:
    static constexpr unsigned sample_rate=16000,packet_samples=320;
    ContactSound(std::uint8_t material,unsigned action,std::uint32_t seed)noexcept
        :profile_(profiles[unsigned(sound_material(material))]),random_(seed?seed:1),
         // One indivisible packet leaves the existing four-packet voice
         // queue room for a hit, break and next contact in the same tick.
         length_((action==0?12:action==1?20:16)*16),
         gain_(3*profile_.amplitude*(action==0?.55f:action==2?.75f:1.f)) {
        const float variation=.96f+.02f*(seed&3);
        const float w1=6.283185307f*profile_.hz1*variation/sample_rate;
        const float w2=6.283185307f*profile_.hz2*variation/sample_rate;
        c1_=2*.996f*std::cos(w1);c2_=2*.992f*std::cos(w2);
        first_=std::sin(w1);second_=std::sin(w2);
    }
    unsigned remaining()const noexcept{return length_-position_;}
    unsigned render(std::span<std::int16_t> output)noexcept {
        const unsigned count=std::min<unsigned>(output.size(),remaining());
        for(unsigned i=0;i<count;++i,++position_){
            random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;
            const float noise=float(std::int32_t(random_>>16)-32768)/32768;
            filter_+=profile_.lowpass*(noise-filter_);
            const float next1=c1_*first_-.996f*.996f*first_old_;
            const float next2=c2_*second_-.992f*.992f*second_old_;
            first_old_=first_;first_=next1;second_old_=second_;second_=next2;
            const float attack=std::min(1.f,float(position_)/32);
            const float release=std::min(1.f,float(length_-position_-1)/48);
            const float contact=profile_.noise*filter_+(1-profile_.noise)*(.72f*first_+.28f*second_);
            output[i]=std::int16_t(contact*gain_*envelope_*attack*release);
            envelope_*=position_<length_/3?.9985f:.994f;
        }
        return count;
    }
};
static_assert(sizeof(ContactSound)<=80);
} // namespace voxel
