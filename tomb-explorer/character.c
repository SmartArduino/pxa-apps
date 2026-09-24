#include "character.h"

#include "palette.h"
#include "textures.h"
#include "tomb_math.h"

#define TOMB_BOX_TEX (float)TOMB_TEXTURE_SIZE

typedef struct {
    tomb_vertex_t vertices[8];
    tomb_face_t faces[6];
} tomb_character_part_t;

typedef struct {
    uint8_t corners[4];
    uint8_t light; /* relative brightness 0..255 of this side */
} tomb_box_face_t;

/* Faces of an axis-aligned box, counter-clockwise from outside. Corner bits:
 * 1 = +x, 2 = +y, 4 = +z. */
static const tomb_box_face_t kBoxFaces[6] = {
    {{2u, 3u, 7u, 6u}, 255u}, /* top (+y) */
    {{4u, 5u, 1u, 0u}, 120u}, /* bottom (-y) */
    {{5u, 4u, 6u, 7u}, 230u}, /* front (+z), the character's facing side */
    {{0u, 1u, 3u, 2u}, 170u}, /* back (-z) */
    {{1u, 5u, 7u, 3u}, 200u}, /* +x */
    {{4u, 0u, 2u, 6u}, 200u}, /* -x */
};

enum {
    kPelvis = 0,
    kTorso,
    kHead,
    kLeftUpperArm,
    kLeftLowerArm,
    kRightUpperArm,
    kRightLowerArm,
    kLeftUpperLeg,
    kLeftLowerLeg,
    kRightUpperLeg,
    kRightLowerLeg,
};

static tomb_character_part_t g_parts[TOMB_CHARACTER_PARTS];
static uint8_t g_brightness;

static void set_brightness(uint8_t brightness) {
    uint32_t part;
    uint32_t side;
    uint32_t c;
    if (brightness == g_brightness) return;
    g_brightness = brightness;
    for (part = 0u; part < TOMB_CHARACTER_PARTS; ++part) {
        for (side = 0u; side < 6u; ++side) {
            const uint32_t value =
                (uint32_t)brightness * kBoxFaces[side].light / 255u;
            for (c = 0u; c < 4u; ++c) {
                g_parts[part].faces[side].brightness[c] = (uint8_t)value;
            }
        }
    }
}

static void build_box(tomb_character_part_t *part, float width, float height,
                      float depth, float pivot_y, uint8_t texture_slot,
                      int flat_color) {
    const float hx = width * 0.5f;
    const float hy = height * 0.5f;
    const float hz = depth * 0.5f;
    uint32_t corner;
    uint32_t side;
    for (corner = 0u; corner < 8u; ++corner) {
        part->vertices[corner].x = (corner & 1u) != 0u ? hx : -hx;
        part->vertices[corner].y = pivot_y + ((corner & 2u) != 0u ? hy : -hy);
        part->vertices[corner].z = (corner & 4u) != 0u ? hz : -hz;
    }
    for (side = 0u; side < 6u; ++side) {
        tomb_face_t *face = &part->faces[side];
        uint32_t c;
        for (c = 0u; c < 4u; ++c) face->vertex[c] = kBoxFaces[side].corners[c];
        /* Bottom-left, bottom-right, top-right, top-left of every side. */
        {
            const float us[4] = {0.0f, TOMB_BOX_TEX, TOMB_BOX_TEX, 0.0f};
            const float vs[4] = {TOMB_BOX_TEX, TOMB_BOX_TEX, 0.0f, 0.0f};
            for (c = 0u; c < 4u; ++c) {
                face->u[c] = (uint16_t)us[c];
                face->v[c] = (uint16_t)vs[c];
            }
        }
        if (texture_slot == TEX_FACE && side != 2u) {
            /* Only the front of the head shows the face; the other sides sample
             * the hair rows at the top of the texture (the underside the skin). */
            const uint16_t v0 = side == 1u ? 50u : 2u;
            const uint16_t v1 = side == 1u ? 62u : 16u;
            face->v[0] = v1;
            face->v[1] = v1;
            face->v[2] = v0;
            face->v[3] = v0;
        }
        if (flat_color >= 0) {
            face->flags = TOMB_FACE_FLAT_COLOR;
            face->u[0] = (uint16_t)flat_color;
        } else {
            face->texture = texture_slot;
        }
    }
}

