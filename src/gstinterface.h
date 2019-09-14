/*********************
 * 8/29/2019
 * github.com/ThePBone
**********************/

#ifndef GST_PLUGIN_JAMESDSP_GSTINTERFACE_H
#define GST_PLUGIN_JAMESDSP_GSTINTERFACE_H
#include "EffectDSPMain.h"
#include "gstjdspfx.h"
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdlib.h>

#define NUM_BANDS 15

char* memory_read_ascii(char *path);
void helper_strreplace(char*,const char*,const char*);

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
///Sends two 16bit ints
void command_set_px4_vx2x2(EffectDSPMain *intf,int32_t cmd,int16_t valueA,int16_t valueB){
    effect_param_t *cep = (effect_param_t *)malloc(5*sizeof(int32_t));
    cep->psize = 4;
    cep->vsize = 4;
    cep->status = 0;

    int32_t * cmd_data_int = (int32_t *)cep->data;
    memset (cep->data, 0, sizeof(8));
    cmd_data_int[0] = cmd;

    int16_t * cmd_data_int16 = (int16_t *)cep->data;
    cmd_data_int16[2] = valueA;
    cmd_data_int16[3] = valueB;

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
///Sends two 32bit int values (as an array)
void command_set_px4_vx8x2(EffectDSPMain *intf,int32_t cmd,int32_t *values){
    effect_param_t *cep = (effect_param_t *)malloc(6*sizeof(int32_t));
    cep->psize = 4;
    cep->vsize = 8;
    cep->status = 0;
    int32_t * cmd_data_int = (int32_t *)cep->data;
    cmd_data_int[0] = cmd;
    for (int i = 0; i < 2; i++)
        cmd_data_int[1 + i] = (int32_t) values[i];

    intf->command(EFFECT_CMD_SET_PARAM, sizeof(int32_t)*6,cep,NULL,NULL);
}
///Sends one 256 byte char array
void command_set_px4_vx256x1(EffectDSPMain *intf,int32_t cmd,const char *buffer){
    effect_param_t *cep = (effect_param_t *)malloc(4*sizeof(int32_t)+256*sizeof(char));
    cep->psize = 4;
    cep->vsize = 256;
    cep->status = 0;

    char * cmd_data_char = (char *)cep->data;
    memset(&cmd_data_char, 0, 256*sizeof(char)); //Fill buffer with zeroes

    int32_t * cmd_data_int = (int32_t*)((int32_t*)cep->data)[0];
    cmd_data_int = &cmd;
    for (int i = 0; i < 2; i++)
        cmd_data_char[4 + i] = (char) buffer[i]; //Offset +4 because the int32_t cmd-id is in front of it

    intf->command(EFFECT_CMD_SET_PARAM, 4*sizeof(int32_t)+256*sizeof(char),cep,NULL,NULL);
}
///Configure buffer
void command_set_buffercfg(EffectDSPMain *intf,int32_t samplerate,int32_t format){
    dsp_config_t *cep = (dsp_config_t *)malloc(sizeof(uint32_t)+sizeof(uint8_t));
    uint8_t result = 0;
    switch(format){
        case s16le:
            result = 0;
            break;
        case s32le:
            result = 2;
            break;

        case f32le:
        default:
            result = 1;
    }
    cep->samplingRate = (uint32_t)samplerate;
    cep->format = result;
    intf->command(EFFECT_CMD_SET_CONFIG, sizeof(uint32_t)+sizeof(uint8_t),cep,NULL,NULL);
}
///Load and send DDC data
void command_set_ddc(EffectDSPMain *intf,char* path,bool enabled){
    if(!path || path == NULL){
        printf("[E] DDC path is NULL\n");
        return;
    }
    int p = 0;
    for (int i = 0; i < strlen(path); ++i) {
        p |= path[i];
    }
    if (p == 0) {
        printf("[E] DDC path char array contains zero data\n");
        return;
    }
    char *ddcString = memory_read_ascii(path);
    if(!ddcString || ddcString == NULL){
        printf("[E] File reader returned a null pointer. Probably unable to open DDC file\n");
        return;
    }
    int d = 0;
    for (int i = 0; i < strlen(ddcString); ++i) {
        d |= ddcString[i];
    }
    if (d == 0) {
        printf("[E] DDC coeffs char array contains zero data\n");
        return;
    }
    int begin = strcspn(ddcString,"S");
    if(strcspn(ddcString,"R")!=begin+1){ //check for 'SR' in the string
        printf("[E] Invalid DDC string\n");
        return;
    }
    helper_strreplace(ddcString,"NaN","0"); //Prevent white noise at full blast caused by invalid SOS data
    helper_strreplace(ddcString,"nan","0");
    helper_strreplace(ddcString,"null","0");
    intf->_loadDDC(ddcString);
    command_set_px4_vx2x1(intf,1212,enabled);
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
            //printf("%d -> %f\n", n,(float)n);
        }
        else{
            float d = (float)n / 100;
            //printf("%d -> %f\n", n,d);
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
///Prepare and send reverb data
void command_set_reverb(EffectDSPMain *intf,Gstjdspfx * self){
    reverbdata_t *r = (reverbdata_t*)malloc(sizeof(*r));
    r->oversamplefactor = self->headset_osf;
    r->ertolate = self->headset_reflection_amount;
    r->erefwet = self->headset_finalwet;
    r->dry = self->headset_finaldry;
    r->ereffactor = self->headset_reflection_factor;
    r->erefwidth = self->headset_reflection_width;
    r->width = self->headset_width;
    r->wet = self->headset_wet;
    r->wander = self->headset_lfo_wander;
    r->bassb = self->headset_bassboost;
    r->spin = self->headset_lfo_spin;
    r->inputlpf = self->headset_inputlpf;
    r->basslpf = self->headset_basslpf;
    r->damplpf = self->headset_damplpf;
    r->outputlpf = self->headset_outputlpf;
    r->rt60 = self->headset_decay;
    r->delay = (double)(self->headset_delay/1000);
    intf->_loadReverb(r);
}
///Read ascii-data from file
char* memory_read_ascii(char *path){
    int c; long size; FILE *file;
    int i = 0;
    file = fopen(path, "r");
    if (file) {
        fseek(file, 0, SEEK_END);
        size = ftell(file);
        fseek(file, 0, SEEK_SET);
        char *buffer = (char*)malloc(size*sizeof(char));
        while ((c = getc(file)) != EOF) {
            buffer[i] = (char)c;
            i++;
        }
        fclose(file);
        return buffer;
    }
    return NULL;
}
///Replace all occurrences of 'str' with 'rep' in 'src'
void helper_strreplace(char *target, const char *needle, const char *replacement)
{
    char buffer[strlen(target)] = { 0 };
    char *insert_point = &buffer[0];
    const char *tmp = target;
    size_t needle_len = strlen(needle);
    size_t repl_len = strlen(replacement);

    while (1) {
        const char *p = strstr(tmp, needle);

        // walked past last occurrence of needle; copy remaining part
        if (p == NULL) {
            strcpy(insert_point, tmp);
            break;
        }

        // copy part before needle
        memcpy(insert_point, tmp, p - tmp);
        insert_point += p - tmp;

        // copy replacement string
        memcpy(insert_point, replacement, repl_len);
        insert_point += repl_len;

        // adjust pointers, move on
        tmp = p + needle_len;
    }

    // write altered string back to target
    strcpy(target, buffer);
}
#endif //GST_PLUGIN_JAMESDSP_GSTINTERFACE_H
