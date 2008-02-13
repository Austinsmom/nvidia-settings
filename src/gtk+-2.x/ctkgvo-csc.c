/*
 * nvidia-settings: A tool for configuring the NVIDIA X driver on Unix
 * and Linux systems.
 *
 * Copyright (C) 2004 NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of Version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See Version 2
 * of the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the:
 *
 *           Free Software Foundation, Inc.
 *           59 Temple Place - Suite 330
 *           Boston, MA 02111-1307, USA
 *
 */

#include <gtk/gtk.h>
#include <NvCtrlAttributes.h>

#include <string.h>

#include "ctkimage.h"

#include "ctkgvo-csc.h"

#include "ctkconfig.h"
#include "ctkhelp.h"

#include "ctkdropdownmenu.h"

/*
 * The CtkGvoCsc widget is used to provide a way for configuring
 * custom Color Space Conversion Matrices, Offsets, and Scale Factors
 * on NVIDIA SDI products.  At the top, we have a checkbox that
 * enables overriding the default CSC matrix.  If that checkbox is not
 * checked, then everything else on the page is insensitive.
 *
 * When the "override" checkbox is checked, then the user can modify
 * each of the 15 floating point values that comprise the 3x3 matrix,
 * 1x3 offset vector, and 1x3 scale vector.
 *
 * The user can also select from an "Initialization" dropdown menu, to
 * initialize the CSC with any of "ITU-601", "ITU-709", "ITU-177", or
 * "Identity".
 *
 * Finally, the user can select how they want changes to be applied:
 * by default, they have to click the "Apply" button to flush their
 * changes from nvidia-settings out over NV-CONTROL to the NVIDIA
 * driver.  Alternatively, the user can check the "Apply Changes
 * Immediately" checkbox, which will cause changes to be sent to the
 * NVIDIA driver whenever the user makes any changes to the CSC.  This
 * is handy to tweak values in "realtime" while SDI output is enabled.
 *
 * Note that on older NVIDIA SDI products, changes to CSC require
 * stopping and restarting SDI output.  Furthermore, on older NVIDIA
 * SDI products, CSC only applies to OpenGL SDI output.  On newer
 * NVIDIA SDI products, the CSC can be applied in real time while CSC
 * is enabled, and can apply both to OpenGL and normal X desktop over
 * SDI.
 */

/*
 * TODO: ability to save/restore CSC to/from file.
 */


/* local prototypes */

static void override_button_toggled         (GtkToggleButton *overrideButton,
                                             gpointer user_data);

static void override_state_toggled          (CtkGvoCsc *data,
                                             gboolean enabled);

static void make_entry                      (CtkGvoCsc *ctk_gvo_csc,
                                             GtkWidget *table,
                                             GtkWidget **widget,
                                             float value,
                                             int row,
                                             int column);

static void make_label                      (CtkGvoCsc *ctk_gvo_csc,
                                             GtkWidget *table,
                                             const char *str,
                                             int row,
                                             int column);

static void spin_button_value_changed       (GtkWidget *button,
                                             gpointer user_data);

static void apply_immediate_button_toggled  (GtkToggleButton
                                             *applyImmediateButton,
                                             gpointer user_data);

static void apply_button_clicked            (GtkButton *button,
                                             gpointer user_data);

static void initialize_csc_dropdown_changed (CtkDropDownMenu *combo,
                                             gpointer user_data);

static void set_apply_button_senstive       (CtkGvoCsc *ctk_gvo_csc);

static void apply_csc_values                (CtkGvoCsc *ctk_gvo_csc);

static GtkWidget *build_opengl_only_msg     (void);

/*
 * Color Space Conversion Standards
 */

#define CSC_STANDARD_ITU_601  0
#define CSC_STANDARD_ITU_709  1
#define CSC_STANDARD_ITU_177  2
#define CSC_STANDARD_IDENTITY 3

#define CSC_STANDARD_ITU_601_STRING  "ITU-601"
#define CSC_STANDARD_ITU_709_STRING  "ITU-709"
#define CSC_STANDARD_ITU_177_STRING  "ITU-177"
#define CSC_STANDARD_IDENTITY_STRING "Identity"



#define FRAME_BORDER 5



