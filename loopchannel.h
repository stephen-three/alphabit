// loopchannel.h
// Aug 2026. Stephen Johnson III
// LoopChannel class for the alphabit pedal.
// This class manages the state and behavior of a single audio loop channel, with functionality for for the multi-channel functions of alphabit.
#ifndef LOOPCHANNEL_H
#define LOOPCHANNEL_H

#include "daisy_seed.h"

class LoopChannel
{
private:
    bool firstPass;
    bool rec;
    bool recorded;
    bool play;
    bool reset;
    int len;
    int mod;
    int position;
    bool rvrs;
    float lvl;
    uint16_t playbackSpeed;
    float *p_loop = nullptr;
    int size;
    int16_t indexTracker;
    float rateRemainder;
    uint8_t pushVal;

public:
    LoopChannel(float *loop, size_t sz);

    // Getters
    inline bool get_rec() { return rec;}

    inline bool get_recdd() { return recorded; }

    inline bool resetEnabled() { return (recorded && reset); }

    inline bool get_play() { return play; }

    inline int get_pos() { return position; }

    inline float get_lvl() { return lvl; }

    // Setters
    void start_REC();

    void stop_REC();

    void latch_REC();

    inline void toggle_play() { if (recorded) play = !play; }

    inline void set_play(bool p) { play = p; }

    inline void set_rvrs(bool setting) { rvrs = setting; }

    inline void set_lvl(float setting) { lvl = setting; }

    inline void set_speed(uint16_t setting) { playbackSpeed = setting; }

    // other functions
    void ClearLoop();

    void ResetBuffer();

    void NextSample(float &playback, daisy::AudioHandle::InputBuffer in, size_t i);

    void NextSample_1(
        float &playback,
        daisy::AudioHandle::InputBuffer in,
        size_t i,
        LoopChannel *a
    );

    void NextSample_2(
        float &playback,
        daisy::AudioHandle::InputBuffer in,
        size_t i,
        LoopChannel *a
    );

private:
    void WriteBuffer(daisy::AudioHandle::InputBuffer in, size_t i);

    void Shift_position();

    // called in NextSample_1()
    void Shift_position(LoopChannel *a);
};

#endif