// Known-answer test for mayhem/test.sh: run upstream's bundled SqueezeNet v1.1 on a deterministic
// synthetic image through both the text and binary param loaders and print the top-3 classes.
#include <stdio.h>
#include <algorithm>
#include <vector>

#include "net.h"

static int run(bool binary, const char* param, const char* bin)
{
    ncnn::Net net;
    net.opt.num_threads = 1;
    net.opt.use_vulkan_compute = false;
    int r = binary ? net.load_param_bin(param) : net.load_param(param);
    if (r != 0 || net.load_model(bin) != 0)
    {
        fprintf(stderr, "load failed\n");
        return 1;
    }
    std::vector<unsigned char> px(227 * 227 * 3);
    for (size_t i = 0; i < px.size(); i++)
        px[i] = (unsigned char)((i * 7 + (i / 681) * 13) & 0xff);
    ncnn::Mat in = ncnn::Mat::from_pixels_resize(px.data(), ncnn::Mat::PIXEL_BGR, 227, 227, 227, 227);
    const float mean_vals[3] = {104.f, 117.f, 123.f};
    in.substract_mean_normalize(mean_vals, 0);
    ncnn::Extractor ex = net.create_extractor();
    if (binary)
        ex.input(0, in); // squeezenet_v1.1.id.h: BLOB_data = 0
    else
        ex.input("data", in);
    ncnn::Mat out;
    if (binary ? ex.extract(net.output_indexes()[0], out) : ex.extract("prob", out))
        return 1;
    std::vector<std::pair<float, int> > v;
    for (int j = 0; j < out.w; j++)
        v.push_back(std::make_pair(out[j], j));
    std::partial_sort(v.begin(), v.begin() + 3, v.end(), std::greater<std::pair<float, int> >());
    printf("%s classes=%d top=%d,%d,%d p0=%.2f\n", binary ? "bin" : "txt", out.w, v[0].second, v[1].second, v[2].second, v[0].first);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc != 4)
        return 2;
    return run(false, argv[1], argv[3]) | run(true, argv[2], argv[3]);
}
