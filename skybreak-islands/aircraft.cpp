#include "aircraft.hpp"

namespace sky {
namespace {

constexpr Color ink=rgb(0x202e3a);
constexpr Vec sunlight=Vec{-.34f,.85f,-.25f};

struct Wing {
    Vec root,elbow,tip,aft,rear;
};

Wing wing(const AirframeSpec& model,float side,bool broken=false) {
    const float shoulder=model.nose*.43f;
    Wing shape{{side*model.body*.66f,0,shoulder},
            {side*model.span*.65f,0,mix(shoulder,model.sweep,.68f)},
            {side*model.span,0,model.sweep},
            {side*model.span,0,model.sweep-model.tip_chord},
            {side*model.body*.72f,0,model.rear}};
    if (broken) {
        shape.tip=shape.elbow+(shape.tip-shape.elbow)*.27f;
        shape.aft=shape.rear+(shape.aft-shape.rear)*.73f;
    }
    return shape;
}

struct Mesh {
    Scene& scene;
    Vec position;
    Basis pose;
    Vec point(Vec v) const { return position+pose.apply(v); }
    void tri(Vec a,Vec b,Vec c,Color color) const {
        scene.triangle_local(point(a),point(b),point(c),color);
    }
    void quad(Vec a,Vec b,Vec c,Vec d,Color color) const {
        const auto first=point(a),third=point(c);
        scene.triangle_local(first,point(b),third,color);
        scene.triangle_local(first,third,point(d),color);
    }
};

struct Hinge {
    Vec origin,axis;
    float sine,cosine;
    Hinge(Vec start,Vec end,float angle)
        : origin(start),axis(unit(end-start)),sine(std::sin(angle)),cosine(std::cos(angle)) {}

