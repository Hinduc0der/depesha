# libwebp 1.3.2, decoder only (BSD-3-Clause, see libwebp/COPYING).
# Qt 4.7 has no WebP support of its own and Telegram stickers are WebP,
# so the decoder is vendored here the way zlib and mbedtls already are.
# Encoder, mux and demux are left out: nothing here writes WebP.

INCLUDEPATH += $$PWD

SOURCES += \
    libwebp/src/dec/alpha_dec.c \
    libwebp/src/dec/buffer_dec.c \
    libwebp/src/dec/frame_dec.c \
    libwebp/src/dec/idec_dec.c \
    libwebp/src/dec/io_dec.c \
    libwebp/src/dec/quant_dec.c \
    libwebp/src/dec/tree_dec.c \
    libwebp/src/dec/vp8_dec.c \
    libwebp/src/dec/vp8l_dec.c \
    libwebp/src/dec/webp_dec.c \
    libwebp/src/dsp/alpha_processing.c \
    libwebp/src/dsp/alpha_processing_mips_dsp_r2.c \
    libwebp/src/dsp/alpha_processing_neon.c \
    libwebp/src/dsp/alpha_processing_sse2.c \
    libwebp/src/dsp/alpha_processing_sse41.c \
    libwebp/src/dsp/cpu.c \
    libwebp/src/dsp/dec.c \
    libwebp/src/dsp/dec_clip_tables.c \
    libwebp/src/dsp/dec_mips32.c \
    libwebp/src/dsp/dec_mips_dsp_r2.c \
    libwebp/src/dsp/dec_msa.c \
    libwebp/src/dsp/dec_neon.c \
    libwebp/src/dsp/dec_sse2.c \
    libwebp/src/dsp/dec_sse41.c \
    libwebp/src/dsp/filters.c \
    libwebp/src/dsp/filters_mips_dsp_r2.c \
    libwebp/src/dsp/filters_msa.c \
    libwebp/src/dsp/filters_neon.c \
    libwebp/src/dsp/filters_sse2.c \
    libwebp/src/dsp/lossless.c \
    libwebp/src/dsp/lossless_mips_dsp_r2.c \
    libwebp/src/dsp/lossless_msa.c \
    libwebp/src/dsp/lossless_neon.c \
    libwebp/src/dsp/lossless_sse2.c \
    libwebp/src/dsp/lossless_sse41.c \
    libwebp/src/dsp/rescaler.c \
    libwebp/src/dsp/rescaler_mips32.c \
    libwebp/src/dsp/rescaler_mips_dsp_r2.c \
    libwebp/src/dsp/rescaler_msa.c \
    libwebp/src/dsp/rescaler_neon.c \
    libwebp/src/dsp/rescaler_sse2.c \
    libwebp/src/dsp/upsampling.c \
    libwebp/src/dsp/upsampling_mips_dsp_r2.c \
    libwebp/src/dsp/upsampling_msa.c \
    libwebp/src/dsp/upsampling_neon.c \
    libwebp/src/dsp/upsampling_sse2.c \
    libwebp/src/dsp/upsampling_sse41.c \
    libwebp/src/dsp/yuv.c \
    libwebp/src/dsp/yuv_mips32.c \
    libwebp/src/dsp/yuv_mips_dsp_r2.c \
    libwebp/src/dsp/yuv_neon.c \
    libwebp/src/dsp/yuv_sse2.c \
    libwebp/src/dsp/yuv_sse41.c \
    libwebp/src/utils/bit_reader_utils.c \
    libwebp/src/utils/color_cache_utils.c \
    libwebp/src/utils/filters_utils.c \
    libwebp/src/utils/huffman_utils.c \
    libwebp/src/utils/quant_levels_dec_utils.c \
    libwebp/src/utils/random_utils.c \
    libwebp/src/utils/rescaler_utils.c \
    libwebp/src/utils/thread_utils.c \
    libwebp/src/utils/utils.c

HEADERS += \
    libwebp/src/enc/backward_references_enc.h \
    libwebp/src/enc/cost_enc.h \
    libwebp/src/enc/histogram_enc.h \
    libwebp/src/enc/vp8i_enc.h \
    libwebp/src/enc/vp8li_enc.h \
    libwebp/src/dec/alphai_dec.h \
    libwebp/src/dec/common_dec.h \
    libwebp/src/dec/vp8_dec.h \
    libwebp/src/dec/vp8i_dec.h \
    libwebp/src/dec/vp8li_dec.h \
    libwebp/src/dec/webpi_dec.h \
    libwebp/src/dsp/common_sse2.h \
    libwebp/src/dsp/common_sse41.h \
    libwebp/src/dsp/cpu.h \
    libwebp/src/dsp/dsp.h \
    libwebp/src/dsp/lossless.h \
    libwebp/src/dsp/lossless_common.h \
    libwebp/src/dsp/mips_macro.h \
    libwebp/src/dsp/msa_macro.h \
    libwebp/src/dsp/neon.h \
    libwebp/src/dsp/quant.h \
    libwebp/src/dsp/yuv.h \
    libwebp/src/utils/bit_reader_inl_utils.h \
    libwebp/src/utils/bit_reader_utils.h \
    libwebp/src/utils/bit_writer_utils.h \
    libwebp/src/utils/color_cache_utils.h \
    libwebp/src/utils/endian_inl_utils.h \
    libwebp/src/utils/filters_utils.h \
    libwebp/src/utils/huffman_encode_utils.h \
    libwebp/src/utils/huffman_utils.h \
    libwebp/src/utils/quant_levels_dec_utils.h \
    libwebp/src/utils/quant_levels_utils.h \
    libwebp/src/utils/random_utils.h \
    libwebp/src/utils/rescaler_utils.h \
    libwebp/src/utils/thread_utils.h \
    libwebp/src/utils/utils.h \
    libwebp/src/webp/decode.h \
    libwebp/src/webp/demux.h \
    libwebp/src/webp/encode.h \
    libwebp/src/webp/format_constants.h \
    libwebp/src/webp/mux.h \
    libwebp/src/webp/mux_types.h \
    libwebp/src/webp/types.h
