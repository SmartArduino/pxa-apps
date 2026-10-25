"""Compile the real engine selector against a small playlist fixture."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class ThemeSelectionTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_zero_one_and_sparse_playlists_in_both_engines(self):
        vendor = Path(__file__).resolve().parents[3] / 'command-conquer/vendor/Vanilla-Conquer'
        for game in ['redalert', 'tiberiandawn']:
            with self.subTest(game=game), tempfile.TemporaryDirectory() as temporary:
                source = (vendor / game / 'theme.cpp').read_text()
                begin = source.index('ThemeType ThemeClass::Next_Song(')
                end = source.index('\n/****************************************************************', begin)
                const = ' const' if game == 'redalert' else ''
                header = '''#include <cassert>
using ThemeType = int;
constexpr int THEME_NONE=-1, THEME_PICK_ANOTHER=-2, THEME_QUIET=-3;
constexpr int THEME_FIRST=0, THEME_LAST=4;
struct { bool Repeat; } _themes[5];
struct { bool IsScoreRepeat, IsScoreShuffle; } Options;
bool allowed[5]; int random_index;
int Sim_Random_Pick(int lo,int hi) { assert(lo==0 && hi>=0); return random_index%(hi+1); }
class ThemeClass { public: bool Is_Allowed(int i) const { return allowed[i]; }
ThemeType Next_Song(ThemeType theme)CONST; };
'''.replace('CONST', const)
                checks = '''
int main() {
 ThemeClass music;
 for (int shuffle=0;shuffle<2;shuffle++) {
  Options.IsScoreShuffle=shuffle;
  for(bool &v:allowed)v=false;
  assert(music.Next_Song(THEME_NONE)==THEME_NONE);
  assert(music.Next_Song(THEME_PICK_ANOTHER)==THEME_NONE);
  allowed[3]=true;
  assert(music.Next_Song(THEME_NONE)==3);
  assert(music.Next_Song(3)==3);
  allowed[1]=true;
  assert(music.Next_Song(3)==1);
  assert(music.Next_Song(1)==3);
 }
 Options.IsScoreShuffle=true;
 allowed[0]=true;random_index=0;assert(music.Next_Song(3)==0);
 random_index=1;assert(music.Next_Song(3)==1);
}
'''
                path = Path(temporary) / 'selector.cpp'
                path.write_text(header + source[begin:end] + checks)
                binary = path.with_suffix('')
                subprocess.run(['c++', '-std=c++14', str(path), '-o', str(binary)], check=True, capture_output=True)
                subprocess.run([str(binary)], check=True, timeout=2)


if __name__ == '__main__':
    unittest.main()
