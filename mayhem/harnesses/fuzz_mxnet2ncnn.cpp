// tools/mxnet/mxnet2ncnn in-process: hand-rolled MXNet symbol JSON reader, attr parsers,
// NDArray .params binary reader, fuse passes and ncnn op emission.
#include "tool_wrap.h"
#include "../../tools/mxnet/mxnet2ncnn.cpp"
#undef main
#undef fopen
#undef fclose

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    static const std::string j = tw_scratch("symbol.json");
    static const std::string pa = tw_scratch("model.params");
    static const std::string op = tw_scratch("out.param");
    static const std::string ob = tw_scratch("out.bin");
    // Split at the first NUL: symbol json | params bytes.
    const uint8_t* nul = (const uint8_t*)memchr(data, 0, size);
    size_t jlen = nul ? (size_t)(nul - data) : size;
    tw_write(j, data, jlen);
    tw_write(pa, nul ? nul + 1 : data, nul ? size - jlen - 1 : 0);
    char* argv[] = {(char*)"mxnet2ncnn", (char*)j.c_str(), (char*)pa.c_str(), (char*)op.c_str(), (char*)ob.c_str(), NULL};
    tool_main(5, argv);
    tw_close_leaked();
    tw_cleanup();
    return 0;
}
