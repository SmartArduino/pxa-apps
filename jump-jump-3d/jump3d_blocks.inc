// Private renderer module. These platforms reuse the lit palette and solid
// triangle batches; no textures, heap allocations, or animation objects.

static void block_cuboid(j3_render_t *render, const j3_block_t *block,
                        uint8_t color) {
    const float base = j3_block_offset(block), top = j3_block_top(block);
    const float x0 = block->x - block->radius, x1 = block->x + block->radius;
    const float z0 = block->z - block->radius, z1 = block->z + block->radius;
    box_side(render, x0, z0, x0, z1, base, top, color, J3_LIGHT_LEFT);
    box_side(render, x0, z1, x1, z1, base, top, color, J3_LIGHT_FRONT);
    draw_rect_on_top(render, block, top, 0, 0, block->radius, block->radius,
                     color, J3_LIGHT_TOP);
}

// Coordinates are relative to the footprint: -1..1 across, 0..1 in height.
// All markings follow both the drop-in offset and charging compression.
static void block_face(j3_render_t *render, const j3_block_t *block,
                       float x0, float y0, float x1, float y1, uint8_t color,
                       bool left = false) {
    const float base = j3_block_offset(block), height = j3_block_top(block) - base;
    const float radius = block->radius;
    const float light = left ? J3_LIGHT_LEFT : J3_LIGHT_FRONT;
    j3_point_t quad[4];
    if (left) {
        const float x = block->x - radius - 0.001F;
        quad[0] = project(render, x, base + height*y0, block->z + radius*x0, light);
        quad[1] = project(render, x, base + height*y0, block->z + radius*x1, light);
        quad[2] = project(render, x, base + height*y1, block->z + radius*x1, light);
        quad[3] = project(render, x, base + height*y1, block->z + radius*x0, light);
    } else {
        const float z = block->z + radius + 0.001F;
        quad[0] = project(render, block->x + radius*x0, base + height*y0, z, light);
        quad[1] = project(render, block->x + radius*x1, base + height*y0, z, light);
        quad[2] = project(render, block->x + radius*x1, base + height*y1, z, light);
        quad[3] = project(render, block->x + radius*x0, base + height*y1, z, light);
    }
    constant_quad(render,quad,color,light);
}

static void block_top_mark(j3_render_t *render, const j3_block_t *block,
                           float x, float z, float half_x, float half_z,
                           uint8_t color) {
    const float r = block->radius;
    const float top = j3_block_top(block)+0.001F;
    j3_point_t quad[4];
    quad[0]=project(render,block->x+(x-half_x)*r,top,block->z+(z-half_z)*r,J3_LIGHT_TOP);
    quad[1]=project(render,block->x+(x+half_x)*r,top,block->z+(z-half_z)*r,J3_LIGHT_TOP);
    quad[2]=project(render,block->x+(x+half_x)*r,top,block->z+(z+half_z)*r,J3_LIGHT_TOP);
    quad[3]=project(render,block->x+(x-half_x)*r,top,block->z+(z+half_z)*r,J3_LIGHT_TOP);
    constant_quad(render,quad,color,J3_LIGHT_TOP);
}

static void block_digit(j3_render_t *render, const j3_block_t *block,
                        unsigned digit, float x) {
    static constexpr uint8_t digits[] = {
        0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f
    };
    // a,b,c,d,e,f,g segments in a fixed, reusable seven-segment grid.
    static constexpr float segments[][4] = {
        {0.04F,0.92F,0.22F,1.00F}, {0.20F,0.53F,0.27F,0.95F},
        {0.20F,0.05F,0.27F,0.47F}, {0.04F,0.00F,0.22F,0.08F},
        {0.00F,0.05F,0.07F,0.47F}, {0.00F,0.53F,0.07F,0.95F},
        {0.04F,0.46F,0.22F,0.54F}
    };
    for (unsigned i = 0; i < 7; ++i) {
        if (!(digits[digit] & (1u << i))) continue;
        const auto& s = segments[i];
        block_face(render, block, x+s[0], 0.28F+0.44F*s[1],
                   x+s[2], 0.28F+0.44F*s[3], J3_SOLID_GREEN);
    }
}

