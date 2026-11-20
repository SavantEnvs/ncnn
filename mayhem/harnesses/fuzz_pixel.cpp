// ncnn pixel API (mat_pixel*.cpp): from_pixels(_roi)(_resize), to_pixels(_resize),
// yuv420sp2rgb*, resize_bilinear_c*, kanna_rotate_c*, warpaffine_bilinear_c*, draw_*.
// Header: op, pixel format, src w/h, dst w/h, roi/rotate/affine params; pixels follow.
// All buffers are allocated with the exact size the documented API contract requires.
#include <stdint.h>
#include <string.h>
#include <vector>

#include "mat.h"

namespace {
struct Reader
{
    const uint8_t* d;
    size_t n;
    uint8_t u8() { if (!n) return 0; n--; return *d++; }
    float f32() { uint8_t b[4] = {u8(), u8(), u8(), u8()}; float f; memcpy(&f, b, 4); return f; }
};

const int kFormats[][2] = {
    // {type, channels}
    {ncnn::Mat::PIXEL_RGB, 3}, {ncnn::Mat::PIXEL_BGR, 3}, {ncnn::Mat::PIXEL_GRAY, 1}, {ncnn::Mat::PIXEL_RGBA, 4},
    {ncnn::Mat::PIXEL_BGRA, 4}, {ncnn::Mat::PIXEL_RGB2BGR, 3}, {ncnn::Mat::PIXEL_BGR2GRAY, 3},
    {ncnn::Mat::PIXEL_RGB2GRAY, 3}, {ncnn::Mat::PIXEL_GRAY2RGB, 1}, {ncnn::Mat::PIXEL_GRAY2BGR, 1},
    {ncnn::Mat::PIXEL_RGBA2RGB, 4}, {ncnn::Mat::PIXEL_RGBA2BGR, 4}, {ncnn::Mat::PIXEL_RGBA2GRAY, 4},
    {ncnn::Mat::PIXEL_BGRA2BGR, 4}, {ncnn::Mat::PIXEL_BGRA2GRAY, 4}, {ncnn::Mat::PIXEL_RGB2RGBA, 3},
    {ncnn::Mat::PIXEL_GRAY2RGBA, 1}, {ncnn::Mat::PIXEL_BGRA2RGBA, 4},
};
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // An input shorter than the 10-byte call header (op, format, w, h, tw, th, a, b, c, d) does not
    // describe a call: without this, the missing bytes read as 0, i.e. w = h = 1, so every truncated
    // input is the same 1-px resize_bilinear crash, and the stored corpus's smallest inputs (which
    // libFuzzer and Mayhem's -runs=5 sanity probe execute first) all abort. Full headers still reach
    // the 1-px resize_bilinear and the warpaffine bugs.
    if (size < 10) return 0;
    Reader r = {data, size};
    int op = r.u8() % 8;
    int fmt = r.u8() % (sizeof(kFormats) / sizeof(kFormats[0]));
    int type = kFormats[fmt][0];
    int c = kFormats[fmt][1];
    int w = r.u8() % 64 + 1, h = r.u8() % 64 + 1;
    int tw = r.u8() % 64 + 1, th = r.u8() % 64 + 1;
    int a = r.u8(), b = r.u8(), cc = r.u8(), dd = r.u8();
    int cflat = (op == 3 || op == 4 || op == 5 || op == 6) ? (a % 4) + 1 : c;
    if (cflat == 2) cflat = 1;
    std::vector<unsigned char> src((size_t)w * h * cflat * 2); // *2 covers yuv420sp needs
    for (size_t i = 0; i < src.size(); i++) src[i] = r.u8();

