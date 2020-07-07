import os, math

# 运行要求：256个系数点，不能有任何一个超过0.1

dataname = "ls_255_910K_10K_25K_70dB"
filename = dataname + ".fcf"
output = dataname + ".h"

with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), filename)) as f:
    data = f.read()
fs = data.split("\n")
dat = []
for i,e in enumerate(fs):
    if e[:10] == "Numerator:":
        for j in range(256):
            dat.append(float(fs[i+j+1]))
        break
s = 0
for e in dat:
    s += e
print("coefficient sum is %f" % s)

# ADC：12bit，1bit留作符号位，剩下32bit。因为是256个数累加，每个数最多是31-8=23bit，从这个点尝试起，直到所有的系数都小于2^15=32768
m = 0
for e in dat:
    a = abs(e)
    if a > m:
        m = a
print("the maximum number is %f" % m)

# l = math.log(32768 * m, 2)
# mb = 15 - math.ceil(l)
# a = (2**mb) * 32768
# print("use %f bit actually, ceil is %d bit, can have 15+%d bit larger, that is %d" % (l, math.ceil(l), mb, a))
# adat = []
# for e in dat:
#     adat.append(int(a * e))
# print(adat)

zfdat = []
demod = 1
for e in dat:
    zfdat.append(e * demod)
    demod *= -1
print(zfdat)

sum_positive = 0
sum_negative = 0
for e in zfdat:
    if e > 0: sum_positive += e
    else: sum_negative += e
print(sum_positive, sum_negative)

# 最差情况是所有正系数都是4095，所有负系数都是0，这样得出来的数是最大的，这时候设可以放大2^l倍，那么要求总的数不能大约2^31
print(math.log(2**31/4095/0.73, 2))
l = math.floor(math.log(2**31/4095/0.73, 2))
print(l)

a = (2**l)

adat = []
for e in zfdat:
    adat.append(int(a * e))
print(adat)

gain_index = 3  # 设置输出的增益，必须是2^n
gain = 2**gain_index

prefix = """#include <stdint.h>

// filter: DATANAME
// gain: %d

int demod_amp_DATANAME = %d;
uint8_t demod_rsh_DATANAME = %d;

int16_t demod_coeff_DATANAME[256] = {
""" % (gain, a, l - gain_index)
prefix = prefix.replace("DATANAME", dataname)

appendix = """};\n"""

with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), output), "w") as f:
    f.write(prefix)
    for i in range(16):
        f.write("    ")
        for j in range(16):
            f.write("%d" % adat[16*i+j])
            if not (i == 15 and j == 15):
                f.write(", ")
        f.write("\n")
    f.write(appendix)
