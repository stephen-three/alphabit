// loopchannel.cpp
// Aug 2026. Stephen Johnson III

#include "loopchannel.h"

LoopChannel::LoopChannel(float *loop, size_t sz)
  : firstPass(true),
    rec(false),
    recorded(false),
    play(false),
    reset(false),
    len(0),
    mod(sz),
    position(0),
    rvrs(false),
    lvl(1.f),
    playbackSpeed(100),
    p_loop(loop),
    size(sz),
    indexTracker(0),
    rateRemainder(0.f),
    pushVal(0)
{}

void LoopChannel::start_REC()
{
    rec = true;
    reset = true;
    play = true;
}

void LoopChannel::stop_REC()
{
    if (firstPass && rec)
    {
        firstPass = false;
        mod = len;
        len = 0;
    }
    rec = false;
}

void LoopChannel::latch_REC()
{
    if (firstPass && rec)
    {
        firstPass = false;
        mod = len;
        len = 0;
    }
    reset = true;
    play = true;
    rec = !rec;
}

void LoopChannel::ClearLoop()
{
    for (int i = 0; i < mod; i++)
    {
        p_loop[i] = 0.f;
    }
}

void LoopChannel::ResetBuffer()
{
    firstPass = true;
    rec = false;
    recorded = false;
    play = false;
    len = 0;
    position = 0;
    for (int i = 0; i < mod; i++)
    {
        p_loop[i] = 0.f;
    }
    mod = size;
}

void LoopChannel::NextSample(float &playback, daisy::AudioHandle::InputBuffer in, size_t i)
{
    if (rec)
    {
        WriteBuffer(in, i);
        recorded = true;
    }

    playback = p_loop[position];

    // automatic looptime
    if (len >= size)
    {
        firstPass = false;
        mod = size;
        len = 0;
    }

    if (play) Shift_position();
}

void LoopChannel::NextSample_1(float &playback, daisy::AudioHandle::InputBuffer in, size_t i, LoopChannel *a)
{
    if (rec)
    {
        WriteBuffer(in, i);
        recorded = true;
        // if recording, playback = this->loop, which is set in WriteBuffer()
        playback = p_loop[position];
    }
    // get playback from A's loop based on this->position
    else
        playback = a->p_loop[this->position];

    if (len >= size)
    {
        firstPass = false;
        mod = size;
        len = 0;
    }

    if (play) Shift_position(a);
}

void LoopChannel::NextSample_2(float &playback, daisy::AudioHandle::InputBuffer in, size_t i, LoopChannel *a)
{
    if (rec)
    {
        WriteBuffer(in, i);
        recorded = true;
    }

    playback = p_loop[position];

    if (len >= size)
    {
        firstPass = false;
        mod = size;
        len = 0;
    }

    // if both chA and this have a recorded loop
    // retime this->loop based on chA
    if (a->recorded && recorded)
    {
        float retime = 0;
        retime = float(mod) / float(a->mod);
        playbackSpeed = retime * a->playbackSpeed;
    }

    if (play) Shift_position();
}

void LoopChannel::WriteBuffer(daisy::AudioHandle::InputBuffer in, size_t i)
{
    if (firstPass)
    {
        p_loop[position] = in[0][i];
        len++;
    }
    else p_loop[position] = (p_loop[position] * 0.5) + (in[0][i] * 0.5);
}

void LoopChannel::Shift_position()
{
    if (rec)
    {
        // Playback is unaffected by the Time & Rvrs when recording
        position++;
        position %= mod;
    }
    else
    {
        // playbackSpeed...
        // stretch
        if (playbackSpeed < 100)
        {
            indexTracker += playbackSpeed;
            if (indexTracker >= 100)
            {
                if (rvrs) position--;
                else position++;
                indexTracker -= 100;
            }
        }
        // normal playback
        else if (playbackSpeed == 100)
        {
            if (rvrs) position--;
            else position++;
        }
        // compress
        else if (playbackSpeed > 100)
        {
            float rate = float(playbackSpeed) / 100.f; // divide by 100 to get into the proper range (0.25-4.0)
            uint8_t rateTRUNC = rate;                  // truncate the float value
            rateRemainder += rate - rateTRUNC;
            pushVal = rateTRUNC;
            if (rateRemainder >= 1.f)
            {
                uint8_t remainTRUNC = rateRemainder;
                float remainMod = rateRemainder - remainTRUNC;
                rateRemainder = remainMod;
                pushVal += remainTRUNC;
            }
            if (rvrs) position -= pushVal;
            else position += pushVal;
        }
        if (rvrs && position < 0) position = mod;
        else position %= mod;
    }
}

void LoopChannel::Shift_position(LoopChannel *a)
{
    if (rec)
    {
        position++;
        position %= mod;
    }
    else
    {
        if (playbackSpeed < 100)
        {
            indexTracker += playbackSpeed;
            if (indexTracker >= 100)
            {
                if (rvrs) position--;
                else position++;
                indexTracker -= 100;
            }
        }
        else if (playbackSpeed == 100)
        {
            if (rvrs) position--;
            else position++;
        }
        else if (playbackSpeed > 100)
        {
            float rate = float(playbackSpeed) / 100.f;
            uint8_t rateTRUNC = rate;
            rateRemainder += rate - rateTRUNC;
            pushVal = rateTRUNC;
            if (rateRemainder >= 1.f)
            {
                uint8_t remainTRUNC = rateRemainder;
                float remainMod = rateRemainder - remainTRUNC;
                rateRemainder = remainMod;
                pushVal += remainTRUNC;
            }
            if (rvrs) position -= pushVal;
            else position += pushVal;
        }
        // !!!
        if (rvrs && position < 0)
        {
            position = a->mod;
        }
        else
        {
            position %= a->mod;
        }
    }
}
