#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>

#include "api.h"
#include "window3D.h"
#include "HSC_interface.h"
#include "support_cb.h"
#include "interface.h"
#include "support.h"

GtkWidget *FileSelectionDialog=NULL;
GtkWidget *FontSelectionDialog=NULL;
GtkWidget *ColorSelectionDialog=NULL;

void variable_ctree_add_var(GtkCTree *ctree, gchar *name, v5d_var_info *vinfo)
{
  GtkCTreeNode *node;
  gchar *nstr[1];
  nstr[0] = name;
  node = gtk_ctree_insert_node(ctree,NULL,NULL,nstr,0,NULL,NULL,NULL,NULL,1,0);
  gtk_ctree_node_set_row_data(ctree,node,(gpointer) vinfo);
}

void
load_data_file  (GtkWidget *window3D, gchar *filename)
{
  gint dc;
  gint numvars, i;
  
  v5d_var_info *vinfo;
  v5d_info *info;

  GtkCTree *hs_ctree;
  
  
  /* todo: should check for errors here */
  info = (v5d_info *) lookup_widget(window3D,"v5d_info");

  dc = vis5d_load_v5dfile(info->v5d_display_context,0,filename,"context");

  if(dc==VIS5D_FAIL){
	 /* TODO: message dialog - open failed */
	 return;
  }

  vis5d_get_dtx_timestep(info->v5d_display_context  ,&info->timestep);
  /* returns Numtimes - lasttime is one less */
  vis5d_get_dtx_numtimes(info->v5d_display_context, &info->numtimes);

  glarea_draw(info->GtkGlArea,NULL,NULL);

  vis5d_get_ctx_numvars(dc,&numvars);
  {
	 float vertargs[MAXVERTARGS];
	 vis5d_get_dtx_vertical(info->v5d_display_context, &(info->vcs), vertargs);
  }
  /* create but do not show tools */
  info->HSliceControls = create_HSliceControls();
  /* point back to info */  
  gtk_object_set_data(GTK_OBJECT(info->HSliceControls),"v5d_info",(gpointer) info);
  hs_ctree = GTK_CTREE(lookup_widget(info->HSliceControls,"hslicectree"));

  /*
  VarDialog = create_VarDialog();
  */
  for(i=0;i < numvars; i++){
	 vinfo = (v5d_var_info *) g_malloc(sizeof(v5d_var_info));
	 
	 vinfo->hc = NULL;
	 vinfo->varid=i;
	 vinfo->v5d_data_context=dc;
	 vinfo->info = info;
	 vis5d_get_ctx_var_name(dc,i,vinfo->vname);
	 vinfo->maxlevel = vis5d_get_levels(dc, i);

	 variable_ctree_add_var(hs_ctree, vinfo->vname, vinfo);
	 
    /*
	 add_variable_toolbar(VarDialog , vinfo);
	 */
  }
  /*
  gtk_widget_show(VarDialog);
  */
}


void
on_fileselect_ok                       (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget *filesel, *window3D;
  gint what;
  gchar *filename;
  v5d_info *info;
  int hires;
  filesel = gtk_widget_get_toplevel (GTK_WIDGET (button));
  filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (filesel));

  what =  GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(filesel), "OpenWhat"));

  window3D = GTK_WIDGET(gtk_object_get_data(GTK_OBJECT(filesel), "window3D"));

  if(window3D==NULL){
	 fprintf(stderr,"Could not find window3D widget\n");
	 exit ;
  }
  info = (v5d_info *) lookup_widget(window3D,"v5d_info");
  switch(what){
  case DATA_FILE:
	 load_data_file(window3D,filename);  
	 break;
  case TOPO_FILE:
	 hires = vis5d_graphics_mode(info->v5d_display_context,VIS5D_HIRESTOPO,VIS5D_GET);
	 vis5d_init_topo(info->v5d_display_context,filename,hires);
	 vis5d_load_topo_and_map(info->v5d_display_context);
	 break;
  case MAP_FILE:
	 vis5d_init_map(info->v5d_display_context,filename);
	 vis5d_load_topo_and_map(info->v5d_display_context);
	 break;
  case PROCEDURE_FILE:
	 
  default:
	 g_print("open what ? %d\n",what);
  }

  gtk_widget_hide (filesel);
  /* This is the only window that should accept input */
  gtk_grab_remove(filesel);

}


void
on_fileselect_cancel                   (GtkButton       *button,
                                        gpointer         user_data)
{
  /* just hide the window instead of closing it */
  GtkWidget *filesel = gtk_widget_get_toplevel (GTK_WIDGET (button));
  gtk_widget_hide (filesel);
  gtk_grab_remove(filesel);
}



void
on_fontselectionbutton_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
  int whichbutton, whichfont;
  gchar *fontname;
  v5d_info *info;

  whichbutton = (int) user_data;

  printf("whichbutton = %d\n",whichbutton);

  if(whichbutton==0)/* OK */{
	 fontname = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG (FontSelectionDialog));

	 whichfont = (int) gtk_object_get_data(GTK_OBJECT(FontSelectionDialog),"Font");
	 info = (v5d_info *) gtk_object_get_data(GTK_OBJECT(FontSelectionDialog),"v5d_info");
  
	 vis5d_set_font(info->v5d_display_context,fontname,0,whichfont);
  }

  gtk_widget_hide(FontSelectionDialog);

}


void
on_ColorSelectionOk_clicked            (GtkButton       *button,
                                        gpointer         user_data)
{
  gtk_widget_hide(ColorSelectionDialog);
}


void
on_ColorSelectionCancel_clicked        (GtkButton       *button,
                                        gpointer         user_data)
{
  /* TODO: need to restore original color */
  gtk_widget_hide(ColorSelectionDialog);
}

GtkWidget *new_ColorSelectionDialog()
{
  if(! ColorSelectionDialog)
	 {
		GtkWidget *colorselection;
		ColorSelectionDialog = create_colorselectiondialog1();
		colorselection = GTK_COLOR_SELECTION_DIALOG(ColorSelectionDialog)->colorsel;
		gtk_color_selection_set_opacity(GTK_COLOR_SELECTION(colorselection),TRUE);
	 }
  gtk_widget_show(ColorSelectionDialog);
  return ColorSelectionDialog;
}