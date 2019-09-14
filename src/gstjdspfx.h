#ifndef __GST_JDSPFX_H__
#define __GST_JDSPFX_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/audio/audio.h>
#include <gst/audio/gstaudiofilter.h>
#include "EffectDSPMain.h"

G_BEGIN_DECLS

#define GST_TYPE_JDSPFX            (gst_jdspfx_get_type())
#define GST_JDSPFX(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_JDSPFX,Gstjdspfx))
#define GST_JDSPFX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_JDSPFX,GstjdspfxClass))
#define GST_JDSPFX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_JDSPFX,GstjdspfxClass))
#define GST_IS_JDSPFX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_JDSPFX))
#define GST_IS_JDSPFX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_JDSPFX))

enum formats{
    notready = 0x0,
    s16le,
    f32le,
    s32le,
    other
};


typedef struct _Gstjdspfx Gstjdspfx;
typedef struct _GstjdspfxClass GstjdspfxClass;

struct _Gstjdspfx {
    GstAudioFilter audiofilter;
    gint samplerate = 0;
    guint format = 0;

    /* properties */
    // global enable
    gboolean fx_enabled;

    // analog remodelling
    gboolean tube_enabled;
    gint32 tube_drive;

    // bass boost
    gboolean bass_enabled;
    gint32 bass_mode;
    gint32 bass_filtertype;
    gint32 bass_freq;

    // reverb
    gboolean headset_enabled;
    gint32 headset_osf;
    gint32 headset_delay;
    gint32 headset_inputlpf;
    gint32 headset_basslpf;
    gint32 headset_damplpf;
    gint32 headset_outputlpf;

    gfloat headset_reflection_amount;
    gfloat headset_reflection_factor;
    gfloat headset_reflection_width;
    gfloat headset_finaldry;
    gfloat headset_finalwet;
    gfloat headset_width;
    gfloat headset_wet;
    gfloat headset_lfo_wander;
    gfloat headset_bassboost;
    gfloat headset_lfo_spin;
    gfloat headset_decay;

    // stereo wide
    gboolean stereowide_enabled;
    gint32 stereowide_mcoeff;
    gint32 stereowide_scoeff;

    // bs2b
    gboolean bs2b_enabled;
    gint32 bs2b_fcut;
    gint32 bs2b_feed;

    // compressor
    gboolean compression_enabled;
    gint32 compression_pregain;
    gint32 compression_threshold;
    gint32 compression_knee;
    gint32 compression_ratio;
    gint32 compression_attack;
    gint32 compression_release;

    // mixed equalizer
    gboolean tone_enabled;
    gint32 tone_filtertype;
    gchar tone_eq[64];

    // master
    gfloat lim_threshold;
    gfloat lim_release;

    // ddc
    gboolean ddc_enabled;
    gchar ddc_coeffs[4096];

    /* < private > */
    EffectDSPMain *effectDspMain;
    void *so_handle;
    GMutex lock;
};

struct _GstjdspfxClass {
    GstAudioFilterClass parent_class;
};

GType gst_jdspfx_get_type(void);

G_END_DECLS

#endif /* __GST_JDSPFX_H__ */