static void block_clock(j3_render_t *render, const j3_block_t *block, float time) {
    block_cuboid(render, block, J3_SOLID_CREAM);
    block_face(render, block, -0.87F,0.18F,0.87F,0.82F,J3_SOLID_SLATE);
    block_face(render, block, -0.80F,0.23F,0.80F,0.77F,J3_SOLID_CHARCOAL);
    const unsigned minutes = (render->clock_minutes + (render->clock_seconds + unsigned(time)) / 60) % 1440;
    block_digit(render, block, minutes/60/10, -0.70F);
    block_digit(render, block, minutes/60%10, -0.37F);
    block_digit(render, block, minutes%60/10, 0.12F);
    block_digit(render, block, minutes%60%10, 0.45F);
    if ((unsigned(time) & 1u) == 0) {
        block_face(render,block,-0.035F,0.38F,0.035F,0.44F,J3_SOLID_GREEN);
        block_face(render,block,-0.035F,0.56F,0.035F,0.62F,J3_SOLID_GREEN);
    }
    block_top_mark(render,block,0,0,0.45F,0.28F,J3_SOLID_GOLD);
}

static void block_glow(j3_render_t *render, const j3_block_t *block, bool occupied) {
    block_cuboid(render,block,J3_INDEX(J3_HUE_BLOCK,1));
    const uint8_t ink = occupied ? J3_WHITE : J3_INDEX(J3_HUE_BLOCK,7);
    for (float x : {-0.58F, 0.58F}) {
        block_top_mark(render,block,x,-0.56F,0.20F,0.045F,ink);
        block_top_mark(render,block,x,-0.56F,0.045F,0.20F,ink);
    }
    block_face(render,block,-0.16F,0.31F,0.16F,0.69F,ink);
    block_face(render,block,-0.38F,0.44F,0.38F,0.56F,ink);
}

static void block_disk(j3_render_t *render, const j3_block_t *block, float time) {
    block_cuboid(render,block,J3_SOLID_CHARCOAL);
    block_top_mark(render,block,0,-0.57F,0.55F,0.31F,J3_GRAY_12);
    block_top_mark(render,block,0.28F,-0.58F,0.13F,0.22F,J3_SOLID_DARK);
    block_top_mark(render,block,0,0.30F,0.66F,0.37F,J3_SOLID_CREAM);
    block_top_mark(render,block,0,0.28F,0.50F,0.025F,J3_SOLID_BLUE);
    block_face(render,block,-0.7F,0.65F,0.7F,0.77F,J3_GRAY_6);
    block_face(render,block,0.53F,0.25F,0.69F,0.40F,
               (unsigned(time)&1u) ? J3_SOLID_DARK : J3_SOLID_GREEN);
}

static void block_golf(j3_render_t *render, const j3_block_t *block) {
    const float base = j3_block_offset(block), top = j3_block_top(block), r = block->radius;
    draw_cylinder_side(render,block,base,top,r,J3_SOLID_GREEN,20,8);
    draw_disc(render,block->x,top,block->z,r,J3_SOLID_GREEN,J3_LIGHT_TOP,J3_SEGMENTS);
    draw_disc(render,block->x+r*0.40F,top+0.001F,block->z-r*0.3F,
              r*0.16F,J3_SOLID_CHARCOAL,J3_LIGHT_TOP,8);
    draw_disc(render,block->x-r*0.54F,top+0.003F,block->z+r*0.16F,
              r*0.09F,J3_GRAY_12,J3_LIGHT_TOP,8);
    draw_disc(render,block->x-r*0.54F,top+0.012F,block->z+r*0.16F,
              r*0.07F,J3_WHITE,J3_LIGHT_TOP,8);
}

