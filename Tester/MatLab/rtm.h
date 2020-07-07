#ifdef __cplusplus

#ifdef WIN32
#define DLLEXPORT  extern "C"  __declspec(dllexport)
#else
#define DLLEXPORT  extern "C"
#endif
#else 
#define DLLEXPORT  __declspec(dllexport) 
#endif
#include <stdbool.h>
DLLEXPORT int seed(int idx);
DLLEXPORT  void init(const char* reader_port, const char* ref_filename, const char* log_filename);
DLLEXPORT float gain_reset() ;

DLLEXPORT void tag_samp_append(int bps, int n_symb, const int *symb_gray, int period, int duty);
DLLEXPORT float channel(float snrthres, float *buffer);
DLLEXPORT void offline_rx(const char* filename, float snrthres, float* buffer);
DLLEXPORT int rx_samp2recv();
DLLEXPORT void test_tag();
DLLEXPORT void car_move(float x, float y, float r);
DLLEXPORT void car_adjust_once();
DLLEXPORT void  enque_calc();
DLLEXPORT int  deque_calc();


#include <decode.h>