    Vec moved(Vec point) const {
        const Vec d=point-origin;
        const Vec perpendicular{axis.y*d.z-axis.z*d.y,axis.z*d.x-axis.x*d.z,axis.x*d.y-axis.y*d.x};
        return origin+d*cosine+perpendicular*sine+axis*(dot(axis,d)*(1-cosine));
    }
};

void exhaust_shimmer(const Mesh& mesh,Vec nozzle,float time) {
    const auto& camera=mesh.scene.camera;
    const Vec first=mesh.point(nozzle),last=mesh.point(nozzle+Vec{0,0,-14});
    if (first.z<Scene::near || last.z<Scene::near) return;
    const Vec start=camera.project(first),end=camera.project(last);
    const Vec direction=end-start;
    const float length=direction.x*direction.x+direction.y*direction.y;
    if (length<3) return;
    const float inverse_length=1.f/length;
    const float radius=std::clamp(camera.scale_at(first.z)*1.7f,1.4f,4.f);
    auto& d=mesh.scene.out;
    const int x0=std::max(1,int(std::min(start.x,end.x)-radius));
    const int x1=std::min(d.width-2,int(std::max(start.x,end.x)+radius));
    const int y0=std::max(0,int(std::min(start.y,end.y)-radius));
    const int y1=std::min(d.height-1,int(std::max(start.y,end.y)+radius));
    const int signature=int(time*60);
    for (int y=y0;y<=y1;++y) {
        auto* row=d.pixels+y*d.stride;
        const int shift=((signature+y/2)&2) ? -1 : 1;
        for (int x=x0;x<=x1;++x) {
            const float dx=x+.5f-start.x,dy=y+.5f-start.y;
            const float t=(dx*direction.x+dy*direction.y)*inverse_length;
            if (t<0 || t>1) continue;
            const float ax=dx-direction.x*t,ay=dy-direction.y*t;
            if (ax*ax+ay*ay>radius*radius*(.47f+t)) continue;
            const int depth=int(mix(first.z,last.z,t)*8)+3;
            const auto* distances=d.depth+y*d.width;
            if (distances[x]>depth && distances[x+shift]>depth)
                row[x]=blend(row[x],row[x+shift],int(16-9*t));
        }
    }
}

void wing_detail(const Mesh& mesh,const Wing& w,Color top,Color edge,Color accent,float deflection,
                 Color lamp,bool broken) {
    const Vec hinge=(w.rear+(w.aft-w.rear)*.39f);
    const Vec inner=hinge+Vec{0,.06f,1.5f},outer=w.aft+Vec{0,.06f,1.24f};
    mesh.quad(w.root,w.elbow,outer,inner,top);
    mesh.tri(w.elbow,w.tip,outer,top);
    mesh.tri(w.root,inner,w.rear,top);
    mesh.tri(w.rear,inner,hinge,top);
    const auto axis=Hinge(inner,outer,deflection);
    const Vec tail=axis.moved(hinge+Vec{0,0,-.16f}),tip=axis.moved(w.aft+Vec{0,0,-.14f});
    mesh.quad(inner,outer,tip,tail,blend(top,edge,14));
    mesh.quad(tail,tip,tip-Vec{0,.65f,0},tail-Vec{0,.65f,0},ink);
    const Vec seam=Vec{0,.055f,0};
    mesh.quad(inner+seam,outer+seam,outer+(tip-outer)*.12f+seam,inner+(tail-inner)*.12f+seam,edge);
    mesh.quad(w.elbow,w.tip,w.tip-Vec{0,.71f,0},w.elbow-Vec{0,.72f,0},edge);
    mesh.tri(w.elbow+Vec{0,.12f,0},w.tip+Vec{0,.12f,0},outer+Vec{0,.08f,0},accent);
    const Vec mid=w.elbow+(outer-w.elbow)*.40f+Vec{0,.15f,0};
    mesh.quad(mid-Vec{0,0,.5f},mid+Vec{0,0,.7f},
              mid+(w.root-w.elbow)*.45f+Vec{0,0,.7f},mid+(w.root-w.elbow)*.45f-Vec{0,0,.5f},
              blend(top,edge,18));
    const float side=w.tip.x<0 ? -1.f : 1.f;
    if (broken) {
        const Vec torn=w.tip+(w.aft-w.tip)*.54f;
        mesh.tri(w.tip,torn,torn+Vec{side*.49f,-.7f,.18f},rgb(0x777063));
        mesh.tri(torn,w.aft,torn+Vec{side*.20f,-.63f,-.3f},ink);
        return;
    }
    mesh.quad(w.tip+Vec{0,.16f,-.3f},w.tip+Vec{-side*1.14f,.16f,-.3f},
              w.tip+Vec{-side*1.1f,.16f,-1.4f},w.tip+Vec{0,.16f,-1.4f},lamp);
    mesh.quad(w.tip+Vec{side*.05f,0,-.25f},w.tip+Vec{side*.05f,0,-1.42f},
              w.tip+Vec{side*.05f,-.65f,-1.42f},w.tip+Vec{side*.05f,-.65f,-.25f},lamp);
}

void wing_scars(const Mesh& mesh,const Wing& w,int hits,Color top) {
    if (!hits) return;
    const Vec across=w.elbow-w.root;
    const Vec aft=w.rear+(w.aft-w.rear)*.44f;
    const Vec front=w.root+across*.55f;
    const Vec center=front+(aft-front)*.42f+Vec{0,.28f,0};
    const float scale=.8f+hits*.24f;
    const Vec side{scale*1.8f,0,0};
    const Vec depth{0,0,scale*1.4f};
    mesh.quad(center-side,center+depth,center+side,center-depth,blend(top,ink,23));
    mesh.tri(center-side*.48f,center+side*.58f,center-depth*.69f,rgb(0x342f2d));
    const Vec chip=center+depth*.54f+Vec{0,.07f,0};
    mesh.tri(chip,chip+side*.42f,chip+depth*.52f,rgb(0x9e7e5b));
}

void wing_hardware(const Mesh& mesh,const Wing& w,const AirframeSpec& model,
                   Color top,Color edge,Color accent) {
    const Vec inner=w.root+(w.elbow-w.root)*.44f;
    const Vec rear=w.rear+(w.aft-w.rear)*.36f;
    const Vec ridge=inner+(rear-inner)*.49f+Vec{0,.23f,0};
    const Vec spine=rear-inner;
    const float side=w.tip.x>0 ? 1.f : -1.f;
    constexpr Vec up{0,.7f,0};
    const Vec bar{side*1.14f,0,0};
    mesh.quad(ridge,ridge+bar,ridge+bar+spine*.23f,ridge+spine*.23f,blend(top,edge,19));
    mesh.tri(ridge+bar*.25f+spine*.25f,ridge+bar*.82f+spine*.25f,ridge+bar*.56f+spine*.40f,accent);
    const Vec inner_hinge=w.rear+(w.aft-w.rear)*.36f;
    const Vec outer_hinge=w.rear+(w.aft-w.rear)*.79f;
    const auto skin=[&](float across,float chord) {
        const Vec leading=w.root+(w.elbow-w.root)*across;
        const Vec trailing=inner_hinge+(outer_hinge-inner_hinge)*across;
        return leading+(trailing-leading)*chord+Vec{0,.11f,0};
    };
    mesh.quad(skin(.17f,.14f),skin(.72f,.14f),skin(.76f,.66f),skin(.22f,.66f),blend(top,edge,7));
    mesh.quad(skin(.21f,.64f),skin(.76f,.64f),skin(.76f,.70f),skin(.22f,.70f),blend(top,edge,16));

    const Vec rail=w.root+(w.tip-w.root)*.63f+Vec{0,-1.37f,-2.55f};
    const float length=model.nacelle>3 ? 3.2f : 2.45f;
    const Vec start=rail+Vec{0,0,length},end=rail-Vec{0,0,length};
    const Vec edge_rail{side*.69f,0,0};
    mesh.quad(start-edge_rail,start+edge_rail,end+edge_rail,end-edge_rail,edge);
    mesh.quad(start+edge_rail,end+edge_rail,end+up,start+up,top);
    mesh.tri(start-edge_rail,start+edge_rail,start+Vec{0,.37f,1.45f},accent);
    mesh.tri(end-edge_rail,end+edge_rail,end+up,ink);
}

void stabilizer(const Mesh& mesh,const AirframeSpec& model,float side,float elevator,
                Color top,Color edge,Color accent) {
    const float rear=model.tail;
    const Vec root{side*model.body*.62f,.42f,rear+3.4f};
    const Vec tip{side*model.span*(model.twin_tail ? .54f : .39f),.27f,rear+.8f};
    const Vec aft=tip+Vec{0,0,-3.1f};
    const Vec joint{root.x,.33f,rear-1.33f};
    const Vec inner=root+(joint-root)*.42f,outer=tip+(aft-tip)*.37f;
    const auto axis=Hinge(inner,outer,side*elevator*.59f);
    const Vec inside=axis.moved(joint),outside=axis.moved(aft);
    mesh.quad(root,tip,outer,inner,blend(top,edge,7));
    mesh.quad(inner,outer,outside,inside,blend(top,edge,19));
    mesh.quad(inside,outside,outside-Vec{0,.44f,0},inside-Vec{0,.44f,0},edge);
    mesh.tri(outer+Vec{0,.09f,0},outside+Vec{0,.09f,0},
             inside+(outside-inside)*.66f+Vec{0,.09f,.15f},accent);
}

void engine(const Mesh& mesh,float x,const AirframeSpec& model,Color shell,Color panel,float heat,
            float time,bool lit,bool shimmer) {
    const float end=model.tail,front=model.rear+9.f,r=model.nozzle;
    const float y=-.52f;
    const Vec a{x-r,.58f,front}, b{x+r,.58f,front};
    const Vec c{x+r,.76f,end+1.5f}, d{x-r,.76f,end+1.5f};
    mesh.quad(a,b,c,d,panel);
    const Vec drop{0,1.95f,0};
    mesh.quad(a,d,d-drop,a-drop,shell);
    mesh.quad(b,c,c-drop,b-drop,shell);
    mesh.quad(a,b,b-drop,a-drop,ink);
    mesh.quad(a+Vec{r*.21f,-.28f,.03f},b+Vec{-r*.21f,-.28f,.03f},
              b+Vec{-r*.21f,-1.31f,.03f},a+Vec{r*.21f,-1.31f,.03f},rgb(0x324859));
    constexpr float circle_x[]{0,.71f,1,.71f,0,-.71f,-1,-.71f};
    constexpr float circle_y[]{.95f,.67f,0,-.67f,-.95f,-.67f,0,.67f};
    const float nozzle_open=.97f+heat*.18f;
    const auto ring=[&](int i,float size,float z) {
        return Vec{x+circle_x[i]*r*size*nozzle_open,y+circle_y[i]*r*size*nozzle_open,z};
    };
    const Color nozzle=rgb(0x84959a);
    const Color flame=lit ? blend(rgb(0x493b37),rgb(0xffcf85),int(heat*32)) : ink;
    const int stride=mesh.scene.out.width<=150 ? 2 : 1;
    for (int i=0;i<8;i+=stride) {
        const int j=(i+stride)&7;
        mesh.quad(ring(i,1.05f,end+.9f),ring(j,1.05f,end+.9f),
                  ring(j,.90f,end),ring(i,.90f,end),blend(nozzle,ink,(i&1) ? 13 : 5));
        mesh.quad(ring(i,.90f,end),ring(j,.90f,end),
                  ring(j,.66f,end-.14f),ring(i,.66f,end-.14f),ink);
        mesh.tri(ring(i,.66f,end-.14f),ring(j,.66f,end-.14f),{x,y,end-.24f},flame);
    }
    if (!lit) return;
    if (heat<.14f) {
        if (shimmer) exhaust_shimmer(mesh,{x,y,end-.8f},time);
        return;
    }
    const float long_flame=heat*(17.f+2.1f*std::sin(time*38));
    const float width=.49f+heat*.91f;
    const Color trail=blend(rgb(0x926b9e),rgb(0xfa8147),int(heat*32));
    const Vec point{x,y,end-long_flame};
    mesh.tri({x-width,y,end-.2f},{x+width,y,end-.2f},point,trail);
    mesh.tri({x,y-width,end-.2f},{x,y+width,end-.2f},point,blend(trail,flame,12));
    if (heat>.30f)
        mesh.tri({x-width*.44f,y,end-.35f},{x+width*.44f,y,end-.35f},{x,y,end-long_flame*.67f},
                 rgb(0xffedb7));
}

void canopy_well(const Mesh& mesh,float width) {
    mesh.quad({-width,2.55f,-3},{width,2.55f,-3},{width*.94f,2.68f,4.6f},{-width*.94f,2.68f,4.6f},ink);
    mesh.quad({-width*.5f,2.59f,-1.7f},{width*.5f,2.59f,-1.7f},
              {width*.5f,2.66f,2.2f},{-width*.5f,2.66f,2.2f},rgb(0x4f5b59));
}

void rescue_canopy(const Mesh& mesh,const AirframeSpec& model,float hatch) {
    const float width=model.body*.61f;
    canopy_well(mesh,width);
    const float angle=hatch*1.26f;
    const float rise=std::sin(angle),back=std::cos(angle);
    const auto lid=[&](float x,float span,float loft=0.f) {
        return Vec{x*width*.45f,3.1f+span*rise+loft,4.5f-span*back};
    };
    mesh.quad(lid(-2.2f,0),lid(2.2f,0),lid(2.3f,7.5f),lid(-2.3f,7.5f),model.edge);
    mesh.quad(lid(-1.75f,.45f,.08f),lid(1.75f,.45f,.08f),lid(1.85f,6.8f,.08f),lid(-1.85f,6.8f,.08f),
              model.glass);
    mesh.tri(lid(-1.6f,1.3f,.13f),lid(-.4f,1.4f,.13f),lid(-1.55f,5.6f,.13f),rgb(0xd0f6e9));
}

} // namespace

void aircraft_shadow(Scene& scene,const World& world,const Craft& craft) {
    const auto pose=attitude(craft.yaw,craft.pitch,craft.bank);
    const auto& model=airframe(craft.type);
    const Vec ray{shadow_x,-1,shadow_z};
    const Vec center=craft.pos+ray*(craft.pos.y-world.ground(craft.pos));
    const auto local=scene.camera.local(center);
    if (local.z<=Scene::near || local.z>1680) return;
    const auto screen=scene.camera.project(local);
    const float margin=scene.camera.scale_at(local.z)*32+4;
    if (screen.x<-margin || screen.x>scene.out.width+margin ||
        screen.y<-margin || screen.y>scene.out.height+margin) return;
    const auto stamp=[&](Vec point) {
        const Vec plane=craft.pos+pose.apply(point);
        const auto coast=plane+ray*plane.y;
        auto ground=plane+ray*(plane.y-world.visual_height(coast.x,coast.z));
        ground.y=world.visual_height(ground.x,ground.z);
        return ground;
    };
    const Wing left=wing(model,-1,craft.damage.missing_tip<0),right=wing(model,1,craft.damage.missing_tip>0);
    const Vec shape[]{
        {0,0,model.nose},right.root,right.elbow,right.tip,right.aft,right.rear,
        {model.body*.74f,0,model.tail},{-model.body*.74f,0,model.tail},
        left.rear,left.aft,left.tip,left.elbow,left.root,
    };
    std::array<Vec,std::size(shape)> outline;
    auto& d=scene.out;
    float low=d.height,high=0.f;
    for (unsigned i=0;i<outline.size();++i) {
        const Vec ground=scene.camera.local(stamp(shape[i]));
        if (ground.z<Scene::near) return;
        outline[i]=scene.camera.project(ground);
        low=std::min(low,outline[i].y);
        high=std::max(high,outline[i].y);
    }
    const float height=std::max(0.f,craft.pos.y-center.y);
    const int alpha=std::clamp(17-int(height*.044f),4,17);
    const float rim=std::clamp(.75f+height*.014f*scene.camera.scale_at(local.z),.75f,2.8f);
    const float inverse=1.f/(rim*1.6f);
    // Rasterize the entire outline once, including its soft edge.
    for (int row=std::max(0,int(low));row<std::min(d.height,int(high+1));++row) {
        const float y=row+.5f;
        std::array<Vec,std::size(shape)> edges;
        unsigned count=0;
        for (unsigned i=0;i<outline.size();++i) {
            const auto a=outline[i],b=outline[(i+1)%outline.size()];
            if ((y<a.y)==(y<b.y)) continue;
            const float along=(y-a.y)/(b.y-a.y);
            const Vec edge{mix(a.x,b.x,along),0,mix(a.z,b.z,along)};
            unsigned where=count++;
            while (where && edges[where-1].x>edge.x) {
                edges[where]=edges[where-1];
                --where;
            }
            edges[where]=edge;
        }
        const float vertical=saturate((std::min(y-low,high-y)+.5f)*inverse);
        for (unsigned i=1;i<count;i+=2) {
            const auto from=edges[i-1],to=edges[i];
            const float span=std::max(.05f,to.x-from.x);
            const float dz=(to.z-from.z)/span;
            const int first=std::max(0,int(from.x-rim*.5f));
            const int last=std::min(d.width-1,int(to.x+rim*.5f));
            for (int x=first;x<=last;++x) {
                const float offset=x+.5f-from.x;
                const float coverage=saturate((std::min(offset,span-offset)+rim*.5f)*inverse);
                const int opacity=int(alpha*std::min(coverage,vertical)+.5f);
                if (opacity) d.shade_ground(x,row,from.z+dz*offset,rgb(0x21394a),opacity);
            }
        }
    }
}

void draw_aircraft(Scene& scene,const Craft& craft,Paint paint,float heat,float hatch,float time) {
    const auto& model=airframe(craft.type);
    const bool enemy=paint==Paint::enemy,wreck=paint==Paint::wreck,rescue=paint==Paint::rescue;
    const bool friend_plane=paint==Paint::wingman;
    const auto p=scene.camera.local(craft.pos);
    const float radius=heat>.12f ? 33.f : 23.f;
    if (p.z+radius<Scene::near || p.z>2100) return;
    if (p.z>Scene::near+radius) {
        const auto center=scene.camera.project(p);
        const float margin=scene.camera.scale_at(p.z-radius)*radius+2;
        if (center.x<-margin || center.x>scene.out.width+margin ||
            center.y<-margin || center.y>scene.out.height+margin) return;
    }
    const auto orientation=attitude(craft.yaw,craft.pitch,craft.bank);
    const auto& camera=scene.camera.axes;
    const auto rotate=[&](Vec axis) {
        return Vec{dot(axis,camera.right),dot(axis,camera.up),dot(axis,camera.front)};
    };
    const Mesh mesh{scene,p,{rotate(orientation.right),rotate(orientation.up),rotate(orientation.front)}};
    const auto& scheme=enemy_profile(craft.type);
    const Color top=wreck ? blend(model.top,rgb(0x353c44),23) : enemy ? scheme.top :
                    friend_plane ? blend(model.top,rgb(0x93c5ba),9) : model.top;
    const Color edge=enemy ? scheme.edge : model.edge;
    const Color accent=enemy ? scheme.accent : friend_plane ? rgb(0x42bda8) : model.accent;
    const Color glass=enemy ? scheme.glass : model.glass;
    const Color bright=blend(top,rgb(0xfff4d6),12);
    const Color wing_top=blend(top,edge,int(9*saturate(.47f-dot(orientation.up,sunlight))));
    const float visible_span=scene.camera.scale_at(std::max(p.z,Scene::near))*model.span;
    const bool coarse=(enemy || friend_plane) && visible_span<12.5f;
    const bool detailed=visible_span>=14.5f;
    const float roof=rescue ? 2.1f : 2.55f;
    const Vec nose{0,.27f,model.nose},shoulder{0,roof,model.nose*.41f},spine{0,1.64f,model.tail+1.5f};
    for (float side:{-1.f,1.f}) {
        const bool broken=side==craft.damage.missing_tip;
        const Wing w=wing(model,side,broken);
        const float b=side*model.body;
        const Vec front{b,.58f,model.nose*.28f},rear{b*.77f,.48f,model.tail+1.3f};
        if (coarse) {
            mesh.quad(w.root,w.tip,w.aft,w.rear,wing_top);
            mesh.tri(w.elbow,w.tip,w.aft,accent);
            mesh.quad(shoulder,front,rear,spine,top);
            mesh.tri(front,shoulder,nose,bright);
            if (model.twin_tail || side>0)
                mesh.tri(rear,{model.twin_tail ? rear.x : 0.f,model.fin_height,model.tail+2.2f},spine,edge);
            continue;
        }
        const bool blink=!wreck && (int(time*2.2f)&1)==0;
        const Color lamp=side<0 ? rgb(0xff674f) : rgb(0x62f5b4);
        const float elevon=craft.type==Airframe::dart ? side*craft.elevator*.32f : 0;
        wing_detail(mesh,w,wing_top,edge,accent,-craft.aileron*.50f+elevon,blend(lamp,edge,blink ? 4 : 18),broken);
        if (visible_span>=8.f) wing_scars(mesh,w,craft.damage.hits(side),wing_top);
        if (detailed) {
            stabilizer(mesh,model,side,craft.elevator,wing_top,edge,accent);
            if (!rescue && !broken) wing_hardware(mesh,w,model,wing_top,edge,accent);
            if (craft.type==Airframe::dart) {
                const Vec hinge{side*model.body,.68f,model.nose*.47f};
                const auto mount=Hinge(hinge,hinge+Vec{side,0,0},side*craft.elevator*.46f);
                const Vec canard=mount.moved({side*(model.body+3.6f),.61f,model.nose*.34f});
                const Vec trailing=mount.moved(hinge+Vec{0,0,-3.65f});
                mesh.tri(hinge,canard,trailing,accent);
                mesh.tri(canard,trailing,canard-Vec{0,.39f,0},edge);
            }
        }
        const Color lit_side=dot(orientation.right*side,{-shadow_x,0,-shadow_z})>0 ? blend(edge,top,7) : edge;
        const Vec keel{b*.64f,-1.75f,model.tail+1.1f},chin{b*.72f,-1.03f,model.nose*.27f};
        mesh.quad(spine,rear,front,shoulder,top);
        mesh.quad(rear,keel,chin,front,lit_side);
        mesh.tri(shoulder,front,nose,bright);
        mesh.tri(front,chin,nose,lit_side);
        mesh.tri({b*.48f,.53f,model.nose*.65f},nose,{0,1.12f,model.nose*.73f},accent);
        if (detailed) {
            const auto panel = [&](float along,float side_fraction) {
                const Vec center=spine+(shoulder-spine)*along;
                const Vec seam=rear+(front-rear)*along;
                return center+(seam-center)*side_fraction+Vec{0,.15f,0};
            };
            mesh.quad(panel(.14f,.55f),panel(.55f,.55f),panel(.61f,.91f),panel(.16f,.91f),blend(top,edge,11));
            for (int vent=0;vent<2;++vent) {
                const float offset=.34f+vent*.075f;
                mesh.quad(panel(offset,.70f),panel(offset+.029f,.70f),
                          panel(offset+.034f,.91f),panel(offset,.91f),blend(edge,ink,11));
            }
        }
        mesh.quad(front+Vec{side*.07f,-.33f,.04f},rear+Vec{side*.08f,-.23f,0},
                  rear+Vec{side*.01f,-.66f,0},front+Vec{side*.01f,-.70f,0},blend(lit_side,ink,17));
        if (model.twin_tail || side>0) {
            const float tail_x=model.twin_tail ? b*.71f : 0;
            const float top_x=tail_x+(model.twin_tail ? side*1.6f : 0);
            const Vec base{tail_x,1,model.tail+6.9f},fin{top_x,model.fin_height,model.tail+3.7f};
            const Vec tip{top_x,model.fin_height-.24f,model.tail+.84f},aft{tail_x,.75f,model.tail-.1f};
            const Vec lower=base+(aft-base)*.59f,upper=fin+(tip-fin)*.53f;
            const auto axis=Hinge(lower,upper,-craft.rudder*.46f);
            const Vec top_edge=axis.moved(tip),bottom_edge=axis.moved(aft);
            mesh.quad(base,fin,upper,lower,accent);
            mesh.quad(lower,upper,top_edge,bottom_edge,blend(accent,edge,15));
            mesh.quad(fin,upper,top_edge,axis.moved(tip+Vec{0,-.74f,0}),bright);
        }
        if (!rescue && !craft.damage.canopy_lost) {
            const Vec rim{side*1.53f,2.29f,1.62f},rim_back{side*1.25f,2.20f,-3.67f};
            const Vec canopy_top{0,4.03f,1.90f},canopy_aft{0,3.12f,-3.34f};
            mesh.quad(canopy_top,rim,rim_back,canopy_aft,blend(glass,ink,side<0 ? 3 : 11));
            mesh.tri(canopy_top,rim,{0,2.63f,model.nose*.39f},blend(glass,bright,13));
            mesh.quad(canopy_aft,rim_back,{side*1.24f,2.23f,-3.35f},{0,3.16f,-3.06f},ink);
            if (detailed) {
                const Vec arc_top=canopy_top+(canopy_aft-canopy_top)*.28f+Vec{0,.08f,0};
                const Vec arc_rim=rim+(rim_back-rim)*.28f+Vec{side*.06f,.05f,0};
                mesh.quad(arc_top,arc_rim,arc_rim+Vec{0,0,-.31f},arc_top+Vec{0,0,-.31f},edge);
            }
            if (side<0) {
                const Vec a=canopy_top+Vec{-.11f,.12f,-.51f};
                const Vec b=canopy_aft+Vec{-.11f,.13f,.48f};
                mesh.quad(a,a+Vec{-.40f,-.36f,.09f},b+Vec{-.29f,-.20f,.12f},b,rgb(0x88becc));
            }
        }
        if (craft.damage.body && visible_span>=8.f) {
            const Vec at=spine+(shoulder-spine)*.31f+Vec{side*.92f,.04f,0};
            mesh.tri(at+Vec{0,.04f,1.42f},at+Vec{side*.69f,-.07f,-.2f},at+Vec{-side*.54f,.15f,-.71f},
                     rgb(0x423935));
        }
        if (model.nacelle>0 || side>0)
            engine(mesh,side*model.nacelle,model,lit_side,top,heat,time,!wreck && paint!=Paint::deck,paint==Paint::player);
    }
    if (!coarse) {
        const float keel=model.body*.64f,chin=model.body*.72f,left=model.tail+1.1f,right=model.nose*.27f;
        mesh.quad({-keel,-1.75f,left},{keel,-1.75f,left},{chin,-1.03f,right},{-chin,-1.03f,right},
                  blend(edge,ink,12));
        mesh.tri({-chin,-1.03f,right},{chin,-1.03f,right},nose,edge);
        mesh.tri({-keel,-1.75f,left},{keel,-1.75f,left},spine,ink);
    }
    if (craft.gear && visible_span>=7.2f) {
        const auto wheel=[&](float x,float z) {
            mesh.quad({x-.19f,-1.12f,z},{x+.19f,-1.12f,z},
                      {x+.19f,-2.47f,z},{x-.19f,-2.47f,z},rgb(0xb2aea1));
            mesh.quad({x-.52f,-2.18f,z},{x+.52f,-2.18f,z},
                      {x+.47f,-2.99f,z},{x-.47f,-2.99f,z},ink);
            mesh.quad({x-.51f,-2.27f,z+.77f},{x-.52f,-2.18f,z},
                      {x-.47f,-2.99f,z},{x-.45f,-2.99f,z+.72f},rgb(0x35434a));
        };
        wheel(0,model.nose*.52f);
        wheel(-model.body*.82f,model.tail*.40f);
        wheel(model.body*.82f,model.tail*.40f);
    }
    if (rescue && !coarse) rescue_canopy(mesh,model,hatch);
    else if (craft.damage.canopy_lost && !coarse) canopy_well(mesh,model.body*.57f);
}

} // namespace sky