void tomb_character_init(void) {
    const uint8_t skin = tomb_palette_index(TOMB_RAMP_SKIN, 12u);
    const uint8_t boots = tomb_palette_index(TOMB_RAMP_LEATHER, 8u);
    const uint8_t belt = tomb_palette_index(TOMB_RAMP_LEATHER, 6u);
    /* width, height, depth, pivot offset (box centre relative to the joint),
     * texture, flat colour (-1 for textured). */
    build_box(&g_parts[kPelvis], 0.34f, 0.18f, 0.22f, -0.09f, TEX_CLOTH, belt);
    build_box(&g_parts[kTorso], 0.40f, 0.50f, 0.24f, 0.25f, TEX_CLOTH, -1);
    build_box(&g_parts[kHead], 0.22f, 0.26f, 0.22f, 0.15f, TEX_FACE, -1);
    build_box(&g_parts[kLeftUpperArm], 0.12f, 0.30f, 0.12f, -0.15f, TEX_CLOTH, -1);
    build_box(&g_parts[kLeftLowerArm], 0.10f, 0.30f, 0.10f, -0.15f, TEX_CLOTH, skin);
    build_box(&g_parts[kRightUpperArm], 0.12f, 0.30f, 0.12f, -0.15f, TEX_CLOTH, -1);
    build_box(&g_parts[kRightLowerArm], 0.10f, 0.30f, 0.10f, -0.15f, TEX_CLOTH, skin);
    build_box(&g_parts[kLeftUpperLeg], 0.15f, 0.42f, 0.16f, -0.21f, TEX_CLOTH, -1);
    build_box(&g_parts[kLeftLowerLeg], 0.14f, 0.42f, 0.14f, -0.21f, TEX_CLOTH, boots);
    build_box(&g_parts[kRightUpperLeg], 0.15f, 0.42f, 0.16f, -0.21f, TEX_CLOTH, -1);
    build_box(&g_parts[kRightLowerLeg], 0.14f, 0.42f, 0.14f, -0.21f, TEX_CLOTH, boots);
    g_brightness = 0u;
    set_brightness(255u);
}

static uint32_t submit_part(tomb_renderer_t *renderer, uint32_t index,
                            const tomb_transform_t *transform,
                            const tomb_submit_options_t *options) {
    const tomb_character_part_t *part = &g_parts[index];
    return tomb_renderer_submit(renderer, part->vertices, 8u, part->faces, 6u,
                                transform, options);
}

/* upper = parent * Translation(x, y) * upper_rotation;
 * lower = upper * Translation(0, -length) * RotationX(lower_angle). */
static void limb_build(tomb_transform_t *upper, tomb_transform_t *lower,
                       const tomb_transform_t *parent, float x, float y,
                       float length, const tomb_transform_t *upper_rotation,
                       float lower_angle) {
    tomb_transform_t translation;
    tomb_transform_t chain;
    tomb_vec3_t offset;
    tomb_vec3_set(&offset, x, y, 0.0f);
    tomb_transform_translation(&translation, offset);
    tomb_transform_mul(&chain, parent, &translation);
    tomb_transform_mul(upper, &chain, upper_rotation);
    tomb_vec3_set(&offset, 0.0f, -length, 0.0f);
    tomb_transform_translation(&translation, offset);
    tomb_transform_mul(&chain, upper, &translation);
    tomb_transform_rotation_x(&translation, lower_angle);
    tomb_transform_mul(lower, &chain, &translation);
}

