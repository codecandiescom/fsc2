/* Form definition file generated with fdesign. */

#include "forms.h"
#include <stdlib.h>
#include "fsc2_rsc_lr.h"

FD_fsc2 *create_form_fsc2(void)
{
  FL_OBJECT *obj;
  FD_fsc2 *fdui = (FD_fsc2 *) fl_calloc(1, sizeof(*fdui));

  fdui->fsc2 = fl_bgn_form(FL_NO_BOX, 620, 450);
  obj = fl_add_box(FL_UP_BOX,0,0,620,450,"");
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_lstyle(obj,FL_FIXED_STYLE);
  fdui->browser = obj = fl_add_browser(FL_NORMAL_BROWSER,115,5,490,360,"");
    fl_set_object_color(obj,FL_WHITE,FL_YELLOW);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_lstyle(obj,FL_FIXED_STYLE);
    fl_set_object_gravity(obj, FL_NorthWest, FL_East);
  fdui->Load = obj = fl_add_button(FL_NORMAL_BUTTON,10,20,90,35,"Load");
    fl_set_button_shortcut(obj,"L",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,load_file,0);
  fdui->reload = obj = fl_add_button(FL_NORMAL_BUTTON,10,70,90,35,"Reload");
    fl_set_button_shortcut(obj,"R",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,load_file,1);
  fdui->test_file = obj = fl_add_button(FL_NORMAL_BUTTON,10,185,90,35,"Test");
    fl_set_button_shortcut(obj,"T",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,test_file,0);
  fdui->run = obj = fl_add_button(FL_NORMAL_BUTTON,10,235,90,35,"Start");
    fl_set_button_shortcut(obj,"S",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,run_file,0);
  fdui->quit = obj = fl_add_button(FL_NORMAL_BUTTON,10,295,90,35,"Quit");
    fl_set_button_shortcut(obj,"Q",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
  fdui->error_browser = obj = fl_add_browser(FL_NORMAL_BROWSER,115,370,490,75,"");
    fl_set_object_color(obj,FL_WHITE,FL_YELLOW);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_lstyle(obj,FL_FIXED_STYLE);
    fl_set_object_gravity(obj, FL_West, FL_SouthEast);
  fdui->Edit = obj = fl_add_button(FL_NORMAL_BUTTON,10,120,90,35,"Edit");
    fl_set_button_shortcut(obj,"E",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,edit_file,0);
  fdui->win_slider = obj = fl_add_slider(FL_VERT_BROWSER_SLIDER,605,5,15,440,"");
    fl_set_object_gravity(obj, FL_NorthEast, FL_SouthEast);
    fl_set_object_resize(obj, FL_RESIZE_Y);
    fl_set_object_callback(obj,win_slider_callback,0);
    fl_set_slider_value(obj, 0.172727);
    fl_set_slider_size(obj, 0.05);
     fl_set_slider_return(obj, FL_RETURN_END_CHANGED);
  fdui->bug_report = obj = fl_add_button(FL_NORMAL_BUTTON,10,400,90,35,"Bug Report");
    fl_set_button_shortcut(obj,"B",1);
    fl_set_object_color(obj,FL_MCOL,FL_COL1);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_gravity(obj, FL_West, FL_West);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,bug_report_callback,0);
  fdui->help = obj = fl_add_button(FL_NORMAL_BUTTON,10,355,90,35,"Help");
    fl_set_button_shortcut(obj,"H",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
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

  fdui->run = fl_bgn_form(FL_NO_BOX, 620, 450);
  obj = fl_add_box(FL_UP_BOX,0,0,620,450,"");
  fdui->z_axis = obj = fl_add_canvas(FL_NORMAL_CANVAS,515,5,100,440,"");
    fl_set_object_gravity(obj, FL_NorthEast, FL_SouthEast);
    fl_set_object_resize(obj, FL_RESIZE_Y);
  fdui->stop = obj = fl_add_button(FL_NORMAL_BUTTON,10,405,70,35,"Stop");
    fl_set_button_shortcut(obj,"S",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_gravity(obj, FL_SouthWest, FL_SouthWest);
    fl_set_object_callback(obj,stop_measurement,0);
  fdui->sigchld = obj = fl_add_button(FL_NORMAL_BUTTON,16,741,1,1,"");
    fl_set_object_boxtype(obj,FL_NO_BOX);
    fl_set_object_gravity(obj, FL_SouthWest, FL_SouthWest);
    fl_set_object_callback(obj,run_sigchld_callback,0);
  fdui->y_axis = obj = fl_add_canvas(FL_NORMAL_CANVAS,65,5,95,390,"");
    fl_set_object_gravity(obj, FL_NorthWest, FL_SouthWest);
    fl_set_object_resize(obj, FL_RESIZE_Y);
  fdui->full_scale_button = obj = fl_add_button(FL_PUSH_BUTTON,115,405,35,35,"FS");
    fl_set_button_shortcut(obj,"F",1);
    fl_set_object_color(obj,FL_TOP_BCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_gravity(obj, FL_SouthWest, FL_SouthWest);
    fl_set_object_callback(obj,fs_button_callback,0);
    fl_set_button(obj, 1);
  fdui->curve_1_button = obj = fl_add_button(FL_PUSH_BUTTON,20,20,25,25,"1");
    fl_set_button_shortcut(obj,"1",1);
    fl_set_object_color(obj,FL_MCOL,FL_TOMATO);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_gravity(obj, FL_NorthWest, FL_NorthWest);
    fl_set_object_callback(obj,curve_button_callback,1);
    fl_set_button(obj, 1);
  fdui->curve_2_button = obj = fl_add_button(FL_PUSH_BUTTON,20,55,25,25,"2");
    fl_set_button_shortcut(obj,"2",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_gravity(obj, FL_NorthWest, FL_NorthWest);
    fl_set_object_callback(obj,curve_button_callback,2);
    fl_set_button(obj, 1);
  fdui->curve_3_button = obj = fl_add_button(FL_PUSH_BUTTON,20,90,25,25,"3");
    fl_set_button_shortcut(obj,"3",1);
    fl_set_object_color(obj,FL_MCOL,FL_YELLOW);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_gravity(obj, FL_NorthWest, FL_NorthWest);
    fl_set_object_callback(obj,curve_button_callback,3);
    fl_set_button(obj, 1);
  fdui->curve_4_button = obj = fl_add_button(FL_PUSH_BUTTON,20,125,25,25,"4");
    fl_set_button_shortcut(obj,"4",1);
    fl_set_object_color(obj,FL_MCOL,FL_CYAN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_gravity(obj, FL_NorthWest, FL_NorthWest);
    fl_set_object_callback(obj,curve_button_callback,4);
    fl_set_button(obj, 1);
  fdui->undo_button = obj = fl_add_pixmapbutton(FL_NORMAL_BUTTON,10,190,45,55,"Undo");
    fl_set_button_shortcut(obj,"U",1);
    fl_set_object_color(obj,FL_COL1,FL_BLACK);
    fl_set_object_lalign(obj,FL_ALIGN_BOTTOM|FL_ALIGN_INSIDE);
    fl_set_object_gravity(obj, FL_NorthWest, FL_NorthWest);
    fl_set_object_callback(obj,undo_button_callback,0);
  fdui->canvas = obj = fl_add_canvas(FL_NORMAL_CANVAS,160,5,455,390,"");
    fl_set_object_gravity(obj, FL_NorthWest, FL_SouthEast);
  fdui->x_axis = obj = fl_add_canvas(FL_NORMAL_CANVAS,160,395,455,50,"");
    fl_set_object_gravity(obj, FL_SouthWest, FL_SouthEast);
    fl_set_object_resize(obj, FL_RESIZE_X);
  fdui->print_button = obj = fl_add_pixmapbutton(FL_NORMAL_BUTTON,10,275,45,55,"Print");
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

  fdui->input_form = fl_bgn_form(FL_NO_BOX, 365, 270);
  obj = fl_add_box(FL_UP_BOX,0,0,365,270,"");
  fdui->comm_input = obj = fl_add_input(FL_MULTILINE_INPUT,10,30,345,205,"Enter your comments:");
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_TOP_LEFT);
    fl_set_object_gravity(obj, FL_NorthWest, FL_SouthEast);
  fdui->comm_done = obj = fl_add_button(FL_NORMAL_BUTTON,140,240,95,25,"Done");
    fl_set_object_color(obj,FL_TOP_BCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
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

  fdui->print = fl_bgn_form(FL_NO_BOX, 440, 340);
  obj = fl_add_box(FL_UP_BOX,0,0,440,340,"");
  obj = fl_add_frame(FL_ENGRAVED_FRAME,10,110,420,90,"");
  obj = fl_add_frame(FL_ENGRAVED_FRAME,10,10,420,95,"");
  fdui->s2p_input = obj = fl_add_input(FL_NORMAL_INPUT,125,55,215,35,"Print command: ");
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
  obj = fl_add_text(FL_NORMAL_TEXT,15,210,90,30,"Paper type:");
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
  fdui->cancel_button = obj = fl_add_button(FL_NORMAL_BUTTON,95,295,85,30,"Cancel");
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
  fdui->print_button = obj = fl_add_button(FL_NORMAL_BUTTON,245,295,80,30,"Print");
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);

  fdui->modes = fl_bgn_group();
  fdui->s2p_button = obj = fl_add_checkbutton(FL_RADIO_BUTTON,25,15,35,35," Send to printer");
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
    fl_set_button(obj, 1);
  fdui->p2f_button = obj = fl_add_checkbutton(FL_RADIO_BUTTON,25,115,35,35," Print to file");
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
  fl_end_group();


  fdui->p2f_group = fl_bgn_group();
  fdui->p2f_input = obj = fl_add_input(FL_NORMAL_INPUT,125,150,215,35,"File name: ");
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
  fdui->p2f_browse = obj = fl_add_button(FL_NORMAL_BUTTON,355,150,65,35,"Browse...");
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
  fl_end_group();

  obj = fl_add_text(FL_NORMAL_TEXT,15,260,85,30,"Printer type:");
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);

  fdui->color_group = fl_bgn_group();
  fdui->bw_button = obj = fl_add_checkbutton(FL_RADIO_BUTTON,125,260,35,35,"Black & White");
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
  fdui->col_button = obj = fl_add_checkbutton(FL_RADIO_BUTTON,260,260,35,35,"Color");
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
    fl_set_button(obj, 1);
  fl_end_group();


  fdui->paper_types = fl_bgn_group();
  fdui->A3 = obj = fl_add_checkbutton(FL_RADIO_BUTTON,190,210,35,35,"A3");
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
  fdui->Letter = obj = fl_add_checkbutton(FL_RADIO_BUTTON,190,235,35,35,"Letter");
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
  fdui->Legal = obj = fl_add_checkbutton(FL_RADIO_BUTTON,260,235,35,35,"Legal");
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
  fdui->A4 = obj = fl_add_checkbutton(FL_RADIO_BUTTON,125,210,35,35,"A4");
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
    fl_set_button(obj, 1);
  fdui->A5 = obj = fl_add_checkbutton(FL_RADIO_BUTTON,260,210,35,35,"A5");
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
  fdui->A6 = obj = fl_add_checkbutton(FL_RADIO_BUTTON,125,235,35,35,"A6");
    fl_set_object_color(obj,FL_COL1,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_resize(obj, FL_RESIZE_NONE);
    fl_set_object_callback(obj,print_callback,0);
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

  fdui->cut = fl_bgn_form(FL_NO_BOX, 620, 450);
  obj = fl_add_box(FL_UP_BOX,0,0,620,450,"");
  fdui->cut_canvas = obj = fl_add_canvas(FL_NORMAL_CANVAS,160,5,350,390,"");
    fl_set_object_gravity(obj, FL_NorthWest, FL_SouthEast);
  fdui->cut_undo_button = obj = fl_add_pixmapbutton(FL_NORMAL_BUTTON,10,190,45,55,"Undo");
    fl_set_button_shortcut(obj,"U",1);
    fl_set_object_color(obj,FL_COL1,FL_BLACK);
    fl_set_object_lalign(obj,FL_ALIGN_BOTTOM|FL_ALIGN_INSIDE);
    fl_set_object_gravity(obj, FL_NorthWest, FL_NorthWest);
    fl_set_object_callback(obj,cut_undo_button_callback,0);
  fdui->cut_print_button = obj = fl_add_pixmapbutton(FL_NORMAL_BUTTON,10,275,45,55,"Print");
    fl_set_button_shortcut(obj,"P",1);
    fl_set_object_color(obj,FL_COL1,FL_BLACK);
    fl_set_object_lalign(obj,FL_ALIGN_BOTTOM|FL_ALIGN_INSIDE);
    fl_set_object_gravity(obj, FL_NorthWest, FL_NorthWest);
  fdui->cut_close_button = obj = fl_add_button(FL_NORMAL_BUTTON,10,405,70,35,"Close");
    fl_set_button_shortcut(obj,"C",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_gravity(obj, FL_SouthWest, FL_SouthWest);
    fl_set_object_callback(obj,cut_close_callback,0);
  fdui->cut_full_scale_button = obj = fl_add_button(FL_PUSH_BUTTON,115,405,35,35,"FS");
    fl_set_button_shortcut(obj,"F",1);
    fl_set_object_color(obj,FL_TOP_BCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_gravity(obj, FL_SouthWest, FL_SouthWest);
    fl_set_object_callback(obj,cut_fs_button_callback,0);
    fl_set_button(obj, 1);
  fdui->cut_x_axis = obj = fl_add_canvas(FL_NORMAL_CANVAS,160,395,350,50,"");
    fl_set_object_gravity(obj, FL_SouthWest, FL_SouthEast);
    fl_set_object_resize(obj, FL_RESIZE_X);
  fdui->cut_z_axis = obj = fl_add_canvas(FL_NORMAL_CANVAS,515,5,100,440,"");
    fl_set_object_gravity(obj, FL_NorthEast, FL_SouthEast);
    fl_set_object_resize(obj, FL_RESIZE_Y);
  obj = fl_add_button(FL_NORMAL_BUTTON,105,40,20,20,"");
    fl_set_button_shortcut(obj,"1",1);
    fl_set_object_boxtype(obj,FL_NO_BOX);
    fl_set_object_callback(obj,curve_button_callback,-1);
  obj = fl_add_button(FL_NORMAL_BUTTON,105,75,20,20,"");
    fl_set_button_shortcut(obj,"2",1);
    fl_set_object_boxtype(obj,FL_NO_BOX);
    fl_set_object_callback(obj,curve_button_callback,-2);
  obj = fl_add_button(FL_NORMAL_BUTTON,105,110,20,20,"");
    fl_set_button_shortcut(obj,"3",1);
    fl_set_object_boxtype(obj,FL_NO_BOX);
    fl_set_object_callback(obj,curve_button_callback,-3);
  obj = fl_add_button(FL_NORMAL_BUTTON,105,145,20,20,"");
    fl_set_button_shortcut(obj,"4",1);
    fl_set_object_boxtype(obj,FL_NO_BOX);
    fl_set_object_callback(obj,curve_button_callback,-4);
  fdui->cut_y_axis = obj = fl_add_canvas(FL_NORMAL_CANVAS,65,5,95,390,"");
    fl_set_object_gravity(obj, FL_NorthWest, FL_SouthWest);
    fl_set_object_resize(obj, FL_RESIZE_Y);
  fdui->up_button = obj = fl_add_button(FL_NORMAL_BUTTON,110,15,20,20,"");
    fl_set_button_shortcut(obj,"&A",1);
    fl_set_object_callback(obj,cut_next_index,0);
  fdui->down_button = obj = fl_add_button(FL_NORMAL_BUTTON,110,45,20,20,"");
    fl_set_button_shortcut(obj,"&B",1);
    fl_set_object_callback(obj,cut_next_index,1);
  fl_end_form();

  fdui->cut->fdui = fdui;

  return fdui;
}
/*---------------------------------------*/

