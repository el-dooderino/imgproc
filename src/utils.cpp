
#include "utils.hh"

#include "globals.hh"

#include <gvs_defer.hh>
#include <gvs_dynbuf.hh>
#include <gvs_timer.hh>
#include <gvs_utils.hh>

#include <cstdio>

#include <jpeglib.h>
#include <png.h>

namespace {

struct png_mem_read_st {
    gvs::dynbuf<uint8_t> *mem{};
    size_t pos{8}; // skip header
};

void png_mem_read(png_structp pngPtr, png_bytep data, png_size_t length) {
    auto a = (png_mem_read_st *) png_get_io_ptr(pngPtr);
    if (a->pos >= a->mem->size()) return;
    const auto cpl = std::min(length, a->mem->size() - a->pos);
    ::memcpy(data, a->mem->data() + a->pos, cpl);
    a->pos += cpl;
}

void png_error_fn(png_structp png_ptr, png_const_charp error_msg) {
    throw gvs::exception{"%s", error_msg};
}

void png_warning_fn(png_structp png_ptr, png_const_charp warning_msg) {
    png_voidp error_ptr = png_get_error_ptr(png_ptr);
}

void jpeg_error_exit(j_common_ptr cinfo) {
    char msg[JMSG_LENGTH_MAX];
    (*(cinfo->err->format_message))(cinfo, msg);
    throw gvs::exception{"%s", msg};
};

void jpeg_emit_message(j_common_ptr cinfo, int lvl) {
    if (lvl < 0) {
        char msg[JMSG_LENGTH_MAX];
        (*(cinfo->err->format_message))(cinfo, msg);
        throw gvs::exception{"%s", msg};
    }
};

std::optional<bmp_t> do_png(gvs::dynbuf<uint8_t> *mem, const std::string &fn) {
    /* initialize stuff */
    try {
        auto *pngp = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, png_error_fn, png_warning_fn);
        if (!pngp) return {};
        auto png_free = gvs::defer([&pngp] { png_destroy_read_struct(&pngp, nullptr, nullptr); });

        auto *infop = png_create_info_struct(pngp);
        if (!infop) return {};
        auto info_free = gvs::defer([&pngp, &infop] { png_destroy_info_struct(pngp, &infop); });

        png_mem_read_st read_st{.mem = mem};
        png_set_read_fn(pngp, &read_st, png_mem_read);

        png_set_sig_bytes(pngp, 8);

        if (setjmp(png_jmpbuf(pngp))) throw gvs::exception{"[read_png_file] Error during read_info"};
        png_read_info(pngp, infop);

        auto color_type = png_get_color_type(pngp, infop);
        auto bit_depth = png_get_bit_depth(pngp, infop);
        if (bit_depth == 16) png_set_strip_16(pngp);

        if (png_get_valid(pngp, infop, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(pngp);
        switch (color_type) {
            case PNG_COLOR_TYPE_PALETTE:
                png_set_palette_to_rgb(pngp);
                break;
            case PNG_COLOR_TYPE_GRAY:
                if (bit_depth < 8) png_set_expand_gray_1_2_4_to_8(pngp);
                png_set_gray_to_rgb(pngp);
                break;
            case PNG_COLOR_TYPE_GRAY_ALPHA:
                png_set_strip_alpha(pngp);
                png_set_gray_to_rgb(pngp);
                break;
            case PNG_COLOR_TYPE_RGB:
                break;
            case PNG_COLOR_TYPE_RGBA:
                png_set_strip_alpha(pngp);
                break;
        }

        png_read_update_info(pngp, infop);

        auto width = png_get_image_width(pngp, infop);
        auto height = png_get_image_height(pngp, infop);
        color_type = png_get_color_type(pngp, infop);
        bit_depth = png_get_bit_depth(pngp, infop);
        const auto rowlen = png_get_rowbytes(pngp, infop);
//    printf("png %dx%d %d %d, %ld\n", width, height, color_type, bit_depth, rowlen/width);

        bmp_t bmp{width, height, static_cast<u_int>(rowlen / width)};

        gvs::dynbuf<png_bytep> rows{height};
        for (int y{}; y < height; ++y) rows[y] = bmp.row(y);

        if (setjmp(png_jmpbuf(pngp))) throw gvs::exception{"[read_png_file] Error during read_image"};
        png_read_image(pngp, rows.data());

        return {std::move(bmp)};
    } catch (const std::exception &ex) {
        fprintf(stderr, "%s: %s\n", fn.c_str(), ex.what());
    }
    return {};
}

std::optional<point_t> do_png_header(const std::string &fn) {
    try {
        FILE *fp = ::fopen(fn.c_str(), "r");
        if (!fp) throw gvs::exception{"%s: %s", fn.c_str(), strerror(errno)};
        auto file_close = gvs::defer([fp] { ::fclose(fp); });

        auto *pngp = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, png_error_fn, png_warning_fn);
        if (!pngp) return {};
        auto png_free = gvs::defer([&pngp] { png_destroy_read_struct(&pngp, nullptr, nullptr); });

        auto *infop = png_create_info_struct(pngp);
        if (!infop) return {};
        auto info_free = gvs::defer([&pngp, &infop] { png_destroy_info_struct(pngp, &infop); });

        if (setjmp(png_jmpbuf(pngp))) return {};

        png_init_io(pngp, fp);
        png_read_info(pngp, infop);

        auto width = png_get_image_width(pngp, infop);
        auto height = png_get_image_height(pngp, infop);

        return point_t{width, height};
    } catch (const std::exception &ex) {
        fprintf(stderr, "%s: %s\n", fn.c_str(), ex.what());
    }
    return {};
}

std::optional<bmp_t> do_jpeg(gvs::dynbuf<uint8_t> *mem, const std::string &fn) {
    jpeg_error_mgr jerr;
    jpeg_decompress_struct cinfo{.err = jpeg_std_error(&jerr)};
    jerr.error_exit = jpeg_error_exit;
    jerr.emit_message = jpeg_emit_message;

    try {
        jpeg_create_decompress(&cinfo);
        auto freeer = [ptr = &cinfo] { jpeg_destroy_decompress(ptr); };
        // Variables for the decompressor itself
        jpeg_mem_src(&cinfo, mem->data(), mem->size());
        jpeg_read_header(&cinfo, true);
        jpeg_start_decompress(&cinfo);
        switch (cinfo.output_components) {
            case 3:
            case 1: {
                bmp_t bmp{cinfo.output_width, cinfo.output_height, static_cast<u_int>(cinfo.output_components)};
                while (cinfo.output_scanline < cinfo.output_height) {
                    unsigned char *barr[1]{bmp.row(cinfo.output_scanline)};
                    jpeg_read_scanlines(&cinfo, barr, 1);
                }
                jpeg_finish_decompress(&cinfo);

                return {std::move(bmp)};
            }

            default:
                throw gvs::exception{"unexpected pixel size %d", cinfo.output_components};
        }
    } catch (const std::exception &ex) {
        fprintf(stderr, "%s: %s\n", fn.c_str(), ex.what());
    }

    return {};
}

std::optional<point_t> do_jpeg_header(const std::string &fn) {
    jpeg_error_mgr jerr;
    jpeg_decompress_struct cinfo{.err = jpeg_std_error(&jerr)};
    jerr.error_exit = jpeg_error_exit;
    jerr.emit_message = jpeg_emit_message;

    try {
        jpeg_create_decompress(&cinfo);
        auto freeer = [ptr = &cinfo] { jpeg_destroy_decompress(ptr); };
        FILE *fp = ::fopen(fn.c_str(), "r");
        if (!fp) throw gvs::exception{"%s: %s", fn.c_str(), strerror(errno)};
        auto file_closer = gvs::defer([fp] { ::fclose(fp); });

        jpeg_stdio_src(&cinfo, fp);
        jpeg_read_header(&cinfo, true);
        return point_t{cinfo.image_width, cinfo.image_height};
    } catch (const std::exception &ex) {
        fprintf(stderr, "%s: %s\n", fn.c_str(), ex.what());
    }

    return {};
}

}