GType ctk_gvo_csc_get_type(void)
{
    static GType ctk_gvo_csc_type = 0;
    
    if (!ctk_gvo_csc_type) {
        static const GTypeInfo ctk_gvo_csc_info = {
            sizeof (CtkGvoCscClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            NULL, /* class_init, */
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof (CtkGvoCsc),
            0, /* n_preallocs */
            NULL, /* instance_init */
        };

        ctk_gvo_csc_type =
            g_type_register_static (GTK_TYPE_VBOX,
                                    "CtkGvoCsc", &ctk_gvo_csc_info, 0);
    }
    
    return ctk_gvo_csc_type;
}


/*
 * ctk_gvo_csc_new() - create a CtkGvoCsc widget
 */

GtkWidget* ctk_gvo_csc_new(NvCtrlAttributeHandle *handle,
                           CtkConfig *ctk_config,
                           CtkEvent *ctk_event,
                           CtkGvo *gvo_parent)
{
    GObject *object;
    CtkGvoCsc *ctk_gvo_csc;
    GtkWidget *frame;
    GtkWidget *hbox, *hbox2;
    GtkWidget *vbox, *vbox2;
    GtkWidget *label;
    GtkWidget *alignment;

    ReturnStatus ret;    

    int row, column, override, caps;
    
    float initialCSCMatrix[3][3];
    float initialCSCOffset[3];
    float initialCSCScale[3];
    
    /* retrieve all the NV-CONTROL attributes that we will need */
    
    ret = NvCtrlGetGvoColorConversion(handle,
                                      initialCSCMatrix,
                                      initialCSCOffset,
                                      initialCSCScale);
    
    if (ret != NvCtrlSuccess) return NULL;
    
    ret = NvCtrlGetAttribute(handle, NV_CTRL_GVO_OVERRIDE_HW_CSC, &override);

    if (ret != NvCtrlSuccess) return NULL;

    ret = NvCtrlGetAttribute(handle, NV_CTRL_GVO_CAPABILITIES, &caps);

    if (ret != NvCtrlSuccess) return NULL;


    /*
     * XXX setup to receive events when another NV-CONTROL client
     * changes any of the above attributes
     */
    
    
    /* create the object */
    
    object = g_object_new(CTK_TYPE_GVO_CSC, NULL);
    
    ctk_gvo_csc = CTK_GVO_CSC(object);
    ctk_gvo_csc->handle = handle;
    ctk_gvo_csc->ctk_config = ctk_config;
    ctk_gvo_csc->gvo_parent = gvo_parent;
    
    gtk_box_set_spacing(GTK_BOX(object), 10);
    
    /* banner */
    
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(object), hbox, FALSE, FALSE, 0);

    ctk_gvo_csc->banner_box = hbox;
    
    /* checkbox to enable override of HW CSC */
    
    ctk_gvo_csc->overrideButton = gtk_check_button_new_with_label
        ("Override default Color Space Conversion");
    
    g_signal_connect(GTK_OBJECT(ctk_gvo_csc->overrideButton), "toggled",
                     G_CALLBACK(override_button_toggled), ctk_gvo_csc);
    
    frame = gtk_frame_new(NULL);
    gtk_container_set_border_width(GTK_CONTAINER(frame), FRAME_BORDER);
    
    vbox2 = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox2), 5);
    
    gtk_box_pack_start(GTK_BOX(vbox2),
                       ctk_gvo_csc->overrideButton,
                       FALSE,                   // expand
                       FALSE,                   // fill
                       0);                      // padding
    
    gtk_container_add(GTK_CONTAINER(frame), vbox2);
    
    gtk_box_pack_start(GTK_BOX(ctk_gvo_csc),
                       frame,
                       FALSE,                   // expand
                       FALSE,                   // fill
                       0);                      // padding
    
    /* create an hbox to store everything else */
    
    hbox = gtk_vbox_new(FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(ctk_gvo_csc),
                       hbox,
                       FALSE,                   // expand
                       FALSE,                   // fill
                       0);                      // padding
    
    vbox = gtk_vbox_new(FALSE, 0);

    gtk_box_pack_start(GTK_BOX(hbox),
                       vbox,
                       FALSE,                   // expand
                       FALSE,                   // fill
                       0);                      // padding
    
    ctk_gvo_csc->cscOptions = hbox;
    
    /* create a drop-down menu for the possible initializing values */

    frame = gtk_frame_new(NULL);
    gtk_container_set_border_width(GTK_CONTAINER(frame), FRAME_BORDER);
    
    vbox2 = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox2), 5);

    gtk_container_add(GTK_CONTAINER(frame), vbox2);
    
    hbox = gtk_hbox_new(FALSE,                  // homogeneous
                        10);                    // spacing
    
    gtk_box_pack_start(GTK_BOX(vbox2),
                       hbox,
                       FALSE,                   // expand
                       FALSE,                   // fill
                       0);                      // padding    
    
    label = gtk_label_new("Initialize Color Space Conversion with:");

    gtk_box_pack_start(GTK_BOX(hbox),
                       label,
                       FALSE,                   // expand
                       FALSE,                   // fill
                       5);                      // padding    
    
    
    ctk_gvo_csc->initializeDropDown =
        ctk_drop_down_menu_new(CTK_DROP_DOWN_MENU_FLAG_MONOSPACE);
    
    ctk_drop_down_menu_append_item
        (CTK_DROP_DOWN_MENU(ctk_gvo_csc->initializeDropDown),
         CSC_STANDARD_ITU_601_STRING,
         CSC_STANDARD_ITU_601);
    
    ctk_drop_down_menu_append_item
        (CTK_DROP_DOWN_MENU(ctk_gvo_csc->initializeDropDown),
         CSC_STANDARD_ITU_709_STRING,
         CSC_STANDARD_ITU_709);
                              
    ctk_drop_down_menu_append_item
        (CTK_DROP_DOWN_MENU(ctk_gvo_csc->initializeDropDown),
         CSC_STANDARD_ITU_177_STRING,
         CSC_STANDARD_ITU_177);
        
    ctk_drop_down_menu_append_item
        (CTK_DROP_DOWN_MENU(ctk_gvo_csc->initializeDropDown),
         CSC_STANDARD_IDENTITY_STRING,
         CSC_STANDARD_IDENTITY);
    
    ctk_drop_down_menu_finalize
        (CTK_DROP_DOWN_MENU(ctk_gvo_csc->initializeDropDown));
    
    gtk_box_pack_start(GTK_BOX(hbox),
                       ctk_gvo_csc->initializeDropDown,
                       FALSE,                   // expand
                       FALSE,                   // fill
                       5);                      // padding    
    
    
    g_signal_connect(G_OBJECT(ctk_gvo_csc->initializeDropDown), "changed",
                     G_CALLBACK(initialize_csc_dropdown_changed),
                     (gpointer) ctk_gvo_csc);


    gtk_box_pack_start(GTK_BOX(vbox),
                       frame,
                       FALSE,                   // expand
                       FALSE,                   // fill
                       0);                      // padding    

    
    /* create an hbox to store the CSC matrix, offset, and scale */
    
    hbox = gtk_hbox_new(FALSE,                  // homogeneous
                        10);                    // spacing
    
    gtk_box_pack_start(GTK_BOX(vbox),
                       hbox,
                       FALSE,                   // expand
                       FALSE,                   // fill
                       0);                      // padding
    

    /* create the CSC matrix */
    
    frame = gtk_frame_new(NULL);
    
    gtk_container_set_border_width(GTK_CONTAINER(frame), FRAME_BORDER);
    
    gtk_box_pack_start(GTK_BOX(hbox),
                       frame,
                       FALSE,                   // expand
                       FALSE,                   // fill
                       0);                      // padding
    
    ctk_gvo_csc->matrixTable =
        gtk_table_new(4,                        // rows
                      4,                        // columns
                      FALSE);                   // homogeneous
    
    gtk_container_add(GTK_CONTAINER(frame), ctk_gvo_csc->matrixTable);

    /* add labels to the matrix table */
    
    make_label(ctk_gvo_csc, ctk_gvo_csc->matrixTable, "Y" , 1, 0);
    make_label(ctk_gvo_csc, ctk_gvo_csc->matrixTable, "Cr", 2, 0);
    make_label(ctk_gvo_csc, ctk_gvo_csc->matrixTable, "Cb", 3, 0);

    make_label(ctk_gvo_csc, ctk_gvo_csc->matrixTable, "Red"  , 0, 1);
    make_label(ctk_gvo_csc, ctk_gvo_csc->matrixTable, "Green", 0, 2);
    make_label(ctk_gvo_csc, ctk_gvo_csc->matrixTable, "Blue" , 0, 3);
    
    /* create the 3x3 matrix */
    
    for (row = 0; row < 3; row++) {
        for (column = 0; column < 3; column++) {

            ctk_gvo_csc->matrix[row][column] = initialCSCMatrix[row][column];

            make_entry(ctk_gvo_csc,
                       ctk_gvo_csc->matrixTable,
                       &ctk_gvo_csc->matrixWidget[row][column],
                       ctk_gvo_csc->matrix[row][column],
                       row + 1,
                       column+1);
        }
    }
    
    
    /* create the CSC offset */

    frame = gtk_frame_new(NULL);
    
    gtk_container_set_border_width(GTK_CONTAINER(frame), FRAME_BORDER);

    gtk_box_pack_start(GTK_BOX(hbox),
                       frame,
                       FALSE,                   // expand
                       FALSE,                   // fill
                       0);                      // padding
    
    ctk_gvo_csc->offsetTable =
        gtk_table_new(4,                        // rows
                      1,                        // columns
                      FALSE);                   // homogeneous
    
    make_label(ctk_gvo_csc, ctk_gvo_csc->offsetTable, "Offset", 0, 0);

    gtk_container_add(GTK_CONTAINER(frame), ctk_gvo_csc->offsetTable);
    
    for (row = 0; row < 3; row++) {

        ctk_gvo_csc->offset[row] = initialCSCOffset[row];

        make_entry(ctk_gvo_csc,
                   ctk_gvo_csc->offsetTable,
                   &ctk_gvo_csc->offsetWidget[row],
                   ctk_gvo_csc->offset[row],
                   row+1,
                   0);
    }
    
    
    /* create the CSC scale */

    frame = gtk_frame_new(NULL);
    
    gtk_container_set_border_width(GTK_CONTAINER(frame), FRAME_BORDER);

    gtk_box_pack_start(GTK_BOX(hbox),
                       frame,
                       FALSE,                   // expand
                       FALSE,                   // fill
                       0);                      // padding
    
    ctk_gvo_csc->scaleTable =
        gtk_table_new(4,                        // rows
                      1,                        // columns
                      FALSE);                   // homogeneous
    
    make_label(ctk_gvo_csc, ctk_gvo_csc->scaleTable, "Scale" , 0, 0);

    gtk_container_add(GTK_CONTAINER(frame), ctk_gvo_csc->scaleTable);
    
    for (row = 0; row < 3; row++) {

        ctk_gvo_csc->scale[row] = initialCSCScale[row];

        make_entry(ctk_gvo_csc,
                   ctk_gvo_csc->scaleTable,
                   &ctk_gvo_csc->scaleWidget[row],
                   ctk_gvo_csc->scale[row],
                   row+1,
                   0);
    }

    
    /*
     * create checkbox for immediate apply; only expose if the X
     * server can support apply CSC values immediately
     */
    
    if (caps & NV_CTRL_GVO_CAPABILITIES_APPLY_CSC_IMMEDIATELY) {
        
        ctk_gvo_csc->applyImmediateButton = gtk_check_button_new_with_label
            ("Apply Changes Immediately");
    
        gtk_toggle_button_set_active
            (GTK_TOGGLE_BUTTON(ctk_gvo_csc->applyImmediateButton), FALSE);

        g_signal_connect(GTK_OBJECT(ctk_gvo_csc->applyImmediateButton),
                         "toggled",
                         G_CALLBACK(apply_immediate_button_toggled),
                         ctk_gvo_csc);
    } else {
        ctk_gvo_csc->applyImmediateButton = NULL;
    }

    ctk_gvo_csc->applyImmediately = FALSE;
    
    
    /*
     * create an apply button; pack the button in an alignment inside
     * an hbox, so that we can properly position the apply button on
     * the far right
     */
    
    ctk_gvo_csc->applyButton = gtk_button_new_from_stock(GTK_STOCK_APPLY);
    
    g_signal_connect(GTK_OBJECT(ctk_gvo_csc->applyButton),
                     "clicked",
                     G_CALLBACK(apply_button_clicked),
                     ctk_gvo_csc);

    alignment = gtk_alignment_new(1.0,          // xalign
                                  0.5,          // yalign
                                  0.0,          // xscale
                                  0.0);         // yscale
    
    gtk_container_add(GTK_CONTAINER(alignment), ctk_gvo_csc->applyButton);

    hbox2 = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox2),
                       alignment,
                       TRUE,                    // expand
                       TRUE,                    // fill
                       0);                      // padding
    

    /* create a frame to pack the apply stuff in */
    
    frame = gtk_frame_new(NULL);
    gtk_container_set_border_width(GTK_CONTAINER(frame), FRAME_BORDER);
    
    vbox2 = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox2), 5);
    
    hbox = gtk_hbox_new(FALSE, 10);
    
    /* pack applyImmediateButton, but only if we created it */

    if (ctk_gvo_csc->applyImmediateButton) {

        gtk_box_pack_start(GTK_BOX(hbox),
                           ctk_gvo_csc->applyImmediateButton,
                           FALSE,               // expand
                           FALSE,               // fill
                           0);                  // padding
    }

    /* pack the Apply button */

    gtk_box_pack_start(GTK_BOX(hbox),
                       hbox2,
                       TRUE,                    // expand
                       TRUE,                    // fill
                       0);                      // padding
    
    /* pack the hbox inside a vbox so that we have proper y padding */

    gtk_box_pack_start(GTK_BOX(vbox2),
                       hbox,
                       FALSE,                   // expand
                       FALSE,                   // fill
                       0);                      // padding
    
    /* pack the vbox inside the frame */

    gtk_container_add(GTK_CONTAINER(frame), vbox2);

    gtk_box_pack_start(GTK_BOX(vbox),
                       frame,
                       FALSE,                   // expand
                       FALSE,                   // fill
                       0);                      // padding

    /*
     * if custom CSC will not be applied to the X screen, make that
     * clear to the user
     */
    
    if ((caps & NV_CTRL_GVO_CAPABILITIES_APPLY_CSC_TO_X_SCREEN) == 0) {

        label = build_opengl_only_msg();
        
        gtk_box_pack_start(GTK_BOX(vbox),
                           label,
                           FALSE,               // expand
                           FALSE,               // fill
                           0);                  // padding
    }
    
    /*
     * initialize the override button to what we read in
     * NV_CTRL_GVO_OVERRIDE_HW_CSC
     */
    
    gtk_toggle_button_set_active
        (GTK_TOGGLE_BUTTON(ctk_gvo_csc->overrideButton),
         override);
    
    override_state_toggled(ctk_gvo_csc, override);
    
    /* show the page */

    gtk_widget_show_all(GTK_WIDGET(object));

    return GTK_WIDGET(object);

} /* ctk_gvo_csc_new() */




