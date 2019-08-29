#include "Effect.h"

Effect::Effect()
    : mSamplingRate(48000.0), formatFloatModeInt32Mode(0)
{
#ifdef DEBUG
	printf("[I] Effect class created\n");
#endif
}

Effect::~Effect()
{
#ifdef DEBUG
	printf("[I] Effect class destroyed\n");
#endif
}

/* Configure a bunch of general parameters. */
int32_t Effect::configure(void* pCmdData, effect_buffer_access_e* mAccessMode)
{
    ////GST DOES NOT NEED FURTHER CONFIGURING, SKIPPING FOR NOW
    double *sr = (double*)(&pCmdData);
    mSamplingRate = *sr;
    *mAccessMode = (effect_buffer_access_e) EFFECT_BUFFER_ACCESS_ACCUMULATE;
    return 0;

    /*effect_config_t *cfg = (effect_config_t*)pCmdData;
    buffer_config_t in = cfg->inputCfg;
    buffer_config_t out = cfg->outputCfg;*/
    /* Check that we aren't asked to do resampling. Note that audioflinger
     * always provides full setup info at initial configure time. */
    /*#ifdef DEBUG
          printf("[E] Sample rate of In: %u and out: %u", in.samplingRate, out.samplingRate);
  #endif
      if ((in.mask & EFFECT_CONFIG_SMP_RATE) && (out.mask & EFFECT_CONFIG_SMP_RATE))
      {
          if (out.samplingRate != in.samplingRate)
          {
  #ifdef DEBUG
      printf("[E] In/out sample rate doesn't match");
  #endif
      return -EINVAL;
          }
          mSamplingRate = (double)in.samplingRate;
      }
      if (in.mask & EFFECT_CONFIG_CHANNELS && out.mask & EFFECT_CONFIG_CHANNELS)
      {
          if (in.channels != AUDIO_CHANNEL_OUT_STEREO)
          {
  #ifdef DEBUG
      printf("[E] Input is non stereo signal. It's channel count is %u", in.channels);
  #endif
              return -EINVAL;
          }
          if (out.channels != AUDIO_CHANNEL_OUT_STEREO)
          {
  #ifdef DEBUG
      printf("[E] Output is non stereo signal. It's channel count is %u", out.channels);
  #endif
              return -EINVAL;
          }
      }
      else
      {
  #ifdef DEBUG
      printf("[E] In/out channel mask doesn't match");
  #endif
      }
      if (in.mask & EFFECT_CONFIG_FORMAT)
      {
          if (in.format != AUDIO_FORMAT_PCM_16_BIT)
          {
              if (in.format == AUDIO_FORMAT_PCM_FLOAT)
                  formatFloatModeInt32Mode = 1;
              else if (in.format == AUDIO_FORMAT_PCM_32_BIT)
                  formatFloatModeInt32Mode = 2;
  #ifdef DEBUG
      printf("[E] Input is not 16 bit PCM. FMT is %u", in.format);
  #endif
          }
      }
      if (out.mask & EFFECT_CONFIG_FORMAT)
      {
          if (out.format != AUDIO_FORMAT_PCM_16_BIT)
          {
              if (out.format == AUDIO_FORMAT_PCM_FLOAT)
                  formatFloatModeInt32Mode = 1;
              else if (in.format == AUDIO_FORMAT_PCM_32_BIT)
                  formatFloatModeInt32Mode = 2;
  #ifdef DEBUG
      printf("[E] Output is not 16 bit PCM. FMT is %u\n", in.format);
  #endif
          }
      }
      if (out.mask & EFFECT_CONFIG_ACC_MODE)
          *mAccessMode = (effect_buffer_access_e) out.accessMode;
      return 0;*/
}

int32_t Effect::command(uint32_t cmdCode, uint32_t cmdSize, void *pCmdData, uint32_t *replySize, void* pReplyData)
{
    switch (cmdCode)
    {
    case EFFECT_CMD_ENABLE:
    case EFFECT_CMD_DISABLE:
    {
        mEnable = cmdCode == EFFECT_CMD_ENABLE;
        if(pReplyData!=NULL) {
            int32_t *replyData = (int32_t *) pReplyData;
            *replyData = 0;
        }
        break;
    }
    case EFFECT_CMD_INIT:
    case EFFECT_CMD_SET_CONFIG:
    case EFFECT_CMD_SET_PARAM:
    case EFFECT_CMD_SET_PARAM_COMMIT:
    {
        if(pReplyData!=NULL) {
            int32_t *replyData = (int32_t *) pReplyData;
            *replyData = 0;
        }
        break;
    }
    case EFFECT_CMD_RESET:
    case EFFECT_CMD_SET_PARAM_DEFERRED:
    case EFFECT_CMD_SET_DEVICE:
    case EFFECT_CMD_SET_AUDIO_MODE:
        break;
    case EFFECT_CMD_GET_PARAM:
    {
        effect_param_t *rep = (effect_param_t *) pReplyData;
        rep->status = -EINVAL;
        rep->psize = 0;
        rep->vsize = 0;
        *replySize = 12;
        break;
    }
    }
    return 0;
}