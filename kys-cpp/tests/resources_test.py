#!/usr/bin/env python3
"""Run with --work-dir pointing into local/artifacts/; leaves no source-tree output."""
import argparse
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location("prepare", Path(__file__).resolve().parents[1] / "tools/prepare_resources.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ResourcePreparationTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(dir=WORK_DIR)
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.source = self.root / "source"
        self.source.mkdir()
        for name in ("Config/KysMod.ini", "Font/Chinese.ttf", "Save/0.db"):
            path = self.source / name
            path.parent.mkdir(exist_ok=True)
            path.write_bytes(b"fixture")
        self.dest = self.root / "result"

    def test_preserves_contents_and_normalizes_case(self):
        self.assertEqual(module.prepare(self.source, self.dest), 3)
        self.assertEqual((self.dest / "save/0.db").read_bytes(), b"fixture")
        self.assertTrue((self.source / "Save/0.db").exists())

    def test_rejects_incomplete_game(self):
        (self.source / "Save/0.db").unlink()
        with self.assertRaises(ValueError):
            module.prepare(self.source, self.dest)
        self.assertFalse(self.dest.exists())

    def test_rejects_case_collision(self):
        (self.source / "Save/0.DB").write_bytes(b"collision")
        with self.assertRaises(ValueError):
            module.prepare(self.source, self.dest)

    def test_rejects_zip_traversal_without_publishing(self):
        with zipfile.ZipFile(self.source / "images.zip", "w") as z:
            z.writestr("../escape", b"bad")
        with self.assertRaises(ValueError):
            module.prepare(self.source, self.dest)
        self.assertFalse(self.dest.exists())

    def test_webp_inside_zip_becomes_png(self):
        from PIL import Image
        image = io.BytesIO()
        Image.new("RGBA", (3, 2), (255, 0, 0, 128)).save(image, "WEBP", lossless=True)
        with zipfile.ZipFile(self.source / "title.zip", "w") as z:
            z.writestr("1.WebP", image.getvalue())
            z.writestr("index.ka", b"offsets")
        module.prepare(self.source, self.dest)
        with zipfile.ZipFile(self.dest / "title.zip") as z:
            self.assertEqual(set(z.namelist()), {"1.png", "index.ka"})
            with Image.open(io.BytesIO(z.read("1.png"))) as im:
                self.assertEqual(im.getpixel((0, 0)), (255, 0, 0, 128))

    def test_never_overwrites_existing_tree(self):
        self.dest.mkdir()
        (self.dest / "keep").write_text("save")
        with self.assertRaises(ValueError):
            module.prepare(self.source, self.dest)
        self.assertEqual((self.dest / "keep").read_text(), "save")

    def test_loose_image_group_is_available_without_directory_scan(self):
        group = self.source / "Resource/title"
        group.mkdir(parents=True)
        (group / "1.png").write_bytes(b"png-fixture")
        (group / "index.txt").write_text("1 0 0")
        module.prepare(self.source, self.dest)
        self.assertFalse((self.dest / "resource/title").exists())
        with zipfile.ZipFile(self.dest / "resource/title.zip") as z:
            self.assertEqual(z.read("1.png"), b"png-fixture")
            self.assertEqual(z.read("index.txt"), b"1 0 0")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    WORK_DIR = args.work_dir.resolve()
    WORK_DIR.mkdir(parents=True, exist_ok=True)
    unittest.main(argv=[__file__])
