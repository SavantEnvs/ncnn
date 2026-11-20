// tools/onnx/onnx2ncnn in-process: protobuf ModelProto parse of an untrusted .onnx file, then
// the graph walk, fuse passes, initializer-tensor reads and ncnn op emission.
#include "tool_wrap.h"
#include "../../tools/onnx/onnx2ncnn.cpp"
#undef main
#undef fopen
#undef fclose

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    static const std::string m = tw_scratch("model.onnx");
    static const std::string op = tw_scratch("out.param");
    static const std::string ob = tw_scratch("out.bin");
    tw_write(m, data, size);
    char* argv[] = {(char*)"onnx2ncnn", (char*)m.c_str(), (char*)op.c_str(), (char*)ob.c_str(), NULL};
    tool_main(4, argv);
    tw_close_leaked();
    tw_cleanup();
    return 0;
}
