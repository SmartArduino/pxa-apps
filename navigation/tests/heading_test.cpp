// SPDX-License-Identifier: MIT
#include "../heading.hpp"
#include <cassert>
#include <iostream>
using navigation::Heading;
using navigation::HeadingFusion;
static bool near(float value, float wanted, float tolerance=1.f) {
    return std::abs(compass::delta(value,wanted)) < tolerance;
}
static void magnetic(Heading& heading, float bearing, std::uint64_t stamp) {
    heading.model.now=stamp;
    const float radians=bearing/compass::DEG;
    heading.magnetic({int(-24000*std::sin(radians)),int(24000*std::cos(radians)),-38000},stamp,stamp);
    heading.acceleration({0,0,9806650},stamp,stamp);
    heading.update(stamp);
}
int main() {
    Heading heading;
    heading.model.status=compass::Status::Streaming;
    heading.model.system_heading=true;
    heading.model.now=1000000;
    heading.model.heading_sample(359000000,1000000);
    heading.update(1000000);
    assert(heading.visible&&near(heading.shown,359));
    heading.model.now=1100000;
    heading.model.heading_sample(1000000,1100000);
    heading.update(1100000);
    assert(heading.visible&&near(heading.shown,0,3));
    heading.model.heading_sample(-1,1100001);
    assert(heading.model.heading_stamp==1100000);
    heading.update(2600000);
    assert(!heading.visible);
    assert(!heading.accept_time(3000001,3000000));
    assert(!heading.accept_time(1000000,3000000));

    heading={};
    heading.model.status=compass::Status::Streaming;
    magnetic(heading,0,1000000);
    assert(!heading.visible&&heading.model.quality==compass::Quality::Calibrate);
    heading.model.saved[0]={{},std::sqrt(24.f*24+38.f*38),0,0,true};
    magnetic(heading,0,1100000);
    assert(heading.visible&&near(heading.shown,0));
    for(unsigned i=1;i<=20;++i)magnetic(heading,90,1100000+i*100000);
    assert(near(heading.shown,90,2)&&heading.cardinal()=="东");

    // Positive device-frame Z is counterclockwise, so clockwise heading falls.
    HeadingFusion fusion;
    fusion.absolute({0,24,-38},{0,0,1},1000000);
    assert(fusion.angular({0,0,compass::PI/2},1100000));
    assert(near(fusion.bearing(),351,.01));
    assert(!fusion.angular({0,0,compass::PI/2},1100000));
    // An older absolute observation is corrected in the newer predicted pose.
    fusion.absolute({0,24,-38},{0,0,1},1050000);
    assert(fusion.pose_time==1100000&&near(fusion.bearing(),352.12,.1));
    assert(!fusion.angular({0,0,1},1400000)&&!fusion.valid);
    fusion.absolute({-24,0,-38},{0,0,1},1500000);
    assert(near(fusion.bearing(),90,.01));
    assert(!fusion.angular({0,0,41},1510000));
    const auto v=HeadingFusion::rotate_body({0,0,1},{compass::PI/2,0,0},1);
    assert(std::abs(v.y-1)<.001&&std::abs(v.z)<.001);

    // Gyroscope traffic never extends the freshness of the magnetic reference.
    heading={};heading.model.status=compass::Status::Streaming;
    heading.model.saved[0]={{},std::sqrt(24.f*24+38.f*38),0,0,true};
    magnetic(heading,0,1000000);
    for(unsigned i=1;i<=16;++i){
        const auto stamp=1000000+i*100000;
        heading.angular({0,0,-1570796},stamp,stamp);
        heading.update(stamp);
    }
    assert(!heading.visible&&!heading.fusion.valid);
    heading.suspend();assert(!heading.visible&&heading.display_time==0);
    Heading shifted;shifted.model.status=compass::Status::Streaming;
    shifted.model.declination=15;
    shifted.model.saved[0]={{},std::sqrt(24.f*24+38.f*38),0,0,true};
    magnetic(shifted,0,1000000);assert(near(shifted.shown,15,.01));
    shifted.angular({0,0,-1570796},1100000,1100000);shifted.update(1100000);
    assert(near(shifted.shown,20.689,.1));
    HeadingFusion vertical;vertical.absolute({0,0,38},{0,0,1},1000000);
    assert(!vertical.readable());
    std::uint8_t saved[compass::SAVE_SIZE];
    assert(compass::encode(heading.model,saved,sizeof(saved))==sizeof(saved));
    compass::Model restored;assert(compass::decode(restored,saved,sizeof(saved)));
    assert(compass::valid_cal(restored.saved[0]));
    saved[20]^=1;assert(!compass::decode(restored,saved,sizeof(saved)));
    std::cout<<"PASS: absolute heading, wraparound, calibration, gyro frame/sign, delayed corrections, stale reference and persistence\n";
}
