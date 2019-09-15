#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sndfile.h>
#include <samplerate.h>
#include "AutoConvolver.h"
SF_INFO sfiIRInfo;
SNDFILE *sfIRFile;
int *GetLoadImpulseResponseInfo(char *mIRFileName)
{
    if (strlen(mIRFileName) <= 0) return 0;
    int* jImpInfo = (int*)malloc(sizeof(int)*4);

    if (!jImpInfo) return 0;
    memset(&sfiIRInfo, 0, sizeof(SF_INFO));
    sfIRFile = sf_open(mIRFileName, SFM_READ, &sfiIRInfo);
    if (sfIRFile == NULL)
    {
        // Open failed or invalid wave file
        printf("[E] Convolver: Invalid file\n");
        return 0;
    }
    // Sanity check
    if ((sfiIRInfo.channels != 1) && (sfiIRInfo.channels != 2) && (sfiIRInfo.channels != 4))
    {
        printf("[E] Convolver: Unsupported amount of channels\n");
        sf_close(sfIRFile);
        return 0;
    }
    if ((sfiIRInfo.samplerate <= 0) || (sfiIRInfo.frames <= 0))
    {
        // Negative sampling rate or empty data ?
        printf("[E] Convolver: Negative samplerate or empty data\n");
        sf_close(sfIRFile);
        return 0;
    }
    jImpInfo[0] = (int)sfiIRInfo.channels;
    jImpInfo[1] = (int)sfiIRInfo.frames;
    jImpInfo[2] = (int)sfiIRInfo.samplerate;
    jImpInfo[3] = (int)sfiIRInfo.format;
    return jImpInfo;
}
float* ReadImpulseResponseToFloat
        (int targetSampleRate)
{
    // Allocate memory block for reading
    float* outbuf;
    int i;
    float *final;
    int frameCountTotal = sfiIRInfo.channels * sfiIRInfo.frames;
    size_t bufferSize = frameCountTotal * sizeof(float);
    outbuf = (float*)malloc(bufferSize);

    float *pFrameBuffer = (float*)malloc(bufferSize);
    if (!pFrameBuffer)
    {
        // Memory not enough
        printf("[E] Convolver: Insufficient Memory\n");
        sf_close(sfIRFile);
        return 0;
    }
    sf_readf_float(sfIRFile, pFrameBuffer, sfiIRInfo.frames);
    sf_close(sfIRFile);
    if (sfiIRInfo.samplerate == targetSampleRate)
    {
        final = (float*)malloc(bufferSize);
        memcpy(final, pFrameBuffer, bufferSize);
        // Prepare return array
        memcpy(outbuf, final, bufferSize);
        free(final);
    }
    else
    {
        double convertionRatio = (double)targetSampleRate / (double)sfiIRInfo.samplerate;
        int resampledframeCountTotal = (int)((double)frameCountTotal * convertionRatio);
        int outFramesPerChannel = (int)((double)sfiIRInfo.frames * convertionRatio);
        bufferSize = resampledframeCountTotal * sizeof(float);
        float *out = (float*)malloc(bufferSize);
        int error;
        SRC_DATA data;
        data.data_in = pFrameBuffer;
        data.data_out = out;
        data.input_frames = sfiIRInfo.frames;
        data.output_frames = outFramesPerChannel;
        data.src_ratio = convertionRatio;
        error = src_simple(&data, 1, sfiIRInfo.channels);
        final = (float*)malloc(bufferSize);
        memcpy(final, out, bufferSize);

        memcpy(outbuf, final, bufferSize);

        free(out);
        free(final);
    }
    free(pFrameBuffer);
    return outbuf;
}
int FFTConvolutionBenchmark
        (int entriesGen, int fs, double* bufc0, double* bufc1)
{
    double **result = PartitionHelperDirect(entriesGen, fs);

    int length = (int)(sizeof(bufc0) / sizeof((bufc0)[0]));
    if (length < entriesGen)
        entriesGen = length;
    for (int i = 0; i < entriesGen; i++)
    {
        bufc0[i] = result[0][i];
        bufc1[i] = result[1][i];
    }
    free(result[0]);
    free(result[1]);
    free(result);
    return entriesGen;
}
char* FFTConvolutionBenchmarkToString
        (int entriesGen, int fs)
{
    return PartitionHelper(entriesGen, fs);
}