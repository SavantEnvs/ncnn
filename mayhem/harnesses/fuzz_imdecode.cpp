// ncnn simpleocv cv::imdecode (bundled stb_image: jpeg/png/bmp/gif/psd/tga/hdr/pnm...), then the
// decoded pixels go through ncnn::Mat::from_pixels_resize / to_pixels (mat_pixel*.cpp).
#include <stdint.h>
#include <vector>

#include "mat.h"
#include "simpleocv.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // simpleocv emulates OpenCV's cv::imdecode, whose contract requires a non-empty buffer
    // (OpenCV: CV_Assert(buf.checkVector(1, CV_8U) > 0)); simpleocv takes &buf.front() unchecked.
    if (size < 2)
        return -1;
    int flags = (data[0] & 1) ? cv::IMREAD_GRAYSCALE : cv::IMREAD_COLOR;
    std::vector<uchar> buf(data + 1, data + size);
    cv::Mat img = cv::imdecode(buf, flags);
    if (img.empty())
        return 0;
    int type = img.channels() == 1 ? ncnn::Mat::PIXEL_GRAY : ncnn::Mat::PIXEL_BGR2RGB;
    ncnn::Mat m = ncnn::Mat::from_pixels_resize(img.data, type, img.cols, img.rows, 17, 13);
    int otype = img.channels() == 1 ? ncnn::Mat::PIXEL_GRAY : ncnn::Mat::PIXEL_RGB;
    std::vector<unsigned char> out((size_t)m.w * m.h * img.channels());
    m.to_pixels(out.data(), otype);
    return 0;
}
