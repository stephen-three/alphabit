// alphabit.cpp
// ver_00f
//
// Three Channel Looper
// 
// May 2022, Stephen Johnson

#include "daisy_seed.h"
#include "daisysp.h"

const std::string ver = "alphabit_00f";
const std::string notes = "Tested. Working as intended.";

#define MAX_SIZE (96000 * 30) // 30 sec of floats at 96 kHz
#define FREQ_MAX 2000 // 2kHz
#define FREQ_MIN 50 // 50Hz
#define FLTR_OFF 1006

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
    LoopChannel(float *loop, size_t sz) 
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
	{	}

 // Getters
    inline bool get_rec()
	{
    	return rec;
	}

    inline bool get_recdd()
	{ 
		return recorded; 
	}

    inline bool resetEnabled()
	{
		return (recorded && reset);
	}

	inline bool get_play()
	{
		return play;
	}

	inline int get_pos()
	{
		return position;
	}

	inline float get_lvl()
	{
		return lvl;
	}

    // Setters
	void start_REC();

	void stop_REC();

	void latch_REC();

	inline void toggle_play()
	{
		if (recorded) play = !play;
	}

	inline void set_play(bool b)
	{
		play = b;
	}

	inline void set_rvrs(bool setting)
	{
		rvrs = setting;
	}

	inline void set_lvl(float setting)
	{
		lvl = setting;
	}

	inline void set_speed(uint16_t setting)
	{
		playbackSpeed = setting;
	}

	// other functions
	void ClearLoop();

	void ResetBuffer();

	void NextSample(float &playback, daisy::AudioHandle::InputBuffer in, size_t i);

	void NextSample_1(
					  float &playback, 
					  daisy::AudioHandle::InputBuffer in, 
					  size_t i, 
					  LoopChannel* a
					 );
	
	void NextSample_2(
					  float &playback, 
					  daisy::AudioHandle::InputBuffer in, 
					  size_t i, 
					  LoopChannel* a
					 );

private:
	void WriteBuffer(daisy::AudioHandle::InputBuffer in, size_t i);

	void Shift_position();

	// called in NextSample_1()
	void Shift_position(LoopChannel* a);
};

class Footswitch
{
private:
	daisy::Switch fswitch;
	bool live;
	bool last;
	bool dcWait; // dc: Double Click
	bool dcWhenReleased;
	bool snglOK;
	long rleasTime;
	bool ignrRelease;
	bool waitForRelease;
	bool hold;
	const static uint8_t dcTimeOut = 200;

public:
	Footswitch(daisy::Pin pin_assignment)
	  :	live(false),
		last(false),
		dcWait(false),
		dcWhenReleased(false),
		snglOK(true),
		rleasTime(-1),
		ignrRelease(false),
		waitForRelease(false),
		hold(false)
	{
		using namespace daisy;
		fswitch.Init(pin_assignment, 0.f, Switch::TYPE_MOMENTARY, Switch::POLARITY_INVERTED, Switch::PULL_UP);
	}

	int Handle(uint16_t holdTime = 600);

	inline long get_releaseTime()
	{
		return rleasTime;
	}
};


daisy::DaisySeed hw;
daisysp::CrossFade cf;
daisysp::Tone fltrA;
daisysp::Tone fltrB;
daisysp::Tone fltrC;
// analog input pins
enum AdcChannel
{
	// ON-OFF-ON switch
	modeSw = 0,
	// potentiometers
	timeApot,
	timeBpot,
	timeCpot,
	lvlApot,
	lvlBpot,
	lvlCpot,
	mixPot,
	volPot,
	NUM_ADC_CHANNELS
};
daisy::AdcChannelConfig adc_config[NUM_ADC_CHANNELS];
// output pins, LEDs
daisy::GPIO byp_relay; // toggles a relay via an NPN transistor
daisy::GPIO bypR;
daisy::GPIO bypGB;
daisy::GPIO ledR;
daisy::GPIO ledG;
daisy::GPIO ledB;
// digital input pin, ON-ON switches
daisy::GPIO fwsBehavior;
daisy::GPIO rvrsA;
daisy::GPIO rvrsB;
daisy::GPIO rvrsC;
float DSY_SDRAM_BSS loopA[MAX_SIZE];
float DSY_SDRAM_BSS loopB[MAX_SIZE];
float DSY_SDRAM_BSS loopC[MAX_SIZE];
LoopChannel A(loopA, MAX_SIZE); // primary channel
LoopChannel B(loopB, MAX_SIZE); // secondary
LoopChannel C(loopC, MAX_SIZE); // secondary
/* global LoopChannels
	LoopChannels are global because
	they need to zero'd, ClearLoop(), before entering the callback
	to prevent a audio glitch for a few samples upon startup
	and a callback with extra parameters cannot be called by hw.StartAudio()
	The static variables in the callback are static for the second reason as well.
*/
bool fswHOLD = false;
/* fswHOLD
	Upon startup, determines the behavior
	of the loop channels' footswitches.
	if true:
		Hold down to record
		Press 1x to toggle playback
		Press 2x to clear loop
	if false:
		Press 1x to start(then again to stop) recording
		Press 2x to toggle playback
		Hold to clear loop
*/


