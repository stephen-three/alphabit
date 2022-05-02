// alphabit.cpp
// ver_00a
//
// Three Channel Looper
// 
// May 2022, Stephen Johnson

#include "daisy_seed.h"
#include "daisysp.h"
#include <string>

using namespace daisy;
using namespace daisy::seed;
using namespace daisysp;

const std::string ver = "alphabit_00a";
const std::string notes = "Tested. Working as intended."
						  "New this Ver:"
						  "---	Fixed fsw Handle() results."
						  "---	Fully fixed ghost knob functionalities."
						  "---	Refactored based on the fsw Handle() toggling loop.play"
						  "		instead of controlling it with the ON-OFF-ON swithes."
						  "---	Eliminated need for loop.GetReference() and deleted it."
						  "Next:		"
						  "---	Limit basic type variables to only necessary ranges/sizes."
						  "Version 15:	"
						  "---	Fully fix ghost knob functionality"
						  "Issues:		"
						  "---	Audible squeak when clearing an 'empty' buffer"
						  "		(in fswHOLD=false mode)"
						  "		(The buffer should be empty but is recording bc"
						  "		the footswitch - > Handle() method returns 1 at"
						  "		the start of a hold.)";

/*	TODO:
	FIXME in LoopChannel::WriteBuffer()
	Improve overdub volume control-
		The current math pushes the overdubs toward silence too
		quickly. By the third pass on a loop, the volume of incoming
		signal is nearly inaudible. This signal should be retained.
		Is there a compromise with this method that would still allow
		for the bonus of a delay when the loop duration is short and
		the fsw is held down, continually overdubbing.
	FIXME in AudioCallback() { if (byp) else ... }
	Improve level logic-
		The current math does not account for if one of the
		channels is not full volume and changes to the playCondition
		are instant, producing (possibly undesirable) jumps in volume.
	The slight difference between starting recording and releasing the
		to end recording could possibly be corrected with a delayed call
		to stop_rec() (delayed by the holdTime amount)
		Maybe also reduce holdTime while still consistently getting single
		presses when desired.
*/

#define MAX_SIZE (96000 * 30) // 30 sec of floats at 96 kHz
// 2,880,000
#define FREQ_MAX 2000 // 2kHz
#define FREQ_MIN 50 // 50Hz
#define FLTR_OFF 1006

class LoopChannel
{
private:
    bool first;
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
    uint8_t pass;

public:
    LoopChannel(float *loop, size_t sz) 
	  : first(true),
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
		pushVal(0),
		pass(0)
	{}

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
};

class Footswitch
{
private:
	Switch fsw;
	bool live;
	bool last;
	bool dcWait; // dc: Double Click
	bool dcWhenReleased;
	bool snglOK;
	long rleasTime;
	bool ignrRelease;
	bool waitRelease;
	bool hold;
	const static uint16_t waitPrd = 1000;
	const static uint8_t dcTimeOut = 250;

public:
	Footswitch()
			:	live(false),
				last(false),
				dcWait(false),
				dcWhenReleased(false),
				snglOK(true),
				rleasTime(-1),
				ignrRelease(false),
				waitRelease(false),
				hold(false)
	{
		// daisy::Switch would need to be initialized externally
		// but with full control of parameters
		
		// except it can't be initialized externally
		// because the fsw member is private
	}

	Footswitch(daisy::Pin pin)
			:	live(false),
				last(false),
				dcWait(false),
				dcWhenReleased(false),
				snglOK(true),
				rleasTime(-1),
				ignrRelease(false),
				waitRelease(false),
				hold(false)
	{
		fsw.Init(pin, 0.f, Switch::TYPE_MOMENTARY, Switch::POLARITY_INVERTED, Switch::PULL_UP);
	}

	int Handle(uint16_t holdTime = 600);
};


float DSY_SDRAM_BSS loopA[MAX_SIZE];
float DSY_SDRAM_BSS loopB[MAX_SIZE];
float DSY_SDRAM_BSS loopC[MAX_SIZE];

bool byp = true;
uint8_t mode = 3;
/* Modes
	3: Three independent loops
	2: Three loops, with the two secondary loops stretched/compressed to match the duration of the primary
	1: One loop, played on three independently controlled channels
*/
float mix = 1.f;
float vol = 1.f;
bool fswHOLD = false; // if fsw is single-pressed or held to REC