/*
 * override_button_toggled() - the override checkbox has been toggled;
 * change the sensitivity of the widget; note that we do not send any
 * NV-CONTROL protocol here if override has been enabled -- that is
 * deferred until the user hits apply.
 */

static void override_button_toggled(GtkToggleButton *overrideButton,
                                    gpointer user_data)
{
    CtkGvoCsc *ctk_gvo_csc = (CtkGvoCsc *) user_data;
    gboolean enabled = gtk_toggle_button_get_active(overrideButton);
    override_state_toggled(ctk_gvo_csc, enabled);

    /*
     * if override was enabled, don't send NV-CONTROL protocol, yet,
     * unless applyImmediately was enabled; otherwise, wait until the
     * user applies it.  However, if override was disabled, apply that
     * immediately.
     */
    
    if (enabled) {
    
        if (ctk_gvo_csc->applyImmediately) {
            NvCtrlSetAttribute(ctk_gvo_csc->handle,
                               NV_CTRL_GVO_OVERRIDE_HW_CSC,
                               NV_CTRL_GVO_OVERRIDE_HW_CSC_TRUE);
        } else {
        
            // make the "Apply" button hot
        
            gtk_widget_set_sensitive(ctk_gvo_csc->applyButton, TRUE);
        }

    } else {
        NvCtrlSetAttribute(ctk_gvo_csc->handle,
                           NV_CTRL_GVO_OVERRIDE_HW_CSC,
                           NV_CTRL_GVO_OVERRIDE_HW_CSC_FALSE);
    }
    
} /* override_button_toggled() */