long remap(
			const long &x, 
			const long &inMin, 
			const long &inMax, 
			const long &outMin, 
			const long &outMax
		  );

float MakePlayback(uint8_t playCondition, const float playbacks[3], LoopChannel* channels[3]);

void AudioCallback(
					daisy::AudioHandle::InputBuffer in,
					daisy::AudioHandle::OutputBuffer out, 
					size_t size
				  );



int main(void)
{
	hw.Init();
	hw.SetAudioBlockSize(4); // number of samples handled per callback
	hw.SetAudioSampleRate(daisy::SaiHandle::Config::SampleRate::SAI_96KHZ);
	float sample_rate = hw.AudioSampleRate();
	cf.Init();
	cf.SetCurve(daisysp::CROSSFADE_CPOW);
	fltrA.Init(sample_rate);
	fltrB.Init(sample_rate);
	fltrC.Init(sample_rate);
	// switch/potentiometer initializers
	{
		// analog pins
		using namespace daisy::seed;
		adc_config[modeSw].InitSingle(A0);
		adc_config[timeApot].InitSingle(A1);
		adc_config[timeBpot].InitSingle(A2);
		adc_config[timeCpot].InitSingle(A3);
		adc_config[lvlApot].InitSingle(A4);
		adc_config[lvlBpot].InitSingle(A5);
		adc_config[lvlCpot].InitSingle(A6);
		adc_config[mixPot].InitSingle(A7);
		adc_config[volPot].InitSingle(A8);
		{
			// digital pins
			using namespace daisy;
			byp_relay.Init(D0, GPIO::Mode::OUTPUT);
			bypR.Init(D8, GPIO::Mode::OUTPUT);
			bypGB.Init(D9, GPIO::Mode::OUTPUT);
			ledR.Init(D10, GPIO::Mode::OUTPUT);
			ledG.Init(D11, GPIO::Mode::OUTPUT);
			ledB.Init(D12, GPIO::Mode::OUTPUT);
			fwsBehavior.Init(D7, GPIO::Mode::INPUT, GPIO::Pull::PULLDOWN);
			rvrsA.Init(D24, GPIO::Mode::INPUT, GPIO::Pull::PULLDOWN);
			rvrsB.Init(D25, GPIO::Mode::INPUT, GPIO::Pull::PULLDOWN);
			rvrsC.Init(D28, GPIO::Mode::INPUT, GPIO::Pull::PULLDOWN);
		}
	}
	hw.adc.Init(adc_config, NUM_ADC_CHANNELS);
	hw.adc.Start();
	byp_relay.Write(true);

	bypR.Write(false);
	bypGB.Write(false);
	ledR.Write(false);
	ledG.Write(false);
	ledB.Write(false);
	// startup animation
	{
		using namespace daisy;
		ledR.Write(true);
		ledB.Write(true);
		uint32_t time = System::GetNow();
		while (System::GetNow() < time + 450){}
		ledR.Write(false);
		ledG.Write(true);
		time = System::GetNow();
		while (System::GetNow() < time + 450){}
		ledR.Write(true);
		ledG.Write(true);
		ledB.Write(false);
		time = System::GetNow();
		while (System::GetNow() < time + 450){}
		ledB.Write(true);
		time = System::GetNow();
		while (System::GetNow() < time + 600){}
		bypR.Write(true);
		bypGB.Write(true);
		time = System::GetNow();
		while (System::GetNow() < time + 800){}
		bypR.Write(false);
		bypGB.Write(false);
		ledR.Write(false);
		ledG.Write(false);
		ledB.Write(false);
		time = System::GetNow();
		while (System::GetNow() < time + 300){}
	}
	fswHOLD = fwsBehavior.Read();
	A.ClearLoop();
	B.ClearLoop();
	C.ClearLoop();

	hw.StartAudio(AudioCallback);
	while(1)
	{	}
}


