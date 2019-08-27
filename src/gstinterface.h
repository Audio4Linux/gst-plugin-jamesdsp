#ifndef GST_PLUGIN_JAMESDSP_GSTINTERFACE_H
#define GST_PLUGIN_JAMESDSP_GSTINTERFACE_H
#include "EffectDSPMain.h"
#include <stdio.h>
#include <stdint.h>
#define NUM_BANDS 15
///Sends 16bit int data
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
///Sends 32bit float arrays
void command_set_px4_vx4x60(EffectDSPMain *intf,int32_t cmd,float *values){
    effect_param_t *cep = (effect_param_t *)malloc(5*sizeof(int32_t)+sizeof(float)*NUM_BANDS);
    cep->psize = 4;
    cep->vsize = 60;
    cep->status = 0;
    float * cmd_data_int = (float *)cep->data;
    cmd_data_int[0] = cmd;
    for (int i = 0; i < NUM_BANDS; i++)
        cmd_data_int[1 + i] = (float) values[i];

    intf->command(EFFECT_CMD_SET_PARAM, sizeof(float)*NUM_BANDS+5*sizeof(int32_t),cep,NULL,NULL);
}
///Sends two 32bit float values (as an array)
void command_set_px4_vx8x2(EffectDSPMain *intf,int32_t cmd,float *values){
    effect_param_t *cep = (effect_param_t *)malloc(6*sizeof(float));
    cep->psize = 4;
    cep->vsize = 8;
    cep->status = 0;
    float * cmd_data_int = (float *)cep->data;
    cmd_data_int[0] = cmd;
    for (int i = 0; i < 2; i++)
        cmd_data_int[1 + i] = (float) values[i];

    intf->command(EFFECT_CMD_SET_PARAM, sizeof(float)*6,cep,NULL,NULL);
}
///Prepare and send limiter data as 32-bit float array
void command_set_limiter(EffectDSPMain *intf,float thres,float release){
    float* data = (float*)malloc(sizeof(float)*2);
    data[0] = thres;
    data[1] = release;
    command_set_px4_vx8x2(intf,1500,data);
}
///Parse and send eq data as 32-bit float array
void command_set_eq(EffectDSPMain *intf,char* eq){
    char *end = eq;
    float data[NUM_BANDS];
    int i = 0;
    while(*end) {
        if(i>=NUM_BANDS) {
            printf("[W] More than %d values in EQ string\n",NUM_BANDS);
            break;
        }
        int n = strtol(eq, &end, 10);
        if(n>1200){
            data[i] = 12;
            printf("[W] EQ: Value at index %d is too high (>1200)\n",i);
        }else if(-1200>n){
            data[i] = -12;
            printf("[W] EQ: Value at index %d is too low (-1200<)\n",i);
        }else if(n==0){
            data[i]=0;
            printf("%d -> %f\n", n,(float)n);
        }
        else{
            float d = (float)n / 100;
            printf("%d -> %f\n", n,d);
            data[i] = d;
        }
        while (*end == ';') {
            end++;
        }
        i++;
        eq = end;
    }
    command_set_px4_vx4x60(intf,115,(float*)data);

}
///Sends command-codes without parameters
void config_set_px0_vx0x0(EffectDSPMain *intf,uint32_t param){
    intf->command(param,NULL,NULL,NULL,NULL);
}
#endif //GST_PLUGIN_JAMESDSP_GSTINTERFACE_H