/*
 * override_state_toggled() - change the state of
 */

static void override_state_toggled(CtkGvoCsc *ctk_gvo_csc, gboolean enabled)
{
    gtk_widget_set_sensitive(ctk_gvo_csc->cscOptions, enabled);
    
} /* override_state_toggled() */



/*
 * make_entry() - helper function to create an adjustment, create a
 * numeric text entry with spin buttons, and pack the entry into the
 * provided table.
 */

static void make_entry(CtkGvoCsc *ctk_gvo_csc,
                       GtkWidget *table,
                       GtkWidget **widget,
                       float value,
                       int row,
                       int column)
{
    GtkAdjustment *adj;
    
    adj = (GtkAdjustment *) gtk_adjustment_new(value,   // value
                                               -1.0,    // lower
                                               1.0,     // upper
                                               0.001,   // step incr
                                               0.1,     // page incr
                                               0.1);    // page size
    
    *widget = gtk_spin_button_new(adj,                  // adjustment
                                  0.001,                // climb rate
                                  6);                   // number of digits
    
    g_signal_connect(G_OBJECT(*widget),
                     "value-changed",
                     G_CALLBACK(spin_button_value_changed),
                     ctk_gvo_csc);

    gtk_table_attach(GTK_TABLE(table),
                     *widget,
                     column,                            // left attach
                     column + 1,                        // right_attach
                     row,                               // top_attach
                     row + 1,                           // bottom_attach
                     0,                                 // xoptions
                     0,                                 // yoptions
                     10,                                // xpadding
                     10);                               // ypadding
    
} /* make_entry() */