std::optional<bmp_t> read_img(const std::string &fn) {
    try {
        gvs::timer timer;
        auto buf = gvs::utl::read_file<gvs::dynbuf<uint8_t>>(fn, 100);
        g::read_avgs.w([&buf, &timer] (auto &z) {
            z.sz += buf.size();
            ++z.cnt;
            z.dur += timer.dur<std::chrono::milliseconds>();
        });

        if (buf.size() < 128) return {};
        if (!png_sig_cmp(buf.data(), 0, 8)) return do_png(&buf, fn);
        return do_jpeg(&buf, fn);
    }
    catch (const gvs::exception &ex) {
        fprintf(stderr, "%s: %s\n", fn.c_str(), ex.what());
    }

    return {};
}

std::optional<point_t> read_img_header(const std::string &fn) {
    try {
        gvs::timer timer;
        if (gvs::utl::statx(fn).st_size < 128) return {};
        gvs::dynbuf<uint8_t, 1> buf;
        gvs::fd{fn, O_RDONLY}.read(buf, 128);
        if (!png_sig_cmp(buf.data(), 0, 8)) return do_png_header(fn);
        return do_jpeg_header(fn);
    }
    catch (const gvs::exception &ex) {
        fprintf(stderr, "%s: %s\n", fn.c_str(), ex.what());
    }

    return {};
}


