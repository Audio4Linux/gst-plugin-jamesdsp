/**
 * SECTION:element-jdspfx
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch audiotestsrc ! audioconverter ! jdspfx ! ! audioconverter ! autoaudiosink
 * ]|
 * </refsect2>
 */

#define PACKAGE "jdspfx-plugin"
#define VERSION "1.0.0"

#include <stdio.h>
#include <string.h>
#include <iterator>
#include <algorithm>
#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/base/gstbasetransform.h>
#include <gst/audio/audio.h>
#include <gst/audio/gstaudiofilter.h>
#include <gst/controller/controller.h>
#include "gstjdspfx.h"
#include "gstinterface.h"

#include "Effect.h"
#include "EffectDSPMain.h"

GST_DEBUG_CATEGORY_STATIC (gst_jdspfx_debug);
#define GST_CAT_DEFAULT gst_jdspfx_debug
/* Filter signals and args */
enum {
    /* FILL ME */
            LAST_SIGNAL
};

enum {
    PROP_0,

    /* global enable */
    PROP_FX_ENABLE,
    /* analog modelling */
    PROP_TUBE_ENABLE,
    PROP_TUBE_DRIVE,
    /* bassboost */
    PROP_BASS_ENABLE,
    PROP_BASS_MODE,
    PROP_BASS_FILTERTYPE,
    PROP_BASS_FREQ,
    /* reverb */
    PROP_HEADSET_ENABLE,
    PROP_HEADSET_OSF,
    PROP_HEADSET_REFLECTION_AMOUNT,
    PROP_HEADSET_FINALWET,
    PROP_HEADSET_FINALDRY,
    PROP_HEADSET_REFLECTION_FACTOR,
    PROP_HEADSET_REFLECTION_WIDTH,
    PROP_HEADSET_WIDTH,
    PROP_HEADSET_WET,
    PROP_HEADSET_LFO_WANDER,
    PROP_HEADSET_BASSBOOST,
    PROP_HEADSET_LFO_SPIN,
    PROP_HEADSET_LPF_INPUT,
    PROP_HEADSET_LPF_BASS,
    PROP_HEADSET_LPF_DAMP,
    PROP_HEADSET_LPF_OUTPUT,
    PROP_HEADSET_DECAY,
    PROP_HEADSET_DELAY,
   /* stereo wide */
    PROP_STEREOWIDE_MCOEFF,
    PROP_STEREOWIDE_SCOEFF,
    PROP_STEREOWIDE_ENABLE,
    /* bs2b */
    PROP_BS2B_FCUT,
    PROP_BS2B_FEED,
    PROP_BS2B_ENABLE,
    /* compressor */
    PROP_COMPRESSOR_ENABLE,
    PROP_COMPRESSOR_PREGAIN,
    PROP_COMPRESSOR_THRESHOLD,
    PROP_COMPRESSOR_KNEE,
    PROP_COMPRESSOR_RATIO,
    PROP_COMPRESSOR_ATTACK,
    PROP_COMPRESSOR_RELEASE,
    /* mixed equalizer */
    PROP_TONE_ENABLE,
    PROP_TONE_FILTERTYPE,
    PROP_TONE_EQ,
    /* limiter */
    PROP_MASTER_LIMTHRESHOLD,
    PROP_MASTER_LIMRELEASE,
    /* ddc */
    PROP_DDC_ENABLE,
    PROP_DDC_COEFFS,
    /* convolver */
    PROP_CONVOLVER_ENABLE,
    PROP_CONVOLVER_GAIN,
    PROP_CONVOLVER_BENCH_C0,
    PROP_CONVOLVER_BENCH_C1,
    PROP_CONVOLVER_FILE,
};

#define ALLOWED_CAPS \
  "audio/x-raw,"                            \
  " format=(string){"GST_AUDIO_NE(F32)","GST_AUDIO_NE(S32)"},"  \
  " rate=(int){44100,48000},"                 \
  " channels=(int)2,"                       \
  " layout=(string)interleaved"

#define gst_jdspfx_parent_class parent_class
G_DEFINE_TYPE (Gstjdspfx, gst_jdspfx, GST_TYPE_AUDIO_FILTER
);

static void gst_jdspfx_set_property(GObject *object, guint prop_id,
                                    const GValue *value, GParamSpec *pspec);

static void gst_jdspfx_get_property(GObject *object, guint prop_id,
                                    GValue *value, GParamSpec *pspec);

static void gst_jdspfx_finalize(GObject *object);

static gboolean gst_jdspfx_setup(GstAudioFilter *self,
                                 const GstAudioInfo *info);

static gboolean gst_jdspfx_stop(GstBaseTransform *base);

static GstFlowReturn gst_jdspfx_transform_ip(GstBaseTransform *base,
                                             GstBuffer *outbuf);

/* GObject vmethod implementations */



