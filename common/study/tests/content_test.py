#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check that the 20 installable variants contain only their assigned deck."""
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
subjects = ("math", "poetry", "words", "hanzi", "cards")
ages = ("preschool", "primary", "teen", "adult")
counts = (0, 6, 24, 12, 12)
for subject, count in zip(subjects, counts):
    base = root / ("learn-" + subject)
    assert not (base / "package.json").exists(), "Mixed-age package must not be installable"
    source = json.loads((base / "content.json").read_text())["cards"] if count else []
    for level, age in enumerate(ages):
        folder = root / ("learn-" + subject + "-" + age)
        manifest = json.loads((folder / "package.json").read_text())
        assert manifest["id"] == "pxa-" + folder.name
        content = (folder / "content.inc").read_text()
        expected = [row for row in source if row["level"] == level]
        assert len(expected) == count
        assert content.count(", 0},") == count
        assert f"set(STUDY_AGE {level})" in (folder / "CMakeLists.txt").read_text()
        for row in expected:
            assert json.dumps(row["question"], ensure_ascii=False) in content
            assert len(row["question"].encode()) <= 180
            assert len(row["answer"].encode()) <= 120
            assert len(row["explanation"].encode()) <= 480
        if expected:
            assert len({r["answer"] for r in expected}) >= 4
            assert len({(r["question"],r["answer"]) for r in expected}) == count
        if subject == "math":
            assert not manifest["permissions"]
            assert "net" not in manifest["components"][0]["services"]
        if subject == "cards":
            sample = json.loads((folder / "example-course.json").read_text())
            assert all(r["level"] == level for r in sample["cards"])
        assert (folder / "assets" / "LICENSE.txt").is_file()
print("Study content: 20 isolated age packages, deck sizes, choices and licenses passed")
