/******************************************************************
 *   [sineLoop~] external oscillator that feeds its output        *
 *   back to its input. Code translated from Pyo's SineLoop class *
 *   written by Alexandros Drymonitis                             *
 ******************************************************************/

// Header files required by Pure Data
#include "m_pd.h"
#include "math.h"

// Constant definitions
#define SINELOOP_STEP 512

// The class pointer
static t_class *sineLoop_class;

// lookup table pointer
float *sine_tab;

static float one_over_step = 1.0 / SINELOOP_STEP;

// The object structure
typedef struct _sineLoop {
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
        float x_sr; // sampling rate
        t_float x_last_sample;
} t_sineLoop;

// Function prototypes
static void *sineLoop_new(void);
static void sineLoop_dsp(t_sineLoop *x, t_signal **sp);
static void sineLoop_ft1(t_sineLoop *x, t_float f);
static void make_cos_tab(void);
static t_int *sineLoop_perform(t_int *w);

// The new instance routine
static void *sineLoop_new(void)
{
	// Basic object setup

	// Instantiate a new feedbackSine~ object
	t_sineLoop *x = (t_sineLoop *) pd_new(sineLoop_class);

	// Create one additional singal inlet and one control inlet, the first one is on the house
  inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));
	inlet_new(&x->obj, &x->obj.ob_pd, &s_float, gensym("ft1"));

  // Create one signal outlet
  outlet_new(&x->obj, gensym("signal"));

	// Initialize phase and frequency to 0
	x->x_phase = 0;
	x->x_frequency = 0;

	// get system's sampling rate and set sampling interval and factor
	x->x_sr = sys_getsr();
	x->x_sifactor = (float) SINELOOP_STEP / x->x_sr;
	x->x_si = x->x_frequency * x->x_sifactor;

	// Return a pointer to the new object
	return x;
}

// The perform routine
static t_int *sineLoop_perform(t_int *w)
{
	// The first five variables are assigned values passed from the dsp method

	// Copy the object pointer
	t_sineLoop *x = (t_sineLoop *) (w[1]);

	// Copy signal vector pointers
	t_float *frequency = (t_float *) (w[2]);
	t_float *fb_amount = (t_float *) (w[3]);
	t_float *out = (t_float *) (w[4]);

	// Copy the signal vector size
	t_int n = w[5];

	// Dereference components from the object structure
  t_float last_sample = x->x_last_sample;
	float si_factor = x->x_sifactor;
	float si = x->x_si;
	float phase = x->x_phase;
	// Local variables
	float phase_local;
  float feedback;
  float frac;
  int int_part;
	float step = (float) SINELOOP_STEP;

	// Perform the DSP loop
	while(n--){
    // add it to the frequency
		si = *frequency++ * si_factor;
    // take the current out sample for the feedback
    feedback = *fb_amount++;
    // clip it
    if(feedback >= 1.0) feedback = 1.0;
    else if(feedback < 0.0) feedback = 0.0;
    feedback *= step;

    // wrap phase (copied from SineLoop code)
    if(phase < 0) phase += ((int)(-phase * one_over_step) + 1) * step;
    else if(phase >= step) phase -= (int)(phase * one_over_step) * step;

    // add the last sample with its index
    phase_local = phase + last_sample * feedback;
    // wrap local phase
    if(phase_local < 0) phase_local += ((int)(-phase_local * one_over_step) + 1) * step;
    else if(phase_local >= step) phase_local -= (int)(phase_local * one_over_step) * step;

    int_part = (int)phase_local;
    frac = phase_local - int_part;
    *out++ = last_sample = sine_tab[int_part] * (1.0 - frac) + sine_tab[int_part + 1] * frac;
		phase += si;
	}
	// Update object's phase and last_sample variables
	x->x_phase = phase;
  x->x_last_sample = last_sample;

	// Return the next address in the DSP chain
	return w + 6;
}

// The DSP method
static void sineLoop_dsp(t_sineLoop *x, t_signal **sp)
{
	// Set table length local variable
	float step = (float) SINELOOP_STEP;

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
	dsp_add(sineLoop_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
}

static void make_cos_tab(void)
{
  int i;
  float *fp, phase, phsinc = (2. * 3.14159) / SINELOOP_STEP;

  // first check if a [sineLoop~] object has already been created
  // if it has, don't acquire memory, but use the already existing table
  if(sine_tab) return;
  // this is copied from [cos~]
  sine_tab = (float *)getbytes(sizeof(float) * (SINELOOP_STEP+1));

  for (i = 0, fp = sine_tab, phase = 0; i < SINELOOP_STEP; i++, fp++, phase += phsinc)
    *fp = cos(phase);

  // copy to first element to the last position of the table for interpolation
  sine_tab[SINELOOP_STEP] = sine_tab[0];
}

// The Pd class definition function
void sineLoop_tilde_setup(void)
{
	// Initialize the class
	sineLoop_class = class_new(gensym("sineLoop~"), (t_newmethod)sineLoop_new, 0, sizeof(t_sineLoop), 0, A_GIMME, 0);

	// Specify signal input, with automatic float to signal conversion
	CLASS_MAINSIGNALIN(sineLoop_class, t_sineLoop, x_f);

	// Bind the DSP method, which is called when the DACs are turned on
	class_addmethod(sineLoop_class, (t_method)sineLoop_dsp, gensym("dsp"), A_CANT, 0);

	// Bind the method to receive a float in the last inlet (control) to reset the phase
  class_addmethod(sineLoop_class, (t_method)sineLoop_ft1, gensym("ft1"), A_FLOAT, 0);

  // fill in the lookup table
  make_cos_tab();

	// Print authorship to Pd window
	post("sineLoop~: Feedback sinewave oscillator\ncode translated from Pyo's SineLoop class\n external by Alexandros Drymonitis");
}

// Method to reset oscillator's phase with float input in last inlet (control)
static void sineLoop_ft1(t_sineLoop *x, t_float f)
{
        float scale_input = (float) SINELOOP_STEP;
        f *= scale_input;
        x->x_phase = f;
}