bool setFltrA = false;
bool setFltrB = false;
bool setFltrC = false;

DaisySeed hw;
CrossFade cf;
Tone fltrA;
Tone fltrB;
Tone fltrC;

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
AdcChannelConfig adc_config[NUM_ADC_CHANNELS];
// output pins, LEDs
GPIO bypR;
GPIO bypGB;
GPIO ledR;
GPIO ledG;
GPIO ledB;
// digital input pin, ON-ON switches
GPIO fwsBehavior;
GPIO rvrsA;
GPIO rvrsB;
GPIO rvrsC;

LoopChannel A(loopA, MAX_SIZE); // primary channel
LoopChannel B(loopB, MAX_SIZE); // secondary
LoopChannel C(loopC, MAX_SIZE); // secondary

long remap(
			const long &x, 
			const long &inMin, 
			const long &inMax, 
			const long &outMin, 
			const long &outMax
		  );

void Controls();

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size);



int main(void)
{
	hw.Init();
	hw.SetAudioBlockSize(4); // number of samples handled per callback
	// change to 48, 96 ^^^ ? !final form
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_96KHZ);
	float sample_rate = hw.AudioSampleRate();
	cf.Init();
	cf.SetCurve(CROSSFADE_CPOW);
	fltrA.Init(sample_rate);
	fltrB.Init(sample_rate);
	fltrC.Init(sample_rate);

	adc_config[modeSw].InitSingle(A0);
	adc_config[timeApot].InitSingle(A1);
	adc_config[timeBpot].InitSingle(A2);
	adc_config[timeCpot].InitSingle(A3);
	adc_config[lvlApot].InitSingle(A4);
	adc_config[lvlBpot].InitSingle(A5);
	adc_config[lvlCpot].InitSingle(A6);
	adc_config[mixPot].InitSingle(A7);
	adc_config[volPot].InitSingle(A8);
	bypR.Init(D8, GPIO::Mode::OUTPUT);
	bypGB.Init(D9, GPIO::Mode::OUTPUT);
	ledR.Init(D10, GPIO::Mode::OUTPUT);
	ledG.Init(D11, GPIO::Mode::OUTPUT);
	ledB.Init(D12, GPIO::Mode::OUTPUT);
	fwsBehavior.Init(D7, GPIO::Mode::INPUT, GPIO::Pull::PULLDOWN);
	rvrsA.Init(D24, GPIO::Mode::INPUT, GPIO::Pull::PULLDOWN);
	rvrsB.Init(D25, GPIO::Mode::INPUT, GPIO::Pull::PULLDOWN);
	rvrsC.Init(D28, GPIO::Mode::INPUT, GPIO::Pull::PULLDOWN);
	hw.adc.Init(adc_config, NUM_ADC_CHANNELS);
	hw.adc.Start();
	// startup animation
	bypR.Write(false);
	bypGB.Write(false);
	ledR.Write(false);
	ledG.Write(false);
	ledB.Write(false);
	{
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
	pass++;
}

void LoopChannel::stop_REC()
{
	if (first && rec)
	// first && recorded?
	{
		first = false;
		mod = len;
		len = 0;
	}
	rec = false;
}

void LoopChannel::latch_REC()
{
	if (first && rec)
	{
		first = false;
		mod = len;
		len = 0;
	}
	reset = true;
	play = true;
	rec = !rec;
}

void LoopChannel::ClearLoop()
{
	for (int i = 0; i < mod; i++) {
		p_loop[i] = 0.f;

	}
}

void LoopChannel::ResetBuffer()
{
	first = true;
	pass = 0;
	rec = false;
	recorded = false;
	play = false;
	len = 0;
	position = 0;
	for (int i = 0; i < mod; i++) {
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
		first = false;
		pass++;
		mod = size;
		len = 0;
	}

	if (play) 
	{
		if (rec)
		{
			// Playback is unaffected by the Time & Rvrs when recording
			position++;
			if (pass == 1) position %= len;
			else position %= mod; 
		}
		else 
		{
			// playbackSpeed...
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
				// Ternary operator
			}
			// compress…
			else if (playbackSpeed > 100)
			{
				float rate = float(playbackSpeed)/100.f; // divide by 100 to get into the proper range (0.25-4.0)
				// truncate the float value
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
			if (rvrs && (position <= 0 || position >= mod)) position = mod;
			else position %= mod;
		}
	}
}