/*
 * make_label() - helper function to create a lable and pack it in the
 * given table.
 */

static void make_label(CtkGvoCsc *ctk_gvo_csc,
                       GtkWidget *table,
                       const char *str,
                       int row,
                       int column)
{
    GtkWidget *label;

    label = gtk_label_new(str);

    gtk_table_attach(GTK_TABLE(table),
                     label,
                     column,
                     column + 1,                        // right_attach
                     row,                               // top_attach
                     row + 1,                           // bottom_attach
                     0,                                 // xoptions
                     0,                                 // yoptions
                     4,                                 // xpadding
                     4);                                // ypadding

} /* make_label() */



/*
 * spin_button_value_changed() - one of the spin buttons changed; 
 */

static void spin_button_value_changed(GtkWidget *button,
                                      gpointer user_data)
{
    CtkGvoCsc *ctk_gvo_csc = (CtkGvoCsc *) user_data;
    gdouble value;

    int row, column;

    value = gtk_spin_button_get_value(GTK_SPIN_BUTTON(button));
    
    /* which spinbutton was it? */
    
    for (row = 0; row < 3; row++) {
        for (column = 0; column < 3; column++) {
            
            if (ctk_gvo_csc->matrixWidget[row][column] == button) {
                ctk_gvo_csc->matrix[row][column] = value;
                goto done;
            }
        }

        if (ctk_gvo_csc->offsetWidget[row] == button) {
            ctk_gvo_csc->offset[row] = value;
            goto done;
        }
        
        if (ctk_gvo_csc->scaleWidget[row] == button) {
            ctk_gvo_csc->scale[row] = value;
            goto done;
        }
    }
            
 done:
    
    /*
     * the data has changed, make sure the apply button is sensitive
     */

    set_apply_button_senstive(ctk_gvo_csc);

    /* if we are supposed to apply immediately, send the data now */

    if (ctk_gvo_csc->applyImmediately) {
        apply_csc_values(ctk_gvo_csc);
    }
    
} /* spin_button_value_changed() */



