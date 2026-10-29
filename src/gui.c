#include <gtk/gtk.h>

// 获取获取一个
static void on_activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *win = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win), "vividlog-c");
    gtk_window_set_default_size(GTK_WINDOW(win), 480, 320);
    gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("cn.efreight.vividlog", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}