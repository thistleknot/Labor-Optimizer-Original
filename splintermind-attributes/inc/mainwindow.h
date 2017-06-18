/*
Dwarf Therapist
Copyright (c) 2009 Trey Stout (chmod)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtCore>
#include <QtGui>
#include <QtNetwork>

class StateTableView;
class DFInstance;
class DwarfModel;
class DwarfModelProxy;
class Dwarf;
class AboutDialog;
class CustomProfession;
class ViewManager;
class Scanner;
class ScriptDialog;

namespace Ui
{
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    QSettings *get_settings() {return m_settings;}
    QToolBar *get_toolbar();
    DwarfModel *get_model() {return m_model;}
    DwarfModelProxy *get_proxy() {return m_proxy;}
    ViewManager *get_view_manager() {return m_view_manager;}
    DFInstance *get_DFInstance() {return m_df;}

    Ui::MainWindow *ui;

    public slots:
        // DF related
        void connect_to_df();
        void read_dwarves();
        void scan_memory();
        void new_pending_changes(int);
        void new_creatures_count(int,int,int,QString);
        void lost_df_connection();

        //settings
        void set_group_by(int);
        void export_custom_professions();
        void import_custom_professions();
        void export_gridviews();
        void import_gridviews();
        void clear_user_settings();

        // dialogs
        void show_about();
        void list_pending();
        void draw_professions();
        void draw_custom_profession_context_menu(const QPoint &);
        void add_new_filter_script();
        void edit_filter_script();
        void remove_filter_script();
        void print_gridview();
        void save_gridview();

        // version check
        void check_latest_version(bool show_result_on_equal=false);
        void version_check_finished(bool error);

        // layout check
        void check_for_layout(const QString & checksum);
        void layout_check_finished(bool error);

        // links
        void go_to_forums();
        void go_to_donate();
        void go_to_project_home();
        void go_to_new_issue();

        // progress
        void set_progress_message(const QString &msg);
        void set_progress_range(int min, int max);
        void set_progress_value(int value);

        // misc
        void show_dwarf_details_dock(Dwarf *d = 0);
        void new_filter_script_chosen(const QString &script_name);
        void reload_filter_scripts();

        void add_new_custom_role();

        //Thistleknot
        void clear_labors();
        void initializeSuperStruct();
        void optimize_labors();

private:
    DFInstance *m_df;
    QLabel *m_lbl_status;
    QLabel *m_lbl_message;
    QProgressBar *m_progress;
    QSettings *m_settings;
    ViewManager *m_view_manager;
    DwarfModel *m_model;
    DwarfModelProxy *m_proxy;
    AboutDialog *m_about_dialog;
    CustomProfession *m_temp_cp;
    Scanner *m_scanner;
    ScriptDialog *m_script_dialog;
    QHttp *m_http;
    bool m_reading_settings;
    bool m_show_result_on_equal; //! used during version checks
    QCompleter *m_dwarf_name_completer;
    QStringList m_dwarf_names_list;
    bool m_force_connect;
    bool m_try_download;
    QString m_tmp_checksum;
    bool m_deleting_settings;

    void closeEvent(QCloseEvent *evt); // override;

    void read_settings();
    void write_settings();

    void refresh_role_menus();
    void refresh_roles_data();
    void write_custom_roles();

    private slots:
        void set_interface_enabled(bool);
        //role stuff
        void edit_custom_role();
        void remove_custom_role();
        void display_group(const int);
        void preference_selected(QStringList names);

};

//used for sorting labor/role percent's
struct superStruct
{
    QString *role_name;

    //probably not needed since we're sorting by w_percent
    //float *r_percent;
    float r_percent;

    //copy of role's priority * dwarf's role %;
    float w_percent;

    int *labor_id;

    //dwarf's ID
    int d_id;

};

//used for loading quantities to assign (from csv file), NOT TO BE POINTERS DUE TO CONVERSIONS FROM NON QT STUFF, AND ONLY INITIALIZED WITH UNIQUE VALUES
struct roles
{
    //loaded from csv
    QString role_name;
    int labor_id;
    float priority;
    int numberToAssign;

    //used during assignments
    int numberAssigned;

    float coverage;

    //used to return to superStruct
    QString *p_r_name()
    {
        return &role_name;
    }

    int *p_l_id()
    {
        return &labor_id;
    }

    float *p_priority()
    {
        return &priority;
    }

    int *n_assign()
    {
        return &numberToAssign;
    }

    float *get_coverage()
    {
        return &coverage;
    }


};

#endif // MAINWINDOW_H
