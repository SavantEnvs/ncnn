// tools/caffe/caffe2ncnn in-process: text-protobuf NetParameter parse of an untrusted .prototxt,
// binary-protobuf parse of the .caffemodel, then the layer walk and ncnn op/weight emission.
#include "tool_wrap.h"
#include "../../tools/caffe/caffe2ncnn.cpp"
#undef main
#undef fopen
#undef fclose

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    static const std::string pt = tw_scratch("deploy.prototxt");
    static const std::string cm = tw_scratch("model.caffemodel");
    static const std::string op = tw_scratch("out.param");
    static const std::string ob = tw_scratch("out.bin");
    // Split at the first NUL: prototxt text | caffemodel bytes.
    const uint8_t* nul = (const uint8_t*)memchr(data, 0, size);
    size_t plen = nul ? (size_t)(nul - data) : size;
    tw_write(pt, data, plen);
    tw_write(cm, nul ? nul + 1 : data, nul ? size - plen - 1 : 0);
    char* argv[] = {(char*)"caffe2ncnn", (char*)pt.c_str(), (char*)cm.c_str(), (char*)op.c_str(), (char*)ob.c_str(), NULL};
    tool_main(5, argv);
    tw_close_leaked();
    tw_cleanup();
    return 0;
}
