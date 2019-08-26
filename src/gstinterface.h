#ifndef GST_PLUGIN_JAMESDSP_GSTINTERFACE_H
#define GST_PLUGIN_JAMESDSP_GSTINTERFACE_H
#include "EffectDSPMain.h"
#include <stdio.h>
#include <stdint.h>
void command_set_px4_vx2x1(EffectDSPMain *intf,int32_t cmd,int16_t value){
    effect_param_t *cep = (effect_param_t *)malloc(5*sizeof(int32_t));
    cep->psize = 4;
    cep->vsize = 2;
    cep->status = 0;

    int32_t * cmd_data_int = (int32_t *)cep->data;
    memset (cep->data, 0, sizeof(8));
    cmd_data_int[0] = cmd;
    cmd_data_int[1] = value;

    intf->command(EFFECT_CMD_SET_PARAM, sizeof(unsigned char)*20,cep,NULL,NULL);
}
void config_set_px0_vx0x0(EffectDSPMain *intf,uint32_t param){
    intf->command(param,NULL,NULL,NULL,NULL);
}
#endif //GST_PLUGIN_JAMESDSP_GSTINTERFACE_H