// class function definitions
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

void LoopChannel::NextSample_1(float &playback, daisy::AudioHandle::InputBuffer in, size_t i, LoopChannel* a)
{
	if (rec)
	{
		WriteBuffer(in, i);
		recorded = true;
		// if recording, playback = this->loop, which is set in WriteBuffer()
		playback = p_loop[position];
	}
	// get playback from A's loop based on this->position
	else playback = a->p_loop[this->position];

	if (len >= size)
	{
		firstPass = false;
		mod = size;
		len = 0;
	}

	if (play) Shift_position(a);
}

void LoopChannel::NextSample_2(float &playback, daisy::AudioHandle::InputBuffer in, size_t i, LoopChannel* a)
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
			float rate = float(playbackSpeed)/100.f; // divide by 100 to get into the proper range (0.25-4.0)
			uint8_t rateTRUNC = rate; // truncate the float value
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

void LoopChannel::Shift_position(LoopChannel* a)
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
			float rate = float(playbackSpeed)/100.f;
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

int Footswitch::Handle(uint16_t holdTime /* =600 */)
{
	uint8_t input = 0;

	fswitch.Debounce();
	live = fswitch.Pressed();

	// pressed
	if (live && !last)
	{
		ignrRelease = false;
		waitForRelease = false;
		snglOK = true;
		hold = false;

		if ((daisy::System::GetNow() - rleasTime) < dcTimeOut && !dcWhenReleased && dcWait) dcWhenReleased = true;
		else dcWhenReleased = false;
		dcWait = false;
	}
	// released
	else if (!live && last)
	{
		if (!ignrRelease)
		{
			rleasTime = daisy::System::GetNow();
			if (!dcWhenReleased) dcWait = true;
			// double click
			else
			{
				input = 2;
				dcWhenReleased = false;
				dcWait = false;
				snglOK = false;
			}
		}
		// released after hold
		else if (hold)
		{
			input = 4;
			hold = false;
			rleasTime = daisy::System::GetNow();
		}
	}

	// single press
	if (!live && (daisy::System::GetNow() - rleasTime) >= dcTimeOut && dcWait && !dcWhenReleased && snglOK && input != 2)
	{
		input = 1;
		dcWait = false;
	}
	// hold
	if (live && fswitch.TimeHeldMs() > holdTime)
	{
		if (!hold)
		{
			input = 3;
			waitForRelease = true;
			ignrRelease = true;
			dcWhenReleased = false;
			dcWait = false;
			hold = true;
		}
	}

	last = live;
	return input;
}