void LoopChannel::NextSample_1(float &playback, daisy::AudioHandle::InputBuffer in, size_t i, LoopChannel* a)
{
	if (rec)
	{
		WriteBuffer(in, i);
		recorded = true;
		playback = p_loop[position];
	}
	else playback = a->p_loop[this->position];

	if (len >= size)
	{
		first = false;
		pass++;
		mod = size;
		len = 0;
	}

	if (play)
	{
		if (rec)
		{
			position++;
			if (pass == 1) position %= a->len;
			else position %= a->mod;
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

			if (rvrs && (position <= 0 || position >= mod))
			{
				position = a->mod;
			}
			else
			{
				position %= a->mod;
			}
		}
	}
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
		first = false;
		pass++;
		mod = size;
		len = 0;
	}

	if (a->recorded && recorded)
	{
		float retime = 0;
		retime = float(mod) / float(a->mod);
		playbackSpeed = retime * a->playbackSpeed;
	}

	if (play)
	{
		if (rec)
		{
			position++;
			if (pass == 1) position %= len;
			else position %= mod;
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

			if (rvrs && (position <= 0 || position >= mod))
			{
				position = mod;
			}
			else position %= mod;
		}
	}
}

void LoopChannel::WriteBuffer(daisy::AudioHandle::InputBuffer in, size_t i)
{

	float crntFctr = 1.f / pass;
	float prevFctr = 1.f / (pass - 1);
	float factor = crntFctr * prevFctr;

	if (first)
	{
		p_loop[position] = in[0][i];
		len++;
	}
	// else p_loop[position] = (p_loop[position] * 0.5) + (in[0][i] * 0.5);
	else p_loop[position] = (p_loop[position] * factor) + (in[0][i] * factor);
	/* FIXME:
		This method of overdub volume control gradually pushes towards silence.
		The volume is noticeably too quiet after just a couple passes.
		This may not be a viable alternative and needs to be corrected.
		On the other hand, it does create an interesting delay effect.
		Is there a possible comprosmise
	*/
}