static void block_round_item(j3_render_t *render, const j3_block_t *block) {
    const float base = j3_block_offset(block), top = j3_block_top(block);
    const float height = top-base, r = block->radius;
    if (block->kind == J3_KIND_CUP) {
        draw_cylinder_side(render,block,base,base+height*0.22F,r,J3_SOLID_BROWN,21,7);
        draw_cylinder_side(render,block,base+height*0.22F,top,r,J3_SOLID_CREAM,22,8);
        draw_disc(render,block->x,top,block->z,r,J3_SOLID_CREAM,J3_LIGHT_TOP,J3_SEGMENTS);
        draw_disc(render,block->x,top+0.001F,block->z,r*0.72F,J3_SOLID_BROWN,J3_LIGHT_TOP,10);
        draw_disc(render,block->x+r*0.18F,top+0.002F,block->z-r*0.15F,
                  r*0.26F,J3_SOLID_GOLD,J3_LIGHT_TOP,8);
    } else if (block->kind == J3_KIND_MEDICINE) {
        draw_cylinder_side(render,block,base,base+height*0.24F,r,J3_SOLID_BROWN,22,7);
        draw_cylinder_side(render,block,base+height*0.24F,base+height*0.73F,r,J3_SOLID_CREAM,23,7);
        draw_cylinder_side(render,block,base+height*0.73F,top,r,J3_SOLID_WHITE,22,8);
        draw_disc(render,block->x,top,block->z,r,J3_SOLID_WHITE,J3_LIGHT_TOP,12);
        block_face(render,block,-0.16F,0.30F,0.16F,0.68F,J3_SOLID_RED);
        block_face(render,block,-0.38F,0.44F,0.38F,0.56F,J3_SOLID_RED);
    } else {
        draw_cylinder_side(render,block,base,top,r,J3_SOLID_CREAM,22,8);
        draw_disc(render,block->x,top,block->z,r,J3_WHITE,J3_LIGHT_TOP,12);
        draw_ring(render,block->x,top+0.001F,block->z,r*0.58F,r*0.62F,
                  J3_GRAY_12,J3_LIGHT_TOP,8);
        draw_disc(render,block->x,top+0.002F,block->z,r*0.23F,
                  J3_SOLID_BROWN,J3_LIGHT_TOP,8);
        draw_disc(render,block->x,top+0.003F,block->z,r*0.16F,
                  J3_SOLID_CHARCOAL,J3_LIGHT_TOP,8);
    }
}

static void block_book(j3_render_t *render, const j3_block_t *block) {
    block_cuboid(render,block,J3_SOLID_RED);
    block_face(render,block,-0.95F,0.12F,0.95F,0.88F,J3_SOLID_CREAM);
    for (float y : {0.32F,0.50F,0.68F})
        block_face(render,block,-0.92F,y,0.92F,y+0.025F,J3_GRAY_12);
    block_face(render,block,-0.4F,0.22F,0.4F,0.78F,J3_SOLID_GOLD,true);
    block_top_mark(render,block,0,0,0.62F,0.43F,J3_SOLID_GOLD);
    block_top_mark(render,block,0,0,0.52F,0.32F,J3_SOLID_RED);
    block_top_mark(render,block,0,0,0.30F,0.035F,J3_SOLID_CREAM);
}

static void block_parcel(j3_render_t *render, const j3_block_t *block) {
    block_cuboid(render,block,J3_INDEX(J3_HUE_BLOCK,14));
    block_top_mark(render,block,0,0,0.12F,1,J3_SOLID_GOLD);
    block_top_mark(render,block,0,0,1,0.12F,J3_SOLID_GOLD);
    block_face(render,block,-0.12F,0,0.12F,1,J3_SOLID_GOLD);
    block_face(render,block,-0.76F,0.20F,-0.23F,0.66F,J3_WHITE);
    for (float x : {-0.65F,-0.53F,-0.41F})
        block_face(render,block,x,0.27F,x+0.04F,0.48F,J3_SOLID_DARK);
}

static void block_gift(j3_render_t *render, const j3_block_t *block) {
    block_cuboid(render,block,J3_INDEX(J3_HUE_BLOCK,13));
    block_top_mark(render,block,0,0,0.12F,1,J3_SOLID_GOLD);
    block_top_mark(render,block,0,0,1,0.12F,J3_SOLID_GOLD);
    block_face(render,block,-0.12F,0,0.12F,1,J3_SOLID_GOLD);
    block_face(render,block,-0.12F,0,0.12F,1,J3_SOLID_GOLD,true);
    block_top_mark(render,block,-0.3F,-0.25F,0.22F,0.13F,J3_SOLID_CREAM);
    block_top_mark(render,block,0.3F,0.25F,0.22F,0.13F,J3_SOLID_CREAM);
}

