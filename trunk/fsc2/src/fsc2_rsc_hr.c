/* Form definition file generated with fdesign. */

#include "forms.h"
#include <stdlib.h>
#include "fsc2_rsc_hr.h"

FD_fsc2 *create_form_fsc2(void)
{
  FL_OBJECT *obj;
  FD_fsc2 *fdui = (FD_fsc2 *) fl_calloc(1, sizeof(*fdui));

  fdui->fsc2 = fl_bgn_form(FL_NO_BOX, 1050, 800);
  obj = fl_add_box(FL_UP_BOX,0,0,1050,800,"");
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_lstyle(obj,FL_FIXED_STYLE);
  fdui->browser = obj = fl_add_browser(FL_NORMAL_BROWSER,185,10,835,580,"");
    fl_set_object_color(obj,FL_WHITE,FL_YELLOW);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_lstyle(obj,FL_FIXED_STYLE);
    fl_set_object_gravity(obj, FL_NorthWest, FL_East);
  fdui->Load = obj = fl_add_button(FL_NORMAL_BUTTON,20,60,130,50,"Load");
    fl_set_button_shortcut(obj,"L",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,load_file,0);
  fdui->reload = obj = fl_add_button(FL_NORMAL_BUTTON,20,150,130,50,"Reload");
    fl_set_button_shortcut(obj,"R",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,load_file,1);
  fdui->test_file = obj = fl_add_button(FL_NORMAL_BUTTON,20,355,130,50,"Test");
    fl_set_button_shortcut(obj,"T",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,test_file,0);
  fdui->run = obj = fl_add_button(FL_NORMAL_BUTTON,20,440,130,50,"Start");
    fl_set_button_shortcut(obj,"S",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,run_file,0);
  fdui->quit = obj = fl_add_button(FL_NORMAL_BUTTON,20,555,130,50,"Quit");
    fl_set_button_shortcut(obj,"Q",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
  fdui->error_browser = obj = fl_add_browser(FL_NORMAL_BROWSER,185,595,835,195,"");
    fl_set_object_color(obj,FL_WHITE,FL_YELLOW);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_lstyle(obj,FL_FIXED_STYLE);
    fl_set_object_gravity(obj, FL_West, FL_SouthEast);
  fdui->Edit = obj = fl_add_button(FL_NORMAL_BUTTON,20,240,130,50,"Edit");
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
  fdui->bug_report = obj = fl_add_button(FL_NORMAL_BUTTON,20,730,130,40,"Bug Report");
    fl_set_button_shortcut(obj,"B",1);
    fl_set_object_color(obj,FL_MCOL,FL_COL1);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,bug_report_callback,0);
  fdui->help = obj = fl_add_button(FL_NORMAL_BUTTON,20,655,130,50,"Help");
    fl_set_button_shortcut(obj,"H",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,run_help,0);
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
  fdui->z_axis = obj = fl_add_canvas(FL_NORMAL_CANVAS,845,10,175,780,"");
    fl_set_object_gravity(obj, FL_NorthEast, FL_SouthEast);
    fl_set_object_resize(obj, FL_RESIZE_Y);
  fdui->stop = obj = fl_add_button(FL_NORMAL_BUTTON,15,740,115,45,"Stop");
    fl_set_button_shortcut(obj,"S",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_SouthWest, FL_SouthWest);
    fl_set_object_callback(obj,stop_measurement,0);
  fdui->sigchld = obj = fl_add_button(FL_NORMAL_BUTTON,16,741,1,1,"");
    fl_set_object_boxtype(obj,FL_NO_BOX);
    fl_set_object_gravity(obj, FL_SouthWest, FL_SouthWest);
    fl_set_object_callback(obj,run_sigchld_callback,0);
  fdui->y_axis = obj = fl_add_canvas(FL_NORMAL_CANVAS,95,10,150,715,"");
    fl_set_object_gravity(obj, FL_NorthWest, FL_SouthWest);
    fl_set_object_resize(obj, FL_RESIZE_Y);
  fdui->full_scale_button = obj = fl_add_button(FL_PUSH_BUTTON,175,740,45,45,"FS");
    fl_set_button_shortcut(obj,"F",1);
    fl_set_object_color(obj,FL_TOP_BCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_SouthWest, FL_SouthWest);
    fl_set_object_callback(obj,fs_button_callback,0);
    fl_set_button(obj, 1);
  fdui->curve_1_button = obj = fl_add_button(FL_PUSH_BUTTON,25,20,35,35,"1");
    fl_set_button_shortcut(obj,"1",1);
    fl_set_object_color(obj,FL_MCOL,FL_TOMATO);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_NorthWest, FL_NorthWest);
    fl_set_object_callback(obj,curve_button_callback,1);
    fl_set_button(obj, 1);
  fdui->curve_2_button = obj = fl_add_button(FL_PUSH_BUTTON,25,60,35,35,"2");
    fl_set_button_shortcut(obj,"2",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_NorthWest, FL_NorthWest);
    fl_set_object_callback(obj,curve_button_callback,2);
    fl_set_button(obj, 1);
  fdui->curve_3_button = obj = fl_add_button(FL_PUSH_BUTTON,25,100,35,35,"3");
    fl_set_button_shortcut(obj,"3",1);
    fl_set_object_color(obj,FL_MCOL,FL_YELLOW);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_NorthWest, FL_NorthWest);
    fl_set_object_callback(obj,curve_button_callback,3);
    fl_set_button(obj, 1);
  fdui->curve_4_button = obj = fl_add_button(FL_PUSH_BUTTON,25,140,35,35,"4");
    fl_set_button_shortcut(obj,"4",1);
    fl_set_object_color(obj,FL_MCOL,FL_CYAN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_NorthWest, FL_NorthWest);
    fl_set_object_callback(obj,curve_button_callback,4);
    fl_set_button(obj, 1);
  fdui->undo_button = obj = fl_add_pixmapbutton(FL_NORMAL_BUTTON,15,200,60,70,"Undo");
    fl_set_button_shortcut(obj,"U",1);
    fl_set_object_color(obj,FL_COL1,FL_BLACK);
    fl_set_object_lalign(obj,FL_ALIGN_BOTTOM|FL_ALIGN_INSIDE);
    fl_set_object_gravity(obj, FL_NorthWest, FL_NorthWest);
    fl_set_object_callback(obj,undo_button_callback,0);
  fdui->canvas = obj = fl_add_canvas(FL_NORMAL_CANVAS,245,10,775,715,"");
    fl_set_object_gravity(obj, FL_NorthWest, FL_SouthEast);
  fdui->x_axis = obj = fl_add_canvas(FL_NORMAL_CANVAS,245,725,775,60,"");
    fl_set_object_gravity(obj, FL_SouthWest, FL_SouthEast);
    fl_set_object_resize(obj, FL_RESIZE_X);
  fdui->print_button = obj = fl_add_pixmapbutton(FL_NORMAL_BUTTON,15,300,60,70,"Print");
    fl_set_button_shortcut(obj,"P",1);
    fl_set_object_color(obj,FL_COL1,FL_BLACK);
    fl_set_object_lalign(obj,FL_ALIGN_BOTTOM|FL_ALIGN_INSIDE);
    fl_set_object_gravity(obj, FL_NorthWest, FL_NorthWest);
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
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_TOP_LEFT);
    fl_set_object_gravity(obj, FL_NorthWest, FL_SouthEast);
  fdui->comm_done = obj = fl_add_button(FL_NORMAL_BUTTON,265,415,120,30,"Done");
    fl_set_object_color(obj,FL_TOP_BCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_South, FL_South);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
  fl_end_form();

  fdui->input_form->fdui = fdui;

  return fdui;
}
/*---------------------------------------*/