// non-class functions
long remap(const long &x, const long &inMin, const long &inMax, const long &outMin, const long &outMax)
{
	return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

float MakePlayback(uint8_t playCondition, const float playbacks[3], LoopChannel* channels[3])
{
	/* playCondition
		if (a) playCondtion + 1
		if (b) playCondition + 2
		if (c) playCondition + 4
	*/
	enum ch {a = 0, b, c};
	float wet = 0.f;
	switch (playCondition)
	{
		float lvls[3];
		float makeup_gain[3];

		case 0:
			// loops recorded but no playbacks
			break;
		case 1:
			wet = playbacks[a] * channels[a]->get_lvl();
			break;
		case 2:
			wet = playbacks[b] * channels[b]->get_lvl();
			break;
		case 3:
			lvls[a] = channels[a]->get_lvl();
			lvls[b] = channels[b]->get_lvl();
			makeup_gain[a] = 1.f - lvls[b];
			makeup_gain[b] = 1.f - lvls[a];
			wet = (playbacks[a] * (lvls[a]/2 + makeup_gain[a])) + (playbacks[b] * (lvls[b]/2 + makeup_gain[b]));
			break;
		case 4:
			wet = playbacks[c] * channels[c]->get_lvl();
			break;
		case 5:
			lvls[a] = channels[a]->get_lvl();
			lvls[c] = channels[c]->get_lvl();
			makeup_gain[a] = 1.f - lvls[c];
			makeup_gain[c] = 1.f - lvls[a];
			wet = (playbacks[a] * (lvls[a]/2 + makeup_gain[a])) + (playbacks[c] * (lvls[c]/2 + makeup_gain[c]));
			break;
		case 6:
			lvls[b] = channels[b]->get_lvl();
			lvls[c] = channels[c]->get_lvl();
			makeup_gain[b] = 1.f - channels[c]->get_lvl();
			makeup_gain[c] = 1.f - channels[b]->get_lvl();
			wet = (playbacks[b] * (lvls[b]/2 + makeup_gain[b])) + (playbacks[c] * (lvls[c]/2 + makeup_gain[c]));
			break;
		case 7:
			lvls[a] = channels[a]->get_lvl();
			lvls[b] = channels[b]->get_lvl();
			lvls[c] = channels[c]->get_lvl();
			makeup_gain[a] = (2.f - lvls[b] - lvls[c]) / 2;
			makeup_gain[b] = (2.f - lvls[a] - lvls[c]) / 2;
			makeup_gain[c] = (2.f - lvls[a] - lvls[b]) / 2;
			wet = (playbacks[a] * (lvls[a]/3 + makeup_gain[a])) + (playbacks[b] * (lvls[b]/3 + makeup_gain[b])) + (playbacks[c] * (lvls[c]/3 + makeup_gain[c]));
			break;
	}

	return wet;
}

void AudioCallback(daisy::AudioHandle::InputBuffer in, daisy::AudioHandle::OutputBuffer out, size_t size)
{
	static bool setFltrA = false;
	static bool setFltrB = false;
	static bool setFltrC = false;
	static bool byp = true;
	static float mix = 1.f;
	static float vol = 1.f;
	static uint8_t mode = 3;
	/* Modes
		3: Three independent loops
		2: Three loops, with the two secondary loops stretched/compressed to match the duration of the primary
		1: One loop, played on three independently controlled channels
	*/
	// Controls
	{
		static Footswitch fsw(daisy::seed::D26);
		static Footswitch fswA(daisy::seed::D27);
		static Footswitch fswB(daisy::seed::D29);
		static Footswitch fswC(daisy::seed::D30);

		uint16_t md = hw.adc.Get(modeSw) / 64; // value: 0-65536/64 = 0-1024
		if (md < 124) mode = 1;
		else if (md < 900) mode = 3;
		else mode = 2;

		mix = hw.adc.GetFloat(mixPot); // value: 0-1.0
		vol = hw.adc.GetFloat(volPot);

		bool direction;
		direction = rvrsA.Read();
		A.set_rvrs(direction);
		direction = rvrsB.Read();
		B.set_rvrs(direction);
		direction = rvrsC.Read();
		C.set_rvrs(direction);

		uint16_t time = hw.adc.Get(timeApot) / 64;
		uint16_t speed = 0;
		if (time > 476 && time < 548) speed = 100;
		else if (time <= 476) speed = remap(time, 0, 476, 400, 100);
		else if (time >= 548) speed = remap(time, 548, 1024, 100, 25);
		A.set_speed(speed);

		time = hw.adc.Get(timeBpot) / 64;
		if (time > 476 && time < 548) speed = 100;
		else if (time <= 476) speed = remap(time, 0, 476, 400, 100);
		else if (time >= 548) speed = remap(time, 548, 1024, 100, 25);
		B.set_speed(speed);

		time = hw.adc.Get(timeCpot) / 64;
		if (time > 476 && time < 548) speed = 100;
		else if (time <= 476) speed = remap(time, 0, 476, 400, 100);
		else if (time >= 548) speed = remap(time, 548, 1024, 100, 25);
		C.set_speed(speed);

		// footswitches
		uint8_t fswCommand;
		fswCommand = fsw.Handle();
		static bool clearing = false;
		static bool ghostKnobs = false;
		static bool gk_takeBench = true;
		static uint16_t freqA_benchMark = 0;
		static uint16_t freqB_benchMark = 0;
		static uint16_t freqC_benchMark = 0;
		switch(fswCommand)
		{
			case 0:
				break;
			case 1:
				// bypass
				byp = !byp;
				byp_relay.Write(byp);
				break;
			case 2:
				if (fswHOLD)
				{
					if(A.resetEnabled())
					{
						A.ResetBuffer();
						clearing = true;
						setFltrA = false;
						if (mode == 1)
						{
							if (!B.get_recdd()) B.set_play(false);
							if (!C.get_recdd()) C.set_play(false);
						}
					}

					if(B.resetEnabled())
					{
						B.ResetBuffer();
						clearing = true;
						setFltrB = false;
					}

					if(C.resetEnabled())
					{
						C.ResetBuffer();
						clearing = true;
						setFltrC = false;
					}
				}
				else
				{
					A.toggle_play();
					if (mode == 1)
					{
						B.set_play(!B.get_play());
						C.set_play(!C.get_play());
					}
					else
					{
						B.toggle_play();
						C.toggle_play();
					}
				}
				break;
			case 3:
				// ghost knobs
				ghostKnobs = true;
				if (gk_takeBench)
				{
					freqA_benchMark = hw.adc.Get(lvlApot) / 64;
					freqB_benchMark = hw.adc.Get(lvlBpot) / 64;
					freqC_benchMark = hw.adc.Get(lvlCpot) / 64;
					gk_takeBench = false;
				}
				break;
			case 4:
				ghostKnobs = false;
				gk_takeBench = true;
				break;
		}

		if (fswHOLD)
		{
			static bool dlayRECstop[3] = {false, false, false};
			enum {a = 0, b, c};
			const uint8_t rec_dlay = 90;

			fswCommand = fswA.Handle(rec_dlay);
			switch (fswCommand)
			{
				case 0:
					if (dlayRECstop[a] && daisy::System::GetNow() - fswA.get_releaseTime() > rec_dlay)
					{
						A.stop_REC();
						dlayRECstop[a] = false;
					}
					/* 
						Delaying the stop_REC() by the same amount as
						the start_REC() gives a much more consistent
						and generally better feeling to using hold-to-REC
					*/
					break;
				case 1:
					A.toggle_play();
					break;
				case 2:
					if (A.resetEnabled())
					{
						A.ResetBuffer();
						clearing = true;
						if (mode == 1)
						{
							if (!B.get_recdd()) B.set_play(false);
							if (!C.get_recdd()) C.set_play(false);
						}
						setFltrA = false;
					}
					break;
				case 3:
					if (!A.get_rec())
					{
						A.start_REC();
					}
					break;
				case 4:
					if (A.get_rec())
					{
						dlayRECstop[a] = true;
					}
					break;
			}
		
			fswCommand = fswB.Handle(rec_dlay);
			switch (fswCommand)
			{
				case 0:
					if (dlayRECstop[b] && daisy::System::GetNow() - fswB.get_releaseTime() > rec_dlay)
					{
						B.stop_REC();
						dlayRECstop[b] = false;
					}
					break;
				case 1:
					if (mode == 1)
					{
						B.set_play(!B.get_play());
						// toggle_play() only toggles if recorded==true
						// which is not necessary in Mode 1
					}
					else
					{
						B.toggle_play();
					}
					break;
				case 2:
					if (B.resetEnabled())
					{
						B.ResetBuffer();
						clearing = true;
						setFltrB = false;
					}
					break;
				case 3:
					if (!B.get_rec())
					{
						B.start_REC();
					}
					break;
				case 4:
					if (B.get_rec())
					{
						dlayRECstop[b] = true;
					}
					break;
			}
		
			fswCommand = fswC.Handle(rec_dlay);
			switch (fswCommand)
			{
				case 0:
					if (dlayRECstop[c] && daisy::System::GetNow() - fswC.get_releaseTime() > rec_dlay)
					{
						C.stop_REC();
						dlayRECstop[c] = false;
					}
					break;
				case 1:
					if (mode == 1)
					{
						C.set_play(!C.get_play());
					}
					else 
					{
						C.toggle_play();
					}
					break;
				case 2:
					if (C.resetEnabled())
					{
						C.ResetBuffer();
						clearing = true;
						setFltrC = false;
					}
					break;
				case 3:
					if (!C.get_rec())
					{
						C.start_REC();
					}
					break;
				case 4:
					if (C.get_rec())
					{
						dlayRECstop[c] = true;
					}
					break;
			}
		
		}
		else 
		{
			fswCommand = fswA.Handle();
			switch(fswCommand)
			{
				case 0:
					break;
				case 1:
					// toggle REC
					if (A.get_rec())
					{
						A.stop_REC();
					}
					else
					{
						A.start_REC();
					}
					break;
				case 2:
					A.toggle_play();
					break;
				case 3:
					// clear loop
					if (A.resetEnabled())
					{
						A.ResetBuffer();
						clearing = true;
						if (mode == 1)
						{
							if (!B.get_recdd()) B.set_play(false);
							if (!C.get_recdd()) C.set_play(false);
						}
						setFltrA = false;
					}
					break;
			}
		
			fswCommand = fswB.Handle();
			switch(fswCommand)
			{
				case 0:
					break;
				case 1:
					// toggle REC
					if (B.get_rec())
					{
						B.stop_REC();
					}
					else
					{
						B.start_REC();
					}
					break;
				case 2:
					if (mode == 1)
					{
						B.set_play(!B.get_play());
					}
					else
					{
						B.toggle_play();
					}
					break;
				case 3:
					// clear loop
					if (B.resetEnabled())
					{
						B.ResetBuffer();
						clearing = true;
						setFltrB = false;
					}
					break;
			}
		
			fswCommand = fswC.Handle();
			switch(fswCommand)
			{
				case 0:
					break;
				case 1:
					// toggle REC
					if (C.get_rec())
					{
						C.stop_REC();
					}
					else
					{
						C.start_REC();
					}
					break;
				case 2:
					if (mode == 1)
					{
						C.set_play(!C.get_play());
					}
					else
					{
						C.toggle_play();
					}
					break;
				case 3:
					// clear loop
					if (C.resetEnabled())
					{
						C.ResetBuffer();
						clearing = true;
						setFltrC = false;
					}
					break;
			}
		}

		if (ghostKnobs)
		{
			uint16_t freqA = hw.adc.Get(lvlApot) / 64;
			uint16_t freqB = hw.adc.Get(lvlBpot) / 64;
			uint16_t freqC = hw.adc.Get(lvlCpot) / 64;

			float freq;
			// if (freqA != freqA_benchMark) with a small window for misreadings
			if (freqA > freqA_benchMark + 2 || freqA < freqA_benchMark - 2)
			{
				if (freqA > FLTR_OFF) 
				{
					setFltrA = false;
				}
				else 
				{
					setFltrA = true;
					// map freqA into frequency range values
					freq = remap(freqA, FLTR_OFF, 0, FREQ_MAX, FREQ_MIN); 
					fltrA.SetFreq(freq); 
				}
				freqA_benchMark = freqA;
			}

			if (freqB > freqB_benchMark + 2 || freqB < freqB_benchMark - 2)
			{
				if (freqB > FLTR_OFF) 
				{
					setFltrB = false;
				}
				else 
				{
					setFltrB = true;
					freq = remap(freqB, FLTR_OFF, 0, FREQ_MAX, FREQ_MIN); 
					fltrB.SetFreq(freq); 
				}
				freqB_benchMark = freqB;
			}

			if (freqC > freqC_benchMark + 2 || freqC < freqC_benchMark - 2)
			{
				if (freqC > FLTR_OFF) 
				{
					setFltrC = false;
				}
				else 
				{
					setFltrC = true;
					freq = remap(freqC, FLTR_OFF, 0, FREQ_MAX, FREQ_MIN); 
					fltrC.SetFreq(freq); 
				}
				freqC_benchMark = freqC;
			}
		}
		else
		{
			A.set_lvl(hw.adc.GetFloat(lvlApot));
			B.set_lvl(hw.adc.GetFloat(lvlBpot));
			C.set_lvl(hw.adc.GetFloat(lvlCpot));
		}

		// LEDs
		uint16_t blink = 4000;
		bool lightA;
		bool lightB;
		bool lightC;
		if (mode == 1)
		{
			lightA = A.get_play() && A.get_pos() < blink;
			lightB = B.get_play() && B.get_pos() < blink;
			lightC = C.get_play() && C.get_pos() < blink;
		}
		else
		{
			lightA = A.get_recdd() && A.get_play() && A.get_pos() < blink;
			lightB = B.get_recdd() && B.get_play() && B.get_pos() < blink;
			lightC = C.get_recdd() && C.get_play() && C.get_pos() < blink;
		}
		bool recNow = (A.get_rec() || B.get_rec() || C.get_rec());

		if (recNow)
		{
			ledR.Write(true);
			ledG.Write(false);
			ledB.Write(false);
		}
		else
		{
			// chA represented by Magenta (red & blu)
			// chB: Cyan (grn & blu)
			// chC: Yellow (red & grn)
			ledR.Write(lightA || lightC);
			ledG.Write(lightB || lightC);
			ledB.Write(lightB || lightA);
		}
		
		if (clearing)
		{
			// "Clearing Loop" lighting cue
			static uint8_t cueCount = 0;
			static uint32_t prev = 0;
			ledR.Write(false);
			ledB.Write(false);
			bypR.Write(false);
			bypGB.Write(false);
			if (cueCount % 2 != 0) ledG.Write(true);
			else if (cueCount < 5) ledG.Write(false);
			else
			{
				ledG.Write(false);
				cueCount = 0;
				clearing = false;
			}

			if ((daisy::System::GetNow() - prev) > 150)
			{
				prev = daisy::System::GetNow();
				cueCount++;
			}
		}
		else
		{
			if (byp)
			{
				bypR.Write(recNow && !ghostKnobs);
				bypGB.Write(ghostKnobs);
			}
			else
			{
				bypR.Write(!ghostKnobs);
				bypGB.Write(!recNow);
			}
		}
	}

	float playbackA = 0.f;
	float playbackB = 0.f;
	float playbackC = 0.f;
	for (size_t i = 0; i < size; i++)
	{
		A.NextSample(playbackA, in, i);
		switch (mode)
		{
			case 1:
				B.NextSample_1(playbackB, in, i, &A);
				C.NextSample_1(playbackC, in, i, &A);
				break;
			case 2:
				B.NextSample_2(playbackB, in, i, &A);
				C.NextSample_2(playbackC, in, i, &A);
				break;
			default:
				B.NextSample(playbackB, in, i);
				C.NextSample(playbackC, in, i);
				break;
		}

		if (setFltrA) playbackA = fltrA.Process(playbackA);
		if (setFltrB) playbackB = fltrB.Process(playbackB);
		if (setFltrC) playbackC = fltrC.Process(playbackC);

		float dry = in[0][i];
		float wet = 0.f;
		if (byp)
		{
			out[0][i] = dry;
		}
		else
		{
			bool recordedA = A.get_recdd();
			bool recordedB = B.get_recdd();
			bool recordedC = C.get_recdd();
			bool playB = B.get_play();
			bool playC = C.get_play();

			float playbacks[3] = {playbackA, playbackB, playbackC};
			LoopChannel *channels[3] = {&A, &B, &C};

			uint8_t playCondition = 0;
			if (recordedA && A.get_play()) playCondition += 1;
			if (mode == 1)
			{
				bool rec_ingB = B.get_rec();
				bool rec_ingC = C.get_rec();
				if ((recordedA && playB) || rec_ingB) playCondition += 2;
				if ((recordedA && playC) || rec_ingC) playCondition += 4;

				if (recordedA || rec_ingB || rec_ingC)
				{
					wet = MakePlayback(playCondition, playbacks, channels);
				}
				else wet = dry;
			}
			else 
			{
				if (recordedB && playB) playCondition += 2;
				if (recordedC && playC) playCondition += 4;

				if (recordedA || recordedB || recordedC)
				{
					wet = MakePlayback(playCondition, playbacks, channels);
				}
				else wet = dry;
			}

			cf.SetPos(mix);
			float output = cf.Process(dry, wet);
			out[0][i] = output * (vol * 2);
		}
	}
}