/*
 * apply_immediate_button_toggled() - the "apply immediately" button
 * has been toggled; change the sensitivity of the "Apply" button, and
 * possibly send the current settings to the X server.
 */

static
void apply_immediate_button_toggled(GtkToggleButton *applyImmediateButton,
                                    gpointer user_data)
{
    CtkGvoCsc *ctk_gvo_csc = (CtkGvoCsc *) user_data;
    gboolean enabled = gtk_toggle_button_get_active(applyImmediateButton);
    
    /* cache the current state */

    ctk_gvo_csc->applyImmediately = enabled;

    /*
     * the Apply button's sensitivity is the opposite of the Immediate
     * apply checkbox -- if changes are applied immediately, then the
     * Apply button is not needed
     */
    
    gtk_widget_set_sensitive(ctk_gvo_csc->applyButton, !enabled);

    /*
     * if the apply immediately button is enabled, then flush our
     * current values to the X server
     */

    if (enabled) {
        apply_csc_values(ctk_gvo_csc);
    }

} /* apply_immediate_button_toggled() */



/*
 * apply_button_clicked() - the apply button has been toggled, send
 * the current settings to the X server, and make this button
 * insensitive.
 */

static void apply_button_clicked(GtkButton *button, gpointer user_data)
{
    CtkGvoCsc *ctk_gvo_csc = (CtkGvoCsc *) user_data;
    
    apply_csc_values(ctk_gvo_csc);
    
    gtk_widget_set_sensitive(ctk_gvo_csc->applyButton, FALSE);
    
} /* apply_button_clicked() */



