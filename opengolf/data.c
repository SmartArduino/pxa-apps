/* SPDX-License-Identifier: MIT. Synchronous, embedded replacement for the original
 * threaded filesystem/GPU asset loader. All physics values and geometry come from
 * pinned original data; entity, geometry and transform constructors are upstream. */
#include "common/data.h"
#include "common/level.h"
#include "common/alloc.h"
#include "common/log.h"
#include "parson/parson.h"
#include "generated/assets.h"

static golf_level_t courses[20];
static bool loaded[20];
static JSON_Value *config_value;
static JSON_Object *config;
static golf_config_t config_tag;

static vec3 array3(JSON_Array *a) {
    return V3((float)json_array_get_number(a,0),(float)json_array_get_number(a,1),(float)json_array_get_number(a,2));
}
static vec3 obj3(JSON_Object *o,const char *key) {return array3(json_object_get_array(o,key));}
static golf_transform_t read_transform(JSON_Object *o) {
    JSON_Array *r=json_object_get_array(o,"rotation");
    return golf_transform(obj3(o,"position"),obj3(o,"scale"),QUAT(
      (float)json_array_get_number(r,0),(float)json_array_get_number(r,1),
      (float)json_array_get_number(r,2),(float)json_array_get_number(r,3)));
}
static golf_movement_t read_movement(JSON_Object *o) {
    const char *type=json_object_get_string(o,"type");
    if (!type || strcmp(type,"none")==0) return golf_movement_none();
    float t0=(float)json_object_get_number(o,"t0"), length=(float)json_object_get_number(o,"length");
    if (strcmp(type,"linear")==0) return golf_movement_linear(t0,obj3(o,"p0"),obj3(o,"p1"),length);
    if (strcmp(type,"spinner")==0) return golf_movement_spinner(t0,length);
    if (strcmp(type,"pendulum")==0) return golf_movement_pendulum(t0,length,(float)json_object_get_number(o,"theta0"),obj3(o,"axis"));
    if (strcmp(type,"ramp")==0) return golf_movement_ramp(t0,length,(float)json_object_get_number(o,"theta0"),(float)json_object_get_number(o,"theta1"),(float)json_object_get_number(o,"transition_length"),obj3(o,"axis"));
    golf_log_error("Unsupported original movement %s",type);
    return golf_movement_none();
}
static golf_geo_t read_geo(JSON_Object *o,bool water) {
    vec_golf_geo_point_t points; vec_init(&points,"course");
    vec_golf_geo_face_t faces; vec_init(&faces,"course");
    JSON_Array *p=json_object_get_array(o,"p");
    for (size_t i=0;i<json_array_get_count(p);i+=3) {
        golf_geo_point_t point=golf_geo_point(V3((float)json_array_get_number(p,i),(float)json_array_get_number(p,i+1),(float)json_array_get_number(p,i+2)));
        vec_push(&points,point);
    }
    JSON_Array *a=json_object_get_array(o,"faces");
    for (size_t i=0;i<json_array_get_count(a);i++) {
        JSON_Object *f=json_array_get_object(a,i);
        vec_int_t idx;vec_init(&idx,"course");
        vec_vec2_t uvs;vec_init(&uvs,"course");
        JSON_Array *ia=json_object_get_array(f,"idxs"),*ua=json_object_get_array(f,"uvs");
        for (size_t n=0;n<json_array_get_count(ia);n++) {int v=(int)json_array_get_number(ia,n);vec_push(&idx,v);}
        for (size_t n=0;n<json_array_get_count(ua);n+=2) {vec2 uv=V2((float)json_array_get_number(ua,n),(float)json_array_get_number(ua,n+1));vec_push(&uvs,uv);}
        const char *uvtype=json_object_get_string(f,"uv_gen_type");
        golf_geo_face_uv_gen_type_t uvgen=GOLF_GEO_FACE_UV_GEN_MANUAL;
        if (uvtype) for (int t=0;t<GOLF_GEO_FACE_UV_GEN_COUNT;t++) if(strcmp(uvtype,golf_geo_uv_gen_type_strings()[t])==0) uvgen=t;
        golf_geo_face_t face=golf_geo_face(json_object_get_string(f,"material_name"),idx,uvgen,uvs,obj3(f,"water_dir"));
        vec_push(&faces,face);
    }
    vec_golf_geo_generator_data_arg_t args;vec_init(&args,"course");
    return golf_geo(points,faces,golf_geo_generator_data(NULL,args),water);
}
static void load_course(unsigned idx) {
    if(loaded[idx]) return;
    golf_level_t *level=&courses[idx];
    vec_init(&level->deps,"course");vec_init(&level->materials,"course");
    vec_init(&level->entities,"course");vec_init(&level->lightmap_images,"course");
    JSON_Value *v=json_parse_string(embedded_courses[idx].json);
    JSON_Object *o=json_value_get_object(v);
    JSON_Array *materials=json_object_get_array(o,"materials");
    for(size_t i=0;i<json_array_get_count(materials);i++) {
        JSON_Object *m=json_array_get_object(materials,i);
        const char *name=json_object_get_string(m,"name"),*type=json_object_get_string(m,"type");
        float f=(float)json_object_get_number(m,"friction"),r=(float)json_object_get_number(m,"restitution"),s=(float)json_object_get_number(m,"vel_scale");
        golf_material_t material;
        if(strcmp(type,"color")==0 || strcmp(type,"diffuse_color")==0) {
            JSON_Array *c=json_object_get_array(m,"color");
            vec4 color=V4((float)json_array_get_number(c,0),(float)json_array_get_number(c,1),(float)json_array_get_number(c,2),(float)json_array_get_number(c,3));
            material=strcmp(type,"diffuse_color")==0 ? golf_material_diffuse_color(name,f,r,s,color) : golf_material_color(name,f,r,s,color);
        } else if(strcmp(type,"environment")==0) material=golf_material_environment(name,f,r,s,json_object_get_string(m,"texture"));
        else if(strcmp(type,"texture")==0) material=golf_material_texture(name,f,r,s,json_object_get_string(m,"texture"));
        else {golf_log_error("Unsupported original material %s",type);abort();}
        vec_push(&level->materials,material);
    }
    JSON_Array *entities=json_object_get_array(o,"entities");
    for(size_t i=0;i<json_array_get_count(entities);i++) {
        JSON_Object *e=json_array_get_object(entities,i);
        const char *type=json_object_get_string(e,"type"),*name=json_object_get_string(e,"name");
        golf_transform_t transform=read_transform(json_object_get_object(e,"transform"));
        golf_movement_t movement=read_movement(json_object_get_object(e,"movement"));
        golf_lightmap_section_t lightmap={0};vec_init(&lightmap.uvs,"course");
        golf_entity_t entity;
        if(strcmp(type,"model")==0) entity=golf_entity_model(name,transform,json_object_get_string(e,"model"),(float)json_object_get_number(e,"uv_scale"),lightmap,movement,json_object_get_boolean(e,"ignore_physics")==1);
        else if(strcmp(type,"ball-start")==0) entity=golf_entity_ball_start(name,transform);
        else if(strcmp(type,"hole")==0) entity=golf_entity_hole(name,transform);
        else if(strcmp(type,"group")==0) entity=golf_entity_group(name,transform);
        else if(strcmp(type,"begin_animation")==0) entity=golf_entity_begin_animation(name,transform);
        else if(strcmp(type,"camera_zone")==0) entity=golf_entity_camera_zone(name,json_object_get_boolean(e,"towards_hole")==1,transform);
        else if(strcmp(type,"geo")==0) entity=golf_entity_geo(name,transform,movement,read_geo(json_object_get_object(e,"geo"),false),lightmap);
        else if(strcmp(type,"water")==0) entity=golf_entity_water(name,transform,read_geo(json_object_get_object(e,"geo"),true),lightmap);
        else {golf_log_error("Unsupported original entity %s",type);continue;}
        entity.parent_idx=(int)json_object_get_number(e,"parent_idx");
        vec_push(&level->entities,entity);
    }
    json_value_free(v);
    loaded[idx]=true;
}
void golf_data_load(const char *path,bool async) {
    (void)async;
    if(strcmp(path,"data/config/game.cfg")==0) {
        if(!config_value) {config_value=json_parse_string(original_game_config);config=json_value_get_object(config_value);}
        return;
    }
    for(unsigned i=0;i<20;i++) if(strcmp(path,embedded_courses[i].path)==0) {load_course(i);return;}
}
golf_data_load_state_t golf_data_get_load_state(const char *path) {
    for(unsigned i=0;i<20;i++) if(strcmp(path,embedded_courses[i].path)==0) return loaded[i]?GOLF_DATA_LOADED:GOLF_DATA_UNLOADED;
    return GOLF_DATA_LOADED;
}
void golf_data_unload(const char *path) {(void)path;/* Cached immutable courses are bounded by 20 originals. */}
golf_level_t *golf_data_get_level(const char *path) {
    for(unsigned i=0;i<20;i++) if(strcmp(path,embedded_courses[i].path)==0) {load_course(i);return &courses[i];}
    return NULL;
}
golf_config_t *golf_data_get_config(const char *path) {golf_data_load(path,false);return &config_tag;}
float golf_config_get_num(golf_config_t *cfg,const char *name) {(void)cfg;return (float)json_object_get_number(config,name);}
const char *golf_config_get_string(golf_config_t *cfg,const char *name) {(void)cfg;return json_object_get_string(config,name);}
vec3 golf_config_get_vec3(golf_config_t *cfg,const char *name) {(void)cfg;return obj3(config,name);}
vec4 golf_config_get_vec4(golf_config_t *cfg,const char *name) {(void)cfg;JSON_Array *a=json_object_get_array(config,name);return V4((float)json_array_get_number(a,0),(float)json_array_get_number(a,1),(float)json_array_get_number(a,2),(float)json_array_get_number(a,3));}
golf_texture_t *golf_data_get_texture(const char *path) {(void)path;return NULL;}
golf_model_t *golf_data_get_model(const char *path) {
    for(unsigned i=0;i<sizeof(embedded_models)/sizeof(*embedded_models);i++) if(strcmp(path,embedded_models[i].path)==0) return embedded_models[i].model;
    golf_log_error("Missing embedded model %s",path);return NULL;
}
golf_model_group_t golf_model_group(const char *name,int start,int count) {golf_model_group_t g;snprintf(g.material_name,sizeof(g.material_name),"%s",name);g.start_vertex=start;g.vertex_count=count;return g;}
golf_model_t golf_model_dynamic(vec_golf_group_t g,vec_vec3_t p,vec_vec3_t n,vec_vec2_t t) {golf_model_t m={0};m.groups=g;m.positions=p;m.normals=n;m.texcoords=t;return m;}
golf_model_t golf_model_dynamic_water(vec_golf_group_t g,vec_vec3_t p,vec_vec3_t n,vec_vec2_t t,vec_vec3_t w) {golf_model_t m=golf_model_dynamic(g,p,n,t);m.is_water=true;m.water_dir=w;return m;}
void golf_model_dynamic_finalize(golf_model_t *m) {(void)m;}
void golf_model_dynamic_update_sg_buf(golf_model_t *m) {(void)m;}
