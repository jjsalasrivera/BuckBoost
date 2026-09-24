#pragma once

constexpr char kSymmetryR = 'R';
constexpr char kSymmetryS = 'S';
constexpr char kSymmetryA = 'A';
constexpr int kStateOff = 0;
constexpr int kStateOn = 1;

struct ConfigurationField {
	const char* name;
	int value;
	int minValue;
	int maxValue;
	int previousValue;
	const char* unit;
};

struct TimingConfiguration {
	ConfigurationField state;
	ConfigurationField frequencyHz;
	ConfigurationField carrierFrequencyMicroseconds;
	ConfigurationField pulsesPerCycle;
	ConfigurationField interPeakDelayMicroseconds;
	ConfigurationField symmetry;
	ConfigurationField groupDelayMilliseconds;
};

struct PulseOutputPin {
	volatile unsigned char* const directionRegister;
	volatile unsigned char* const outputRegister;
	const unsigned char bitMask;
};

struct PulseOutputGroup {
	const PulseOutputPin positive;
	const PulseOutputPin negative;
};
