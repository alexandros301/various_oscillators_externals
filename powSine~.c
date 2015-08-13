/***************************************************************
 *   This is my first attempt to write a Pd external, based on *
 *   "Designing Audio Objects for Max/MSP and Pd" by Eric Lyon *
 ***************************************************************/

// Header files required by Pure Data
#include "m_pd.h"
#include "math.h"

// Constant definitions
#define POWSINE_STEPSIZE 8192

// The class pointer
static t_class *powSine_class;

// The object structure
typedef struct _powSine {
	// The Pd object
        t_object obj;
	// Convert floats to signals
        t_float x_f;
	// Rest of variables
	float x_frequency;
	// float x_power; // use this variable only after the argument problem is solved
        float x_phase;
	int x_sign; // variable to control the direction of each cycle
        float x_si; // sample increment
        float x_sifactor; // factor for generating sampling increment
        float x_twopi;
        float x_sr; // sampling rate
} t_powSine;

// Function prototypes
void *powSine_new(void);
void powSine_dsp(t_powSine *x, t_signal **sp);
void powSine_ft1(t_powSine *x, t_float f);
t_int *powSine_perform(t_int *w);

// The Pd class definition function
void powSine_tilde_setup(void)
{
	// Initialize the class
	powSine_class = class_new(gensym("powSine~"), (t_newmethod)powSine_new, 0, sizeof(t_powSine), 0, A_GIMME, 0);

	// Specify signal input, with automatic float to signal conversion
	CLASS_MAINSIGNALIN(powSine_class, t_powSine, x_f);

	// Bind the DSP method, which is called when the DACs are turned on
	class_addmethod(powSine_class, (t_method)powSine_dsp, gensym("dsp"), A_CANT, 0);

	// Bind the method to receive a float in the last inlet (control) to reset the phase
        class_addmethod(powSine_class, (t_method)powSine_ft1, gensym("ft1"), A_FLOAT, 0);

	// Print authorship to Pd window
	post("powSine~: Sinewave oscillator raised to a power\n external by Alexandros Drymonitis");
}

// The new instance routine
void *powSine_new(void)
{
	// Basic object setup

	// Instantiate a new powSine~ object
	t_powSine *x = (t_powSine *) pd_new(powSine_class);

	// Create two additional singal inlets and one control inlet, the first one is on the house
        inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));
        inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));
	inlet_new(&x->obj, &x->obj.ob_pd, &s_float, gensym("ft1"));

        // Create one signal outlet
        outlet_new(&x->obj, gensym("signal"));

	/* It is rather difficult to set variables for use in case no signal is connected
	to the respective inlet, at least for me. If somebody can actually make this, he/she
	is more than welcome

	// Check for creation arguments and set their values to the corresponding variables
	if(argc >= 1) { x->x_frequency = atom_getfloatarg(0, argc, argv); }
	if(argc >= 2) { x->x_power = atom_getfloatarg(1, argc, argv); }
	*/

	// Initialize phase and frequency to 0 and sign variable to 1
	x->x_phase = 0;
	x->x_frequency = 0;
	x->x_sign = 1;

	// define twoPi
	x->x_twopi = 8.0 * atan(1.0);

	// get system's sampling rate and set sampling interval and factor
	x->x_sr = sys_getsr();
	x->x_sifactor = (float) POWSINE_STEPSIZE / x->x_sr;
	x->x_si = x->x_frequency * x->x_sifactor;

	// Return a pointer to the new object
	return x;
}

// The perform routine
t_int *powSine_perform(t_int *w)
{
	// The first six variables are assigned values passed from the dsp method

	// Copy the object pointer
	t_powSine *x = (t_powSine *) (w[1]);

	// Copy signal vector pointers
	t_float *frequency = (t_float *) (w[2]);
	t_float *phase_mod = (t_float *) (w[3]);
	t_float *power = (t_float *) (w[4]);
	t_float *out = (t_float *) (w[5]);

	// Copy the signal vector size
	t_int n = w[6];

	// Dereference components from the object structure
	float si_factor = x->x_sifactor;
	float si = x->x_si;
	float phase = x->x_phase;
	float twopi = x->x_twopi;
	float sign = x->x_sign;
	// Local variables
	float phase_add, phase_double;
	int phase_trunc, trunc_double;
	float phase_wrap, wrap_double;
	float step = (float) POWSINE_STEPSIZE;

	// Perform the DSP loop
	while(n--){
		si = *frequency++ * si_factor;
		// This is kind of copied from [wrap~]'s code
		phase_add = ((phase / step) + *phase_mod++);
		phase_trunc = phase_add;
		if(phase_add > 0) phase_wrap = phase_add - phase_trunc;
		else phase_wrap = phase_add - (phase_trunc - 1);
		// [wrap]'s code was up to line above
		// it's copied once more for wrapping the doubled frequency sent to cos
		if(phase_wrap > 0.5) sign = 1;
		else sign = -1;
		phase_double = phase_wrap * 2;
		trunc_double = phase_double;
		if(phase_double > 0) wrap_double = phase_double - trunc_double;
		else wrap_double = phase_double - (trunc_double - 1);
		*out++ = pow(((cos(twopi * wrap_double) * -0.5) + 0.5), *power++) * sign;
		phase += si;
		while(phase > step) {
			phase -= step;
		}
		while(phase < 0) {
			phase += step;
		}
	}
	// Update object's phase and sign variables
	x->x_phase = phase;
	x->x_sign = sign;

	// Return the next address in the DSP chain
	return w + 7;
}

// The DSP method
void powSine_dsp(t_powSine *x, t_signal **sp)
{
	// Set table length local variable
	float step = (float) POWSINE_STEPSIZE;

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
	dsp_add(powSine_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[0]->s_n);
}

// Method to reset oscillator's phase with float input in last inlet (control)
void powSine_ft1(t_powSine *x, t_float f)
{
        float scale_input = (float) POWSINE_STEPSIZE;
        f *= scale_input;
        x->x_phase = f;
}
