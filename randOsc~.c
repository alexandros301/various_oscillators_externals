/****************************************
 * Random oscillator Pure Data external *
 * written by Alexandros Drymonitis     *
 ****************************************/

// Header files required by Pure Data
#include "m_pd.h"
#include "math.h"

// Constant definitions
#define RANDOSC_STEPSIZE 8192

// The class pointer
static t_class *randOsc_class;

// The object structure
typedef struct _randOsc {
	// The Pd object
        t_object obj;
	// Convert floats to signals
        t_float x_f;
	// Rest of variables
	float x_frequency;
	float x_xfade;
	float x_power;
        float x_phase;
	float x_random_sample, x_old_random_sample;
        float x_si; // sample increment
        float x_sifactor; // factor for generating sampling increment
        float x_twopi;
        float x_sr; // sampling rate
} t_randOsc;

// Function prototypes
void *randOsc_new(void);
void randOsc_dsp(t_randOsc *x, t_signal **sp);
void randOsc_ft1(t_randOsc *x, t_float f);
t_int *randOsc_perform(t_int *w);

// The Pd class definition function
void randOsc_tilde_setup(void)
{
	// Initialize the class
	randOsc_class = class_new(gensym("randOsc~"), (t_newmethod)randOsc_new, 0, sizeof(t_randOsc), 0, A_GIMME, 0);

	// Specify signal input, with automatic float to signal conversion
	CLASS_MAINSIGNALIN(randOsc_class, t_randOsc, x_f);

	// Bind the DSP method, which is called when the DACs are turned on
	class_addmethod(randOsc_class, (t_method)randOsc_dsp, gensym("dsp"), A_CANT, 0);

	// Bind the method to receive a float in the last inlet (control) to reset the phase
        class_addmethod(randOsc_class, (t_method)randOsc_ft1, gensym("ft1"), A_FLOAT, 0);

	// Print authorship to Pd window
	post("randOsc~: Random oscillator (not white noise)\n external by Alexandros Drymonitis");
}

// The new instance routine
void *randOsc_new(void)
{
	// Basic object setup

	// Instantiate a new powSine~ object
	t_randOsc *x = (t_randOsc *) pd_new(randOsc_class);

	// Create two additional singal inlets and one control inlet, the first one is on the house
        inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));
        inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));
	inlet_new(&x->obj, &x->obj.ob_pd, &s_float, gensym("ft1"));

        // Create one signal outlet
        outlet_new(&x->obj, gensym("signal"));

	/* It is rather difficult to set arguments in order to use them in case there is no
	signal connected to the respective inlet, at least for me. If somebody can actually
	make it, he/she is more than welcome

	// Check for creation arguments and set their values to the corresponding variables
	if(argc >= 1) { x->x_frequency = atom_getfloatarg(0, argc, argv); }
	if(argc >= 2) { x->x_xfade = atom_getfloatarg(1, argc, argv); }
	if(argc >= 3) { x->x_power = atom_getfloatarg(2, argc, argv); }
	*/

	// Initialize phase, frequency and random samples to 0
	x->x_phase = 0;
	x->x_frequency = 0;
	x->x_random_sample = 0;
	x->x_old_random_sample = 0;

	// define twoPi
	x->x_twopi = 8.0 * atan(1.0);

	// get system's sampling rate and set sampling interval and factor
	x->x_sr = sys_getsr();
	x->x_sifactor = (float) RANDOSC_STEPSIZE / x->x_sr;
	x->x_si = x->x_frequency * x->x_sifactor;

	// Return a pointer to the new object
	return x;
}

