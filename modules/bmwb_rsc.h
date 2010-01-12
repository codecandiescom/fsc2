#ifndef FD_bmbw_rsc_h_
#define FD_bmbw_rsc_h_

#include <forms.h>

/* Callbacks, globals and object handlers */

extern void mode_cb( FL_OBJECT *, long );
extern void freq_cb( FL_OBJECT *, long );
extern void attenuation_cb( FL_OBJECT *, long );
extern void attenuation_button_cb( FL_OBJECT *, long );
extern void signal_phase_cb( FL_OBJECT *, long );
extern void bias_cb( FL_OBJECT *, long );
extern void lock_phase_cb( FL_OBJECT *, long );
extern void iris_cb( FL_OBJECT *, long );
extern void quit_cb( FL_OBJECT *, long );


/* Forms and Objects */

typedef struct {
    FL_FORM   * form;
    void      * vdata;
    char      * cdata;
    long        ldata;
    FL_OBJECT * dc_canvas;
    FL_OBJECT * afc_canvas;
    FL_OBJECT * tune_canvas;
	FL_OBJECT * mode_select;
	FL_OBJECT * unlocked_indicator;
	FL_OBJECT * uncalibrated_indicator;
	FL_OBJECT * afc_indicator;
	FL_OBJECT * main_frame;
	FL_OBJECT * freq_group;
    FL_OBJECT * freq_slider;
	FL_OBJECT * freq_text;
	FL_OBJECT * attenuation_group;
    FL_OBJECT * attenuation_counter;
    FL_OBJECT * attenuation_button;
	FL_OBJECT * signal_phase_group;
    FL_OBJECT * signal_phase_slider;
	FL_OBJECT * secondary_frame;
	FL_OBJECT * bias_group;
    FL_OBJECT * bias_slider;
	FL_OBJECT * lock_phase_group;
    FL_OBJECT * lock_phase_slider;
	FL_OBJECT * iris_group;
    FL_OBJECT * iris_up;
    FL_OBJECT * iris_down;
} FD_bmbw_rsc;

extern FD_bmbw_rsc * create_form_bmbw_rsc( void );

#endif /* FD_bmbw_rsc_h_ */
