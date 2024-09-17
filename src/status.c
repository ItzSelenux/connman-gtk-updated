/*
 * ConnMan GTK GUI
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
 * Author: Jaakko Hannikainen <jaakko.hannikainen@intel.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "config.h"
#include "configurator.h"
#include "connection.h"
#include "main.h"
#include "status.h"
#include "service.h"
#include "technology.h"

#ifdef USE_STATUS_ICON
#include <libayatana-appindicator/app-indicator.h>

AppIndicator *indicator;
GtkWidget *menu;
gboolean menuinit = TRUE;
gulong indicator_signal_id;

	GtkWidget *traymenu_item_openapp;
	GtkWidget *traymenu_item_openapp2;
	GtkWidget *traymenu_item_wireless;
	GtkWidget *traymenu_item_bluetooth;
	GtkWidget *traymenu_item_vpn;
	GtkWidget *traymenu_item_exit;

static void status_activate(gpointer *ignored, gpointer user_data)
{
	g_signal_emit_by_name(user_data, "activate");
//todo, not working
}

static void status_exit(gpointer *ignored, gpointer user_data)
{
	g_signal_emit_by_name(main_window, "destroy");
}

static void status_toggle_connection(gpointer *ignored, gpointer user_data)
{
	service_toggle_connection(user_data);
}

static GtkWidget *create_service_item(struct service *serv)
{
	gchar *name, *state, *state_r, *label;
	GtkWidget *item, *child;

	name = service_get_property_string(serv, "Name", NULL);
	state = service_get_property_string(serv, "State", NULL);
	state_r = service_get_property_string_raw(serv, "State", NULL);

	if (strcmp(state_r, "idle"))
		label = g_markup_printf_escaped("<b>%s - %s</b>", name, state);
	else
		label = g_markup_printf_escaped("%s - %s", name, state);

	item = gtk_menu_item_new_with_label(NULL);
	child = gtk_bin_get_child(GTK_BIN(item));
	gtk_label_set_markup(GTK_LABEL(child), label);
	g_signal_connect(item, "activate", G_CALLBACK(status_toggle_connection),
					 serv);

	g_free(name);
	g_free(state);
	g_free(state_r);
	g_free(label);

	return item;
}

static void status_menu(gpointer *ignored, guint button, guint activate_time, gpointer user_data)
{
	if (menuinit)
	{
		GList *children, *iter;

		children = gtk_container_get_children(GTK_CONTAINER(menu));

		for (iter = children; iter != NULL; iter = g_list_next(iter))
		{
			gtk_widget_destroy(GTK_WIDGET(iter->data));
		}

		g_list_free(children);
		menuinit = FALSE;
		return;
	}

	GtkWidget *open, *exit;
	int index;

	open = gtk_menu_item_new_with_label(_("Open app"));
	exit = gtk_menu_item_new_with_label(_("Exit"));

	g_signal_connect(open, "activate", G_CALLBACK(status_activate),
					 user_data);
	g_signal_connect(exit, "activate", G_CALLBACK(status_exit), user_data);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), open);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

	for (index = CONNECTION_TYPE_ETHERNET; index < CONNECTION_TYPE_COUNT; index++)
	{
		const gchar *label;
		struct technology *tech;
		GHashTableIter iter;
		gpointer key, service;
		GtkMenuItem *item;
		GtkMenu *submenu;
		gboolean has_items = FALSE;

		tech = technologies[index];
		if (!tech)
			continue;
		if (!technology_get_property_bool(tech, "Powered"))
			continue;
		submenu = GTK_MENU(gtk_menu_new());
		label = translated_tech_name(tech->type);
		item = GTK_MENU_ITEM(gtk_menu_item_new_with_label(label));

		g_hash_table_iter_init(&iter, tech->services);
		while (g_hash_table_iter_next(&iter, &key, &service))
		{
			GtkWidget *item = create_service_item(service);
			gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
			has_items = TRUE;
		}

		gtk_menu_item_set_submenu(item, GTK_WIDGET(submenu));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(item));
		gtk_widget_set_sensitive(GTK_WIDGET(item), has_items);
	}

	gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), exit);
	gtk_widget_show_all(menu);
	//gtk_widget_show_all(menu);
	g_signal_handler_disconnect(indicator, indicator_signal_id);
	app_indicator_set_menu(indicator, GTK_MENU(menu));
	//app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
}

void status_update(void)
{
	int best_status = 0;
	int index;

	if (!status_icon_enabled)
		return;

	for (index = CONNECTION_TYPE_ETHERNET; index < CONNECTION_TYPE_COUNT; index++)
	{
		struct technology *tech;
		GHashTableIter iter;
		gpointer key, service;

		tech = technologies[index];
		if (!tech)
			continue;

		g_hash_table_iter_init(&iter, tech->services);
		while (g_hash_table_iter_next(&iter, &key, &service))
		{
			gchar *state;
			state = service_get_property_string_raw(service, "State",
													NULL);
			switch (best_status)
			{
			case 0:
				if (!strcmp(state, "association"))
					best_status = 1;
			case 1:
				if (!strcmp(state, "configuration"))
					best_status = 2;
			case 2:
				if (!strcmp(state, "ready"))
					best_status = 3;
			case 3:
				if (!strcmp(state, "online"))
					best_status = 4;
			}

			g_free(state);

			if (best_status == 4)
				break;
		}
	}

	switch (best_status)
	{
	case 0:
		app_indicator_set_icon(indicator, "network-offline");
		break;
	case 1:
	case 2:
		app_indicator_set_icon(indicator, "preferences-system-network");
		break;
	case 3:
	case 4:
		app_indicator_set_icon(indicator, "network-transmit-receive");
		break;
	}
}

void status_init(GtkApplication *app)
{
	if (!status_icon_enabled)
		return;

	indicator = app_indicator_new("network-status",
		"preferences-system-network",
		APP_INDICATOR_CATEGORY_SYSTEM_SERVICES);

	menu = gtk_menu_new();

		// PLACEHOLDERS, FIX, small menu size if the new menu is bigger than the previous menu
	traymenu_item_openapp = gtk_menu_item_new_with_label("Open App");
	traymenu_item_openapp2 = gtk_menu_item_new_with_label("Open App");
	traymenu_item_wireless = gtk_menu_item_new_with_label("Wireless");
	traymenu_item_bluetooth = gtk_menu_item_new_with_label("Bluetooth");
	traymenu_item_vpn = gtk_menu_item_new_with_label("VPN");
	traymenu_item_exit = gtk_menu_item_new_with_label("Exit");

	gtk_menu_shell_append(GTK_MENU_SHELL(menu), traymenu_item_openapp);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), traymenu_item_openapp2);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), traymenu_item_wireless);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), traymenu_item_bluetooth);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), traymenu_item_vpn);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), traymenu_item_exit);
	gtk_widget_show_all(menu);

	app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
		indicator_signal_id = g_signal_connect(indicator, "new-icon", G_CALLBACK(status_menu), app);
		//g_signal_connect(menu, "realize", G_CALLBACK(status_menu), app);
	status_update();
}

#endif /* USE_STATUS_ICON */