/***********************************************
 * All standard oscillator waveforms externals *
 * written by Alexandros Drymonitis            *
 ***********************************************/

// Header files required by Pure Data
#include "m_pd.h"
#include "math.h"

// Constant definitions
#define ALLOSC_STEPSIZE 8192

// The class pointer
static t_class *allOsc_class;

// The object structure
typedef struct _allOsc {
	// The Pd object
        t_object obj;
	// Convert floats to signals
        t_float x_f;
	// Rest of variables
	float x_frequency;
	// float x_power; // use this variable only after the argument problem is solved
        float x_phase;
        float x_si; // sample increment
        float x_sifactor; // factor for generating sampling increment
        float x_twopi;
        float x_sr; // sampling rate
} t_allOsc;

// Function prototypes
void *allOsc_new(void);
void allOsc_dsp(t_allOsc *x, t_signal **sp);
void allOsc_ft1(t_allOsc *x, t_float f);
t_int *allOsc_perform(t_int *w);

// The Pd class definition function
void allOsc_tilde_setup(void)
{
	// Initialize the class
	allOsc_class = class_new(gensym("allOsc~"), (t_newmethod)allOsc_new, 0, sizeof(t_allOsc), 0, A_GIMME, 0);

	// Specify signal input, with automatic float to signal conversion
	CLASS_MAINSIGNALIN(allOsc_class, t_allOsc, x_f);

	// Bind the DSP method, which is called when the DACs are turned on
	class_addmethod(allOsc_class, (t_method)allOsc_dsp, gensym("dsp"), A_CANT, 0);

	// Bind the method to receive a float in the last inlet (control) to reset the phase
        class_addmethod(allOsc_class, (t_method)allOsc_ft1, gensym("ft1"), A_FLOAT, 0);

	// Print authorship to Pd window
	post("allOsc~: All four standard waveforms oscillator\n external by Alexandros Drymonitis");
}

// The new instance routine
void *allOsc_new(void)
{
	int i; // variable for outlet creation loop
	// Basic object setup

	// Instantiate a new powSine~ object
	t_allOsc *x = (t_allOsc *) pd_new(allOsc_class);

	// Create two additional singal inlets and one control inlet, the first one is on the house
        inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));
        inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));
	inlet_new(&x->obj, &x->obj.ob_pd, &s_float, gensym("ft1"));

        // Create four signal outlets
	for(i = 0; i < 4; i++)
        	outlet_new(&x->obj, gensym("signal"));

	// Initialize phase and frequency to 0
	x->x_phase = 0;
	x->x_frequency = 0;

	// define twoPi
	x->x_twopi = 8.0 * atan(1.0);

	// get system's sampling rate and set sampling interval and factor
	x->x_sr = sys_getsr();
	x->x_sifactor = (float) ALLOSC_STEPSIZE / x->x_sr;
	x->x_si = x->x_frequency * x->x_sifactor;

	// Return a pointer to the new object
	return x;
}

// The perform routine
t_int *allOsc_perform(t_int *w)
{
	// The first six variables are assigned values passed from the dsp method

	// Copy the object pointer
	t_allOsc *x = (t_allOsc *) (w[1]);

	// Copy signal vector pointers
	t_float *frequency = (t_float *) (w[2]);
	t_float *phase_mod = (t_float *) (w[3]);
	t_float *duty_cycle = (t_float *) (w[4]);
	t_float *out1 = (t_float *) (w[5]);
	t_float *out2 = (t_float *) (w[6]);
	t_float *out3 = (t_float *) (w[7]);
	t_float *out4 = (t_float *) (w[8]);

	// Copy the signal vector size
	t_int n = w[9];

	// Dereference components from the object structure
	float si_factor = x->x_sifactor;
	float si = x->x_si;
	float phase = x->x_phase;
	float twopi = x->x_twopi;
	// Local variables
	float duty_cycle_local;
	float phase_add;
	int phase_trunc;
	float phase_wrap;
	float invert_phase; // used for the triangle
	float cos_phase, tri_phase, saw_phase, square_phase;
	float step = (float) ALLOSC_STEPSIZE;

	// Perform the DSP loop
	while(n--){
		si = *frequency++ * si_factor;
		duty_cycle_local = *duty_cycle++;
		// This is kind of copied from [wrap~]'s code
		phase_add = ((phase / step) + *phase_mod++);
		phase_trunc = phase_add;
		if(phase_add > 0) phase_wrap = phase_add - phase_trunc;
		else phase_wrap = phase_add - (phase_trunc - 1);

		// cosine values (starting from -1 to be in phase with the triangle)
		cos_phase = cos(twopi * phase_wrap) * -1;

		// triangle values
		invert_phase = (phase_wrap * -1) + 1;
		if(phase_wrap < invert_phase) tri_phase = phase_wrap;
		else tri_phase = invert_phase;
		tri_phase = (tri_phase * 4) - 1;

		// sawtooth values
		saw_phase = (phase_wrap * 2) - 1;

		// square wave values
		if(phase_wrap < duty_cycle_local) square_phase = -1;
		else square_phase = 1;

		// Set values to respective outlets
		*out1++ = cos_phase;
		*out2++ = tri_phase;
		*out3++ = saw_phase;
		*out4++ = square_phase;
		phase += si;
		while(phase > step) {
			phase -= step;
		}
		while(phase < 0) {
			phase += step;
		}
	}
	// Update object's phase variable
	x->x_phase = phase;

	// Return the next address in the DSP chain
	return w + 10;
}

// The DSP method
void allOsc_dsp(t_allOsc *x, t_signal **sp)
{
	// Set table length local variable
	float step = (float) ALLOSC_STEPSIZE;

	// Check if samplerate has changed
	if(x->x_sr != sp[0]->s_sr){
                if(! sp[0]->s_sr){
                        error("zero sampling rate!");
                        return;
                }
                x->x_si *= x->x_sr / sp[0]->s_sr;
                x->x_sr = sp[0]->s_sr;
                x->x_sifactor = step / x->x_sr;
	}

	/* Attach the object to the DSP chain, passing the DSP routine powSine_perform(),
	inlet and outlet pointers, and the signal vector size */
	dsp_add(allOsc_perform, 9, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec, sp[0]->s_n);
}

// Method to reset oscillator's phase with float input in last inlet (control)
void allOsc_ft1(t_allOsc *x, t_float f)
{
        float scale_input = (float) ALLOSC_STEPSIZE;
        f *= scale_input;
        x->x_phase = f;
}
