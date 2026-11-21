"""Check alternating planted feet, crossing order and fixed bone lengths."""
from pathlib import Path
import importlib.util, math

root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('walk',root/'tools/build_walk.py')
walk=importlib.util.module_from_spec(spec);spec.loader.exec_module(walk)
for frame in range(8):
    for near in [False,True]:
        hip,knee,foot=walk.joints(frame,near)
        assert abs(math.dist(hip,knee)-27)<1e-6
        assert abs(math.dist(knee,foot)-25)<1e-6
        assert 15<=foot[0]<=80 and 100<=foot[1]<=112
assert walk.joints(0,True)[2][0]>walk.joints(0,False)[2][0]
assert walk.joints(4,True)[2][0]<walk.joints(4,False)[2][0]
assert walk.joints(2,False)[2][1]<walk.joints(2,True)[2][1]
assert walk.joints(6,True)[2][1]<walk.joints(6,False)[2][1]
# In a forward stride each frame moves the root 10px; the planted foot stays.
for near,frames in [(True,range(5)),(False,range(4,9))]:
    positions=[walk.joints(n%8,near)[2][0]+n*10 for n in frames]
    assert max(positions)-min(positions)<1e-6,positions
print('Pose reference: both legs alternate lead, plant and swing; joints stay connected; stance is planted.')
