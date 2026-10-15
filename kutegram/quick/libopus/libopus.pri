# libopus 1.3.1 -- ТОЛЬКО декодер, целочисленный (BSD-3-Clause, см. COPYING).
#
# Зачем в дереве: голосовые Telegram -- это Opus в Ogg, а телефон 2007 года
# такого кодека не знает. До сих пор голосовые перегонял сервер-прокладка;
# с этой библиотекой телефон раскодирует их сам, и сервер для голосовых
# больше не нужен.
#
# Чего здесь нет и почему:
#   * анализа тональности (analysis.c, mlp.c) -- он весь на плавающей точке;
#     кодировщик обходится без него при DISABLE_FLOAT_API, см. kg/config.h;
#   * плавающей ветки SILK (silk/float) -- см. FIXED_POINT в kg/config.h;
#   * наборов команд SSE/AVX/NEON/MIPS -- не та машина, да и GCCE 4.4
#     их внутренних функций не знает.
#
# Единственная правка в вендорном коде: silk/debug.h переименован в
# silk/silk_debug.h (и пять ссылок на него). Причина -- у нас есть свой
# libkg/debug.h, и два файла с одним именем в путях поиска означают, что
# кто-то из двоих получит чужой: SILK -- наш QDebug, а platformutils.cpp --
# отладочные счётчики SILK. Переименование снимает вопрос навсегда, в отличие
# от расстановки каталогов в правильном порядке.

# Деление целых: GCCE зовёт __aeabi_idiv/__aeabi_uidiv, которых в Symbian нет.
# Заплатка уже лежит в libkg (mbedtls/library/symbian.c) и работает на
# аппарате; оба проекта, которые подключают opus, подключают и libkg, поэтому
# второй раз её объявлять нельзя -- будет повтор символа при линковке.

INCLUDEPATH += \
    $$PWD/kg \
    $$PWD/include \
    $$PWD \
    $$PWD/celt \
    $$PWD/silk \
    $$PWD/silk/fixed

DEFINES += HAVE_CONFIG_H

# Массивы переменной длины -- это C99; по умолчанию gcc 4.4 собирает C89.
QMAKE_CFLAGS += -std=gnu99

symbian:LIBS += -llibc

