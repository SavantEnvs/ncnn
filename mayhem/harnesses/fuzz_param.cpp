// ncnn text .param graph parser only (no weights, no inference): net.cpp load_param,
// paramdict.cpp text values/arrays/strings, layer creation and every layer's load_param.
#include "net_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    std::vector<char> param(data, data + size);
    param.push_back('\0');
    ncnn::Net net;
    fuzz_net_options(net);
    if (net.load_param_mem(param.data()) != 0)
        return 0;
    size_t n = 0;
    for (size_t i = 0; i < net.input_names().size(); i++)
        n += strlen(net.input_names()[i]);
    for (size_t i = 0; i < net.output_names().size(); i++)
        n += strlen(net.output_names()[i]);
    const std::vector<ncnn::Blob>& blobs = net.blobs();
    for (size_t i = 0; i < blobs.size(); i++)
        n += blobs[i].name.size() + (size_t)(blobs[i].producer + 1);
    const std::vector<ncnn::Layer*>& layers = net.layers();
    for (size_t i = 0; i < layers.size(); i++)
        n += layers[i]->bottoms.size() + layers[i]->tops.size();
    (void)n;
    return 0;
}
