/* Form definition file generated with fdesign. */

#include "forms.h"
#include <stdlib.h>
#include "fsc2_rsc.h"

FD_fsc2 *create_form_fsc2(void)
{
  FL_OBJECT *obj;
  FD_fsc2 *fdui = (FD_fsc2 *) fl_calloc(1, sizeof(*fdui));

  fdui->fsc2 = fl_bgn_form(FL_NO_BOX, 1030, 800);
  obj = fl_add_box(FL_UP_BOX,0,0,1030,800,"");
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_object_lstyle(obj,FL_FIXED_STYLE);
  fdui->browser = obj = fl_add_browser(FL_NORMAL_BROWSER,230,10,790,580,"");
    fl_set_object_color(obj,FL_WHITE,FL_YELLOW);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
  fdui->Load = obj = fl_add_button(FL_NORMAL_BUTTON,50,60,130,50,"Load");
    fl_set_button_shortcut(obj,"L",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_callback(obj,load_file,0);
  fdui->reload = obj = fl_add_button(FL_NORMAL_BUTTON,50,150,130,50,"Reload");
    fl_set_button_shortcut(obj,"R",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_callback(obj,load_file,1);
  fdui->test_file = obj = fl_add_button(FL_NORMAL_BUTTON,50,440,130,50,"Test");
    fl_set_button_shortcut(obj,"T",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_callback(obj,test_file,0);
  fdui->run = obj = fl_add_button(FL_NORMAL_BUTTON,50,520,130,50,"Start");
    fl_set_button_shortcut(obj,"S",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_callback(obj,run_file,0);
  fdui->quit = obj = fl_add_button(FL_NORMAL_BUTTON,50,700,130,50,"Quit");
    fl_set_button_shortcut(obj,"Q",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
  fdui->error_browser = obj = fl_add_browser(FL_NORMAL_BROWSER,230,600,790,190,"");
    fl_set_object_color(obj,FL_WHITE,FL_YELLOW);
    fl_set_object_lsize(obj,FL_MEDIUM_SIZE);
  fdui->Edit = obj = fl_add_button(FL_NORMAL_BUTTON,50,240,130,50,"Edit");
    fl_set_button_shortcut(obj,"E",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_callback(obj,edit_file,0);
  fdui->device_button = obj = fl_add_button(FL_NORMAL_BUTTON,50,340,130,50,"Devices");
    fl_set_button_shortcut(obj,"D",1);
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_callback(obj,device_select,0);
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
  fdui->stop = obj = fl_add_button(FL_NORMAL_BUTTON,40,700,130,50,"Stop");
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_callback(obj,stop_measurement,0);
  fdui->save = obj = fl_add_button(FL_NORMAL_BUTTON,40,590,130,50,"Save");
    fl_set_object_color(obj,FL_MCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_callback(obj,save_data,0);
  fdui->redraw_dummy = obj = fl_add_button(FL_NORMAL_BUTTON,510,330,30,30,"");
    fl_set_object_boxtype(obj,FL_NO_BOX);
    fl_set_object_callback(obj,new_data_callback,0);
  fdui->xy_plot = obj = fl_add_xyplot(FL_NORMAL_XYPLOT,220,20,790,760,"");
    fl_set_object_color(obj,FL_WHITE,FL_BLACK);
    fl_set_object_lsize(obj,FL_NORMAL_SIZE);
    fl_set_xyplot_xtics(obj, 10, 2);
    fl_set_xyplot_ytics(obj, 10, 2);
  fdui->sigchld = obj = fl_add_button(FL_NORMAL_BUTTON,260,270,10,10,"");
    fl_set_object_boxtype(obj,FL_NO_BOX);
    fl_set_object_callback(obj,run_sigchld_callback,0);
  fl_end_form();

  fdui->run->fdui = fdui;

  return fdui;
}
/*---------------------------------------*/

FD_device *create_form_device(void)
{
  FL_OBJECT *obj;
  FD_device *fdui = (FD_device *) fl_calloc(1, sizeof(*fdui));

  fdui->device = fl_bgn_form(FL_NO_BOX, 830, 540);
  obj = fl_add_box(FL_UP_BOX,0,0,830,540,"");
  obj = fl_add_frame(FL_ENGRAVED_FRAME,280,90,220,180,"");

  fdui->pulser_group = fl_bgn_group();
  fdui->dg2020_button = obj = fl_add_round3dbutton(FL_RADIO_BUTTON,310,150,50,50,"  DG2020");
    fl_set_object_color(obj,FL_LEFT_BCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_button(obj, 1);
  fl_end_group();

  obj = fl_add_frame(FL_ENGRAVED_FRAME,30,90,220,180,"");
  obj = fl_add_text(FL_NORMAL_TEXT,280,20,280,50,"Device Select");
    fl_set_object_lsize(obj,FL_HUGE_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    fl_set_object_lstyle(obj,FL_NORMAL_STYLE+FL_SHADOW_STYLE);
  obj = fl_add_text(FL_NORMAL_TEXT,60,110,170,40,"Digitizer");
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    fl_set_object_lstyle(obj,FL_NORMAL_STYLE+FL_SHADOW_STYLE);
  obj = fl_add_text(FL_NORMAL_TEXT,310,110,160,40,"Pulser");
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    fl_set_object_lstyle(obj,FL_NORMAL_STYLE+FL_SHADOW_STYLE);
  obj = fl_add_frame(FL_ENGRAVED_FRAME,530,90,220,180,"");
  obj = fl_add_text(FL_NORMAL_TEXT,560,110,160,40,"Lock-In");
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    fl_set_object_lstyle(obj,FL_NORMAL_STYLE+FL_SHADOW_STYLE);
  obj = fl_add_frame(FL_ENGRAVED_FRAME,30,300,220,180,"");
  obj = fl_add_text(FL_NORMAL_TEXT,60,320,160,40,"Magnet");
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    fl_set_object_lstyle(obj,FL_NORMAL_STYLE+FL_SHADOW_STYLE);
  obj = fl_add_frame(FL_ENGRAVED_FRAME,280,300,220,180,"");
  obj = fl_add_text(FL_NORMAL_TEXT,310,320,160,40,"Boxcar");
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    fl_set_object_lstyle(obj,FL_NORMAL_STYLE+FL_SHADOW_STYLE);
  fdui->device_select_ok = obj = fl_add_button(FL_NORMAL_BUTTON,590,410,130,50,"Accept");
    fl_set_object_color(obj,FL_TOP_BCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_callback(obj,device_select,1);
  fdui->device_select_cancel = obj = fl_add_button(FL_NORMAL_BUTTON,590,320,130,50,"Cancel");
    fl_set_object_color(obj,FL_TOP_BCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_object_callback(obj,device_select,0);

  fdui->digitizer_group = fl_bgn_group();
  fdui->tds_754a_button = obj = fl_add_round3dbutton(FL_RADIO_BUTTON,60,190,50,50,"  TDS754A");
    fl_set_object_color(obj,FL_TOP_BCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_button(obj, 1);
  fdui->tds_520c_button = obj = fl_add_round3dbutton(FL_RADIO_BUTTON,60,150,50,50,"  TDS520C");
    fl_set_object_color(obj,FL_TOP_BCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
  fl_end_group();


  fdui->lockin_group = fl_bgn_group();
  fdui->sr510_button = obj = fl_add_round3dbutton(FL_RADIO_BUTTON,560,150,50,50,"  SR510");
    fl_set_object_color(obj,FL_TOP_BCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_button(obj, 1);
  fl_end_group();


  fdui->boxcar_group = fl_bgn_group();
  fdui->stanford_button = obj = fl_add_round3dbutton(FL_RADIO_BUTTON,310,360,50,50,"  Stanford");
    fl_set_object_color(obj,FL_TOP_BCOL,FL_GREEN);
    fl_set_object_lsize(obj,FL_LARGE_SIZE);
    fl_set_button(obj, 1);
  fl_end_group();

  fl_end_form();

  fdui->device->fdui = fdui;

  return fdui;
}
/*---------------------------------------*/

