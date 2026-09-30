#pragma once

constexpr char kSymmetryR = 'R';
constexpr char kSymmetryS = 'S';
constexpr char kSymmetryA = 'A';
constexpr long kStateOff = 0;
constexpr long kStateOn = 1;

struct ConfigurationField {
	const char* name;
	long value;
	long minValue;
	long maxValue;
	long previousValue;
	const char* unit;
};

struct DerivedTiming {
	long pulsePhaseMicroseconds;
	long frequencyPeriodMicroseconds;
	long asymmetricHalfPeriodMicroseconds;
	long groupDelayMicroseconds;
	long groupDurationMicroseconds;
	long groupPeriodMicroseconds;
};

struct TimingConfiguration {
	ConfigurationField state;
	ConfigurationField frequencyHz;
	ConfigurationField carrierFrequencyMicroseconds;
	ConfigurationField pulsesPerCycle;
	ConfigurationField interPeakDelayMicroseconds;
	ConfigurationField symmetry;
	ConfigurationField groupDelay10Microseconds; // Pasar a centesimas de milisegundo para que sea más fácil de manejar en el código y en la pantalla del LCD
	mutable DerivedTiming derived;
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