FD_print *create_form_print(void)
{
  FL_OBJECT *obj;
  FD_print *fdui = (FD_print *) fl_calloc(1, sizeof(*fdui));

  fdui->print = fl_bgn_form(FL_NO_BOX, 570, 545);
  obj = fl_add_box(FL_UP_BOX,0,0,570,545,"");
  obj = fl_add_frame(FL_ENGRAVED_FRAME,15,15,540,160,"");
  obj = fl_add_frame(FL_ENGRAVED_FRAME,15,185,540,145,"");
  fdui->s2p_input = obj = fl_add_input(FL_NORMAL_INPUT,180,100,330,45,"Print command: ");
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
  obj = fl_add_text(FL_NORMAL_TEXT,25,360,130,30,"Paper type:");
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
  fdui->cancel_button = obj = fl_add_button(FL_NORMAL_BUTTON,145,480,115,45,"Cancel");
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
  fdui->print_button = obj = fl_add_button(FL_NORMAL_BUTTON,330,480,115,45,"Print");
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);

  fdui->paper_types = fl_bgn_group();
  fdui->A3 = obj = fl_add_checkbutton(FL_RADIO_BUTTON,255,350,45,45,"A3");
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
  fdui->Letter = obj = fl_add_checkbutton(FL_RADIO_BUTTON,340,350,45,45,"Letter");
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
  fdui->Legal = obj = fl_add_checkbutton(FL_RADIO_BUTTON,450,350,45,45,"Legal");
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
  fdui->A4 = obj = fl_add_checkbutton(FL_RADIO_BUTTON,170,350,45,45,"A4");
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
    fl_set_button(obj, 1);
  fl_end_group();


  fdui->modes = fl_bgn_group();
  fdui->s2p_button = obj = fl_add_checkbutton(FL_RADIO_BUTTON,25,30,45,45," Send to printer");
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
    fl_set_button(obj, 1);
  fdui->p2f_button = obj = fl_add_checkbutton(FL_RADIO_BUTTON,30,200,45,45," Print to file");
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
  fl_end_group();


  fdui->p2f_group = fl_bgn_group();
  fdui->p2f_input = obj = fl_add_input(FL_NORMAL_INPUT,140,260,290,45,"File name: ");
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
  fdui->p2f_browse = obj = fl_add_button(FL_NORMAL_BUTTON,445,260,95,45,"Browse...");
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
  fl_end_group();

  obj = fl_add_text(FL_NORMAL_TEXT,25,405,110,30,"Printer type:");
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);

  fdui->color_group = fl_bgn_group();
  fdui->bw_button = obj = fl_add_checkbutton(FL_RADIO_BUTTON,170,395,45,45,"Black & White");
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
  fdui->col_button = obj = fl_add_checkbutton(FL_RADIO_BUTTON,340,395,45,45,"Color");
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
    fl_set_button(obj, 1);
  fl_end_group();

  fl_end_form();

  fdui->print->fdui = fdui;

  return fdui;
}
/*---------------------------------------*/

