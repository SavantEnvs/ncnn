// Shared helpers for the ncnn Net harnesses.
#pragma once
#include <stdint.h>
#include <string.h>
#include <vector>

#include "net.h"
#include "datareader.h"

// Deterministic small float input for every network input blob.
static inline ncnn::Mat fuzz_make_input(const uint8_t* data, size_t size)
{
    ncnn::Mat m(8, 8, 3);
    float* p = m;
    for (int i = 0; i < 8 * 8 * 3; i++)
        p[i] = size ? (float)data[i % size] / 32.f - 4.f : 0.5f;
    return m;
}

static inline void fuzz_net_options(ncnn::Net& net)
{
    net.opt.num_threads = 1;
    net.opt.use_vulkan_compute = false;
    net.opt.lightmode = true;
}

// Feed every input by blob index and extract every output by blob index.
static inline void fuzz_run_by_index(ncnn::Net& net, const uint8_t* data, size_t size)
{
    const std::vector<int>& ins = net.input_indexes();
    const std::vector<int>& outs = net.output_indexes();
    if (ins.empty() || outs.empty())
        return;
    ncnn::Extractor ex = net.create_extractor();
    ncnn::Mat in = fuzz_make_input(data, size);
    for (size_t i = 0; i < ins.size(); i++)
        ex.input(ins[i], in);
    for (size_t i = 0; i < outs.size(); i++)
    {
        ncnn::Mat out;
        ex.extract(outs[i], out);
    }
}
