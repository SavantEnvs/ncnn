// tools/ncnnmerge in-process: the input is a text .param merged with itself
// (fscanf-based layer/blob name copy with fixed buffers, bin concatenation).
#include "tool_wrap.h"
#include "../../tools/ncnnmerge.cpp"
#undef main
#undef fopen
#undef fclose

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    static const std::string p = tw_scratch("model.param");
    static const std::string b = tw_scratch("model.bin");
    static const std::string op = tw_scratch("out.param");
    static const std::string ob = tw_scratch("out.bin");
    // Split at the first NUL: param text | weights bytes.
    const uint8_t* nul = (const uint8_t*)memchr(data, 0, size);
    size_t plen = nul ? (size_t)(nul - data) : size;
    tw_write(p, data, plen);
    tw_write(b, nul ? nul + 1 : data, nul ? size - plen - 1 : 0);
    char* argv[] = {(char*)"ncnnmerge", (char*)p.c_str(), (char*)b.c_str(), (char*)p.c_str(), (char*)b.c_str(),
                    (char*)op.c_str(), (char*)ob.c_str(), NULL};
    tool_main(7, argv);
    tw_close_leaked();
    tw_cleanup();
    return 0;
}