static void block_stool(j3_render_t *render, const j3_block_t *block) {
    const float top=j3_block_top(block),base=j3_block_offset(block),r=block->radius;
    // Rear legs first. Four openings remain visible without alpha textures.
    for (float z : {-0.72F,0.72F}) {
        for (float x : {0.72F,-0.72F}) {
            const float x0=block->x+(x-0.16F)*r,x1=block->x+(x+0.16F)*r;
            const float z0=block->z+(z-0.16F)*r,z1=block->z+(z+0.16F)*r;
            box_side(render,x0,z0,x0,z1,base,top,J3_SOLID_BROWN,J3_LIGHT_LEFT);
            box_side(render,x0,z1,x1,z1,base,top,J3_SOLID_BROWN,J3_LIGHT_FRONT);
        }
    }
    box_side(render,block->x-r,block->z-r,block->x-r,block->z+r,
             base+(top-base)*0.78F,top,J3_SOLID_ORANGE,J3_LIGHT_LEFT);
    box_side(render,block->x-r,block->z+r,block->x+r,block->z+r,
             base+(top-base)*0.78F,top,J3_SOLID_ORANGE,J3_LIGHT_FRONT);
    block_top_mark(render,block,0,0,1,1,J3_SOLID_ORANGE);
    block_top_mark(render,block,0,0,0.70F,0.025F,J3_SOLID_GOLD);
}

static void block_console(j3_render_t *render, const j3_block_t *block) {
    block_cuboid(render,block,J3_SOLID_BLUE);
    block_top_mark(render,block,0,-0.24F,0.70F,0.49F,J3_SOLID_CHARCOAL);
    block_top_mark(render,block,0,-0.24F,0.54F,0.36F,J3_INDEX(J3_HUE_BLOCK,6));
    block_top_mark(render,block,0.30F,0.46F,0.11F,0.11F,J3_SOLID_RED);
    block_top_mark(render,block,-0.38F,0.46F,0.19F,0.055F,J3_SOLID_DARK);
    block_top_mark(render,block,-0.38F,0.46F,0.055F,0.19F,J3_SOLID_DARK);
    block_face(render,block,-0.50F,0.35F,0.50F,0.63F,J3_SOLID_CHARCOAL);
}

static void block_face_character(j3_render_t *render, const j3_block_t *block) {
    block_cuboid(render,block,J3_INDEX(J3_HUE_BLOCK,block->color));
    for (float x : {-0.47F,0.22F})
        block_face(render,block,x,0.48F,x+0.25F,0.73F,J3_WHITE);
    for (float x : {-0.39F,0.30F})
        block_face(render,block,x,0.52F,x+0.10F,0.64F,J3_SOLID_CHARCOAL);
    block_face(render,block,-0.31F,0.22F,0.31F,0.32F,J3_SOLID_CHARCOAL);
    block_top_mark(render,block,-0.67F,-0.62F,0.21F,0.16F,J3_SOLID_CREAM);
    block_top_mark(render,block,0.67F,-0.62F,0.21F,0.16F,J3_SOLID_CREAM);
}

static void block_money(j3_render_t *render, const j3_block_t *block) {
    block_cuboid(render,block,J3_SOLID_GOLD);
    block_top_mark(render,block,0,0,0.40F,0.065F,J3_SOLID_CHARCOAL);
    block_face(render,block,-0.055F,0.20F,0.055F,0.79F,J3_SOLID_BROWN);
    block_face(render,block,-0.32F,0.39F,0.32F,0.46F,J3_SOLID_BROWN);
    block_face(render,block,-0.32F,0.56F,0.32F,0.63F,J3_SOLID_BROWN);
}

static void draw_decorated_block(j3_render_t *render, const j3_block_t *block,
                                 float time, bool occupied) {
    switch (block->kind) {
    case J3_KIND_CLOCK: block_clock(render,block,time); break;
    case J3_KIND_GLOW: block_glow(render,block,occupied); break;
    case J3_KIND_DISK: block_disk(render,block,time); break;
    case J3_KIND_GOLF: block_golf(render,block); break;
    case J3_KIND_CUP:
    case J3_KIND_MEDICINE:
    case J3_KIND_PAPER: block_round_item(render,block); break;
    case J3_KIND_BOOK: block_book(render,block); break;
    case J3_KIND_PARCEL: block_parcel(render,block); break;
    case J3_KIND_GIFT: block_gift(render,block); break;
    case J3_KIND_STOOL: block_stool(render,block); break;
    case J3_KIND_CONSOLE: block_console(render,block); break;
    case J3_KIND_FACE: block_face_character(render,block); break;
    case J3_KIND_MONEY: block_money(render,block); break;
    default: draw_box(render,block); break;
    }
}
