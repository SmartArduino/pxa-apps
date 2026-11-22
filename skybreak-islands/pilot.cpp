#include "pilot.hpp"

namespace sky {
namespace {

struct SuitColors { Color front, side, top; };
constexpr SuitColors suit{rgb(0xd98141),rgb(0x945131),rgb(0xf1b16d)};
constexpr SuitColors harness{rgb(0x365563),rgb(0x243640),rgb(0x6b838b)};
constexpr SuitColors trousers{rgb(0x465a71),rgb(0x344353),rgb(0x62778c)};
constexpr SuitColors boots{rgb(0x302e2d),rgb(0x21242a),rgb(0x635145)};
constexpr SuitColors helmet{rgb(0xebe0bc),rgb(0xb9b6a1),rgb(0xffedcd)};
constexpr SuitColors visor{rgb(0x204b57),rgb(0x1a3541),rgb(0x46868b)};
constexpr SuitColors straps{rgb(0x8f9e9c),rgb(0x556864),rgb(0xb6b99c)};
constexpr SuitColors buckle{rgb(0xf1cd89),rgb(0xac7849),rgb(0xffe9b7)};
constexpr SuitColors glove{rgb(0x493e3a),rgb(0x262b2e),rgb(0x695947)};

struct Model {
    Scene& scene;
    Vec pos;
    Basis pose;

