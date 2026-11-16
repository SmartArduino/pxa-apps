#!/usr/bin/env python3
"""Include the application, shared SDK and embedded font notices in the app package."""
from pathlib import Path


def main():
    root = Path(__file__).resolve().parents[1]
    sources = [
        root / "LICENSE",
        root / "licenses/PXA-SDK-LICENSE.txt",
        root / "licenses/Noto-NOTICE.txt",
        root / "licenses/Noto-OFL.txt",
    ]
    sections = ["Relic Run: original code, procedural geometry/materials and synthesized music are MIT.\nBranding and UI skin images were generated for this project with OpenAI image generation and are distributed under the project license.\nEmbedded Noto glyphs remain SIL OFL-1.1.\n"]
    for source in sources:
        sections.append("\n=== " + str(source.relative_to(root)) + " ===\n" + source.read_text(encoding="utf-8"))
    target = root / "assets/THIRD_PARTY_LICENSES.txt"
    target.parent.mkdir(exist_ok=True)
    target.write_text("\n".join(sections), encoding="utf-8")
    print("Updated " + str(target))


if __name__ == "__main__":
    main()
