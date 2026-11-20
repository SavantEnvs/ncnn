// ncnn text .param + .bin weights loaded from memory, then inference.
// Input layout: <param text> NUL <model bytes>. Reaches net.cpp/paramdict.cpp (text parser),
// modelbin.cpp + the sized DataReaderFromMemory, and every layer's load_param/load_model/forward.
#include "net_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const uint8_t* nul = (const uint8_t*)memchr(data, 0, size);
    size_t plen = nul ? (size_t)(nul - data) : size;
    std::vector<char> param(data, data + plen);
    param.push_back('\0');
    size_t mlen = nul ? size - plen - 1 : 0;
    // exact-size heap copy so ASan sees reads past the model end (modelbin references into it)
    unsigned char* model = new unsigned char[mlen ? mlen : 1];
    if (mlen)
        memcpy(model, nul + 1, mlen);

    {
        ncnn::Net net;
        fuzz_net_options(net);
        if (net.load_param_mem(param.data()) == 0)
        {
            const unsigned char* mp = model;
            ncnn::DataReaderFromMemory dr(mp, mlen);
            if (net.load_model(dr) == 0)
                fuzz_run_by_index(net, data, size);
        }
    }
    delete[] model;
    return 0;
}
