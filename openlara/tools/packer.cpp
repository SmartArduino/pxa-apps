// Portable entry point for the upstream BSD-2-Clause PC -> fixed converter.
#include "common.h"
#include "IMA.h"
#include "TR1_PC.h"
#include "TR1_PSX.h"
#include "out_GBA.h"
#include <memory>

int main(int argc, char **argv) {
    try {
        if (argc != 4) {
            fprintf(stderr, "usage: packer LEVEL_NAME input.phd output.pkd\n");
            return 2;
        }
        int id = -1;
        for (int i = 0; i < LVL_MAX; ++i)
            if (std::string(argv[1]) == levelNames[i]) id = i;
        ASSERT(id >= 0);
        FileStream input(argv[2], false);
        ASSERT(input.isValid());
        uint32 magic; input.read(magic); ASSERT(magic == 0x20); input.setPos(0);
        std::uintmax_t size=std::filesystem::file_size(argv[2]);
        bool demo=(id==LVL_TR1_TITLE && size==316138) || (id==LVL_TR1_2 && size==2873406);
        auto level = std::make_unique<TR1_PC>(input, LevelID(id), demo);
        // Keep every original room and sound. cutData() in the GBA demo
        // deliberately hides parts of Lara's home and removes samples.
        level->generateLODs();
        auto converter = std::make_unique<out_GBA>();
        converter->roomVerticesCount = 0;
        converter->roomVertices = new out_GBA::RoomVertex[MAX_ROOM_VERTICES];
        FileStream output(argv[3], true); ASSERT(output.isValid());
        converter->convertGBA(output, level.get());
        ASSERT(fflush(output.f) == 0);
        printf("%s: rooms=%d items=%d meshes=%d boxes=%d bytes=%u\n",
               argv[1], level->roomsCount, level->itemsCount,
               level->meshOffsetsCount, level->boxesCount, output.getPos());
        delete[] converter->roomVertices;
        return 0;
    } catch (const std::exception &error) {
        fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
