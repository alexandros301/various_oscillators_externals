/************************************************
 * Various shapes oscillator Pure Data external *
 * written by Alexandros Drymonitis             *
 ************************************************/

// Header files required by Pure Data
#include "m_pd.h"
#include "math.h"

// Constant definitions
#define VARSHAPES_STEPSIZE 8192

// The class pointer
static t_class *varShapesOsc_class;

// The object structure
typedef struct _varShapesOsc {
	// The Pd object
        t_object obj;
	// Convert floats to signals
        t_float x_f;
	// Rest of variables
	float x_frequency;
        float x_phase;
        float x_si; // sample increment
        float x_sifactor; // factor for generating sampling increment
        float x_twopi;
        float x_sr; // sampling rate
} t_varShapesOsc;

// Function prototypes
void *varShapesOsc_new(void);
void varShapesOsc_dsp(t_varShapesOsc *x, t_signal **sp);
void varShapesOsc_ft1(t_varShapesOsc *x, t_float f);
t_int *varShapesOsc_perform(t_int *w);

// The Pd class definition function
void varShapesOsc_tilde_setup(void)
{
	// Initialize the class
	varShapesOsc_class = class_new(gensym("varShapesOsc~"), (t_newmethod)varShapesOsc_new, 0, sizeof(t_varShapesOsc), 0, A_GIMME, 0);

	// Specify signal input, with automatic float to signal conversion
	CLASS_MAINSIGNALIN(varShapesOsc_class, t_varShapesOsc, x_f);

	// Bind the DSP method, which is called when the DACs are turned on
	class_addmethod(varShapesOsc_class, (t_method)varShapesOsc_dsp, gensym("dsp"), A_CANT, 0);

	// Bind the method to receive a float in the last inlet (control) to reset the phase
        class_addmethod(varShapesOsc_class, (t_method)varShapesOsc_ft1, gensym("ft1"), A_FLOAT, 0);

	// Print authorship to Pd window
	post("varShapesOsc~: Various shapes oscillator\n external by Alexandros Drymonitis");
}

// The new instance routine
void *varShapesOsc_new(void)
{
	// Basic object setup

	// Instantiate a new powSine~ object
	t_varShapesOsc *x = (t_varShapesOsc *) pd_new(varShapesOsc_class);

	// Create five additional singal inlets and one control inlet, the first one is on the house
        inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));
        inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));
	inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));
        inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));
	inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));
	inlet_new(&x->obj, &x->obj.ob_pd, &s_float, gensym("ft1"));

        // Create one signal outlet
        outlet_new(&x->obj, gensym("signal"));

	/* It is rather difficult to set arguments to be used in case no signal is
	connected to the respective inlet, at least for me. If somebody can actually
	make this, he/she is more than welcome

	// Check for creation arguments and set their values to the corresponding variables
	if(argc >= 1) { x->x_frequency = atom_getfloatarg(0, argc, argv); }
	if(argc >= 2) { x->x_breakpoint = atom_getfloatarg(1, argc, argv); }
	if(argc >= 3) { x->x_rise_power = atom_getfloatarg(2, argc, argv); }
	if(argc >= 4) { x->x_fall_power = atom_getfloatarg(3, argc, argv); }
	if(argc >= 5) { x->x_xfade = atom_getfloatarg(4, argc, argv); }
	*/

	// Initialize phase and frequency to 0
	x->x_phase = 0;
	x->x_frequency = 0;

	// define twoPi
	x->x_twopi = 8.0 * atan(1.0);

	// get system's sampling rate and set sampling interval and factor
	x->x_sr = sys_getsr();
	x->x_sifactor = (float) VARSHAPES_STEPSIZE / x->x_sr;
	x->x_si = x->x_frequency * x->x_sifactor;

	// Return a pointer to the new object
	return x;
}

// The perform routine
t_int *varShapesOsc_perform(t_int *w)
{
	// The first six variables are assigned values passed from the dsp method

	// Copy the object pointer
	t_varShapesOsc *x = (t_varShapesOsc *) (w[1]);

	// Copy signal vector pointers
	t_float *frequency = (t_float *) (w[2]);
	t_float *phase_mod = (t_float *) (w[3]);
	t_float *xfade = (t_float *) (w[4]);
	t_float *breakpoint = (t_float *) (w[5]);
	t_float *rise_power = (t_float *) (w[6]);
	t_float *fall_power = (t_float *) (w[7]);
	t_float *out = (t_float *) (w[8]);

	// Copy the signal vector size
	t_int n = w[9];

	// Dereference components from the object structure
	float si_factor = x->x_sifactor;
	float si = x->x_si;
	float phase = x->x_phase;
	float twopi = x->x_twopi;
	// Local variables
        float step = (float) VARSHAPES_STEPSIZE;
	float breakpoint_local, invert_brk;
	float xfade_local, invert_xfade;
	float cos_phase, tri_phase;
	float rise_power_local, fall_power_local;
	float cos_tri_add, power_add;
	float phase_add;
	int phase_trunc;
	float phase_wrap;

	// Perform the DSP loop
	while(n--){
		breakpoint_local = *breakpoint++;
		invert_brk = 1 - breakpoint_local;
		xfade_local = *xfade++;
		invert_xfade = 1 - xfade_local;
		rise_power_local = *rise_power++;
		fall_power_local = *fall_power++;
		si = *frequency++ * si_factor;
		// This is kind of copied from [wrap~]'s code
		phase_add = ((phase / step) + *phase_mod++);
		phase_trunc = phase_add;
		if(phase_add > 0) phase_wrap = phase_add - phase_trunc;
		else phase_wrap = phase_add - (phase_trunc - 1);
		// [wrap]'s code was up to line above
		if(phase_wrap < breakpoint_local) {
			cos_phase = ((phase_wrap / breakpoint_local) * 0.5) + 0.5;
			tri_phase = phase_wrap / breakpoint_local;
			fall_power_local = 0;
		}
		else {
			cos_phase = ((phase_wrap - breakpoint_local) / invert_brk) * 0.5;
			tri_phase = (invert_brk - (phase_wrap - breakpoint_local)) / invert_brk;
			rise_power_local = 0;
		}
		cos_tri_add = (((cos(twopi * cos_phase) * 0.5) + 0.5) * invert_xfade) + (tri_phase * xfade_local);
		power_add = rise_power_local + fall_power_local;
		*out++ = (pow(cos_tri_add, power_add) * 2) - 1;
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
void varShapesOsc_dsp(t_varShapesOsc *x, t_signal **sp)
{
	// Set table length local variable
	float step = (float) VARSHAPES_STEPSIZE;

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
	dsp_add(varShapesOsc_perform, 9, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec, sp[0]->s_n);
}

// Method to reset oscillator's phase with float input in last inlet (control)
void varShapesOsc_ft1(t_varShapesOsc *x, t_float f)
{
        float scale_input = (float) VARSHAPES_STEPSIZE;
        f *= scale_input;
        x->x_phase = f;
}
