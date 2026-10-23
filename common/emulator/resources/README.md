# Free game media

`free-games.json` records reviewed redistribution licenses, original authors,
immutable content hashes, download locations, and complete source archives.
The first generation contains Thwaite 0.04 (NES, GPL-3.0-or-later) and
2048-gb (Game Boy, Zlib). These are independent homebrew games.

Build a signed, architecture-independent media package with the same publisher
key as the emulator:

```sh
python3 local/pxa-apps/common/emulator/tools/build_free_resources.py nes \
  --sdk-root deps/pxa-system \
  --output-root local/artifacts/free-media/<timestamp> \
  --signing-key /path/to/publisher-private.pem
```

Use `game-boy-emulator` for the other recipe. The output includes a signed PXA
resource package and `binding.json`. Copy the generated `resources` field to the
application's source-format 0.2 `package.json`, increment its release sequence,
and rebuild all platform variants. Upload the media package before publishing
the application. Its signed manifest binds the exact resource generation, so a
normal Store install downloads both parts and lets the system place them on the
chosen storage volume. A resource-capable Host profile is required.

The installed media contains the playable ROM, author and license notices,
and complete source. User ROMs remain separate from immutable installed media;
saves remain in app-private writable storage. Updating a recipe requires checking
its license, pinning new hashes and sizes, testing the actual game, building a new
resource generation, and publishing code that binds that generation. A changed
upstream download is rejected until the recipe is deliberately reviewed.
