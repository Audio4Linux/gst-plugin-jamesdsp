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
    /* convolver */
            PROP_TUBE_ENABLE,
    PROP_TUBE_DRIVE
};

#define ALLOWED_CAPS \
  "audio/x-raw,"                            \
  " format=(string){"GST_AUDIO_NE(S16)"},"  \
  " rate=(int)[44100,MAX],"                 \
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
                                                     0, 100000, 0,
                                                     (GParamFlags)(G_PARAM_WRITABLE | GST_PARAM_CONTROLLABLE)));


    gst_element_class_set_static_metadata(gstelement_class,
                                          "jdspfx",
                                          "Filter/Effect/Audio",
                                          "JamesDSP Core wrapper for GStreamer1",
                                          "ThePBone <tim.schneeberger@outlook.com>");

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

    // analog modelling

    config_set_px0_vx0x0(self->effectDspMain, EFFECT_CMD_ENABLE);


    command_set_px4_vx2x1(self->effectDspMain,
                          1206, (int16_t) self->tube_drive);

    command_set_px4_vx2x1(self->effectDspMain,
                          150, self->tube_enabled);

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
    // convolver
    self->tube_enabled = FALSE;
    self->tube_drive = 0;

    /* initialize private resources */
    self->effectDspMain = NULL;
    self->effectDspMain = new EffectDSPMain();

    if (self->effectDspMain != NULL)
        sync_all_parameters(self);

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
            if (self->fx_enabled)config_set_px0_vx0x0(self->effectDspMain, EFFECT_CMD_ENABLE);
            else config_set_px0_vx0x0(self->effectDspMain, EFFECT_CMD_DISABLE);
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
    if (sample_rate <= 0)
        return FALSE;

    GST_DEBUG_OBJECT(self, "current sample_rate = %d", sample_rate);

    g_mutex_lock(&self->lock);

    config_set_px0_vx0x0(self->effectDspMain, EFFECT_CMD_INIT);
    self->effectDspMain->command(EFFECT_CMD_SET_CONFIG, (uint32_t)
    sizeof(double), (void *) sample_rate, NULL, NULL);

    //self->effectDspMain->command(EFFECT_CMD_RESET,NULL,NULL,NULL,NULL);
    g_mutex_unlock(&self->lock);

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
    if (filter->fx_enabled) {
        Gstjdspfx * filter = GST_JDSPFX (base);
        volatile guint idx, num_samples;
        short *pcm_data;
        GstClockTime timestamp, stream_time;
        GstMapInfo map;

        timestamp = GST_BUFFER_TIMESTAMP(buf);
        stream_time =
                gst_segment_to_stream_time(&base->segment, GST_FORMAT_TIME, timestamp);

        if (GST_CLOCK_TIME_IS_VALID(stream_time))
            gst_object_sync_values(GST_OBJECT(filter), stream_time);

        if (G_UNLIKELY(GST_BUFFER_FLAG_IS_SET(buf, GST_BUFFER_FLAG_GAP)))
            return GST_FLOW_OK;


        printf("\n\n------BEGIN------\n");

        gst_buffer_map(buf, &map, GST_MAP_READWRITE);
        num_samples = map.size / GST_AUDIO_FILTER_BPS(filter) / 2;
        pcm_data = (int16_t * )(map.data);
        for (idx = 0; idx < num_samples * 2; idx++) {
            pcm_data[idx] >>= 1;

        }
        audio_buffer_t *in = (audio_buffer_t *) malloc(
                sizeof(size_t)); //Allocate memory for the frameCount (size_t) in the struct
        in->frameCount = (size_t) num_samples;
        in->s16 = (int16_t *) malloc(2 * num_samples *
                                     sizeof(int16_t)); //Allocate memory for the 16bit int array seperately (because it's a pointer)
        for (idx = 0; idx < num_samples * 2; idx++) {
            in->s16[idx] = pcm_data[idx];
            printf("%d ", in->s16[idx]);
        }

        audio_buffer_t *out = (audio_buffer_t *) malloc(sizeof(size_t));
        out->frameCount = num_samples;
        out->s16 = (int16_t *) malloc(2 * num_samples * sizeof(int16_t));

        printf("\n\n\n");

        g_mutex_lock(&filter->lock);
        filter->effectDspMain->process(in, out);
        g_mutex_unlock(&filter->lock);
        for (idx = 0; idx < num_samples * 2; idx++) {
            if (out->s16[idx]) {
                printf("%d ", out->s16[idx]);
                pcm_data[idx] = out->s16[idx];
                pcm_data[idx] <<= 1;
            } else printf("Index %d is null (outputbuffer) :(\n", idx);
        }
        printf("\n------END------");

        gst_buffer_unmap(buf, &map);
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
"LGPL",
"GStreamer",
"http://gstreamer.net/"
)