SOURCES += \
    $$PWD/celt/bands.c \
    $$PWD/celt/celt.c \
    $$PWD/celt/celt_decoder.c \
    $$PWD/celt/cwrs.c \
    $$PWD/celt/entcode.c \
    $$PWD/celt/entdec.c \
    $$PWD/celt/entenc.c \
    $$PWD/celt/kiss_fft.c \
    $$PWD/celt/laplace.c \
    $$PWD/celt/mathops.c \
    $$PWD/celt/mdct.c \
    $$PWD/celt/modes.c \
    $$PWD/celt/pitch.c \
    $$PWD/celt/celt_lpc.c \
    $$PWD/celt/quant_bands.c \
    $$PWD/celt/rate.c \
    $$PWD/celt/vq.c \
    $$PWD/silk/CNG.c \
    $$PWD/silk/code_signs.c \
    $$PWD/silk/init_decoder.c \
    $$PWD/silk/decode_core.c \
    $$PWD/silk/decode_frame.c \
    $$PWD/silk/decode_parameters.c \
    $$PWD/silk/decode_indices.c \
    $$PWD/silk/decode_pulses.c \
    $$PWD/silk/decoder_set_fs.c \
    $$PWD/silk/dec_API.c \
    $$PWD/silk/enc_API.c \
    $$PWD/silk/encode_indices.c \
    $$PWD/silk/encode_pulses.c \
    $$PWD/silk/gain_quant.c \
    $$PWD/silk/interpolate.c \
    $$PWD/silk/LP_variable_cutoff.c \
    $$PWD/silk/NLSF_decode.c \
    $$PWD/silk/NSQ.c \
    $$PWD/silk/NSQ_del_dec.c \
    $$PWD/silk/PLC.c \
    $$PWD/silk/shell_coder.c \
    $$PWD/silk/tables_gain.c \
    $$PWD/silk/tables_LTP.c \
    $$PWD/silk/tables_NLSF_CB_NB_MB.c \
    $$PWD/silk/tables_NLSF_CB_WB.c \
    $$PWD/silk/tables_other.c \
    $$PWD/silk/tables_pitch_lag.c \
    $$PWD/silk/tables_pulses_per_block.c \
    $$PWD/silk/VAD.c \
    $$PWD/silk/control_audio_bandwidth.c \
    $$PWD/silk/quant_LTP_gains.c \
    $$PWD/silk/VQ_WMat_EC.c \
    $$PWD/silk/HP_variable_cutoff.c \
    $$PWD/silk/NLSF_encode.c \
    $$PWD/silk/NLSF_VQ.c \
    $$PWD/silk/NLSF_unpack.c \
    $$PWD/silk/NLSF_del_dec_quant.c \
    $$PWD/silk/process_NLSFs.c \
    $$PWD/silk/stereo_LR_to_MS.c \
    $$PWD/silk/stereo_MS_to_LR.c \
    $$PWD/silk/check_control_input.c \
    $$PWD/silk/control_SNR.c \
    $$PWD/silk/init_encoder.c \
    $$PWD/silk/control_codec.c \
    $$PWD/silk/A2NLSF.c \
    $$PWD/silk/ana_filt_bank_1.c \
    $$PWD/silk/biquad_alt.c \
    $$PWD/silk/bwexpander_32.c \
    $$PWD/silk/bwexpander.c \
    $$PWD/silk/debug.c \
    $$PWD/silk/decode_pitch.c \
    $$PWD/silk/inner_prod_aligned.c \
    $$PWD/silk/lin2log.c \
    $$PWD/silk/log2lin.c \
    $$PWD/silk/LPC_analysis_filter.c \
    $$PWD/silk/LPC_inv_pred_gain.c \
    $$PWD/silk/table_LSF_cos.c \
    $$PWD/silk/NLSF2A.c \
    $$PWD/silk/NLSF_stabilize.c \
    $$PWD/silk/NLSF_VQ_weights_laroia.c \
    $$PWD/silk/pitch_est_tables.c \
    $$PWD/silk/resampler.c \
    $$PWD/silk/resampler_down2_3.c \
    $$PWD/silk/resampler_down2.c \
    $$PWD/silk/resampler_private_AR2.c \
    $$PWD/silk/resampler_private_down_FIR.c \
    $$PWD/silk/resampler_private_IIR_FIR.c \
    $$PWD/silk/resampler_private_up2_HQ.c \
    $$PWD/silk/resampler_rom.c \
    $$PWD/silk/sigm_Q15.c \
    $$PWD/silk/sort.c \
    $$PWD/silk/sum_sqr_shift.c \
    $$PWD/silk/stereo_decode_pred.c \
    $$PWD/silk/stereo_encode_pred.c \
    $$PWD/silk/stereo_find_predictor.c \
    $$PWD/silk/stereo_quant_pred.c \
    $$PWD/silk/LPC_fit.c \
    $$PWD/silk/fixed/LTP_analysis_filter_FIX.c \
    $$PWD/silk/fixed/LTP_scale_ctrl_FIX.c \
    $$PWD/silk/fixed/corrMatrix_FIX.c \
    $$PWD/silk/fixed/encode_frame_FIX.c \
    $$PWD/silk/fixed/find_LPC_FIX.c \
    $$PWD/silk/fixed/find_LTP_FIX.c \
    $$PWD/silk/fixed/find_pitch_lags_FIX.c \
    $$PWD/silk/fixed/find_pred_coefs_FIX.c \
    $$PWD/silk/fixed/noise_shape_analysis_FIX.c \
    $$PWD/silk/fixed/process_gains_FIX.c \
    $$PWD/silk/fixed/regularize_correlations_FIX.c \
    $$PWD/silk/fixed/residual_energy16_FIX.c \
    $$PWD/silk/fixed/residual_energy_FIX.c \
    $$PWD/silk/fixed/warped_autocorrelation_FIX.c \
    $$PWD/silk/fixed/apply_sine_window_FIX.c \
    $$PWD/silk/fixed/autocorr_FIX.c \
    $$PWD/silk/fixed/burg_modified_FIX.c \
    $$PWD/silk/fixed/k2a_FIX.c \
    $$PWD/silk/fixed/k2a_Q16_FIX.c \
    $$PWD/silk/fixed/pitch_analysis_core_FIX.c \
    $$PWD/silk/fixed/vector_ops_FIX.c \
    $$PWD/silk/fixed/schur64_FIX.c \
    $$PWD/silk/fixed/schur_FIX.c \
    $$PWD/src/opus.c \
    $$PWD/src/opus_encoder.c \
    $$PWD/celt/celt_encoder.c \
    $$PWD/src/opus_decoder.c \
    $$PWD/src/opus_multistream.c \
    $$PWD/src/opus_multistream_decoder.c \
    $$PWD/src/repacketizer.c