uint32_t tomb_character_submit(tomb_renderer_t *renderer, tomb_vec3_t position,
                               float yaw, const tomb_pose_t *pose,
                               uint8_t brightness, uint8_t group,
                               tomb_rect_t scissor) {
    tomb_submit_options_t options;
    float swing;
    float knee;
    float bob;
    float hip_height;
    float arm_up;
    float tuck;
    tomb_transform_t root;
    tomb_transform_t pelvis;
    tomb_transform_t torso;
    tomb_transform_t head;
    tomb_transform_t left_arm_upper;
    tomb_transform_t left_arm_lower;
    tomb_transform_t right_arm_upper;
    tomb_transform_t right_arm_lower;
    tomb_transform_t left_leg_upper;
    tomb_transform_t left_leg_lower;
    tomb_transform_t right_leg_upper;
    tomb_transform_t right_leg_lower;
    uint32_t ok = 0u;
    set_brightness(brightness);
    options.group = group;
    options.scissor = scissor;
    /* The character stands on large floor quads whose depth key is their
     * centre; a small bias keeps the feet in front of the floor. */
    options.depth_bias = -0.3f;

    swing = tomb_sin(pose->walk_phase) * 0.6f * pose->walk_weight;
    knee = (0.5f + 0.5f * tomb_sin(pose->walk_phase + 1.2f)) * 0.9f * pose->walk_weight;
    bob = tomb_fabs(tomb_cos(pose->walk_phase)) * 0.04f * pose->walk_weight -
          pose->crouch * 0.25f;
    hip_height = 0.86f + bob;
    arm_up = pose->airborne * 1.6f;

    tomb_transform_uniform(&root, position, yaw, 0.0f, 0.0f);
    {
        const tomb_vec3_t offset = {0.0f, hip_height, 0.0f};
        tomb_transform_t translation;
        tomb_transform_translation(&translation, offset);
        tomb_transform_mul(&pelvis, &root, &translation);
    }
    /* RotationX(+a) swings a hanging limb tip towards +z (the facing
     * direction) and tilts an upright part backwards, so forward lean and
     * backward knee/elbow folds take the opposite signs below. */
    {
        const float angle = -(pose->walk_weight * 0.08f + pose->crouch * 0.3f);
        tomb_transform_t rotation;
        tomb_transform_rotation_x(&rotation, angle);
        tomb_transform_mul(&torso, &pelvis, &rotation);
    }
    {
        const tomb_vec3_t offset = {0.0f, 0.52f, 0.0f};
        tomb_transform_t translation;
        tomb_transform_translation(&translation, offset);
        tomb_transform_mul(&head, &torso, &translation);
    }

    /* Arms swing opposite to the leg on the same side and reach forward/up
     * while airborne; elbows fold forward. */
    {
        tomb_transform_t t;
        tomb_transform_rotation_x(&t, -swing * 0.8f + arm_up);
        limb_build(&left_arm_upper, &left_arm_lower, &torso, -0.26f, 0.45f, 0.30f,
                   &t, 0.35f + pose->airborne * 0.6f);
    }
    {
        tomb_transform_t t;
        tomb_transform_rotation_x(&t, swing * 0.8f + arm_up);
        limb_build(&right_arm_upper, &right_arm_lower, &torso, 0.26f, 0.45f, 0.30f,
                   &t, 0.35f + pose->airborne * 0.6f);
    }
    /* Knees fold backwards on the leg swinging forward and when crouching or
     * airborne, while the thighs tuck forward. */
    tuck = pose->crouch * 0.9f + pose->airborne * 0.7f;
    {
        tomb_transform_t t;
        const float lower =
            -(knee * (swing > 0.0f ? 1.0f : 0.2f) + tuck);
        tomb_transform_rotation_x(&t, swing + tuck * 0.5f);
        limb_build(&left_leg_upper, &left_leg_lower, &pelvis, -0.11f, -0.02f,
                   0.42f, &t, lower);
    }
    {
        tomb_transform_t t;
        const float lower =
            -(knee * (swing < 0.0f ? 1.0f : 0.2f) + tuck);
        tomb_transform_rotation_x(&t, -swing + tuck * 0.5f);
        limb_build(&right_leg_upper, &right_leg_lower, &pelvis, 0.11f, -0.02f,
                   0.42f, &t, lower);
    }

    ok += submit_part(renderer, kPelvis, &pelvis, &options);
    ok += submit_part(renderer, kTorso, &torso, &options);
    ok += submit_part(renderer, kHead, &head, &options);
    ok += submit_part(renderer, kLeftUpperArm, &left_arm_upper, &options);
    ok += submit_part(renderer, kLeftLowerArm, &left_arm_lower, &options);
    ok += submit_part(renderer, kRightUpperArm, &right_arm_upper, &options);
    ok += submit_part(renderer, kRightLowerArm, &right_arm_lower, &options);
    ok += submit_part(renderer, kLeftUpperLeg, &left_leg_upper, &options);
    ok += submit_part(renderer, kLeftLowerLeg, &left_leg_lower, &options);
    ok += submit_part(renderer, kRightUpperLeg, &right_leg_upper, &options);
    ok += submit_part(renderer, kRightLowerLeg, &right_leg_lower, &options);
    return ok;
}
