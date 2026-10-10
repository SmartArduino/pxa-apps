#!/usr/bin/env python3
"""Adapt only the reference harness ABI to the current Host, leaving C gameplay intact."""
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
changes={
 '    uint32_t prefilled_commands;':'    uint32_t prefilled_commands;\n    uint8_t scratch_mode;',
 '    uint64_t frame_id;':'    uint64_t frame_id;\n    uint64_t texture_mask;\n    uint8_t uses_palette;\n    uint8_t uses_depth;',
 'int32_t pxa_control(':'int32_t pxa_submit(',
 'int32_t pxa_io(uint32_t handle,':'int32_t pxa_io(uint64_t handle,',
 '    j3_game_t game;':'    j3_game_t game = {0};',
}
for before,after in changes.items():
 assert s.count(before)==1,before
 s=s.replace(before,after)
Path(sys.argv[2]).write_text(s)
