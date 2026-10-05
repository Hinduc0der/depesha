# Сгенерировано из kutegram/quick/libwebp/libwebp.pri: тот файл рассчитан на
# включение из каталога quick (пути в нём относительные), а Депеша лежит
# рядом. Содержимое то же, пути через $$KUTEGRAM.

# libwebp 1.3.2, decoder only (BSD-3-Clause, see $$KUTEGRAM/libwebp/COPYING).
# Qt 4.7 has no WebP support of its own and Telegram stickers are WebP,
# so the decoder is vendored here the way zlib and mbedtls already are.
# Encoder, mux and demux are left out: nothing here writes WebP.

INCLUDEPATH += $$KUTEGRAM/libwebp

SOURCES += \
    $$KUTEGRAM/libwebp/src/dec/alpha_dec.c \
    $$KUTEGRAM/libwebp/src/dec/buffer_dec.c \
    $$KUTEGRAM/libwebp/src/dec/frame_dec.c \
    $$KUTEGRAM/libwebp/src/dec/idec_dec.c \
    $$KUTEGRAM/libwebp/src/dec/io_dec.c \
    $$KUTEGRAM/libwebp/src/dec/quant_dec.c \
    $$KUTEGRAM/libwebp/src/dec/tree_dec.c \
    $$KUTEGRAM/libwebp/src/dec/vp8_dec.c \
    $$KUTEGRAM/libwebp/src/dec/vp8l_dec.c \
    $$KUTEGRAM/libwebp/src/dec/webp_dec.c \
    $$KUTEGRAM/libwebp/src/dsp/alpha_processing.c \
    $$KUTEGRAM/libwebp/src/dsp/alpha_processing_mips_dsp_r2.c \
    $$KUTEGRAM/libwebp/src/dsp/alpha_processing_neon.c \
    $$KUTEGRAM/libwebp/src/dsp/alpha_processing_sse2.c \
    $$KUTEGRAM/libwebp/src/dsp/alpha_processing_sse41.c \
    $$KUTEGRAM/libwebp/src/dsp/cpu.c \
    $$KUTEGRAM/libwebp/src/dsp/dec.c \
    $$KUTEGRAM/libwebp/src/dsp/dec_clip_tables.c \
    $$KUTEGRAM/libwebp/src/dsp/dec_mips32.c \
    $$KUTEGRAM/libwebp/src/dsp/dec_mips_dsp_r2.c \
    $$KUTEGRAM/libwebp/src/dsp/dec_msa.c \
    $$KUTEGRAM/libwebp/src/dsp/dec_neon.c \
    $$KUTEGRAM/libwebp/src/dsp/dec_sse2.c \
    $$KUTEGRAM/libwebp/src/dsp/dec_sse41.c \
    $$KUTEGRAM/libwebp/src/dsp/filters.c \
    $$KUTEGRAM/libwebp/src/dsp/filters_mips_dsp_r2.c \
    $$KUTEGRAM/libwebp/src/dsp/filters_msa.c \
    $$KUTEGRAM/libwebp/src/dsp/filters_neon.c \
    $$KUTEGRAM/libwebp/src/dsp/filters_sse2.c \
    $$KUTEGRAM/libwebp/src/dsp/lossless.c \
    $$KUTEGRAM/libwebp/src/dsp/lossless_mips_dsp_r2.c \
    $$KUTEGRAM/libwebp/src/dsp/lossless_msa.c \
    $$KUTEGRAM/libwebp/src/dsp/lossless_neon.c \
    $$KUTEGRAM/libwebp/src/dsp/lossless_sse2.c \
    $$KUTEGRAM/libwebp/src/dsp/lossless_sse41.c \
    $$KUTEGRAM/libwebp/src/dsp/rescaler.c \
    $$KUTEGRAM/libwebp/src/dsp/rescaler_mips32.c \
    $$KUTEGRAM/libwebp/src/dsp/rescaler_mips_dsp_r2.c \
    $$KUTEGRAM/libwebp/src/dsp/rescaler_msa.c \
    $$KUTEGRAM/libwebp/src/dsp/rescaler_neon.c \
    $$KUTEGRAM/libwebp/src/dsp/rescaler_sse2.c \
    $$KUTEGRAM/libwebp/src/dsp/upsampling.c \
    $$KUTEGRAM/libwebp/src/dsp/upsampling_mips_dsp_r2.c \
    $$KUTEGRAM/libwebp/src/dsp/upsampling_msa.c \
    $$KUTEGRAM/libwebp/src/dsp/upsampling_neon.c \
    $$KUTEGRAM/libwebp/src/dsp/upsampling_sse2.c \
    $$KUTEGRAM/libwebp/src/dsp/upsampling_sse41.c \
    $$KUTEGRAM/libwebp/src/dsp/yuv.c \
    $$KUTEGRAM/libwebp/src/dsp/yuv_mips32.c \
    $$KUTEGRAM/libwebp/src/dsp/yuv_mips_dsp_r2.c \
    $$KUTEGRAM/libwebp/src/dsp/yuv_neon.c \
    $$KUTEGRAM/libwebp/src/dsp/yuv_sse2.c \
    $$KUTEGRAM/libwebp/src/dsp/yuv_sse41.c \
    $$KUTEGRAM/libwebp/src/utils/bit_reader_utils.c \
    $$KUTEGRAM/libwebp/src/utils/color_cache_utils.c \
    $$KUTEGRAM/libwebp/src/utils/filters_utils.c \
    $$KUTEGRAM/libwebp/src/utils/huffman_utils.c \
    $$KUTEGRAM/libwebp/src/utils/quant_levels_dec_utils.c \
    $$KUTEGRAM/libwebp/src/utils/random_utils.c \
    $$KUTEGRAM/libwebp/src/utils/rescaler_utils.c \
    $$KUTEGRAM/libwebp/src/utils/thread_utils.c \
    $$KUTEGRAM/libwebp/src/utils/utils.c