/*
 * initialize_csc_dropdown_changed() - the "initialize" dropdown menu
 * changed; update the values in the matrix, offset, and scale
 */

static void initialize_csc_dropdown_changed(CtkDropDownMenu *menu,
                                            gpointer user_data)
{
    CtkGvoCsc *ctk_gvo_csc = (CtkGvoCsc *) user_data;
    const gfloat (*std)[5];
    gint column, row, value;
        
    //     red      green    blue    offset  scale
    static const float itu601[3][5] = {
        {  0.2991,  0.5870,  0.1150, 0.0625, 0.85547 }, // Y
        {  0.5000, -0.4185, -0.0810, 0.5000, 0.87500 }, // Cr
        { -0.1685, -0.3310,  0.5000, 0.5000, 0.87500 }, // Cb
    };
    
    static const float itu709[3][5] = {
        {  0.2130,  0.7156,  0.0725, 0.0625, 0.85547 }, // Y
        {  0.5000, -0.4542, -0.0455, 0.5000, 0.87500 }, // Cr
        { -0.1146, -0.3850,  0.5000, 0.5000, 0.87500 }, // Cb
    };
    
    static const float itu177[3][5] = {
        { 0.412391, 0.357584, 0.180481, 0.0, 0.85547 }, // Y
        { 0.019331, 0.119195, 0.950532, 0.0, 0.87500 }, // Cr
        { 0.212639, 0.715169, 0.072192, 0.0, 0.87500 }, // Cb
    };
    
    static const float identity[3][5] = {
        {  0.0000,  1.0000,  0.0000, 0.0000, 1.0 }, // Y (Green)
        {  1.0000,  0.0000,  0.0000, 0.0000, 1.0 }, // Cr (Red)
        {  0.0000,  0.0000,  1.0000, 0.0000, 1.0 }, // Cb (Blue)
    };
    

    value = ctk_drop_down_menu_get_current_value(menu);
 
    switch (value) {
      case CSC_STANDARD_ITU_601:  std = itu601; break;
      case CSC_STANDARD_ITU_709:  std = itu709; break;
      case CSC_STANDARD_ITU_177:  std = itu177; break;
      case CSC_STANDARD_IDENTITY: std = identity; break;
      default: return;
    }
        
    for (row = 0; row < 3; row++) {
        for (column = 0; column < 3; column++) {
            ctk_gvo_csc->matrix[row][column] = std[row][column];
            gtk_spin_button_set_value
                (GTK_SPIN_BUTTON(ctk_gvo_csc->matrixWidget[row][column]),
                 ctk_gvo_csc->matrix[row][column]);
        }
        
        ctk_gvo_csc->offset[row] = std[row][3];
        gtk_spin_button_set_value
            (GTK_SPIN_BUTTON(ctk_gvo_csc->offsetWidget[row]),
             ctk_gvo_csc->offset[row]);

        ctk_gvo_csc->scale[row] = std[row][4];
        gtk_spin_button_set_value
            (GTK_SPIN_BUTTON(ctk_gvo_csc->scaleWidget[row]),
             ctk_gvo_csc->scale[row]);
    }
    
    /*
     * the data has changed, make sure the apply button is sensitive
     */

    set_apply_button_senstive(ctk_gvo_csc);

    /* if we are supposed to apply immediately, send the data now */

    if (ctk_gvo_csc->applyImmediately) {
        apply_csc_values(ctk_gvo_csc);
    }
    
} /* initialize_csc_dropdown_changed() */



/*
 * set_apply_button_senstive() - make the "Apply" button sensitive
 */

