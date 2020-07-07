import sys, os, math
from dxfwrite import DXFEngine as dxf

sys.path.append(os.getcwd())
from demo_data import data

"""
一共发送了12 bytes，速率为4kbps，持续24ms
数据采样率为40kS/s
前12ms用来画偏振图，后12ms用来画DSM图
每12ms里面有12*40=480个点
"""

FILTER_DISTANCE_SQUARE = math.pow(0.2, 2)
def point_filter(points):
    if len(points) < 2:
        return [p for p in points]
    ret = [points[0]]
    last_point = points[0]
    for i in range(1, len(points)):
        dist2 = math.pow(last_point[0] - points[i][0], 2) + math.pow(last_point[1] - points[i][1], 2)
        if dist2 >= FILTER_DISTANCE_SQUARE:
            ret.append(points[i])
            last_point = points[i]
    return ret

for i in range(16):
    drawing = dxf.drawing('individual_%d.dxf' % i)
    drawing.add(
        dxf.polyline(
            points=point_filter([(j/480*6,e) for j,e in enumerate(data[i][480:960])])
        )
    )
    drawing.save()


all_plot = [ [], [] ]
for j in range(480):
    all_plot[0].append(0.)
    all_plot[1].append(0.)
    for i in range(16):
        if (i%4) < 2:
            all_plot[0][-1] += data[i][j]
        else:
            all_plot[1][-1] += data[i][j]
for k in range(2):
    for r in range(2):
        drawing = dxf.drawing('all_%d_%d.dxf' % (k, r))
        points = [(j/480*6*4, e if r==0 else 9-e) for j,e in enumerate(all_plot[k])]
        reverse = [(e[0], -e[1]) for e in points]
        reverse.reverse()
        points = points + reverse
        points = point_filter(points)
        points.append(points[0])
        drawing.add(
            dxf.polyline(
                points=points
            )
        )
        drawing.save()