FD_cut *create_form_cut(void)
{
  FL_OBJECT *obj;
  FD_cut *fdui = (FD_cut *) fl_calloc(1, sizeof(*fdui));

  fdui->cut = fl_bgn_form(FL_NO_BOX, 1030, 800);
  obj = fl_add_box(FL_UP_BOX,0,0,1030,800,"");
  obj = fl_add_button(FL_NORMAL_BUTTON,150,105,30,35,"");
    fl_set_button_shortcut(obj,"1",1);
    fl_set_object_boxtype(obj,FL_NO_BOX);
    fl_set_object_callback(obj,curve_button_callback,-1);
  fdui->cut_undo_button = obj = fl_add_pixmapbutton(FL_NORMAL_BUTTON,15,200,60,70,"Undo");
    fl_set_button_shortcut(obj,"U",1);
    fl_set_object_color(obj,FL_COL1,FL_BLACK);
    fl_set_object_lalign(obj,FL_ALIGN_BOTTOM|FL_ALIGN_INSIDE);
    fl_set_object_gravity(obj, FL_NorthWest, FL_NorthWest);
    fl_set_object_callback(obj,cut_undo_button_callback,0);
  fdui->cut_canvas = obj = fl_add_canvas(FL_NORMAL_CANVAS,245,10,615,715,"");
    fl_set_object_gravity(obj, FL_NorthWest, FL_SouthEast);
  fdui->cut_print_button = obj = fl_add_pixmapbutton(FL_NORMAL_BUTTON,15,300,60,70,"Print");
    fl_set_button_shortcut(obj,"P",1);
    fl_set_object_color(obj,FL_COL1,FL_BLACK);
    fl_set_object_lalign(obj,FL_ALIGN_BOTTOM|FL_ALIGN_INSIDE);
    fl_set_object_gravity(obj, FL_NorthWest, FL_NorthWest);
  fdui->cut_close_button = obj = fl_add_button(FL_NORMAL_BUTTON,15,740,115,45,"Close");
    fl_set_button_shortcut(obj,"C",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_SouthWest, FL_SouthWest);
    fl_set_object_callback(obj,cut_close_callback,0);
  fdui->cut_x_axis = obj = fl_add_canvas(FL_NORMAL_CANVAS,245,725,615,65,"");
    fl_set_object_gravity(obj, FL_SouthWest, FL_SouthEast);
    fl_set_object_resize(obj, FL_RESIZE_X);
  fdui->cut_full_scale_button = obj = fl_add_button(FL_PUSH_BUTTON,175,740,45,45,"FS");
    fl_set_button_shortcut(obj,"F",1);
    fl_set_object_color(obj,FL_TOP_BCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_gravity(obj, FL_SouthWest, FL_SouthWest);
    fl_set_object_callback(obj,cut_fs_button_callback,0);
    fl_set_button(obj, 1);
  fdui->cut_z_axis = obj = fl_add_canvas(FL_NORMAL_CANVAS,860,10,160,780,"");
    fl_set_object_gravity(obj, FL_NorthEast, FL_SouthEast);
    fl_set_object_resize(obj, FL_RESIZE_Y);
  obj = fl_add_button(FL_NORMAL_BUTTON,150,160,30,35,"");
    fl_set_button_shortcut(obj,"2",1);
    fl_set_object_boxtype(obj,FL_NO_BOX);
    fl_set_object_callback(obj,curve_button_callback,-2);
  obj = fl_add_button(FL_NORMAL_BUTTON,150,215,30,35,"");
    fl_set_button_shortcut(obj,"3",1);
    fl_set_object_boxtype(obj,FL_NO_BOX);
    fl_set_object_callback(obj,curve_button_callback,-3);
  obj = fl_add_button(FL_NORMAL_BUTTON,150,270,30,35,"");
    fl_set_button_shortcut(obj,"4",1);
    fl_set_object_boxtype(obj,FL_NO_BOX);
    fl_set_object_callback(obj,curve_button_callback,-4);
  fdui->cut_y_axis = obj = fl_add_canvas(FL_NORMAL_CANVAS,95,10,150,715,"");
    fl_set_object_gravity(obj, FL_NorthWest, FL_SouthWest);
    fl_set_object_resize(obj, FL_RESIZE_Y);
  fl_end_form();

  fdui->cut->fdui = fdui;

  return fdui;
}
/*---------------------------------------*/

