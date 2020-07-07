


%{


GET_REF4B_5MIN = 50
GET_REF4B_10MIN = 150
GET_REF4B_20MIN = 350
JUST_GEN_WF = 0

TEST_LOOP = GET_REF4B_5MIN  # GET_REF4B_5MIN !!
TEST_LCD = 32  # 1  # use 1 for test



DUTY = 8
CYCLE = 32
timestr = datetime.now().strftime("%y%m%d_%H%M%S")
genfolder = "ref4b8p" + timestr
port = "COM9"
dataport = "COM10"

# use M-sequence for testing, very efficient!
# XXXX 0000 0111 1010 1100 1000
#  ---   00 0137 FEDA 5B6C 9248
# - for signal 1, 0 for signal 0, other identify the correspond pattern


def genwf(tag):
    assert tag >= 0 and tag < 32
    wf = preamble
    i = 1 << tag
    duty = hex(i).replace('0x','').zfill(8) + ':%d\n00000000:%d\n' % (DUTY, CYCLE-DUTY)
    high = hex(i).replace('0x','').zfill(8) + ':%d\n' % CYCLE
    low = '00000000:%d\n' % CYCLE
    for loop in range(max(TEST_LOOP, 1)):
        wf += high + high + high + high
        wf += low + low + low + low
        wf += low + duty + duty + duty
        wf += duty + low + duty + low
        wf += duty + duty + low + low
        wf += duty + low + low + low
    return wf
a = ((d(2:3,:) - d(1,:))' \ (a - d(1,:))')';
avgd * () ^ -1

%48 periods.

%}

if 1
period = 32;
duty = 12;
totsym = 48;
avg = 50;
trainpak = @(x) [ struct('bps', 2, 'period', period * 4, 'duty', period * 4, 'd', x * [1 0]), ...
 struct('bps', 2, 'period', period, 'duty', duty, 'd', x * [0 1 1 1 1 0 1 0 1 1 0 0 1 0 0 0])];

data_raw = channel_preamble([trainpak(1) trainpak(2)], avg, 0);
ofst = 5446;
avgd = grpacc(data_raw(ofst:end, :), 10 * period * totsym, avg);
timeaux = -0.5+(mod(floor((0:10*period*totsym-1)/(10*period))',2)==1);

load reflib
record = struct2table(struct('dx', datetime(), 'comment', '30d', 'avg', avgd), 'AsArray', true);
if ~exist('tb', 'var')  
	tb = record;
else
	tb = [tb; record];
end
clear avgd;
save reflib
end
figure; plot([avgd, timeaux]);
abort
% do not change
cyclecnt = 24;
mseqmap = [0,1,3,7,15,14,13,10,5,11,6,12,9,2,4,8];

x = readraw(sprintf("%s/%s_%d.raw", refdir, refdir, i - 1)); %
%x = channel_preamble(...)
avgd = grpacc(x(ofst(i):end, :), cycle*cyclecnt, count); %noise
avgd = avgd - sum(avgd(1+cycle*6:cycle*9,:))/cycle/3; %bottom->0
avgd = avgd * (cycle*3/sum(avgd(cycle+1:cycle*4,1+(i<=16)))); %top->1

figure; plot([avgd, -0.5+(mod(floor((0:cycle*cyclecnt-1)/cycle)',2)==1)]);

for k = 1:16
	sliceavg(:,:,mseqmap(k)+1) = avgd(cycle*(7+k)+1:cycle*(8+k),:)'; % extract patt.
end
sliceavg;