HEADERS += \
    $$KUTEGRAM/libwebp/src/enc/backward_references_enc.h \
    $$KUTEGRAM/libwebp/src/enc/cost_enc.h \
    $$KUTEGRAM/libwebp/src/enc/histogram_enc.h \
    $$KUTEGRAM/libwebp/src/enc/vp8i_enc.h \
    $$KUTEGRAM/libwebp/src/enc/vp8li_enc.h \
    $$KUTEGRAM/libwebp/src/dec/alphai_dec.h \
    $$KUTEGRAM/libwebp/src/dec/common_dec.h \
    $$KUTEGRAM/libwebp/src/dec/vp8_dec.h \
    $$KUTEGRAM/libwebp/src/dec/vp8i_dec.h \
    $$KUTEGRAM/libwebp/src/dec/vp8li_dec.h \
    $$KUTEGRAM/libwebp/src/dec/webpi_dec.h \
    $$KUTEGRAM/libwebp/src/dsp/common_sse2.h \
    $$KUTEGRAM/libwebp/src/dsp/common_sse41.h \
    $$KUTEGRAM/libwebp/src/dsp/cpu.h \
    $$KUTEGRAM/libwebp/src/dsp/dsp.h \
    $$KUTEGRAM/libwebp/src/dsp/lossless.h \
    $$KUTEGRAM/libwebp/src/dsp/lossless_common.h \
    $$KUTEGRAM/libwebp/src/dsp/mips_macro.h \
    $$KUTEGRAM/libwebp/src/dsp/msa_macro.h \
    $$KUTEGRAM/libwebp/src/dsp/neon.h \
    $$KUTEGRAM/libwebp/src/dsp/quant.h \
    $$KUTEGRAM/libwebp/src/dsp/yuv.h \
    $$KUTEGRAM/libwebp/src/utils/bit_reader_inl_utils.h \
    $$KUTEGRAM/libwebp/src/utils/bit_reader_utils.h \
    $$KUTEGRAM/libwebp/src/utils/bit_writer_utils.h \
    $$KUTEGRAM/libwebp/src/utils/color_cache_utils.h \
    $$KUTEGRAM/libwebp/src/utils/endian_inl_utils.h \
    $$KUTEGRAM/libwebp/src/utils/filters_utils.h \
    $$KUTEGRAM/libwebp/src/utils/huffman_encode_utils.h \
    $$KUTEGRAM/libwebp/src/utils/huffman_utils.h \
    $$KUTEGRAM/libwebp/src/utils/quant_levels_dec_utils.h \
    $$KUTEGRAM/libwebp/src/utils/quant_levels_utils.h \
    $$KUTEGRAM/libwebp/src/utils/random_utils.h \
    $$KUTEGRAM/libwebp/src/utils/rescaler_utils.h \
    $$KUTEGRAM/libwebp/src/utils/thread_utils.h \
    $$KUTEGRAM/libwebp/src/utils/utils.h \
    $$KUTEGRAM/libwebp/src/webp/decode.h \
    $$KUTEGRAM/libwebp/src/webp/demux.h \
    $$KUTEGRAM/libwebp/src/webp/encode.h \
    $$KUTEGRAM/libwebp/src/webp/format_constants.h \
    $$KUTEGRAM/libwebp/src/webp/mux.h \
    $$KUTEGRAM/libwebp/src/webp/mux_types.h \
    $$KUTEGRAM/libwebp/src/webp/types.h