static void set_apply_button_senstive(CtkGvoCsc *ctk_gvo_csc)
{
    /* if data is applied immediately, then we don't */

    if (ctk_gvo_csc->applyImmediately) return;
    
    gtk_widget_set_sensitive(ctk_gvo_csc->applyButton, TRUE);
    
} /* set_apply_button_senstive() */



/*
 * apply_csc_values() - apply the current CSC values to the X server
 * and make sure CSC override is enabled
 */

static void apply_csc_values(CtkGvoCsc *ctk_gvo_csc)
{
    NvCtrlSetGvoColorConversion(ctk_gvo_csc->handle,
                                ctk_gvo_csc->matrix,
                                ctk_gvo_csc->offset,
                                ctk_gvo_csc->scale);
    
    NvCtrlSetAttribute(ctk_gvo_csc->handle,
                       NV_CTRL_GVO_OVERRIDE_HW_CSC,
                       NV_CTRL_GVO_OVERRIDE_HW_CSC_TRUE);

} /* apply_csc_values() */


/*
 * build_opengl_only_msg() - build a message to inform the user that
 * custom CSC will only be applied to OpenGL GVO output; this returns
 * a frame containing the message.
 */

static GtkWidget *build_opengl_only_msg(void)
{
    GdkPixbuf *pixbuf;
    GtkWidget *label;
    GtkWidget *hbox;
    GtkWidget *vbox;
    GtkWidget *frame;
    GtkWidget *image = NULL;
    
    /* create the label */

    label = gtk_label_new("Note that the overridden Color Space Conversion "
                          "will only apply to OpenGL applications "
                          "using the GLX_NV_video_out extension.");

    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

    /* create the information icon */

    pixbuf = gtk_widget_render_icon(label,
                                    GTK_STOCK_DIALOG_INFO,
                                    GTK_ICON_SIZE_DIALOG,
                                    "CSC information message");

    /* create a pixmap from the icon */

    if (pixbuf) {
        image = gtk_image_new_from_pixbuf(pixbuf);
    } else {
        image = NULL;
    }    
    
    /* create an hbox and pack the icon and label in it */

    hbox = gtk_hbox_new(FALSE, 5);
    
    if (image) {
        gtk_box_pack_start(GTK_BOX(hbox),
                           image,
                           FALSE,                   // expand
                           FALSE,                   // fill
                           5);                      // padding
    }
    
    gtk_box_pack_start(GTK_BOX(hbox),
                       label,
                       FALSE,                       // expand
                       FALSE,                       // fill
                       5);                          // padding
    
    /* pack the hbox in a vbox to get vertical padding */

    vbox = gtk_vbox_new(FALSE, 5);
    
    gtk_box_pack_start(GTK_BOX(vbox),
                       hbox,
                       FALSE,                       // expand
                       FALSE,                       // fill
                       5);                          // padding

    /* pack the whole thing in a frame */

    frame = gtk_frame_new(NULL);
    gtk_container_set_border_width(GTK_CONTAINER(frame), FRAME_BORDER);

    gtk_container_add(GTK_CONTAINER(frame), vbox);

    return frame;
    
} /* build_opengl_only_msg() */



void ctk_gvo_csc_select(GtkWidget *widget)
{
    CtkGvoCsc *ctk_gvo_csc = CTK_GVO_CSC(widget);
    CtkGvo *ctk_gvo = ctk_gvo_csc->gvo_parent;

    /* Grab the GVO parent banner */

    gtk_container_add(GTK_CONTAINER(ctk_gvo_csc->banner_box),
                      ctk_gvo->banner.widget);

    ctk_config_start_timer(ctk_gvo->ctk_config, (GSourceFunc) ctk_gvo_probe,
                           (gpointer) ctk_gvo);
}



void ctk_gvo_csc_unselect(GtkWidget *widget)
{
    CtkGvoCsc *ctk_gvo_csc = CTK_GVO_CSC(widget);
    CtkGvo *ctk_gvo = ctk_gvo_csc->gvo_parent;

    /* Return the GVO parent banner */

    gtk_container_remove(GTK_CONTAINER(ctk_gvo_csc->banner_box),
                         ctk_gvo->banner.widget);

    ctk_config_stop_timer(ctk_gvo->ctk_config, (GSourceFunc) ctk_gvo_probe,
                          (gpointer) ctk_gvo);
}