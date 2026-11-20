// ncnn binary .param.bin + .bin weights loaded from memory, then inference.
// Input layout: uint32 LE length L | L bytes param.bin | model bytes.
#include "net_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 4)
        return -1;
    uint32_t l = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    size_t rest = size - 4;
    size_t plen = l < rest ? l : rest;
    size_t mlen = rest - plen;
    unsigned char* pbin = new unsigned char[plen ? plen : 1];
    unsigned char* model = new unsigned char[mlen ? mlen : 1];
    if (plen)
        memcpy(pbin, data + 4, plen);
    if (mlen)
        memcpy(model, data + 4 + plen, mlen);

    {
        ncnn::Net net;
        fuzz_net_options(net);
        const unsigned char* pp = pbin;
        ncnn::DataReaderFromMemory pdr(pp, plen);
        if (net.load_param_bin(pdr) == 0)
        {
            const unsigned char* mp = model;
            ncnn::DataReaderFromMemory mdr(mp, mlen);
            if (net.load_model(mdr) == 0)
                fuzz_run_by_index(net, data, size);
        }
    }
    delete[] pbin;
    delete[] model;
    return 0;
}