int Footswitch::Handle(uint16_t holdTime /* =600 */)
{
	uint8_t input = 0;

	fsw.Debounce();
	live = fsw.Pressed();

	// pressed
	if (live && !last)
	{
		ignrRelease = false;
		waitRelease = false;
		snglOK = true;
		hold = false;

		if ((System::GetNow() - rleasTime) < dcTimeOut && !dcWhenReleased && dcWait) dcWhenReleased = true;
		else dcWhenReleased = false;
		dcWait = false;
	}
	// released
	else if (!live && last)
	{
		if (!ignrRelease)
		{
			rleasTime = System::GetNow();
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
		}
	}

	// single press
	if (!live && (System::GetNow() - rleasTime) >= dcTimeOut && dcWait && !dcWhenReleased && snglOK && input != 2)
	{
		input = 1;
		dcWait = false;
	}
	// hold
	if (live && fsw.TimeHeldMs() > holdTime)
	{
		if (!hold)
		{
			input = 3;
			waitRelease = true;
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

void Controls()
{
	static Footswitch fsw(D26);
	static Footswitch fswA(D27);
	static Footswitch fswB(D29);
	static Footswitch fswC(D30);

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
	fswCommand = fsw.Handle(1000);
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
			/* !final form
			  Relay Bypass controlled by bypass() func
			  that both toggles bool byp and flips the 
			  NPN transistor connected to the relay.
			*/
			break;
		case 2:
			if(A.resetEnabled())
			{
				A.ResetBuffer();
				clearing = true;
				setFltrA = false;
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
		fswCommand = fswA.Handle(70);
		switch (fswCommand)
		{
			case 0:
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
						B.set_play(false);
						C.set_play(false);
					}
					setFltrA = false;
				}
				break;
			case 3:
				A.start_REC();
				break;
			case 4:
				if (A.get_rec())
				{
					A.stop_REC();
				}
				break;
		}
	
		fswCommand = fswB.Handle(70);
		switch (fswCommand)
		{
			case 0:
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
				B.start_REC();
				break;
			case 4:
				if (B.get_rec())
				{
					B.stop_REC();
				}
				break;
		}
	
		fswCommand = fswC.Handle(70);
		switch (fswCommand)
		{
			case 0:
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
				C.start_REC();
				break;
			case 4:
				if (C.get_rec())
				{
					C.stop_REC();
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
						B.set_play(false);
						C.set_play(false);
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

		if ((System::GetNow() - prev) > 150)
		{
			prev = System::GetNow();
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

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	float playbackA = 0.f;
	float playbackB = 0.f;
	float playbackC = 0.f;
	Controls();

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
			/* !final form
				This would be unnecessary? with the Relay bypass
			*/
		}
		else
		{
			bool recordedA = A.get_recdd();
			bool recordedB = B.get_recdd();
			bool recordedC = C.get_recdd();
			bool playA = A.get_play();
			bool playB = B.get_play();
			bool playC = C.get_play();

			uint8_t playCondition = 0;
			if (recordedA && playA) playCondition += 1;
			if (mode == 1)
			{
				bool rec_ingB = B.get_rec();
				bool rec_ingC = C.get_rec();
				if ((recordedA && playB) || rec_ingB) playCondition += 2;
				if ((recordedA && playC) || rec_ingC) playCondition += 4;

				if (recordedA || rec_ingB || rec_ingC)
				{
					switch (playCondition)
					{
						float levelA;
						float levelB;
						float levelC;
						case 0:
							// loops recorded but no playbacks
							break;
						case 1:
							levelA = A.get_lvl();
							wet = playbackA * levelA;
							break;
						case 2:
							levelB = B.get_lvl();
							wet = playbackB * levelB;
							break;
						case 3:
							levelA = A.get_lvl();
							levelB = B.get_lvl();
							wet = (playbackA * (levelA/2)) + (playbackB * (levelB/2));
							break;
						case 4:
							levelC = C.get_lvl();
							wet = playbackC * levelC;
							break;
						case 5:
							levelA = A.get_lvl();
							levelC = C.get_lvl();
							wet = (playbackA * (levelA/2)) + (playbackC * (levelC/2));
							break;
						case 6:
							levelB = B.get_lvl();
							levelC = C.get_lvl();
							wet = (playbackB * (levelB/2)) + (playbackC * (levelC/2));
							break;
						case 7:
							levelA = A.get_lvl();
							levelB = B.get_lvl();
							levelC = C.get_lvl();
							wet = (playbackA * (levelA/3)) + (playbackB * (levelB/3)) + (playbackC *  (levelC/3));
							break;
					}
				}
				else wet = dry;
			}
			else 
			{
				if (recordedB && playB) playCondition += 2;
				if (recordedC && playC) playCondition += 4;

				if (recordedA || recordedB || recordedC)
				{
					switch (playCondition)
					{
						float levelA;
						float levelB;
						float levelC;
						case 0:
							// loops recorded but no playbacks
							break;
						case 1:
							levelA = A.get_lvl();
							wet = playbackA * levelA;
							break;
						case 2:
							levelB = B.get_lvl();
							wet = playbackB * levelB;
							break;
						case 3:
							levelA = A.get_lvl();
							levelB = B.get_lvl();
							wet = (playbackA * (levelA/2)) + (playbackB * (levelB/2));
							break;
						case 4:
							levelC = C.get_lvl();
							wet = playbackC * levelC;
							break;
						case 5:
							levelA = A.get_lvl();
							levelC = C.get_lvl();
							wet = (playbackA * (levelA/2)) + (playbackC * (levelC/2));
							break;
						case 6:
							levelB = B.get_lvl();
							levelC = C.get_lvl();
							wet = (playbackB * (levelB/2)) + (playbackC * (levelC/2));
							break;
						case 7:
							levelA = A.get_lvl();
							levelB = B.get_lvl();
							levelC = C.get_lvl();
							wet = (playbackA * (levelA/3)) + (playbackB * (levelB/3)) + (playbackC *  (levelC/3));
							break;
					}
				}
				else wet = dry;
			}
			/* FIXME:
				Level logic in switch (playCondition) statements is suboptimal
				and should still be refined.
				The main issue that when a loop has been recorded but its volume
				is turned down, the other loops should be able to "fill the void".
				As it stands now, each channel occupies a equal fraction to the
				number of recorded channels. If 2 channels have been recorded, the
				channels can, at most, occupy 1/2 of the total voluem. If 3 channels 
				have been recorded, 1/3.
			*/

			cf.SetPos(mix);
			float output = cf.Process(dry, wet);
			out[0][i] = output * (vol * 2);
		}
	}
}