    switch (op)
    {
    case 0:
    {
        ncnn::Mat m = ncnn::Mat::from_pixels(src.data(), type, w, h);
        int otype = (c == 1) ? ncnn::Mat::PIXEL_GRAY : (c == 3 ? ncnn::Mat::PIXEL_RGB : ncnn::Mat::PIXEL_RGBA);
        if (!m.empty() && m.c == c)
        {
            std::vector<unsigned char> out((size_t)tw * th * c);
            m.to_pixels_resize(out.data(), otype, tw, th);
        }
        break;
    }
    case 1:
    {
        ncnn::Mat m = ncnn::Mat::from_pixels_resize(src.data(), type, w, h, tw, th);
        break;
    }
    case 2:
    {
        int rx = a % w, ry = b % h;
        int rw = cc % (w - rx) + 1, rh = dd % (h - ry) + 1;
        ncnn::Mat m = ncnn::Mat::from_pixels_roi_resize(src.data(), type, w, h, rx, ry, rw, rh, tw, th);
        break;
    }
    case 3:
    {
        std::vector<unsigned char> dst((size_t)tw * th * cflat);
        if (cflat == 1) ncnn::resize_bilinear_c1(src.data(), w, h, dst.data(), tw, th);
        else if (cflat == 3) ncnn::resize_bilinear_c3(src.data(), w, h, dst.data(), tw, th);
        else ncnn::resize_bilinear_c4(src.data(), w, h, dst.data(), tw, th);
        break;
    }
    case 4:
    {
        int rt = b % 8 + 1;
        int dw = rt >= 5 ? h : w, dh = rt >= 5 ? w : h;
        std::vector<unsigned char> dst((size_t)dw * dh * cflat);
        if (cflat == 1) ncnn::kanna_rotate_c1(src.data(), w, h, dst.data(), dw, dh, rt);
        else if (cflat == 3) ncnn::kanna_rotate_c3(src.data(), w, h, dst.data(), dw, dh, rt);
        else ncnn::kanna_rotate_c4(src.data(), w, h, dst.data(), dw, dh, rt);
        break;
    }
    case 5:
    {
        float tm[6];
        for (int i = 0; i < 6; i++) tm[i] = r.f32();
        std::vector<unsigned char> dst((size_t)tw * th * cflat);
        int bt = b % 3;
        if (cflat == 1) ncnn::warpaffine_bilinear_c1(src.data(), w, h, dst.data(), tw, th, tm, bt, cc);
        else if (cflat == 3) ncnn::warpaffine_bilinear_c3(src.data(), w, h, dst.data(), tw, th, tm, bt, cc);
        else ncnn::warpaffine_bilinear_c4(src.data(), w, h, dst.data(), tw, th, tm, bt, cc);
        break;
    }
    case 6:
    {
        int x = (int)(signed char)r.u8(), y = (int)(signed char)r.u8();
        int fs = r.u8() % 32 + 1;
        char text[32];
        int tl = r.u8() % 31;
        for (int i = 0; i < tl; i++) text[i] = (char)(r.u8() % 95 + 32);
        text[tl] = 0;
        unsigned int color = (unsigned)a | (b << 8) | (cc << 16);
        int thick = (int)(signed char)dd % 8;
        if (cflat == 1) { ncnn::draw_rectangle_c1(src.data(), w, h, x, y, tw, th, color, thick); ncnn::draw_text_c1(src.data(), w, h, text, x, y, fs, color); ncnn::draw_circle_c1(src.data(), w, h, x, y, tw, color, thick); ncnn::draw_line_c1(src.data(), w, h, x, y, tw, th, color, thick); }
        else if (cflat == 3) { ncnn::draw_rectangle_c3(src.data(), w, h, x, y, tw, th, color, thick); ncnn::draw_text_c3(src.data(), w, h, text, x, y, fs, color); ncnn::draw_circle_c3(src.data(), w, h, x, y, tw, color, thick); ncnn::draw_line_c3(src.data(), w, h, x, y, tw, th, color, thick); }
        else { ncnn::draw_rectangle_c4(src.data(), w, h, x, y, tw, th, color, thick); ncnn::draw_text_c4(src.data(), w, h, text, x, y, fs, color); ncnn::draw_circle_c4(src.data(), w, h, x, y, tw, color, thick); ncnn::draw_line_c4(src.data(), w, h, x, y, tw, th, color, thick); }
        break;
    }
    case 7:
    {
        int ew = w & ~1 ? w & ~1 : 2, eh = h & ~1 ? h & ~1 : 2;
        std::vector<unsigned char> yuv((size_t)ew * eh * 3 / 2);
        for (size_t i = 0; i < yuv.size(); i++) yuv[i] = src[i % src.size()];
        std::vector<unsigned char> rgb((size_t)ew * eh * 3);
        if (a % 3 == 0) ncnn::yuv420sp2rgb(yuv.data(), ew, eh, rgb.data());
        else if (a % 3 == 1) ncnn::yuv420sp2rgb_nv12(yuv.data(), ew, eh, rgb.data());
        else { std::vector<unsigned char> half((size_t)(ew / 2) * (eh / 2) * 3); ncnn::yuv420sp2rgb_half(yuv.data(), ew, eh, half.data()); }
        break;
    }
    }
    return 0;
}
