// ncnn C API (c_api.cpp): net loading through ncnn_net_load_param_memory (text) or a
// caller-supplied ncnn_datareader_t (binary param), weights via ncnn_net_load_model_datareader,
// then ncnn_extractor_* inference. Input: selector byte | <param> NUL-or-len | <model>.
#include <stdint.h>
#include <string.h>
#include <vector>

#include "c_api.h"

struct MemReader
{
    const unsigned char* p;
    size_t left;
};

// The reader the callback serves; set for each load below, so it never outlives one input.
static MemReader* g_reader;

static size_t mem_read(ncnn_datareader_t, void* buf, size_t size)
{
    MemReader* r = g_reader;
    if (size > r->left)
        return 0;
    memcpy(buf, r->p, size);
    r->p += size;
    r->left -= size;
    return size;
}

// A caller-supplied reader: ncnn_datareader_create() with its read callback replaced.
static ncnn_datareader_t make_reader(MemReader* r)
{
    g_reader = r;
    ncnn_datareader_t dr = ncnn_datareader_create();
    dr->read = mem_read;
    return dr;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1)
        return -1;
    const bool binary = data[0] & 1;
    data++;
    size--;

    std::vector<unsigned char> param, model;
    if (binary)
    {
        if (size < 2)
            return -1;
        size_t l = data[0] | (data[1] << 8);
        data += 2;
        size -= 2;
        if (l > size)
            l = size;
        param.assign(data, data + l);
        model.assign(data + l, data + size);
    }
    else
    {
        const uint8_t* nul = (const uint8_t*)memchr(data, 0, size);
        size_t plen = nul ? (size_t)(nul - data) : size;
        param.assign(data, data + plen);
        param.push_back(0);
        if (nul)
            model.assign(nul + 1, data + size);
    }

    ncnn_option_t opt = ncnn_option_create();
    ncnn_option_set_num_threads(opt, 1);
    ncnn_option_set_use_vulkan_compute(opt, 0);
    ncnn_net_t net = ncnn_net_create();
    ncnn_net_set_option(net, opt);

    int ret;
    if (binary)
    {
        MemReader r = {param.data(), param.size()};
        ncnn_datareader_t dr = make_reader(&r);
        ret = ncnn_net_load_param_bin_datareader(net, dr);
        ncnn_datareader_destroy(dr);
    }
    else
    {
        ret = ncnn_net_load_param_memory(net, (const char*)param.data());
    }

    if (ret == 0)
    {
        MemReader r = {model.data(), model.size()};
        ncnn_datareader_t dr = make_reader(&r);
        int mret = ncnn_net_load_model_datareader(net, dr);
        ncnn_datareader_destroy(dr);
        g_reader = NULL;
        if (mret == 0)
        {
            int nin = ncnn_net_get_input_count(net);
            int nout = ncnn_net_get_output_count(net);
            if (nin > 0 && nout > 0)
            {
                ncnn_extractor_t ex = ncnn_extractor_create(net);
                ncnn_mat_t in = ncnn_mat_create_3d(8, 8, 3, NULL);
                ncnn_mat_fill_float(in, 0.25f);
                for (int i = 0; i < nin; i++)
                    ncnn_extractor_input_index(ex, ncnn_net_get_input_index(net, i), in);
                for (int i = 0; i < nout; i++)
                {
                    ncnn_mat_t out = NULL;
                    if (ncnn_extractor_extract_index(ex, ncnn_net_get_output_index(net, i), &out) == 0 && out)
                        ncnn_mat_destroy(out);
                }
                ncnn_mat_destroy(in);
                ncnn_extractor_destroy(ex);
            }
        }
    }

    ncnn_net_destroy(net);
    ncnn_option_destroy(opt);
    return 0;
}
