/*
 * util.cc
 * Copyright 2014 William Pitcock
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>

#include "libaudqt.h"

namespace audqt {

bool restarting;
static QApplication * qapp;

EXPORT void init ()
{
    if (restarting)
        return;

    static char app_name[] = "audacious";
    static int dummy_argc = 1;
    static char * dummy_argv[] = {app_name, nullptr};

    qapp = new QApplication (dummy_argc, dummy_argv);
    qapp->setAttribute (Qt::AA_UseHighDpiPixmaps);
    qapp->setApplicationDisplayName ("Audacious");
    qapp->setWindowIcon (QIcon::fromTheme (app_name));
}

EXPORT void run ()
{
    qapp->exec ();
}

EXPORT void quit ()
{
    qapp->quit ();
}

EXPORT void cleanup ()
{
    if (restarting)
        return;

    aboutwindow_hide ();
    equalizer_hide ();
    infowin_hide ();
    log_inspector_hide ();
    prefswin_hide ();
    queue_manager_hide ();

    delete qapp;
    qapp = nullptr;
}

EXPORT void enable_layout (QLayout * layout, bool enabled)
{
    int count = layout->count ();
    for (int i = 0; i < count; i ++)
    {
        auto item = layout->itemAt (i);
        if (QLayout * layout2 = item->layout ())
            enable_layout (layout2, enabled);
        if (QWidget * widget = item->widget ())
            widget->setEnabled (enabled);
    }
}

EXPORT void clear_layout (QLayout * layout)
{
    while (QLayoutItem * item = layout->takeAt (0))
    {
        if (QLayout * layout2 = item->layout ())
            clear_layout (layout2);
        if (QWidget * widget = item->widget ())
            delete widget;

        delete item;
    }
}

/* the goal is to force a window to come to the front on any Qt platform */
EXPORT void window_bring_to_front (QWidget * window)
{
    window->show ();

    Qt::WindowStates state = window->windowState ();

    state &= ~Qt::WindowMinimized;
    state |= Qt::WindowActive;

    window->setWindowState (state);
    window->raise ();
    window->activateWindow ();
}

EXPORT void simple_message (const char * title, const char * text)
{
    QDialog msgbox;
    QVBoxLayout vbox;
    QLabel label;
    QDialogButtonBox bbox;

    label.setText (text);
    label.setTextInteractionFlags (Qt::TextSelectableByMouse);

    bbox.setStandardButtons (QDialogButtonBox::Close);
    bbox.button (QDialogButtonBox::Close)->setText (translate_str (N_("_Close")));

    QObject::connect (& bbox, & QDialogButtonBox::rejected, & msgbox, & QDialog::reject);

    vbox.setSizeConstraint (QLayout::SetFixedSize);
    vbox.addWidget (& label);
    vbox.addWidget (& bbox);

    msgbox.setWindowTitle (title);
    msgbox.setLayout (& vbox);
    msgbox.exec ();
}

/* translate GTK+ accelerators and also handle dgettext() */
EXPORT QString translate_str (const char * str, const char * domain)
{
    /* translate the GTK+ accelerator (_) into a Qt accelerator (&) */
    return QString (dgettext (domain, str)).replace ('_', '&');
}

} // namespace audqt
