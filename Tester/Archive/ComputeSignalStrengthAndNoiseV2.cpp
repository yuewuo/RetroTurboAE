/*
 * 关于信号强度随距离的变化，以及噪声水平
 * 信号强度还有增益一项，但假设存储在数据库里，本程序只计算波形部分的幅值
 * 
 * 20191212
 * 改进：使用相关运算来寻找峰值，而不是找边沿（对噪声过于敏感）
 */

#define MONGODAT_IMPLEMENTATION
#include "mongodat.h"
#include "sysutil.h"
#include "Demodulate_WY_190715.h"
#include <vector>
#include <string>
#include <map>
#include <complex>
#include <algorithm>
using namespace std;

const char *MONGO_URL, *MONGO_DATABASE;
MongoDat mongodat;

int main(int argc, char** argv) {
	HANDLE_DATA_BASIC_ARG_MODIFY_ARGC_ARGV(argc, argv, &MONGO_URL, &MONGO_DATABASE);

	MongoDat::LibInit();
	mongodat.open(MONGO_URL, MONGO_DATABASE);

	if (argc != 3) {
		printf("usage: <collection> <id(12 byte hex = 24 char)>");
		return -1;
	}

	const char* collection_str = argv[1];
	const char* record_id_str = argv[2];

    BsonOp record = mongodat.get_bsonop(collection_str, record_id_str);
	assert(record["record_id"].existed() && record["record_id"].type() == BSON_TYPE_UTF8);
	string data_record_id_str = record["record_id"].value<string>();

	bson_oid_t record_id = MongoDat::parseOID(data_record_id_str.c_str());
	vector<char> binary = mongodat.get_binary_file(record_id);
	typedef struct { int16_t s[4]; } uni_out_t;
	const uni_out_t* buffer2 = (const uni_out_t*)binary.data();
	int length = binary.size() / sizeof(uni_out_t);
    printf("binary size: %d, has %d samples\n", (int)binary.size(), (int)(binary.size() / sizeof(uni_out_t)));

	double sample_rate = 56875;
#define EDGE_MS 4
    int edge_interval = EDGE_MS * sample_rate / 1000;

    double tag_frequency = 20;
    int period_length = sample_rate / tag_frequency;
    int period_count = 0;
    vector< complex<float> > single_period; single_period.resize(period_length);
    vector< complex<float> > period_1s; vector< complex<float> > period_0s; 
    for (int i=0; (i+1)*sample_rate/20 < length; ++i, ++period_count) {
        int bias = i*sample_rate/20;
        for (int j=0; j<period_length; ++j) {
            uni_out_t x = buffer2[bias+j];
            single_period[j] += complex<float>(x.s[0], x.s[1]);
        }
    }
    for (int j=0; j<period_length; ++j) {
        single_period[j] /= period_count;
    }
    vector<float> Is; Is.resize(period_length);
    vector<float> Qs; Qs.resize(period_length);
    for (int j=0; j<period_length; ++j) {
        Is[j] = single_period[j].real();
        Qs[j] = single_period[j].imag();
    }

    // // save single_period (optional for debug)
    // mongodat.upload_record("single_period", (float*)single_period.data(), 2, single_period.size()
    //     , NULL, "single_period", 1/56.875, "time(ms)", 1, "I,Q");
    // printf("upload single_period with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());

    // 使用相关运算的方式来寻找一个周期
    // 在一个周期里算最小二乘 min[A*R(x)+C-f(x)]^2，其中R(x)=-1,-1,-1,-1,...,1,1,1,1
    complex<float> C = accumulate(single_period.begin(), single_period.end(), complex<float>(0,0)) / (float)single_period.size();
    vector< complex<float> > no_bias;
    int single_period_size = (int)single_period.size();
    for (int i=0; i<single_period_size; ++i) {
        no_bias.push_back(single_period[i] - C);
    }
    int half = single_period_size / 2;
    double minAbsDiff = 0;
    int mid_idx = 0;
    for (int i=0; i<single_period_size; ++i) {
        complex<float> A = 0;
        for (int j=0; j<half; ++j) A += - no_bias[(i+j)%single_period_size] + no_bias[(i+j+half)%single_period_size];
        A /= single_period_size;
        // 计算偏差
        double diff = 0;
        for (int j=0; j<half; ++j) {
            auto a = no_bias[(i+j)%single_period_size] + A;
            auto b = no_bias[(i+j+half)%single_period_size] - A;
            diff += a.real()*a.real() + a.imag()*a.imag() + b.real()*b.real() + b.imag()*b.imag();
        }
        diff /= single_period_size;
        if (i == 0 || diff < minAbsDiff) {
            minAbsDiff = diff;
            mid_idx = i;
        }
    }

    printf("mid_idx: %d (%f ms)\n", mid_idx, (double)mid_idx / sample_rate * 1000);
    // 把序列沿着这个中间值拼接
    vector< complex<float> > aligned_period; aligned_period.resize(period_length);
    for (int j=0; j<period_length; ++j) {
        aligned_period[j] = single_period[(j+mid_idx)%period_length];
    }
    for (int i=0; (i+1)*sample_rate/20 + mid_idx < length; ++i, ++period_count) {
        int bias = i*sample_rate/20 + mid_idx;
        for (int j=bias+edge_interval; j<bias+period_length/2-edge_interval; ++j) {
            period_1s.push_back(complex<float>(buffer2[j].s[0], buffer2[j].s[1]));
            period_0s.push_back(complex<float>(buffer2[j+period_length/2].s[0], buffer2[j+period_length/2].s[1]));
        }
    }

	// save aligned_period
	mongodat.upload_record("aligned_period", (float*)aligned_period.data(), 2, aligned_period.size()
		, NULL, "aligned_period", 1/56.875, "time(ms)", 1, "I,Q");
	printf("upload aligned_period with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
    record["aligned_period"] = MongoDat::OID2str(mongodat.get_fileID());

    // 最后两段平均
    int start = edge_interval;
    int end = period_length / 2 - edge_interval;
    assert(end > start);
    complex<float> stage1, stage2;
    for (int j=start; j<end; ++j) {
        stage1 += aligned_period[j];
        stage2 += aligned_period[j+period_length/2];
    }
    stage1 /= (end-start);
    stage2 /= (end-start);
    printf("stage1: %f + %fj, stage2: %f + %fj\n", stage1.real(), stage1.imag(), stage2.real(), stage2.imag());

    double Iamp = stage2.real() - stage1.real();
    double Qamp = stage2.imag() - stage1.imag();
    double amp = sqrt(Iamp*Iamp + Qamp*Qamp);
    printf("Iamp: %f, Qamp: %f, amp: %f\n", Iamp, Qamp, amp);

    // 计算噪声，这里只算方差，如果要详细结果可以用 period_1s 和 period_0s 看频谱
    // 消除平均值
    complex<float> avr1s = accumulate(period_1s.begin(), period_1s.end(), complex<float>(0,0)) / (float)period_1s.size();
    complex<float> avr0s = accumulate(period_0s.begin(), period_0s.end(), complex<float>(0,0)) / (float)period_0s.size();
    for (auto it=period_1s.begin(); it!=period_1s.end(); ++it) *it -= avr1s;
    for (auto it=period_0s.begin(); it!=period_0s.end(); ++it) *it -= avr0s;
	mongodat.upload_record("noise_1s", (float*)period_1s.data(), 2, period_1s.size()
		, NULL, "noise_1s", 1/56.875, "time(ms)", 1, "I,Q");
	printf("upload noise_1s with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
    record["noise_1s"] = MongoDat::OID2str(mongodat.get_fileID());
	mongodat.upload_record("noise_0s", (float*)period_0s.data(), 2, period_0s.size()
		, NULL, "noise_0s", 1/56.875, "time(ms)", 1, "I,Q");
	printf("upload noise_0s with ID: %s\n", MongoDat::OID2str(mongodat.get_fileID()).c_str());
    record["noise_0s"] = MongoDat::OID2str(mongodat.get_fileID());
    // 计算方差
    double square1s = 0;
    for (auto it=period_1s.begin(); it!=period_1s.end(); ++it) square1s += it->real() * it->real() + it->imag() * it->imag();
    square1s = sqrt(square1s / (double)period_1s.size());
    double square0s = 0;
    for (auto it=period_0s.begin(); it!=period_0s.end(); ++it) square0s += it->real() * it->real() + it->imag() * it->imag();
    square0s = sqrt(square0s / (double)period_0s.size());
    printf("square1s: %f, square0s: %f\n", square1s, square0s);

    record["Iamp"] = Iamp;
    record["Qamp"] = Qamp;
    record["amp"] = amp;
    record["mid_idx"] = mid_idx;
    record["square1s"] = square1s;
    record["square0s"] = square0s;
    record.save();

	mongodat.close();
	record.remove();

	return 0;
}
