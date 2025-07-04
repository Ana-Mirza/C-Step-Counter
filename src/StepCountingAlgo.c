/* 
The MIT License (MIT)

Copyright (c) 2020 Anna Brondin and Marcus Nordström

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <stddef.h>
#include "StepCountingAlgo.h"
#include "ringbuffer.h"
#include "preProcessingStage.h"
#include "motionDetectStage.h"
#include "filterStage.h"
#include "scoringStage.h"
#include "detectionStage.h"
#include "postProcessingStage.h"

#include "string.h"
#include <stdio.h>

#define STRIDECONST 0.414

/* General data */
static met_t met;
static float bmrPerMinute;
float stride;

static steps_t steps;
static float distance;

/* Extern variables */
float bmr;
double kcalories;

/* User data */
static gender_t gender;
static age_t age;
static height_t height;
static weight_t weight;

/* Buffers */
static ring_buffer_t rawBuf;
static ring_buffer_t ppBuf;
static ring_buffer_t mdBuf;
#ifndef SKIP_FILTER
static ring_buffer_t smoothBuf;
#endif
static ring_buffer_t peakScoreBuf;
static ring_buffer_t peakBuf;

static void increaseMET();
static void increaseDistance();
 
static void increaseStepCallback(void)
{
    steps++;
    increaseDistance();
}

static void increaseDistance() 
{
    /* compute distance dynamically */
    data_point_t lastDataPoint = getLastDataPoint();

    distance += lastDataPoint.orig_magnitude * lastDataPoint.weight;
}

void initUserData(char* userGender, uint8_t userAge, uint8_t userHeight, uint8_t userWeight) 
{
    /* init user information */
    gender = userGender;
    age = userAge;
    height = userHeight;
    weight = userWeight;
    kcalories = 0;

    /* init mbr */
    bmr = strcmp(gender, "F") == 0 ? 
            (9.56 * weight) + (1.85 * height) - (4.68 * age) + 655 :
            (13.75 * weight) + (5 * height) - (6.76 * age) + 66;
    bmrPerMinute = bmr / (24 * 60); /* convert to bmr per min */

    /* init static stride length */
    float height_float = height;
    stride = (height_float / 100) * STRIDECONST;
}

void initAlgo(char* gender, uint8_t age, uint8_t height, uint8_t weight)
{
    /* Set user data */
    initUserData(gender, age, height, weight);

    /* Init buffers */
    ring_buffer_init(&rawBuf);
    ring_buffer_init(&ppBuf);
    ring_buffer_init(&mdBuf);
#ifndef SKIP_FILTER
    ring_buffer_init(&smoothBuf);
#endif
    ring_buffer_init(&peakScoreBuf);
    ring_buffer_init(&peakBuf);

    initPreProcessStage(&rawBuf, &ppBuf, motionDetectStage);
#ifdef SKIP_FILTER
    initMotionDetectStage(&ppBuf, &mdBuf, scoringStage);
    initScoringStage(&mdBuf, &peakScoreBuf, detectionStage);
#else
    initMotionDetectStage(&ppBuf, &mdBuf, filterStage);
    initFilterStage(&mdBuf, &smoothBuf, scoringStage);
    initScoringStage(&smoothBuf, &peakScoreBuf, detectionStage);
#endif
    initDetectionStage(&peakScoreBuf, &peakBuf, postProcessingStage);
    initPostProcessingStage(&peakBuf, &increaseStepCallback);

    /* Set parameters */
    changeWindowSize(OPT_WINDOWSIZE);
    changeDetectionThreshold(OPT_DETECTION_THRESHOLD, OPT_DETECTION_THRESHOLD_FRAC);
    changeTimeThreshold(OPT_TIME_THRESHOLD);
    changeMotionThreshold(MOTION_THRESHOLD);

    currentTime = 0;
}

void processSample(time_accel_t time, accel_t x, accel_t y, accel_t z)
{
    preProcessSample(time, x, y, z);
}

void resetSteps(void)
{
    steps = 0;
    distance = 0;
    met = 0;
    kcalories = 0;
}

void resetAlgo(void)
{
    resetPreProcess();
    resetDetection();
    resetPostProcess();
    ring_buffer_init(&rawBuf);
    ring_buffer_init(&ppBuf);
    ring_buffer_init(&mdBuf);
#ifndef SKIP_FILTER
    ring_buffer_init(&smoothBuf);
#endif
    ring_buffer_init(&peakScoreBuf);
    ring_buffer_init(&peakBuf);

    kcalories = 0;
    met = 0;
    distance = 0;
}

steps_t getSteps(void)
{
    return steps;
}

float getDistance(void) {
    /* constant stride length distance computation */
    // float static_dist = steps * stride;

    float total_dist = distance / 1000;
    
    return total_dist;
}

float getStepsPerSec(void) {
    data_point_t lastDataPoint = getLastDataPoint();
    float stepsPerSec = (float)steps / ((float)lastDataPoint.time / 1000);

    return stepsPerSec;
}

calorie_t getCalories(void) 
{
    return (kcalories / 24 / 60 / 60 / 1000); /* convert to calories from calorie per day to ms of activity */
}

float getMeanAvg(void) {
    return meanPeakTime;
}