// The perform routine
t_int *randOsc_perform(t_int *w)
{
	// The first six variables are assigned values passed from the dsp method

	// Copy the object pointer
	t_randOsc *x = (t_randOsc *) (w[1]);

	// Copy signal vector pointers
	t_float *frequency = (t_float *) (w[2]);
	t_float *xfade = (t_float *) (w[3]);
	t_float *power = (t_float *) (w[4]);
	t_float *out = (t_float *) (w[5]);

	// Copy the signal vector size
	t_int n = w[6];

	// Dereference components from the object structure
	float si_factor = x->x_sifactor;
	float si = x->x_si;
	float phase = x->x_phase;
	float twopi = x->x_twopi;
	float random_sample = x->x_random_sample;
	float old_random_sample = x->x_old_random_sample;
	// Local variables
        float step = (float) RANDOSC_STEPSIZE;
	float xfade_local, invert_xfade;
	float cos_phase, tri_phase;
	float cos_tri_add;
	float noise;
	int randval;
	// Variables for scale and offset
	float scale, offset;
	float random_bipolar, old_random_bipolar;
	float abs_random_bipolar, abs_old_random_bipolar;
	float max_sample, max_absolute_sample;
	float sample_diff;


	// Perform the DSP loop
	while(n--){
		// Create a random sample at each loop iteration (copied from [noise~]'s code
		noise = ((float)((randval & 0x7fffffff) - 0x40000000)) *
           	 (float)(1.0 / 0x40000000);
        	randval = randval * 435898247 + 382842987;
		noise = noise * 0.5 + 0.5;
		// Set xfade vector to a local variable
		xfade_local = *xfade++;
		invert_xfade = 1 - xfade_local;
		// Set scale and offset values according to random samples
		sample_diff = random_sample - old_random_sample;
		if(sample_diff < 0) {
			scale = sample_diff * -1;
		}
		else {
			scale = sample_diff;
		}
		random_bipolar = (random_sample * 2) - 1;
		old_random_bipolar = (old_random_sample * 2) - 1;
		// Get absolute values of bipolar random values
		if(random_bipolar < 0) {
			abs_random_bipolar = random_bipolar * -1;
		}
		else {
			abs_random_bipolar = random_bipolar;
		}
		if(old_random_bipolar < 0) {
			abs_old_random_bipolar = old_random_bipolar * -1;
		}
		else {
			abs_old_random_bipolar = old_random_bipolar;
		}
		// Get maximum absolute sample
		if(abs_random_bipolar > abs_old_random_bipolar) {
			max_absolute_sample = abs_random_bipolar;
		}
		else {
			max_absolute_sample = abs_old_random_bipolar;
		}
		offset = max_absolute_sample - scale;
		// Get maximum sample
		if(random_bipolar > old_random_bipolar) {
			max_sample = random_bipolar;
		}
		else {
			max_sample = old_random_bipolar;
		}
		if(max_sample > scale) {
			offset *= 1;
		}
		else {
			offset *= -1;
		}
		// Phase increment
		si = *frequency++ * si_factor;
		// Cosine and triangle values
		if(random_sample > old_random_sample) {
			cos_phase = ((phase / step) * 0.5) + 0.5;
			tri_phase = phase / step;
		}
		else {
			cos_phase = (phase / step) * 0.5;
			tri_phase = ((phase / step) * -1) + 1;
		}
		cos_tri_add = (((cos(twopi * cos_phase) * 0.5) + 0.5) * invert_xfade) + (tri_phase * xfade_local);
		*out++ = (((pow(cos_tri_add, *power++) * 2) - 1) * scale) + offset;
		phase += si;
		while(phase > step) {
			phase -= step;
			old_random_sample = random_sample;
        		random_sample = noise;
		}
		while(phase < 0) {
			phase += step;
		}
	}

	// Update object's phase variable
	x->x_phase = phase;
	x->x_random_sample = random_sample;
	x->x_old_random_sample = old_random_sample;

	// Return the next address in the DSP chain
	return w + 7;
}

// The DSP method
void randOsc_dsp(t_randOsc *x, t_signal **sp)
{
	// Set table length local variable
	float step = (float) RANDOSC_STEPSIZE;

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
	dsp_add(randOsc_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[0]->s_n);
}

// Method to reset oscillator's phase with float input in last inlet (control)
void randOsc_ft1(t_randOsc *x, t_float f)
{
        float scale_input = (float) RANDOSC_STEPSIZE;
        f *= scale_input;
        x->x_phase = f;
}
