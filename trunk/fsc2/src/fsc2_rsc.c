/* Form definition file generated with fdesign. */

#include "forms.h"
#include <stdlib.h>
#include "fsc2_rsc.h"

FD_fsc2 *create_form_fsc2(void)
{
  FL_OBJECT *obj;
  FD_fsc2 *fdui = (FD_fsc2 *) fl_calloc(1, sizeof(*fdui));

  fdui->fsc2 = fl_bgn_form(FL_NO_BOX, 1050, 800);
  obj = fl_add_box(FL_UP_BOX,0,0,1050,800,"");
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_lstyle(obj,FL_FIXED_STYLE);
  fdui->browser = obj = fl_add_browser(FL_NORMAL_BROWSER,230,10,790,580,"");
    fl_set_object_color(obj,FL_WHITE,FL_YELLOW);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_NorthWest, FL_East);
  fdui->Load = obj = fl_add_button(FL_NORMAL_BUTTON,50,60,130,50,"Load");
    fl_set_button_shortcut(obj,"L",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,load_file,0);
  fdui->reload = obj = fl_add_button(FL_NORMAL_BUTTON,50,150,130,50,"Reload");
    fl_set_button_shortcut(obj,"R",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,load_file,1);
  fdui->test_file = obj = fl_add_button(FL_NORMAL_BUTTON,50,370,130,50,"Test");
    fl_set_button_shortcut(obj,"T",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,test_file,0);
  fdui->run = obj = fl_add_button(FL_NORMAL_BUTTON,50,460,130,50,"Start");
    fl_set_button_shortcut(obj,"S",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,run_file,0);
  fdui->quit = obj = fl_add_button(FL_NORMAL_BUTTON,50,600,130,50,"Quit");
    fl_set_button_shortcut(obj,"Q",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
  fdui->error_browser = obj = fl_add_browser(FL_NORMAL_BROWSER,230,600,790,190,"");
    fl_set_object_color(obj,FL_WHITE,FL_YELLOW);
    fl_set_object_lsize(obj,FL_MEDIUM_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_SouthEast);
  fdui->Edit = obj = fl_add_button(FL_NORMAL_BUTTON,50,240,130,50,"Edit");
    fl_set_button_shortcut(obj,"E",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,edit_file,0);
  fdui->win_slider = obj = fl_add_slider(FL_VERT_BROWSER_SLIDER,1020,10,20,780,"");
    fl_set_object_gravity(obj, FL_NorthEast, FL_SouthEast);
    fl_set_object_resize(obj, FL_RESIZE_Y);
    fl_set_object_callback(obj,win_slider_callback,0);
    fl_set_slider_value(obj, 0.172727);
    fl_set_slider_size(obj, 0.05);
     fl_set_slider_return(obj, FL_RETURN_END_CHANGED);
  fdui->bug_report = obj = fl_add_button(FL_NORMAL_BUTTON,50,730,130,40,"Bug Report");
    fl_set_button_shortcut(obj,"B",1);
    fl_set_object_color(obj,FL_MCOL,FL_COL1);
    fl_set_object_lsize(obj,FL_MEDIUM_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,bug_report_callback,0);
  fl_end_form();

  fdui->fsc2->fdui = fdui;

  return fdui;
}
/*---------------------------------------*/

FD_run *create_form_run(void)
{
  FL_OBJECT *obj;
  FD_run *fdui = (FD_run *) fl_calloc(1, sizeof(*fdui));

  fdui->run = fl_bgn_form(FL_NO_BOX, 1030, 800);
  obj = fl_add_box(FL_UP_BOX,0,0,1030,800,"");
  fdui->stop = obj = fl_add_button(FL_NORMAL_BUTTON,25,730,110,45,"Stop");
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_SouthWest, FL_SouthWest);
    fl_set_object_callback(obj,stop_measurement,0);
  fdui->redraw_dummy = obj = fl_add_button(FL_NORMAL_BUTTON,500,400,1,1,"");
    fl_set_object_boxtype(obj,FL_NO_BOX);
    fl_set_object_callback(obj,new_data_callback,0);
  fdui->sigchld = obj = fl_add_button(FL_NORMAL_BUTTON,502,400,1,1,"");
    fl_set_object_boxtype(obj,FL_NO_BOX);
    fl_set_object_callback(obj,run_sigchld_callback,0);
  fdui->y_axis = obj = fl_add_canvas(FL_NORMAL_CANVAS,170,10,90,700,"");
    fl_set_object_gravity(obj, FL_NorthWest, FL_SouthWest);
    fl_set_object_resize(obj, FL_RESIZE_Y);
  fdui->x_axis = obj = fl_add_canvas(FL_NORMAL_CANVAS,260,710,760,80,"");
    fl_set_object_gravity(obj, FL_SouthWest, FL_SouthEast);
    fl_set_object_resize(obj, FL_RESIZE_X);
  fdui->canvas = obj = fl_add_canvas(FL_NORMAL_CANVAS,260,10,760,700,"");
    fl_set_object_gravity(obj, FL_NorthWest, FL_SouthEast);
  fdui->full_scale_button = obj = fl_add_button(FL_NORMAL_BUTTON,190,730,45,45,"FS");
    fl_set_button_shortcut(obj,"F",1);
    fl_set_object_color(obj,FL_TOP_BCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_SouthWest, FL_SouthWest);
    fl_set_object_callback(obj,fs_button_callback,0);
  fl_end_form();

  fdui->run->fdui = fdui;

  return fdui;
}
/*---------------------------------------*/

FD_input_form *create_form_input_form(void)
{
  FL_OBJECT *obj;
  FD_input_form *fdui = (FD_input_form *) fl_calloc(1, sizeof(*fdui));

  fdui->input_form = fl_bgn_form(FL_NO_BOX, 665, 455);
  obj = fl_add_box(FL_UP_BOX,0,0,665,455,"");
  fdui->comm_input = obj = fl_add_input(FL_MULTILINE_INPUT,15,45,635,360,"Enter your comments:");
    fl_set_object_lsize(obj,FL_MEDIUM_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_TOP_LEFT);
    fl_set_object_gravity(obj, FL_NorthWest, FL_SouthEast);
  fdui->comm_done = obj = fl_add_button(FL_NORMAL_BUTTON,250,415,120,30,"Done");
    fl_set_object_color(obj,FL_TOP_BCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_MEDIUM_SIZE);
    fl_set_object_gravity(obj, FL_South, FL_South);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
  fl_end_form();

  fdui->input_form->fdui = fdui;

  return fdui;
}
/*---------------------------------------*/

