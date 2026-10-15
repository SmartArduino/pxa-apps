"""Read actual pixel geometry from a PXA_BROWSER_TRACE=1 test package."""
import re


def geometry(logs):
    matches = re.findall(r'BROWSER VIEWPORT address=([\d,]+) content=([\d,]+) toolbar=([\d,]+)', logs)
    assert matches, 'build the test package with PXA_BROWSER_TRACE=1'
    result = {key: tuple(map(int, value.split(',')))
              for key, value in zip(('address', 'content', 'toolbar'), matches[-1])}
    status = re.findall(r'BROWSER VIEWPORT[^\n]* status=([\d,]+)', logs)
    if status: result['status'] = tuple(map(int, status[-1].split(',')))
    return result


def navigation(layout, index, count=5):
    x, y, width, height = layout['toolbar']
    return x + (2*index+1)*width//(2*count), y + height//2


def content_crop(layout, inset=1):
    x, y, width, height = layout['content']
    return x+inset, y+inset, x+width-inset, y+height-inset