/* initialize the jdspfx's class */
static void
gst_jdspfx_class_init(GstjdspfxClass *klass) {
    GObjectClass *gobject_class = (GObjectClass *) klass;
    GstElementClass *gstelement_class = (GstElementClass *) klass;
    GstBaseTransformClass *basetransform_class = (GstBaseTransformClass *) klass;
    GstAudioFilterClass *audioself_class = (GstAudioFilterClass *) klass;
    GstCaps *caps;



    /* debug category for fltering log messages
     */
    GST_DEBUG_CATEGORY_INIT(gst_jdspfx_debug, "jdspfx", 0, "jdspfx element");

    gobject_class->set_property = gst_jdspfx_set_property;
    gobject_class->get_property = gst_jdspfx_get_property;
    gobject_class->finalize = gst_jdspfx_finalize;

    /* global switch */
    g_object_class_install_property(gobject_class, PROP_FX_ENABLE,
                                    g_param_spec_boolean("enable", "FXEnabled", "Enable JamesDSP processing",
                                                         FALSE,
                                                         (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));

    /* analog modelling */
    g_object_class_install_property(gobject_class, PROP_TUBE_ENABLE,
                                    g_param_spec_boolean("analogmodelling-enable", "TubeEnabled",
                                                         "Enable analog modelling",
                                                         FALSE,
                                                         (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_TUBE_DRIVE,
                                    g_param_spec_int("analogmodelling-tubedrive", "TubeDrive", "Tube drive/strength",
                                                     0, 12000, 0,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));

    /* bass boost */
    g_object_class_install_property(gobject_class, PROP_BASS_ENABLE,
                                    g_param_spec_boolean("bass-enable", "BassEnabled",
                                                         "Enable bass boost",
                                                         FALSE,
                                                         (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_BASS_MODE,
                                    g_param_spec_int("bass-mode", "BassMode", "Bass boost mode/strength",
                                                     0, 3000, 1200,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_BASS_FREQ,
                                    g_param_spec_int("bass-freq", "BassFreq", "Bass boost cutoff frequency (Hz)",
                                                     30, 300, 55,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_BASS_FILTERTYPE,
                                    g_param_spec_int("bass-filtertype", "BassFilterType", "Bass boost filtertype [Linear phase 2049/4097 lowpass filter]",
                                                     0, 1, 0,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));

    /* reverb */
    g_object_class_install_property(gobject_class, PROP_HEADSET_ENABLE,
                                    g_param_spec_boolean("headset-enable", "ReverbEnabled",
                                                         "Enable reverbation",
                                                         FALSE,
                                                         (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));

    g_object_class_install_property(gobject_class, PROP_HEADSET_OSF,
                                    g_param_spec_int("headset-osf", "ReverbOSF", "Oversample factor: how much to oversample",
                                                     1, 4, 1,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_HEADSET_REFLECTION_AMOUNT,
                                    g_param_spec_float("headset-reflection-amount", "ReverbErtolate", "Early reflection amount",
                                                       0, 1, 0,
                                                       (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_HEADSET_FINALWET,
                                    g_param_spec_float("headset-finalwet", "ReverbFWET", "Final wet mix (dB)",
                                                       -70, 10, 0,
                                                       (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_HEADSET_FINALDRY,
                                    g_param_spec_float("headset-finaldry", "ReverbFDRY", "Final dry mix (dB)",
                                                       -70, 10, 0,
                                                       (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_HEADSET_REFLECTION_FACTOR,
                                    g_param_spec_float("headset-reflection-factor", "ReverbEreffactor", "Early reflection factor",
                                                       0.5, 2.5, 0.5,
                                                       (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_HEADSET_REFLECTION_WIDTH,
                                    g_param_spec_float("headset-reflection-width", "ReverbErefwidth", "Early reflection width",
                                                       -1, 1, 0,
                                                       (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_HEADSET_WIDTH,
                                    g_param_spec_float("headset-width", "ReverbWidth", "Width of reverb L/R mix",
                                                       0, 1, 0,
                                                       (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_HEADSET_WET,
                                    g_param_spec_float("headset-wet", "ReverbWET", "Reverb wetness (dB)",
                                                       -70, 10, 0,
                                                       (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_HEADSET_LFO_WANDER,
                                    g_param_spec_float("headset-lfo-wander", "ReverbLFOWander", "LFO wander amount",
                                                       0.1, 0.6, 0.1,
                                                       (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_HEADSET_BASSBOOST,
                                    g_param_spec_float("headset-bassboost", "ReverbBassBoost", "Bass Boost",
                                                       0, 0.5, 0,
                                                       (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_HEADSET_LFO_SPIN,
                                    g_param_spec_float("headset-lfo-spin", "ReverbLFOSpin", "LFO spin amount",
                                                       0, 10, 0,
                                                       (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_HEADSET_DECAY,
                                    g_param_spec_float("headset-decay", "ReverbRT60", "Time decay",
                                                       0.1, 30, 0.1,
                                                       (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));

    g_object_class_install_property(gobject_class, PROP_HEADSET_DELAY,
                                    g_param_spec_int("headset-delay", "ReverbDelay", "Delay in milliseconds",
                                                     -500, 500, 0,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_HEADSET_LPF_INPUT,
                                    g_param_spec_int("headset-lpf-input", "ReverbLPFInput", "Lowpass cutoff for input (Hz)",
                                                      200, 18000, 200,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_HEADSET_LPF_BASS,
                                    g_param_spec_int("headset-lpf-bass", "ReverbLPFBass", "Lowpass cutoff for bass (Hz)",
                                                     50, 1050, 50,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_HEADSET_LPF_DAMP,
                                    g_param_spec_int("headset-lpf-damp", "ReverbLPFDamp", "Lowpass cutoff for dampening (Hz)",
                                                     200, 18000, 200,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_HEADSET_LPF_OUTPUT,
                                    g_param_spec_int("headset-lpf-output", "ReverbLPFOutput", "Lowpass cutoff for output (Hz)",
                                                     200, 18000, 200,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));



    /* stereo wide */
    g_object_class_install_property(gobject_class, PROP_STEREOWIDE_ENABLE,
                                    g_param_spec_boolean("stereowide-enable", "StereoWideEnabled",
                                                         "Enable stereo widener",
                                                         FALSE,
                                                         (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_STEREOWIDE_MCOEFF,
                                    g_param_spec_int("stereowide-mcoeff", "StereoWideM", "Stereo widener M Coeff (1000=1)",
                                                     0, 10000, 0,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_STEREOWIDE_SCOEFF,
                                    g_param_spec_int("stereowide-scoeff", "StereoWideS", "Stereo widener S Coeff (1000=1)",
                                                     0, 10000, 0,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));

    /* bs2b */
    g_object_class_install_property(gobject_class, PROP_BS2B_ENABLE,
                                    g_param_spec_boolean("bs2b-enable", "BS2BEnabled",
                                                         "Enable BS2B",
                                                         FALSE,
                                                         (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_BS2B_FCUT,
                                    g_param_spec_int("bs2b-fcut", "BS2BFCut", "BS2B cutoff frequency (Hz)",
                                                     300, 2000, 700,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_BS2B_FEED,
                                    g_param_spec_int("bs2b-feed", "BS2BFeed", "BS2B crossfeeding level at low frequencies (10=1dB)",
                                                     10, 150, 60,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));

    /* compressor */
    g_object_class_install_property(gobject_class, PROP_COMPRESSOR_ENABLE,
                                    g_param_spec_boolean("compression-enable", "CompEnabled",
                                                         "Enable Compressor",
                                                         FALSE,
                                                         (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_COMPRESSOR_PREGAIN,
                                    g_param_spec_int("compression-pregain", "CompPregain", "Compressor pregain (dB)",
                                                     0, 24, 12,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_COMPRESSOR_THRESHOLD,
                                    g_param_spec_int("compression-threshold", "CompThres", "Compressor threshold (dB)",
                                                     -80, 0, -60,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_COMPRESSOR_KNEE,
                                    g_param_spec_int("compression-knee", "CompKnee", "Compressor knee (dB)",
                                                     0, 40, 30,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_COMPRESSOR_RATIO,
                                    g_param_spec_int("compression-ratio", "CompRatio", "Compressor ratio (1:xx)",
                                                     -20, 20, 12,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_COMPRESSOR_ATTACK,
                                    g_param_spec_int("compression-attack", "CompAttack", "Compressor attack (ms)",
                                                     1, 1000, 1,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_COMPRESSOR_RELEASE,
                                    g_param_spec_int("compression-release", "CompRelease", "Compressor release (ms)",
                                                     1, 1000, 24,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));

    /* mixed equalizer */

    g_object_class_install_property(gobject_class, PROP_TONE_ENABLE,
                                    g_param_spec_boolean("tone-enable", "EqEnabled",
                                                         "Enable Equalizer",
                                                         FALSE,
                                                         (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property(gobject_class, PROP_TONE_FILTERTYPE,
                                    g_param_spec_int("tone-filtertype", "EqFilter", "Equalizer filter type (Minimum/Linear phase)",
                                                     0, 1, 0,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));

    g_object_class_install_property (gobject_class, PROP_TONE_EQ,
                                     g_param_spec_string ("tone-eq", "EQCustom", "15-band EQ data (ex: 1200;50;-200;-500;-500;-500;-500;-450;-250;0;-300;-50;0;0;50) 100=1dB; min: -12dB, max: 12dB",
                                                          "0;0;0;0;0;0;0;0;0;0;0;0;0;0;0", (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));


    /* limiter */
    g_object_class_install_property(gobject_class, PROP_MASTER_LIMTHRESHOLD,
                                    g_param_spec_float("masterswitch-limthreshold", "LimThreshold", "Limiter threshold (dB)",
                                                     -60, 0, 0,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));


    g_object_class_install_property(gobject_class, PROP_MASTER_LIMRELEASE,
                                    g_param_spec_float("masterswitch-limrelease", "LimRelease", "Limiter release (ms)",
                                                        1.5, 2000, 60,
                                                       (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));

    /* ddc */
    g_object_class_install_property(gobject_class, PROP_DDC_ENABLE,
                                    g_param_spec_boolean("ddc-enable", "DDCEnabled",
                                                         "Enable DDC",
                                                         FALSE,
                                                         (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property (gobject_class, PROP_DDC_COEFFS,
                                     g_param_spec_string ("ddc-file", "DDCFileCoeffs", "DDC filepath",
                                                          "", (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));


    /* convolver */
    g_object_class_install_property(gobject_class, PROP_CONVOLVER_ENABLE,
                                    g_param_spec_boolean("convolver-enable", "ConvEnabled",
                                                         "Enable Convolver",
                                                         FALSE,
                                                         (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    /*g_object_class_install_property (gobject_class, PROP_CONVOLVER_BENCH_C0,
                                     g_param_spec_string ("convolver-bench-c0", "ConvC0", "Benchmark data for the convolver (JDSP4Linux-GUI is capable to generate these values)",
                                                          "", (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property (gobject_class, PROP_CONVOLVER_BENCH_C1,
                                     g_param_spec_string ("convolver-bench-c1", "ConvC1", "Benchmark data for the convolver (JDSP4Linux-GUI is capable to generate these values)",
                                                          "", (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));*/
    g_object_class_install_property(gobject_class, PROP_CONVOLVER_GAIN,
                                    g_param_spec_float("convolver-gain", "ConvGain", "Convolver gain (dB)",
                                                       -80, 30, 0,
                                                       (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));
    g_object_class_install_property (gobject_class, PROP_CONVOLVER_FILE,
                                     g_param_spec_string ("convolver-file", "ConvFile", "Impulse response file",
                                                          "", (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));




    gst_element_class_set_static_metadata(gstelement_class,
                                          "jdspfx",
                                          "Filter/Effect/Audio",
                                          "JamesDSP Core wrapper for GStreamer1",
                                          "ThePBone <tim.schneeberger@outlook.de>");


    caps = gst_caps_from_string(ALLOWED_CAPS);


    gst_audio_filter_class_add_pad_templates((GstAudioFilterClass * )GST_JDSPFX_CLASS (klass), caps);
    gst_caps_unref(caps);
    audioself_class->setup = GST_DEBUG_FUNCPTR(gst_jdspfx_setup);
    basetransform_class->transform_ip =
            GST_DEBUG_FUNCPTR(gst_jdspfx_transform_ip);
    basetransform_class->transform_ip_on_passthrough = FALSE;
    basetransform_class->stop = GST_DEBUG_FUNCPTR(gst_jdspfx_stop);
}

/* sync all parameters to fx core
*/
static void sync_all_parameters(Gstjdspfx * self) {
    int32_t idx;

    config_set_px0_vx0x0(self->effectDspMain, EFFECT_CMD_ENABLE);

    // analog modelling
    command_set_px4_vx2x1(self->effectDspMain,
                          1206, (int16_t) self->tube_drive);

    command_set_px4_vx2x1(self->effectDspMain,
                          150, self->tube_enabled);

    // bassboost
    command_set_px4_vx2x1(self->effectDspMain,
                          112, (int16_t)self->bass_mode);

    command_set_px4_vx2x1(self->effectDspMain,
                          113, (int16_t)self->bass_filtertype);

    command_set_px4_vx2x1(self->effectDspMain,
                          114, (int16_t)self->bass_freq);

    command_set_px4_vx2x1(self->effectDspMain,
                          1201, self->bass_enabled);

    // reverb
    command_set_reverb(self->effectDspMain, self);

    command_set_px4_vx2x1(self->effectDspMain,
                          1203, self->headset_enabled);

    // stereo wide
    command_set_px4_vx2x2(self->effectDspMain,
                          137, (int16_t)self->stereowide_mcoeff,(int16_t)self->stereowide_scoeff);

    command_set_px4_vx2x1(self->effectDspMain,
                          1204, self->stereowide_enabled);

    // bs2b
    if(self->bs2b_feed != 0)
        command_set_px4_vx2x2(self->effectDspMain,
                          188, (int16_t)self->bs2b_fcut,(int16_t)self->bs2b_feed);

    command_set_px4_vx2x1(self->effectDspMain,
                          1208, self->bs2b_enabled);


    // compressor
    command_set_px4_vx2x1(self->effectDspMain,
                          100, (int16_t)self->compression_pregain);
    command_set_px4_vx2x1(self->effectDspMain,
                          101, (int16_t)self->compression_threshold);
    command_set_px4_vx2x1(self->effectDspMain,
                          102, (int16_t)self->compression_knee);
    command_set_px4_vx2x1(self->effectDspMain,
                          103, (int16_t)self->compression_ratio);
    command_set_px4_vx2x1(self->effectDspMain,
                          104, (int16_t)self->compression_attack);
    command_set_px4_vx2x1(self->effectDspMain,
                          105, (int16_t)self->compression_release);
    command_set_px4_vx2x1(self->effectDspMain,
                          1200, self->compression_enabled);

    // mixed equalizer
    command_set_eq (self->effectDspMain, self->tone_eq);
    command_set_px4_vx2x1(self->effectDspMain,
                          151, (int16_t)self->tone_filtertype);
    command_set_px4_vx2x1(self->effectDspMain,
                          1202, self->tone_enabled);

    // limiter
    command_set_limiter(self->effectDspMain,self->lim_threshold,self->lim_release);

    // ddc
    command_set_ddc(self->effectDspMain,self->ddc_coeffs,self->ddc_enabled);
    command_set_px4_vx2x1(self->effectDspMain,
                          1212, self->ddc_enabled);

    // convolver
    command_set_convolver(self->effectDspMain, self->convolver_file,self->convolver_gain,self->convolver_quality,
                          self->convolver_bench_c0,self->convolver_bench_c1,self->samplerate);
    command_set_px4_vx2x1(self->effectDspMain,
                          1205, self->convolver_enabled);

}

/* initialize the new element
 * allocate private resources
 */
static void
gst_jdspfx_init(Gstjdspfx * self) {
    gint32 idx;

    gst_base_transform_set_in_place(GST_BASE_TRANSFORM(self), TRUE);
    gst_base_transform_set_gap_aware(GST_BASE_TRANSFORM(self), TRUE);

    /* initialize properties */
    self->fx_enabled = FALSE;

    self->tube_enabled = FALSE;
    self->tube_drive = 0;
    self->bass_mode = 0;
    self->bass_filtertype = 0;
    self->bass_freq = 55;
    self->bass_enabled = FALSE;

    self->headset_osf=1;
    self->headset_delay=0;
    self->headset_inputlpf=200;
    self->headset_basslpf=50;
    self->headset_damplpf=200;
    self->headset_outputlpf=200;
    self->headset_reflection_amount=0;
    self->headset_reflection_factor=0,5;
    self->headset_reflection_width=0;
    self->headset_finaldry=0;
    self->headset_finalwet=0;
    self->headset_width=0;
    self->headset_wet=0;
    self->headset_lfo_wander=0.1;
    self->headset_bassboost=0;
    self->headset_lfo_spin=0;
    self->headset_decay=0.1;
    self->headset_enabled = FALSE;
    self->stereowide_mcoeff = 0;
    self->stereowide_scoeff = 0;
    self->stereowide_enabled = FALSE;
    self->bs2b_enabled = FALSE;
    self->bs2b_fcut = 700;
    self->bs2b_feed = 0;
    self->compression_pregain = 12;
    self->compression_threshold = -60;
    self->compression_knee = 30;
    self->compression_ratio = 12;
    self->compression_attack = 1;
    self->compression_release = 24;
    self->compression_enabled = FALSE;
    self->tone_filtertype = 0;
    self->tone_enabled = FALSE;
    memset (self->tone_eq, 0,
            sizeof(self->tone_eq));
    self->lim_threshold = 0;
    self->lim_release = 60;
    self->ddc_enabled = FALSE;
    memset (self->ddc_coeffs, 0,
            sizeof(self->ddc_coeffs));

    self->convolver_enabled = FALSE;
    memset (self->convolver_bench_c0, 0,
            sizeof(self->convolver_bench_c0));
    strcpy(self->convolver_bench_c0 ,
           "0.000000");
    memset (self->convolver_bench_c1, 0,
            sizeof(self->convolver_bench_c1));
    strcpy(self->convolver_bench_c1 ,
           "0.000000");
    memset (self->convolver_file, 0,
            sizeof(self->convolver_file));
    self->convolver_gain = 0;
    self->convolver_quality = 100;

    /* initialize private resources */
    self->effectDspMain = NULL;
    self->effectDspMain = new EffectDSPMain();

    if (self->effectDspMain != NULL)
        sync_all_parameters(self);

    printf("\n--------INIT DONE--------\n\n");

    g_mutex_init(&self->lock);
}

/* free private resources
*/
static void
gst_jdspfx_finalize(GObject *object) {
    Gstjdspfx * self = GST_JDSPFX (object);

    if (self->effectDspMain != NULL) {
        delete self->effectDspMain;
    }

    g_mutex_clear(&self->lock);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gst_jdspfx_set_property(GObject *object, guint prop_id,
                        const GValue *value, GParamSpec *pspec) {
    Gstjdspfx * self = GST_JDSPFX (object);

    switch (prop_id) {
        case PROP_FX_ENABLE: {
            g_mutex_lock(&self->lock);
            self->fx_enabled = g_value_get_boolean(value);
            g_mutex_unlock(&self->lock);
        }
            break;

        case PROP_TUBE_ENABLE: {
            g_mutex_lock(&self->lock);
            self->tube_enabled = g_value_get_boolean(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  1206, self->tube_enabled);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_TUBE_DRIVE: {
            g_mutex_lock(&self->lock);
            self->tube_drive = g_value_get_int(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  150, (int16_t) self->tube_drive);
            g_mutex_unlock(&self->lock);
        }
            break;

        case PROP_BASS_ENABLE: {
            g_mutex_lock(&self->lock);
            self->bass_enabled = g_value_get_boolean(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  1201, self->bass_enabled);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_BASS_MODE: {
            g_mutex_lock(&self->lock);
            self->bass_mode = g_value_get_int(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  112, (int16_t) self->bass_mode);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_BASS_FILTERTYPE: {
            g_mutex_lock(&self->lock);
            self->bass_filtertype = g_value_get_int(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  113, (int16_t) self->bass_filtertype);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_BASS_FREQ: {
            g_mutex_lock(&self->lock);
            self->bass_freq = g_value_get_int(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  114, (int16_t) self->bass_freq);
            g_mutex_unlock(&self->lock);
        }
            break;

        case PROP_HEADSET_ENABLE: {
            g_mutex_lock(&self->lock);
            self->headset_enabled = g_value_get_boolean(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  1203, self->headset_enabled);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_HEADSET_OSF: {
            g_mutex_lock(&self->lock);
            self->headset_osf = g_value_get_int(value);
            command_set_reverb(self->effectDspMain,self);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_HEADSET_DELAY: {
            g_mutex_lock(&self->lock);
            self->headset_delay = g_value_get_int(value);
            command_set_reverb(self->effectDspMain,self);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_HEADSET_LPF_INPUT: {
            g_mutex_lock(&self->lock);
            self->headset_inputlpf = g_value_get_int(value);
            command_set_reverb(self->effectDspMain,self);
            g_mutex_unlock(&self->lock);
        }
        case PROP_HEADSET_LPF_BASS: {
            g_mutex_lock(&self->lock);
            self->headset_basslpf = g_value_get_int(value);
            command_set_reverb(self->effectDspMain,self);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_HEADSET_LPF_DAMP: {
            g_mutex_lock(&self->lock);
            self->headset_damplpf = g_value_get_int(value);
            command_set_reverb(self->effectDspMain,self);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_HEADSET_LPF_OUTPUT: {
            g_mutex_lock(&self->lock);
            self->headset_outputlpf = g_value_get_int(value);
            command_set_reverb(self->effectDspMain,self);
            g_mutex_unlock(&self->lock);
        }
            break;

        case PROP_HEADSET_REFLECTION_WIDTH: {
            g_mutex_lock(&self->lock);
            self->headset_reflection_width = g_value_get_float(value);
            command_set_reverb(self->effectDspMain,self);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_HEADSET_REFLECTION_FACTOR: {
            g_mutex_lock(&self->lock);
            self->headset_reflection_factor = g_value_get_float(value);
            command_set_reverb(self->effectDspMain,self);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_HEADSET_REFLECTION_AMOUNT: {
            g_mutex_lock(&self->lock);
            self->headset_reflection_amount = g_value_get_float(value);
            command_set_reverb(self->effectDspMain,self);
            g_mutex_unlock(&self->lock);
        }
            break;

        case PROP_HEADSET_FINALDRY: {
            g_mutex_lock(&self->lock);
            self->headset_finaldry = g_value_get_float(value);
            command_set_reverb(self->effectDspMain,self);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_HEADSET_FINALWET: {
            g_mutex_lock(&self->lock);
            self->headset_finalwet = g_value_get_float(value);
            command_set_reverb(self->effectDspMain,self);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_HEADSET_WIDTH: {
            g_mutex_lock(&self->lock);
            self->headset_width = g_value_get_float(value);
            command_set_reverb(self->effectDspMain,self);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_HEADSET_WET: {
            g_mutex_lock(&self->lock);
            self->headset_wet = g_value_get_float(value);
            command_set_reverb(self->effectDspMain,self);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_HEADSET_LFO_WANDER: {
            g_mutex_lock(&self->lock);
            self->headset_lfo_wander = g_value_get_float(value);
            command_set_reverb(self->effectDspMain,self);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_HEADSET_BASSBOOST: {
            g_mutex_lock(&self->lock);
            self->headset_bassboost = g_value_get_float(value);
            command_set_reverb(self->effectDspMain,self);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_HEADSET_LFO_SPIN: {
            g_mutex_lock(&self->lock);
            self->headset_lfo_spin = g_value_get_float(value);
            command_set_reverb(self->effectDspMain,self);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_HEADSET_DECAY: {
            g_mutex_lock(&self->lock);
            self->headset_decay = g_value_get_float(value);
            command_set_reverb(self->effectDspMain,self);
            g_mutex_unlock(&self->lock);
        }
            break;

        case PROP_STEREOWIDE_ENABLE: {
            g_mutex_lock(&self->lock);
            self->stereowide_enabled = g_value_get_boolean(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  1204, self->stereowide_enabled);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_STEREOWIDE_MCOEFF: {
            g_mutex_lock(&self->lock);
            self->stereowide_mcoeff = g_value_get_int(value);
            command_set_px4_vx2x2(self->effectDspMain,
                                  137, (int16_t) self->stereowide_mcoeff,self->stereowide_scoeff);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_STEREOWIDE_SCOEFF: {
            g_mutex_lock(&self->lock);
            self->stereowide_scoeff = g_value_get_int(value);
            command_set_px4_vx2x2(self->effectDspMain,
                                  137, (int16_t) self->stereowide_mcoeff,self->stereowide_scoeff);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_BS2B_ENABLE: {
            g_mutex_lock(&self->lock);
            self->bs2b_enabled = g_value_get_boolean(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  1208, self->bs2b_enabled);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_BS2B_FCUT: {
            g_mutex_lock(&self->lock);
            self->bs2b_fcut = g_value_get_int(value);
            if(self->bs2b_feed != 0)
                command_set_px4_vx2x2(self->effectDspMain,
                                  188, (int16_t)self->bs2b_fcut,(int16_t)self->bs2b_feed);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_BS2B_FEED: {
            g_mutex_lock(&self->lock);
            self->bs2b_feed = g_value_get_int(value);
            if(self->bs2b_feed != 0)
                command_set_px4_vx2x2(self->effectDspMain,
                                  188, (int16_t)self->bs2b_fcut,(int16_t)self->bs2b_feed);
            g_mutex_unlock(&self->lock);
        }
            break;


        case PROP_COMPRESSOR_ENABLE: {
            g_mutex_lock(&self->lock);
            self->compression_enabled = g_value_get_boolean(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  1200, self->compression_enabled);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_COMPRESSOR_PREGAIN: {
            g_mutex_lock(&self->lock);
            self->compression_pregain = g_value_get_int(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  100, (int16_t) self->compression_pregain);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_COMPRESSOR_THRESHOLD: {
            g_mutex_lock(&self->lock);
            self->compression_threshold  = g_value_get_int(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  101, (int16_t) self->compression_threshold);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_COMPRESSOR_KNEE: {
            g_mutex_lock(&self->lock);
            self->compression_knee  = g_value_get_int(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  102, (int16_t) self->compression_knee);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_COMPRESSOR_RATIO: {
            g_mutex_lock(&self->lock);
            self->compression_ratio  = g_value_get_int(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  103, (int16_t) self->compression_ratio);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_COMPRESSOR_ATTACK: {
            g_mutex_lock(&self->lock);
            self->compression_attack  = g_value_get_int(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  104, (int16_t) self->compression_attack);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_COMPRESSOR_RELEASE: {
            g_mutex_lock(&self->lock);
            self->compression_release  = g_value_get_int(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  105, (int16_t) self->compression_release);
            g_mutex_unlock(&self->lock);
        }
            break;

        case PROP_TONE_ENABLE: {
            g_mutex_lock(&self->lock);
            self->tone_enabled = g_value_get_boolean(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  1202, self->tone_enabled);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_TONE_FILTERTYPE: {
            g_mutex_lock(&self->lock);
            self->tone_filtertype = g_value_get_int(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  151, (int16_t) self->tone_filtertype);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_TONE_EQ:
        {
            g_mutex_lock (&self->lock);
            if (strlen (g_value_get_string (value)) < 64) {
                memset (self->tone_eq, 0,
                        sizeof(self->tone_eq));
                strcpy(self->tone_eq,
                       g_value_get_string (value));
                command_set_eq (self->effectDspMain, self->tone_eq);
            }else{
                printf("[E] EQ string too long (>64 bytes)");
            }
            g_mutex_unlock (&self->lock);
        }
            break;
        case PROP_MASTER_LIMTHRESHOLD:
        {
            g_mutex_lock (&self->lock);
            self->lim_threshold = g_value_get_float(value);
            command_set_limiter(self->effectDspMain,self->lim_threshold,self->lim_release);
            g_mutex_unlock (&self->lock);
        }
            break;
        case PROP_MASTER_LIMRELEASE:
        {
            g_mutex_lock (&self->lock);
            self->lim_release = g_value_get_float(value);
            command_set_limiter(self->effectDspMain,self->lim_threshold,self->lim_release);
            g_mutex_unlock (&self->lock);
        }
            break;
        case PROP_DDC_ENABLE: {
            g_mutex_lock(&self->lock);
            self->ddc_enabled = g_value_get_boolean(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  1212, self->ddc_enabled);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_DDC_COEFFS: {
            g_mutex_lock(&self->lock);
            memset (self->ddc_coeffs , 0,
                    sizeof(self->ddc_coeffs ));
            strcpy(self->ddc_coeffs ,
                   g_value_get_string (value));
            command_set_ddc(self->effectDspMain, self->ddc_coeffs,self->ddc_enabled);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_CONVOLVER_ENABLE: {
            g_mutex_lock(&self->lock);
            self->convolver_enabled = g_value_get_boolean(value);
            command_set_px4_vx2x1(self->effectDspMain,
                                  1205, self->convolver_enabled);
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_CONVOLVER_FILE: {
            g_mutex_lock(&self->lock);
            memset (self->convolver_file , 0,
                    sizeof(self->convolver_file ));
            strcpy(self->convolver_file ,
                   g_value_get_string (value));
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_CONVOLVER_BENCH_C0: {
            g_mutex_lock(&self->lock);
            memset (self->convolver_bench_c0 , 0,
                    sizeof(self->convolver_bench_c0 ));
            strcpy(self->convolver_bench_c0 ,
                   g_value_get_string (value));
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_CONVOLVER_BENCH_C1: {
            g_mutex_lock(&self->lock);
            memset (self->convolver_bench_c1 , 0,
                    sizeof(self->convolver_bench_c1 ));
            strcpy(self->convolver_bench_c1 ,
                   g_value_get_string (value));
            g_mutex_unlock(&self->lock);
        }
            break;
        case PROP_CONVOLVER_GAIN:
        {
            g_mutex_lock (&self->lock);
            self->convolver_gain = g_value_get_float(value);
            g_mutex_unlock (&self->lock);
        }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_jdspfx_get_property(GObject *object, guint prop_id,
                        GValue *value, GParamSpec *pspec) {
    Gstjdspfx * self = GST_JDSPFX (object);

    switch (prop_id) {

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

/* GstBaseTransform vmethod implementations */

static gboolean
gst_jdspfx_setup(GstAudioFilter *base, const GstAudioInfo *info) {
    Gstjdspfx * self = GST_JDSPFX (base);
    gint sample_rate = 0;

    if (self->effectDspMain == NULL)
        return FALSE;


    if (info) {
        sample_rate = GST_AUDIO_INFO_RATE(info);
    } else {
        sample_rate = GST_AUDIO_FILTER_RATE(self);
    }
    if (sample_rate <= 0){
        return FALSE;
    }


    return TRUE;
}

static gboolean
gst_jdspfx_stop(GstBaseTransform *base) {
    Gstjdspfx * self = GST_JDSPFX (base);

    g_mutex_lock(&self->lock);
    EffectDSPMain *intf = self->effectDspMain;
    intf->command(EFFECT_CMD_RESET, NULL, NULL, NULL, NULL);
    g_mutex_unlock(&self->lock);
    return TRUE;
}

/* this function does the actual processing
 */
static GstFlowReturn
gst_jdspfx_transform_ip(GstBaseTransform *base, GstBuffer *buf) {
    Gstjdspfx * filter = GST_JDSPFX (base);
    volatile guint idx, num_samples;
    short *pcm_data_s16;
    int32_t *pcm_data_s32;
    float *pcm_data_f32;
    GstClockTime timestamp, stream_time;
    GstMapInfo map;

    //Samplerate check
    if(filter->samplerate!=GST_AUDIO_FILTER_RATE(filter)){
        filter->samplerate = GST_AUDIO_FILTER_RATE(filter);
        g_mutex_lock(&filter->lock);
        command_set_buffercfg(filter->effectDspMain,filter->samplerate,filter->format);
        command_set_convolver(filter->effectDspMain, filter->convolver_file,filter->convolver_gain,filter->convolver_quality,
                              filter->convolver_bench_c0,filter->convolver_bench_c1,filter->samplerate);
        g_mutex_unlock(&filter->lock);
    }

    //Format check
    guint fmt = 0;
    GstStructure *stru;
    stru = gst_caps_get_structure (gst_pad_get_current_caps (base->sinkpad), 0);
    if(strstr(gst_structure_get_string(stru, "format"),"S16LE")!=NULL)
        fmt = s16le;
    else if(strstr(gst_structure_get_string(stru, "format"),"F32LE")!=NULL)
        fmt = f32le;
    else if(strstr(gst_structure_get_string(stru, "format"),"S32LE")!=NULL)
        fmt = s32le;
    else
        fmt = other;

    if(filter->format != fmt){
        filter->format = fmt;

        g_mutex_lock(&filter->lock);
        command_set_buffercfg(filter->effectDspMain,filter->samplerate,filter->format);
        g_mutex_unlock(&filter->lock);

        if(filter->format == other)
            printf("[E] FORMAT NOT SUPPORTED, attempting to continue anyway... (this should never happen, gstreamer checks formats before initializing pads)\n");
    }


    if (filter->fx_enabled) {
        timestamp = GST_BUFFER_TIMESTAMP(buf);
        stream_time =
                gst_segment_to_stream_time(&base->segment, GST_FORMAT_TIME, timestamp);

        if (GST_CLOCK_TIME_IS_VALID(stream_time))
            gst_object_sync_values(GST_OBJECT(filter), stream_time);

        if (G_UNLIKELY(GST_BUFFER_FLAG_IS_SET(buf, GST_BUFFER_FLAG_GAP)))
            return GST_FLOW_OK;


        gst_buffer_map(buf, &map, GST_MAP_READWRITE);
        num_samples = map.size / GST_AUDIO_FILTER_BPS(filter) / 2;

        audio_buffer_t *in = (audio_buffer_t *) malloc(
                sizeof(size_t)); //Allocate memory for the frameCount (size_t) in the struct
        audio_buffer_t *out = (audio_buffer_t *) malloc(sizeof(size_t));


        switch(filter->format){
            case s16le:
                pcm_data_s16 = (int16_t * )(map.data);
                for (idx = 0; idx < num_samples * 2; idx++) {
                    //pcm_data_s16[idx] >>= 1;
                }
                in->frameCount = (size_t) num_samples;
                in->s16 = (int16_t *) malloc(2 * num_samples * sizeof(int16_t)); //Allocate memory for the 16bit int array seperately (because it's a pointer)
                for (idx = 0; idx < num_samples * 2; idx++) {
                    in->s16[idx] = pcm_data_s16[idx];
                    //printf("%d ", in->s16[idx]);
                }

                out->frameCount = num_samples;
                out->s16 = (int16_t *) malloc(2 * num_samples * sizeof(int16_t));

                g_mutex_lock(&filter->lock);
                filter->effectDspMain->process(in, out);
                g_mutex_unlock(&filter->lock);
                for (idx = 0; idx < num_samples * 2; idx++) {
                    if (out->s16[idx])
                        pcm_data_s16[idx] = out->s16[idx];
                    //else printf("Index %d is null (outputbuffer) :(\n", idx);
                  }

                break;
                case s32le:
                pcm_data_s32 = (int32_t * )(map.data);

                in->frameCount = (size_t) num_samples;
                in->s32 = (int32_t *) malloc(2 * num_samples * sizeof(int32_t)); //Allocate memory for the 16bit int array seperately (because it's a pointer)
                for (idx = 0; idx < num_samples * 2; idx++) {
                    in->s32[idx] = pcm_data_s32[idx];
                    //printf("%d ", in->s16[idx]);
                }

                out->frameCount = num_samples;
                out->s32 = (int32_t *) malloc(2 * num_samples * sizeof(int32_t));

                g_mutex_lock(&filter->lock);
                filter->effectDspMain->process(in, out);
                g_mutex_unlock(&filter->lock);
                for (idx = 0; idx < num_samples * 2; idx++) {
                    if (out->s32[idx])
                        pcm_data_s32[idx] = out->s32[idx];
                    //else printf("Index %d is null (outputbuffer) :(\n", idx);
                }

                break;
            case f32le:
                pcm_data_f32 = (float * )(map.data);
                in->frameCount = (size_t) num_samples;
                in->f32 = (float *) malloc(2 * num_samples * sizeof(float));
                for (idx = 0; idx < num_samples * 2; idx++)
                    in->f32[idx] = pcm_data_f32[idx];

                out->frameCount = num_samples;
                out->f32 = (float*) malloc(2 * num_samples * sizeof(float));

                g_mutex_lock(&filter->lock);
                filter->effectDspMain->process(in, out);
                g_mutex_unlock(&filter->lock);
                for (idx = 0; idx < num_samples * 2; idx++) {
                    if (out->f32[idx])
                        pcm_data_f32[idx] = out->f32[idx];
                    //else printf("Index %d is null (outputbuffer) :(\n", idx);
                }
                break;
        }

        gst_buffer_unmap(buf, &map);

        switch(filter->format){
            case s16le:
                delete in->s16;
                delete out->s16;
                break;
            case f32le:
                delete in->f32;
                delete out->f32;
                break;
        }
        delete in;
        delete out;
    }

    return GST_FLOW_OK;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
jdspfx_init(GstPlugin *jdspfx) {
    return gst_element_register(jdspfx, "jdspfx", GST_RANK_NONE,
                                GST_TYPE_JDSPFX);
}

/* gstreamer looks for this structure to register jdspfx
 */
GST_PLUGIN_DEFINE (
        GST_VERSION_MAJOR,
        GST_VERSION_MINOR,
        jdspfx,
"jdspfx element",
jdspfx_init,
VERSION,
"GPL",
"GStreamer",
"http://gstreamer.net/"
)
