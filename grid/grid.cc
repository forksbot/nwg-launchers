/*
 * GTK-based application grid
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include "grid.h"
#include "grid_tools.cpp"
#include "grid_classes.cc"
#include <sys/time.h>

int main(int argc, char *argv[]) {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    long int start_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    /* Try to lock /tmp/nwggrid.lock file. This will return -1 if the command is already running.
     * Thanks to chmike at https://stackoverflow.com/a/1643134 */
    // Create pid file if not yet exists
    if (!std::ifstream("/tmp/nwggrid.lock")) {
        save_string_to_file("nwggrid lock file", "/tmp/nwggrid.lock");
    }

    if (tryGetLock("/tmp/nwggrid.lock") == -1) {
        // kill if already running
        std::remove("/tmp/nwggrid.lock");
        std::string cmd = "pkill -f nwggrid";
        const char *command = cmd.c_str();
        std::system(command);
        std::exit(0);
    }

    std::string lang ("");

    InputParser input(argc, argv);
    if(input.cmdOptionExists("-h")){
        std::cout << "GTK application grid: nwggrid " << version << " (c) Piotr Miller 2020\n\n";
        std::cout << "nwggrid [-h] [-f] [-o <opacity>] [-c <col>] [-s <size>] [-l <ln>]\n\n";
        std::cout << "Options:\n";
        std::cout << "-h            show this help message and exit\n";
        std::cout << "-f            display favourites\n";
        std::cout << "-o <opacity>  background opacity (0.0 - 1.0, default 0.9)\n";
        std::cout << "-n <col>      number of grid columns (default: 6)\n";
        std::cout << "-s <size>     button image size (default: 72)\n";
        std::cout << "-c <name>     css file name (default: style.css)\n";
        std::cout << "-l <ln>       force use of <ln> language\n";
        std::cout << "-wm <wmname>  window manager name (if can not be detected)\n";
        std::exit(0);
    }
    if (input.cmdOptionExists("-f")){
        favs = true;
    }
    const std::string &forced_lang = input.getCmdOption("-l");
    if (!forced_lang.empty()){
        lang = forced_lang;
    }
    const std::string &cols = input.getCmdOption("-n");
    if (!cols.empty()) {
        try {
            int n_c = std::stoi(cols);
            if (n_c > 0 && n_c < 100) {
                num_col = n_c;
            } else {
                std::cout << "\nERROR: Columns must be in range 1 - 99\n\n";
            }
        } catch (...) {
            std::cout << "\nERROR: Invalid number of columns\n\n";
        }
    }

    const std::string &css_name = input.getCmdOption("-c");
    if (!css_name.empty()){
        custom_css_file = css_name;
    }
    
    const std::string &wm_name = input.getCmdOption("-wm");
    if (!wm_name.empty()){
        wm = wm_name;
    }

    const std::string &opa = input.getCmdOption("-o");
    if (!opa.empty()){
        try {
            double o = std::stod(opa);
            if (o >= 0.0d && o <= 1.0d) {
                opacity = o;
            } else {
                std::cout << "\nERROR: Opacity must be in range 0.0 to 1.0\n\n";
            }
        } catch (...) {
            std::cout << "\nERROR: Invalid opacity value\n\n";
        }
    }

    const std::string &i_size = input.getCmdOption("-s");
    if (!i_size.empty()){
        try {
            int i_s = std::stoi(i_size);
            if (i_s >= 16 && i_s <= 256) {
                image_size = i_s;
            } else {
                std::cout << "\nERROR: Size must be in range 16 - 256\n\n";
            }
        } catch (...) {
            std::cout << "\nERROR: Invalid image size\n\n";
        }
    }

    cache_file = get_cache_path();
    try {
        cache = get_cache(cache_file);
    }  catch (...) {
        std::cout << "Cache file not found, creating...\n";
        save_json(cache, cache_file);
    }
    std::cout << cache.size() << " cache entries loaded\n";

    std::string config_dir = get_config_dir();
    if (!fs::is_directory(config_dir)) {
        std::cout << "Config dir not found, creating...\n";
        fs::create_directory(config_dir);
    }

    // default and custom style sheet
    std::string default_css_file = config_dir + "/style.css";
    // css file to be used
    std::string css_file = config_dir + "/" + custom_css_file;
    std::cout << css_file << std::endl;
    // copy default file if not found
    const char *custom_css = css_file.c_str();
    if (!fs::exists(default_css_file)) {
        fs::path source_file = "/usr/share/nwggrid/style.css";
        fs::path target = default_css_file;
        try {
            fs::copy_file("/usr/share/nwggrid/style.css", target, fs::copy_options::overwrite_existing);
        } catch (...) {
            std::cout << "Failed copying default style.css\n";
        }
    }

    // This will be read-only, to find n most clicked items (n = number of grid columns)
    std::vector<CacheEntry> favourites {};
    if (cache.size() > 0) {
        int n = 0;
        (int)cache.size() >= num_col ? n = num_col : n = (int)cache.size();
        favourites = get_favourites(cache, n);
    }

    /* get current WM name if not forced */
    if (wm.empty()) {
        wm = detect_wm();
    }
    std::cout << "WM: " << wm << "\n";

    /* get lang (2 chars long string) if not yet forced */
    if (lang.length() != 2) {
        lang = get_locale();
    }
    std::cout << "Locale: " << lang << "\n";

    /* get all applications dirs */
    std::vector<std::string> app_dirs = get_app_dirs();

    /* get a list of paths to all *.desktop entries */
    std::vector<std::string> entries = list_entries(app_dirs);
    std::cout << entries.size() << " .desktop entries found\n";

    /* create the vector of DesktopEntry structs */
    std::vector<DesktopEntry> desktop_entries {};
    for (std::string entry : entries) {
        // string path -> vector<string> {name, exec, icon, comment}
        std::vector<std::string> e = desktop_entry(entry, lang);
        struct DesktopEntry de = {e[0], e[1], e[2], e[3]};

        // only add if 'name' and 'exec' not empty
        if (!e[0].empty() && !e[1].empty()) {
            // avoid adding duplicates
            bool found = false;
            for (DesktopEntry entry : desktop_entries) {
                if (entry.name == de.name && entry.exec == de.exec) {
                    found = true;
                }
            }
            if (!found) {
                desktop_entries.push_back(de);
            }
        }
    }

    /* sort above by the 'name' field */
    sort(desktop_entries.begin(), desktop_entries.end(), [](const DesktopEntry& lhs, const DesktopEntry& rhs) {
        return lhs.name < rhs.name;
    });

    /* turn off borders, enable floating on sway */
    if (wm == "sway") {
        std::string cmd = "swaymsg for_window [title=\"~nwggrid*\"] floating enable";
        const char *command = cmd.c_str();
        std::system(command);
        cmd = "swaymsg for_window [title=\"~nwggrid*\"] border none";
        command = cmd.c_str();
        std::system(command);
    }

    Gtk::Main kit(argc, argv);

    GtkCssProvider* provider = gtk_css_provider_new();
    GdkDisplay* display = gdk_display_get_default();
    GdkScreen* screen = gdk_display_get_default_screen(display);
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    if (std::ifstream(custom_css)) {
        gtk_css_provider_load_from_path(provider, custom_css, NULL);
        std::cout << "Using " << custom_css << std::endl;
    } else {
        gtk_css_provider_load_from_path(provider, "/usr/share/nwggrid/style.css", NULL);
        std::cout << "Using /usr/share/nwggrid/style.css\n";
    }
    g_object_unref(provider);

    MainWindow window;
    window.show();

    window.signal_button_press_event().connect(sigc::ptr_fun(&on_window_clicked));

    /* Detect focused display geometry: {x, y, width, height} */
    std::vector<int> geometry = display_geometry(wm, window);
    std::cout << "Focused display: " << geometry[0] << ", " << geometry[1] << ", " << geometry[2] << ", "
    << geometry[3] << '\n';

    int x = geometry[0];
    int y = geometry[1];
    int w = geometry[2];
    int h = geometry[3];

    if (wm == "sway" || wm == "i3" || wm == "openbox") {
        window.resize(w, h);
        window.move(x, y);
    }

    Gtk::Box outer_box(Gtk::ORIENTATION_VERTICAL);
    outer_box.set_spacing(15);

    // hbox for searchbox
    Gtk::HBox hbox_header;

    hbox_header.pack_start(window.searchbox, Gtk::PACK_EXPAND_PADDING);

    outer_box.pack_start(hbox_header, Gtk::PACK_SHRINK, Gtk::PACK_EXPAND_PADDING);

    Gtk::ScrolledWindow scrolled_window;
    scrolled_window.set_propagate_natural_height(true);
    scrolled_window.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_ALWAYS);

    /* Create buttons for all desktop entries */
    for(auto it = desktop_entries.begin(); it != desktop_entries.end(); it++) {
        Gtk::Image* image = app_image(it -> icon);
        AppBox *ab = new AppBox(it -> name, it -> exec, it -> comment);
        ab -> set_image_position(Gtk::POS_TOP);
        ab -> set_image(*image);
        ab -> signal_clicked().connect(sigc::bind<std::string>(sigc::ptr_fun(&on_button_clicked), it -> exec));
        ab -> signal_enter_notify_event().connect(sigc::bind<std::string>(sigc::ptr_fun(&on_button_entered), it -> comment));
        ab -> signal_focus_in_event().connect(sigc::bind<std::string>(sigc::ptr_fun(&on_button_focused), it -> comment));

        window.all_boxes.push_back(ab);
    }
    window.label_desc.set_text(std::to_string(window.all_boxes.size()));

    /* Create buttons for favourites */
    if (favs && favourites.size() > 0) {
        for (CacheEntry entry : favourites) {
            for(auto it = desktop_entries.begin(); it != desktop_entries.end(); it++) {
                if (it -> exec == entry.exec) {
                    Gtk::Image* image = app_image(it -> icon);
                    AppBox *ab = new AppBox(it -> name, it -> exec, it -> comment);
                    ab -> set_image_position(Gtk::POS_TOP);
                    ab -> set_image(*image);
                    ab -> signal_clicked().connect(sigc::bind<std::string>(sigc::ptr_fun(&on_button_clicked), it -> exec));
                    ab -> signal_enter_notify_event().connect(sigc::bind<std::string>(sigc::ptr_fun(&on_button_entered), it -> comment));
                    ab -> signal_focus_in_event().connect(sigc::bind<std::string>(sigc::ptr_fun(&on_button_focused), it -> comment));

                    // avoid adding twice the same exec w/ another name
                    bool already_added {false};
                    for (AppBox *app_box : window.fav_boxes) {
                        if (app_box -> exec == it -> exec) {
                            already_added = true;
                        }
                    }
                    if (!already_added) {
                        window.fav_boxes.push_back(ab);
                    }
                }
            }
        }
    }

    int column = 0;
    int row = 0;

    if (favs && favourites.size() > 0) {
        for (AppBox *box : window.fav_boxes) {
            window.favs_grid.attach(*box, column, row, 1, 1);
            if (column < num_col - 1) {
                column++;
            } else {
                column = 0;
                row++;
            }
        }
    }

    column = 0;
    row = 0;
    for (AppBox *box : window.all_boxes) {
        window.apps_grid.attach(*box, column, row, 1, 1);
        if (column < num_col - 1) {
            column++;
        } else {
            column = 0;
            row++;
        }
    }

    Gtk::VBox inner_vbox;

    Gtk::HBox favs_hbox;
    favs_hbox.pack_start(window.favs_grid, true, false, 0);
    inner_vbox.pack_start(favs_hbox, false, false, 5);
    if (favs && favourites.size() > 0) {
        inner_vbox.pack_start(window.separator, false, true, 0);
    }

    Gtk::HBox apps_hbox;
    apps_hbox.pack_start(window.apps_grid, Gtk::PACK_EXPAND_PADDING);
    inner_vbox.pack_start(apps_hbox, true, true, 0);

    scrolled_window.add(inner_vbox);

    outer_box.pack_start(scrolled_window, Gtk::PACK_EXPAND_WIDGET);
    scrolled_window.show_all_children();

    outer_box.pack_start(window.label_desc, Gtk::PACK_SHRINK);

    window.add(outer_box);
    window.show_all_children();

    // Set keyboard focus to the first visible button
    if (favs && favourites.size() > 0) {
        auto* first = window.favs_grid.get_child_at(0, 0);
        if (first) {
            first -> set_property("has_focus", true);
        }
    } else {
        auto* first = window.apps_grid.get_child_at(0, 0);
        if (first) {
            first -> set_property("has_focus", true);
        }
    }

    gettimeofday(&tp, NULL);
    long int end_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    std::cout << "Time: " << end_ms - start_ms << std::endl;

    Gtk::Main::run(window);

    return 0;
}
