/** Header file generated with fdesign on Wed Nov  8 19:37:16 2000.**/

#ifndef FD_fsc2_h_
#define FD_fsc2_h_

/** Callbacks, globals and object handlers **/
extern void load_file(FL_OBJECT *, long);
extern void test_file(FL_OBJECT *, long);
extern void run_file(FL_OBJECT *, long);
extern void edit_file(FL_OBJECT *, long);
extern void win_slider_callback(FL_OBJECT *, long);
extern void bug_report_callback(FL_OBJECT *, long);
extern void run_help(FL_OBJECT *, long);

extern void stop_measurement(FL_OBJECT *, long);
extern void run_sigchld_callback(FL_OBJECT *, long);
extern void fs_button_callback(FL_OBJECT *, long);
extern void curve_button_callback(FL_OBJECT *, long);
extern void undo_button_callback(FL_OBJECT *, long);


extern void print_callback(FL_OBJECT *, long);

extern void curve_button_callback(FL_OBJECT *, long);
extern void cut_undo_button_callback(FL_OBJECT *, long);
extern void cut_close_callback(FL_OBJECT *, long);
extern void cut_fs_button_callback(FL_OBJECT *, long);


/**** Forms and Objects ****/
typedef struct {
	FL_FORM *fsc2;
	void *vdata;
	char *cdata;
	long  ldata;
	FL_OBJECT *browser;
	FL_OBJECT *Load;
	FL_OBJECT *reload;
	FL_OBJECT *test_file;
	FL_OBJECT *run;
	FL_OBJECT *quit;
	FL_OBJECT *error_browser;
	FL_OBJECT *Edit;
	FL_OBJECT *win_slider;
	FL_OBJECT *bug_report;
	FL_OBJECT *help;
} FD_fsc2;

extern FD_fsc2 * create_form_fsc2(void);
typedef struct {
	FL_FORM *run;
	void *vdata;
	char *cdata;
	long  ldata;
	FL_OBJECT *z_axis;
	FL_OBJECT *stop;
	FL_OBJECT *sigchld;
	FL_OBJECT *y_axis;
	FL_OBJECT *full_scale_button;
	FL_OBJECT *curve_1_button;
	FL_OBJECT *curve_2_button;
	FL_OBJECT *curve_3_button;
	FL_OBJECT *curve_4_button;
	FL_OBJECT *undo_button;
	FL_OBJECT *canvas;
	FL_OBJECT *x_axis;
	FL_OBJECT *print_button;
} FD_run;

extern FD_run * create_form_run(void);
typedef struct {
	FL_FORM *input_form;
	void *vdata;
	char *cdata;
	long  ldata;
	FL_OBJECT *comm_input;
	FL_OBJECT *comm_done;
} FD_input_form;

extern FD_input_form * create_form_input_form(void);
typedef struct {
	FL_FORM *print;
	void *vdata;
	char *cdata;
	long  ldata;
	FL_OBJECT *s2p_input;
	FL_OBJECT *cancel_button;
	FL_OBJECT *print_button;
	FL_OBJECT *paper_types;
	FL_OBJECT *A3;
	FL_OBJECT *Letter;
	FL_OBJECT *Legal;
	FL_OBJECT *A4;
	FL_OBJECT *modes;
	FL_OBJECT *s2p_button;
	FL_OBJECT *p2f_button;
	FL_OBJECT *p2f_group;
	FL_OBJECT *p2f_input;
	FL_OBJECT *p2f_browse;
	FL_OBJECT *color_group;
	FL_OBJECT *bw_button;
	FL_OBJECT *col_button;
} FD_print;

extern FD_print * create_form_print(void);
typedef struct {
	FL_FORM *cut;
	void *vdata;
	char *cdata;
	long  ldata;
	FL_OBJECT *cut_undo_button;
	FL_OBJECT *cut_canvas;
	FL_OBJECT *cut_print_button;
	FL_OBJECT *cut_close_button;
	FL_OBJECT *cut_x_axis;
	FL_OBJECT *cut_full_scale_button;
	FL_OBJECT *cut_z_axis;
	FL_OBJECT *cut_y_axis;
} FD_cut;

extern FD_cut * create_form_cut(void);

#endif /* FD_fsc2_h_ */