    Vec at(Vec v) const { return pos+pose.apply(v); }
    void quad(Vec a,Vec b,Vec c,Vec d,Color c0) const {
        scene.quad(at(a),at(b),at(c),at(d),c0);
    }
    void box(Vec lo,Vec hi,SuitColors color) const {
        const Vec a{lo.x,lo.y,lo.z},b{hi.x,lo.y,lo.z},c{hi.x,hi.y,lo.z},d{lo.x,hi.y,lo.z};
        const Vec e{lo.x,lo.y,hi.z},f{hi.x,lo.y,hi.z},g{hi.x,hi.y,hi.z},h{lo.x,hi.y,hi.z};
        quad(a,b,c,d,color.front);
        quad(e,f,g,h,color.front);
        quad(a,e,h,d,color.side);
        quad(b,f,g,c,color.side);
        quad(c,d,h,g,color.top);
        quad(a,b,f,e,color.side);
    }
    void limb(Vec a,Vec b,float radius,SuitColors color) const {
        const Vec axis=b-a;
        const Vec side=unit({-axis.y,axis.x,.025f})*radius;
        const Vec thick{0,0,radius*.79f};
        quad(a-side-thick,b-side-thick,b+side-thick,a+side-thick,color.front);
        quad(a-side+thick,b-side+thick,b+side+thick,a+side+thick,color.top);
        quad(a-side-thick,b-side-thick,b-side+thick,a-side+thick,color.side);
        quad(a+side-thick,b+side-thick,b+side+thick,a+side+thick,color.side);
    }
};

void canopy(const Model& model,const Game& game,const std::array<Vec,2>& hands) {
    constexpr int sectors=12;
    constexpr std::array<float,sectors> circle_x{0,.50f,.866f,1,.866f,.50f,0,-.50f,-.866f,-1,-.866f,-.50f};
    constexpr std::array<float,sectors> circle_z{1,.866f,.50f,0,-.50f,-.866f,-1,-.866f,-.50f,0,.50f,.866f};
    constexpr float radius[]{1,.94f,.69f,.10f};
    constexpr float height[]{15,18.6f,21.8f,23.1f};
    const float open=saturate(game.phase_time*2.45f);
    const float spread=std::max(.12f,1-(1-open)*(1-open));
    const float flutter=std::sin(game.phase_time*8.4f)*std::max(0.f,1-game.phase_time*.85f);
    const float tilt=game.pilot_lean*1.2f;
    std::array<std::array<Vec,sectors>,4> arc;
    for (int row=0;row<4;++row) for (int i=0;i<sectors;++i) {
        const float x=circle_x[i], z=circle_z[i];
        const float scallop=row<2 && i%2 ? (row==0 ? .75f : .22f)*spread : 0.f;
        arc[row][i]={x*radius[row]*11.7f*spread+tilt,
                     4.1f+(height[row]-4.1f)*spread+x*tilt-scallop+(row==0 ? flutter*z*.42f : 0.f),
                     z*radius[row]*9.5f*spread+flutter*.8f};
    }
    for (int i=0;i<sectors;++i) {
        const int next=(i+1)%sectors;
        if ((i&1)==0) {
            const Vec hand=hands[circle_x[i]<-.05f ? 0 : 1];
            model.scene.stroke(model.at(hand),model.at(arc[0][i]),rgb(0xe0e2c7));
        }
        const Vec radial=model.pose.right*circle_x[i]+model.pose.front*circle_z[i];
        const int shade=std::clamp(int(8+13*(radial.x*shadow_x+radial.z*shadow_z)),3,15);
        const Color cloth=i&1 ? rgb(0xf5e3c4) : rgb(0xee914e);
        for (int row=0;row<3;++row)
            model.quad(arc[row][i],arc[row][next],arc[row+1][next],arc[row+1][i],
                       blend(cloth,rgb(0x784c36),shade+(2-row)*3));
        if (open>.72f) {
            model.scene.stroke(model.at(arc[0][i]),model.at(arc[0][next]),
                               blend(cloth,rgb(0x704636),16));
            if (!(i&1)) {
                const Vec lift{0,.11f,0};
                model.scene.stroke(model.at(arc[1][i]+lift),model.at(arc[2][i]+lift),
                                   blend(cloth,rgb(0xffe3b3),13));
            }
        }
    }
}

} // namespace

void draw_pilot(Scene& scene,const Game& game) {
    const bool ejected=game.phase==Phase::ejection, parachute=game.phase==Phase::parachute;
    const bool boarding=game.phase==Phase::pickup;
    const float time=game.phase_time;
    const float brace=ejected ? saturate(time*2.15f) : 0.f;
    const float climb=boarding ? saturate(time/.55f) : 0.f;
    const float sway=std::sin(game.elapsed*4.2f);
    const float pitch=ejected ? mix(-.54f,.12f,brace) : boarding ? .35f*climb : .04f;
    const float roll=parachute ? game.pilot_lean*.25f+sway*.10f : ejected ? .16f-.10f*brace : .07f;
    const float crouch=boarding ? climb*.62f : 0.f;
    const Model model{scene,game.pilot-Vec{0,crouch,0},attitude(game.pilot_yaw,pitch,roll)};
    model.box({-1.38f,-.6f,-.91f},{1.38f,2.72f,1.06f},suit);
    model.box({-1.22f,-.46f,-1.36f},{1.22f,1.84f,-.64f},harness);
    const bool details=scene.camera.scale_at(std::max(Scene::near,scene.camera.local(game.pilot).z))>1.35f;
    if (details) {
        for (float side:{-1.f,1.f}) {
            const float x=side*.79f;
            model.box({x-.16f,-.40f,-1.42f},{x+.16f,2.58f,-1.28f},straps);
            model.box({x-.16f,-.32f,1.05f},{x+.16f,2.57f,1.16f},harness);
        }
        model.box({-.87f,-.39f,-1.46f},{.87f,-.14f,-1.33f},harness);
        model.box({-.22f,-.40f,-1.49f},{.22f,-.13f,-1.44f},buckle);
    }
    model.box({-1.12f,2.83f,-.92f},{1.12f,4.43f,1.14f},helmet);
    model.box({-.82f,4.42f,-.70f},{.88f,4.92f,.80f},helmet);
    model.box({-.85f,3.13f,1.08f},{.88f,4.07f,1.27f},visor);
    if (details) {
        model.box({-.67f,3.87f,1.26f},{.53f,4.03f,1.30f},straps);
        model.box({-.13f,4.44f,-.94f},{.13f,4.80f,-.72f},suit);
        model.box({-1.22f,3.21f,-.22f},{-1.11f,3.95f,.48f},harness);
        model.box({1.11f,3.21f,-.22f},{1.22f,3.95f,.48f},harness);
    }
    std::array<Vec,2> hands;
    for (int i=0;i<2;++i) {
        const float side=i==0 ? -1.f : 1.f;
        const float stride=side*sway;
        const Vec shoulder{side*1.34f,2.33f,-.04f},hip{side*.68f,-.38f,.05f};
        Vec elbow,wrist,knee,foot;
        if (parachute) {
            elbow={side*(2.23f+.18f*stride),2.90f,.1f};
            wrist={side*2.05f,4.24f+game.pilot_lean*side*.48f,.24f};
            knee={side*.87f,-1.92f,.59f+stride*.39f};
            foot={side*.94f,-3.45f,-.13f+stride*.82f};
        } else if (ejected) {
            elbow={side*mix(1.64f,2.88f,brace),mix(1.13f,2.34f,brace),.31f};
            wrist={side*mix(.83f,3.43f,brace),mix(1.82f,1.10f,brace),mix(.68f,-.21f,brace)};
            knee={side*.96f,mix(-1.18f,-2.11f,brace),mix(1.67f,.55f,brace)};
            foot={side*1.10f,mix(-2.54f,-3.51f,brace),mix(1.12f,-.31f,brace)};
        } else {
            elbow={side*mix(2.10f,1.43f,climb),1.15f,.42f+climb};
            wrist={side*mix(2.47f,.92f,climb),.28f+climb*.85f,.53f+climb*1.23f};
            knee={side*.82f,-1.84f+.39f*climb,.32f+climb*.92f};
            foot={side*.93f,-3.25f+1.20f*climb,-.06f+climb*.55f};
        }
        const Vec pad{.32f,.38f,.35f};
        model.limb(shoulder,elbow,.41f,suit);
        model.limb(elbow,wrist,.35f,suit);
        model.box(wrist-pad,wrist+pad,glove);
        model.limb(hip,knee,.48f,trousers);
        model.limb(knee,foot,.40f,trousers);
        if (details) model.box(knee-Vec{.28f,.37f,-.20f},knee+Vec{.28f,.26f,.43f},straps);
        model.box(foot-Vec{.43f,.39f,.37f},foot+Vec{.43f,.22f,.67f},boots);
        hands[i]=wrist;
    }
    if (parachute) canopy(model,game,hands);
}

} // namespace sky
