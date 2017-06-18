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
#include <QtGui>
#include <QtNetwork>
#include <QtDebug>

#include <algorithm> //used for swap, would like to use for sort, but oh well
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
//#include <math.h>
#include <string>
#include <sstream>
#include <vector>

#include "mainwindow.h"
#include "ui_about.h"
#include "ui_mainwindow.h"
#include "ui_pendingchanges.h"
#include "aboutdialog.h"
#include "dwarf.h"
#include "dwarfmodel.h"
#include "dwarfmodelproxy.h"
#include "memorylayout.h"
#include "viewmanager.h"
#include "viewcolumnset.h"
#include "uberdelegate.h"
#include "customprofession.h"
#include "labor.h"
#include "defines.h"
#include "version.h"
#include "dwarftherapist.h"
#include "importexportdialog.h"
#include "gridviewdock.h"
#include "skilllegenddock.h"
#include "dwarfdetailsdock.h"
#include "columntypes.h"
#include "rotatedheader.h"
#include "scanner.h"
#include "scriptdialog.h"
#include "truncatingfilelogger.h"
#include "roledialog.h"
#include "viewcolumn.h"
#include "rolecolumn.h"
#include "statetableview.h"
#include "preferencesdock.h"

#include "dfinstance.h"
#ifdef Q_WS_WIN
#include "dfinstancewindows.h"
#endif
#ifdef Q_WS_X11
#include "dfinstancelinux.h"
#endif
#ifdef _OSX
#include "dfinstanceosx.h"
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_df(0)
    , m_lbl_status(new QLabel(tr("not connected"), this))
    , m_lbl_message(new QLabel(tr("Initializing"), this))
    , m_progress(new QProgressBar(this))
    , m_settings(0)
    , m_model(new DwarfModel(this))
    , m_proxy(new DwarfModelProxy(this))
    , m_about_dialog(new AboutDialog(this))
    , m_temp_cp(0)
    , m_scanner(0)
    , m_script_dialog(new ScriptDialog(this))
    , m_http(0)
    , m_reading_settings(false)
    , m_show_result_on_equal(false)
    , m_dwarf_name_completer(0)
    , m_force_connect(false)
    , m_try_download(true)
    , m_deleting_settings(false)
    , m_view_manager(0)
{
    ui->setupUi(this);

    //connect to df first, we need to read raws for some ui elements first!!!
    //connect_to_df();

    m_view_manager = new ViewManager(m_model, m_proxy, this);
    ui->v_box->addWidget(m_view_manager);

    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);

    /* docks! */
    GridViewDock *grid_view_dock = new GridViewDock(m_view_manager, this);
    grid_view_dock->setHidden(true); // hide by default
    grid_view_dock->setFloating(true);
    addDockWidget(Qt::RightDockWidgetArea, grid_view_dock);

    SkillLegendDock *skill_legend_dock = new SkillLegendDock(this);
    skill_legend_dock->setHidden(true); // hide by default
    skill_legend_dock->setFloating(true);
    addDockWidget(Qt::RightDockWidgetArea, skill_legend_dock);

    DwarfDetailsDock *dwarf_details_dock = new DwarfDetailsDock(this);
    dwarf_details_dock->setHidden(true);
    dwarf_details_dock->setFloating(true);
    addDockWidget(Qt::RightDockWidgetArea, dwarf_details_dock);

    PreferencesDock *pref_dock = new PreferencesDock(this);
    pref_dock->setHidden(true);
    pref_dock->setFloating(true);
    addDockWidget(Qt::RightDockWidgetArea, pref_dock);

    ui->menu_docks->addAction(ui->dock_pending_jobs_list->toggleViewAction());
    ui->menu_docks->addAction(ui->dock_custom_professions->toggleViewAction());
    ui->menu_docks->addAction(grid_view_dock->toggleViewAction());
    ui->menu_docks->addAction(skill_legend_dock->toggleViewAction());
    ui->menu_docks->addAction(dwarf_details_dock->toggleViewAction());
    ui->menu_docks->addAction(pref_dock->toggleViewAction());

    ui->menuWindows->addAction(ui->main_toolbar->toggleViewAction());

    LOGD << "setting up connections for MainWindow";
    connect(m_model, SIGNAL(new_creatures_count(int,int,int, QString)), this, SLOT(new_creatures_count(int,int,int, QString)));
    connect(m_model, SIGNAL(new_pending_changes(int)), this, SLOT(new_pending_changes(int)));
    connect(ui->act_clear_pending_changes, SIGNAL(triggered()), m_model, SLOT(clear_pending()));
    connect(ui->act_commit_pending_changes, SIGNAL(triggered()), m_model, SLOT(commit_pending()));
    connect(ui->act_expand_all, SIGNAL(triggered()), m_view_manager, SLOT(expand_all()));
    connect(ui->act_collapse_all, SIGNAL(triggered()), m_view_manager, SLOT(collapse_all()));
    connect(ui->act_add_new_gridview, SIGNAL(triggered()), grid_view_dock, SLOT(add_new_view()));

    //thistleknot
    connect(ui->btn_clear_labors, SIGNAL(clicked()), this, SLOT(clear_labors()));
    connect(ui->btn_optimize_labors, SIGNAL(clicked()), this, SLOT(initializeSuperStruct()));

    connect(ui->list_custom_professions, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(draw_custom_profession_context_menu(const QPoint &)));
    connect(ui->tree_pending, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
            m_view_manager, SLOT(jump_to_dwarf(QTreeWidgetItem *, QTreeWidgetItem *)));
    connect(ui->list_custom_professions, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)),
        m_view_manager, SLOT(jump_to_profession(QListWidgetItem *, QListWidgetItem *)));
    connect(m_view_manager, SIGNAL(dwarf_focus_changed(Dwarf*)), dwarf_details_dock, SLOT(show_dwarf(Dwarf*)));
    connect(ui->cb_filter_script, SIGNAL(currentIndexChanged(const QString &)), SLOT(new_filter_script_chosen(const QString &)));
    connect(m_script_dialog, SIGNAL(apply_script(const QString &)), m_proxy, SLOT(apply_script(const QString&)));
    connect(m_script_dialog, SIGNAL(scripts_changed()), SLOT(reload_filter_scripts()));
    connect(m_view_manager,SIGNAL(group_changed(int)), this, SLOT(display_group(int)));
    connect(pref_dock,SIGNAL(item_selected(QStringList)),this,SLOT(preference_selected(QStringList)));

    m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, COMPANY, PRODUCT, this);

    m_progress->setVisible(false);
    statusBar()->addPermanentWidget(m_lbl_message, 0);    
    statusBar()->addPermanentWidget(m_lbl_status, 0);
    set_interface_enabled(false);

    ui->cb_group_by->setItemData(0, DwarfModel::GB_NOTHING);    
    ui->cb_group_by->addItem(tr("Caste"), DwarfModel::GB_CASTE);
    ui->cb_group_by->addItem(tr("Current Job"), DwarfModel::GB_CURRENT_JOB);
    ui->cb_group_by->addItem(tr("Happiness"), DwarfModel::GB_HAPPINESS);
    ui->cb_group_by->addItem(tr("Has Nickname"),DwarfModel::GB_HAS_NICKNAME);
    ui->cb_group_by->addItem(tr("Highest Moodable Skill"), DwarfModel::GB_HIGHEST_MOODABLE);
    ui->cb_group_by->addItem(tr("Highest Skill"), DwarfModel::GB_HIGHEST_SKILL);    
    ui->cb_group_by->addItem(tr("Legendary Status"), DwarfModel::GB_LEGENDARY);
    ui->cb_group_by->addItem(tr("Migration Wave"),DwarfModel::GB_MIGRATION_WAVE);
    ui->cb_group_by->addItem(tr("Military Status"),DwarfModel::GB_MILITARY_STATUS);
    ui->cb_group_by->addItem(tr("Profession"), DwarfModel::GB_PROFESSION);
    ui->cb_group_by->addItem(tr("Race"), DwarfModel::GB_RACE);
    ui->cb_group_by->addItem(tr("Sex"), DwarfModel::GB_SEX);
    ui->cb_group_by->addItem(tr("Squad"), DwarfModel::GB_SQUAD);
    ui->cb_group_by->addItem(tr("Total Assigned Labors"),DwarfModel::GB_ASSIGNED_LABORS);
    ui->cb_group_by->addItem(tr("Total Skill Levels"),DwarfModel::GB_TOTAL_SKILL_LEVELS);

    read_settings();
    draw_professions();
    reload_filter_scripts();
    refresh_role_menus();

    if (m_settings->value("options/check_for_updates_on_startup", true).toBool())
        check_latest_version();

    //m_view_manager->reload_views();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::clear_labors()
{
    foreach(Dwarf *d, m_view_manager->get_selected_dwarfs())

    {
        foreach(int key, d->get_labors().uniqueKeys())
        {
            if(d->labor_enabled(key))
            {
                d->toggle_labor(key);
            }

        }
    }
    m_model->calculate_pending();
}

void MainWindow::initializeSuperStruct()
{
    using namespace std;

    QList<Dwarf*> m_selected_dwarfs = m_view_manager->get_selected_dwarfs();
    int d_count = m_selected_dwarfs.size();

    //get labors per dwarf
    bool ok;
    QString labors_p = QInputDialog::getText(0, tr("Labors per dwarf"),
         tr("How many dwarves do you want to assign per labor?"), QLineEdit::Normal,"", &ok);
    if (!ok)
        return;
    int labors_per = labors_p.toInt();

    //used to derive %'s for dwarfs
    float totalCoverage = 0;
    //Miner, Hunter, Woodcutter total Coverage;
    float MHWCoverage = 0;
    float modifiedTotalCoverage = 0;

    //get csv filename
    ok = 0;

    QString file_name = QInputDialog::getText(0, tr("CSV input file"),
         tr("Name of input file for assigning labors?"), QLineEdit::Normal,"", &ok);
    if (!ok)
        return;
    //int file_name = csv_n.toInt();

    ok = 0;

    QString percent_jobs = QInputDialog::getText(0, tr("Percent of Population/Jobs to assign"),
         tr("What percent do you wish to assign? (include decimal, i.e. .7)"), QLineEdit::Normal,"", &ok);
    if (!ok)
        return;
    float pct_jobs = percent_jobs.toFloat();

    //dwarf count, to be pulled from selection
    //labor count, to be pulled from init .csv
    int role_count = 0;
    int total_jobs = m_selected_dwarfs.size() * labors_per;
    //rounding is done later
    float jobs_to_assign = total_jobs * pct_jobs;

    bool MHWExceed = 0;

    string st;

    //SUPERSTRUCT!
    QVector <superStruct> sorter;

    QVector <roles> ListOfRolesV;

    //ifstream f( filename.c_str() );
    //ifstream f("role_labor_list.csv");
    string f_holder = file_name.toStdString();
    ifstream f(f_holder.c_str());
    //ifstream f (file_name.c_str());


      //loops through each line in the file
      while (getline (f, st))
      {
          int pos = 1;
          //placeholder
          roles p_holder;

          //gets the line for processing
          istringstream iss( st );

            //loops through each , in one line
            while (getline (iss, st, ','))
            {
                //role_name, labor_id, priority_weight, num_to_assign
                if (pos == 1)
                {
                    p_holder.role_name = QString::fromStdString(st);
                }
                else if (pos == 2)
                {
                    int fieldValue = 0;
                    istringstream (st) >> fieldValue;

                    p_holder.labor_id = fieldValue;
                }
                else if (pos == 3)
                {
                    float fieldValue = 0.0f;
                    istringstream (st) >> fieldValue;

                    p_holder.priority = fieldValue;
                }
                else if (pos == 4)
                {
                    float fieldValue = 0;
                    istringstream (st) >> fieldValue;

                    p_holder.coverage = fieldValue;
                }
                //initialize numberAssigned to 0;
                p_holder.numberAssigned = 0;
                pos++;
            }
            ListOfRolesV.push_back(p_holder);
            pos = 1;
        }

        f.close();

        role_count = ListOfRolesV.size();

    //calculate total Coverage
        for (int x = 0; x < role_count; x++)
        {
            int labor = ListOfRolesV[x].labor_id;
            if ((labor == 0) || (labor == 10) || (labor == 44))
            {
                MHWCoverage = MHWCoverage + ListOfRolesV[x].coverage;

            }
            totalCoverage += ListOfRolesV[x].coverage;
        }

    //Check if ((MHWCoverage / totalCoverage ) * total_jobs )> m_selected_dwarfs.size();
        if (((MHWCoverage / totalCoverage) * jobs_to_assign ) > m_selected_dwarfs.size())
        {
            if (DT->user_settings()->value("options/labor_exclusions",true).toBool())
            {
                //only set flag if option is checked in options
                MHWExceed = 1;
            }

            modifiedTotalCoverage = totalCoverage - MHWCoverage;
        }
        else
        {
            modifiedTotalCoverage = totalCoverage;
        }

    //time to set numberToAssign
        for (int x = 0; x < role_count; x++)
        {
            int labor = ListOfRolesV[x].labor_id;
            float pre_number_to_assign = 0;


            if ( (MHWExceed) && ((labor == 0) || (labor == 10) || (labor == 44)))
            {
                //truncate miner, hunter, woodcutter to population size
                pre_number_to_assign = ListOfRolesV[x].coverage / MHWCoverage * m_selected_dwarfs.size();
            }
                else if (MHWExceed)
                {
                    //since miner, hunter, woodcutter are being truncated down to population size, we need to remove that from the jobs to assign
                    pre_number_to_assign = ListOfRolesV[x].coverage / modifiedTotalCoverage * (jobs_to_assign - m_selected_dwarfs.size());
                }
            else
            {
                //normal
                pre_number_to_assign = (ListOfRolesV[x].coverage / modifiedTotalCoverage) * jobs_to_assign;
            }
            //old, works without conflicting labors
            //float pre_number_to_assign = (ListOfRolesV[x].coverage / totalCoverage) * jobs_to_assign;

            //rounding magic
            if (( pre_number_to_assign + 0.5) >= (int(pre_number_to_assign ) + 1) )
            {
                ListOfRolesV[x].numberToAssign = int(pre_number_to_assign)+1;
            }
            else
            {
                ListOfRolesV[x].numberToAssign = int(pre_number_to_assign);
            }
        }

    //now time to initialize the superStruct

        //count # dwarf's
        //foreach(Dwarf *d, m_view_manager->get_selected_dwarfs()) {d_count++;}

        //resize based on # dwarfs * # of roles
        sorter.resize(d_count*role_count);
        QString *temp;

        int sstruct_pos = 0;
        //for(int sstruct_pos = 0; sstruct_pos < sorter.size(); sstruct_pos++)
        {
            //cycle through each Dwarf,
            foreach (Dwarf *d, m_view_manager->get_selected_dwarfs())
            {
                int dwarf_count = 0;
                //cycle through each Role in ListOfRolesV
                for (int role_entry = 0; role_entry < ListOfRolesV.size(); role_entry++)
                {
                    //problem here... need a role per dwarf which I have, but Dwarf doesn't have anywhere to save a w_percent...
                    //Could just load into superStruct, but it won't be a pointer...
                    sorter[sstruct_pos].d_id = d->id();
                    sorter[sstruct_pos].labor_id = ListOfRolesV[role_entry].p_l_id();
                    //QString role_name = ListOfRolesV[role_entry].p_r_name();
                    //not sure if this is what I want
                    sorter[sstruct_pos].role_name = ListOfRolesV[role_entry].p_r_name();

                    //can't use pointers with this
                    sorter[sstruct_pos].r_percent = d->get_raw_role_rating(ListOfRolesV[role_entry].role_name);
                    sorter[sstruct_pos].w_percent = ListOfRolesV[role_entry].priority * d->get_raw_role_rating(ListOfRolesV[role_entry].role_name);

                    sstruct_pos++;
                }
                //probably not needed
                dwarf_count ++;

            }
        }
        //used for debug pause
        int test = 0;

        //sort superstruct by w_percent

        //runs through list
        for (int i = 0; i < sorter.size(); i++)
        {
            //checks against lower element
            //cout << *s->at(i).pPercent << endl;
            for (int j = 1; j < (sorter.size()-i); j++)
            {
                //using pointer dereferences messed this up, but, not using them doesn't sort right
                if (sorter[j-1].w_percent < sorter[j].w_percent)
                {
                    swap(sorter[j-1], sorter[j]);
                }
            }
        }

    //now to assign labors!

        //vs running through each labor, and assigning up to max # dwarf's...
          //1. need to start from top of allRoles
          //2. see what labor is, see if goal is met.
            //DONE (via csv input) for DEPLOY APP, need to change this to see if labor is asked for
          //3. Y - Skip (use while statement)
          //4. N -
            //a. Check Dwarf to see if he is available (again, while Statement),
                //not available if has conflicting labor
            //  Y - Assign
            //  N - Skip

        //start from top of list.
        for (int sPos = 0; sPos < sorter.size(); sPos++)
        {

            //dwarf position (used for searching of name)

            //I would like to set these to pointers, but I was getting out of bound issues...
            int dPos = 0;
            //role position
            int rPos = 0;
            //cycle through each dwarf? I should, but, I can use the *s->at(x).name to find the Dwarf #
            //no, need to compare roleName to dwarfName

            //search for Dwarf (y) for that role            

            for (int d = 0; d < d_count; d++)
            {
                //dPos is never getting updated.
                //int temp = d->id();
                if (sorter[sPos].d_id == m_selected_dwarfs.at(d)->id())
                {
                    dPos = d;
                    //break;
                };
                //dPos++;
            }

            //match superStruct roleName to myRoles name position (to check if filled up)
            for (int r = 0; r < ListOfRolesV.size(); r++)
            {
                if (sorter[sPos].role_name == ListOfRolesV[r].role_name)
                {
                    rPos = r;
                    //break;
                };
            }

            //search if role @ rPos is filled up
            //role check started...
            int number_to_assign = *ListOfRolesV[rPos].n_assign();

            if (ListOfRolesV[rPos].numberAssigned < number_to_assign)
            {
                int labor = ListOfRolesV[rPos].labor_id;
                bool incompatible = 0;
                //Miner is 0
                //Woodcutter is 10
                //Hunter is 44
                //only set incompatible flag if option is set.
                if (DT->user_settings()->value("options/labor_exclusions",true).toBool())
                {
                    if ((labor == 0) || (labor == 10) || (labor == 44))
                    {
                        if ((m_selected_dwarfs.at(dPos)->labor_enabled(0)) || (m_selected_dwarfs.at(dPos)->labor_enabled(10)) || (m_selected_dwarfs.at(dPos)->labor_enabled(44)))
                        {
                            //dwarf has an incompatible labor, set flag
                            incompatible = 1;
                        }
                    }
                }


                //need to compare to total_assigned_labors,
                //this will be used for Custom Professions

                //I'm going to have to have a dwarf labors_assigned variable to count for
                //CUSTOM PROFESSIONS
                int d_currentAssigned = m_selected_dwarfs.at(dPos)->get_dirty_labors().size();
                        //m_selected_dwarfs.at(dPos)->total_assigned_labors();

              //is Dwarf (d) laborsAssigned full? Used in proto-type
              if (d_currentAssigned < labors_per)
              {
                  //if Dwarf doesn't have an incompatible labor enabled, proceed.
                  if (incompatible == 0)
                  {
                      //another check to check for conflicting laborid's,
                      //AND
                      //to see if labor_id flag is checked in Options.
                      //m_selected_dwarfs.at(dPos)->toggle_labor(ListOfRolesV[rPos].labor_id);
                      m_selected_dwarfs.at(dPos)->toggle_labor(labor);
                      //assign labor (z) to Dwarf @ dPos
                      //add labor (z?) to Dwarf's (d) labor vector

                      //m_selected_dwarfs.at(dPos).laborsAssigned.push_back(r->at(rPos).name);

                      //increase labors assigned to roles vector
                      //role check completed
                      ListOfRolesV[rPos].numberAssigned++;
                  }
                }
              incompatible = 0;
            }

        }
     m_model->calculate_pending();


};

void MainWindow::optimize_labors()
{

}

void MainWindow::read_settings() {
    m_reading_settings = true;
    m_settings->beginGroup("window");
    { // WINDOW SETTINGS
        QByteArray geom = m_settings->value("geometry").toByteArray();
        if (!geom.isEmpty()) {
            restoreGeometry(geom);
        }
        // Toolbars etc...
        QByteArray state = m_settings->value("state").toByteArray();
        if (!state.isEmpty()) {
            restoreState(state);
        }
    }
    m_settings->endGroup();

    m_settings->beginGroup("gui_options");
    { // GUI OPTIONS
        int group_by = m_settings->value("group_by", 0).toInt();
        ui->cb_group_by->setCurrentIndex(group_by);
        m_model->set_group_by(group_by);
    }
    m_settings->endGroup();
    m_reading_settings = false;

}

void MainWindow::write_settings() {
    if (m_settings && !m_reading_settings) {
        LOGD << "beginning to write settings";
        QByteArray geom = saveGeometry();
        QByteArray state = saveState();
        m_settings->beginGroup("window");
        m_settings->setValue("geometry", QVariant(geom));
        m_settings->setValue("state", QVariant(state));
        m_settings->endGroup();
        m_settings->beginGroup("gui_options");
        m_settings->setValue("group_by", m_model->current_grouping());

        DwarfDetailsDock *dock = qobject_cast<DwarfDetailsDock*>(QObject::findChild<DwarfDetailsDock*>("dock_dwarf_details"));
        if(dock){
            QByteArray sizes = dock->splitter_sizes();
            if(!sizes.isNull())
                m_settings->setValue("detailPanesSizes", sizes);
        }

        m_settings->endGroup();

        LOGD << "finished writing settings";
    }
}

void MainWindow::closeEvent(QCloseEvent *evt) {
    LOGI << "Beginning shutdown";
    if(!m_deleting_settings) {
        write_settings();
        m_view_manager->write_views();
    }
    evt->accept();
    LOGI << "Closing Dwarf Therapist normally";
}

void MainWindow::connect_to_df() {
    LOGD << "attempting connection to running DF game";
    if (m_df) {
        LOGD << "already connected, disconnecting";
        delete m_df;
        set_interface_enabled(false);
        m_df = 0;
    }

#ifdef Q_WS_WIN
    m_df = new DFInstanceWindows();
#else
#ifdef Q_WS_MAC
    m_df = new DFInstanceOSX();
#else
#ifdef Q_WS_X11
    m_df = new DFInstanceLinux();
#endif
#endif
#endif
    // find_running_copy can fail for several reasons, and will take care of
    // logging and notifying the user.
    if (m_force_connect && m_df && m_df->find_running_copy(true)) {
        m_scanner = new Scanner(m_df, this);


        if(m_df->memory_layout()){
            LOGD << "Connection to DF version" << m_df->memory_layout()->game_version() << "established.";
            m_lbl_status->setText(tr("Connected to %1").arg(m_df->memory_layout()->game_version()));
            m_lbl_status->setToolTip(tr("Currently using layout file: %1").arg(m_df->memory_layout()->filename()));
        }else{
            LOGD << "Connection to unknown DF Version established.";
            m_lbl_status->setText("Connected to unknown version");
        }

        set_interface_enabled(true);

        connect(m_df, SIGNAL(progress_message(QString)),
                SLOT(set_progress_message(QString)));
        connect(m_df, SIGNAL(progress_range(int,int)),
                SLOT(set_progress_range(int,int)));
        connect(m_df, SIGNAL(progress_value(int)),
                SLOT(set_progress_value(int)));

        if (m_df->memory_layout() && m_df->memory_layout()->is_complete()) {
            // if the memory layout is still being mapped don't read all this
            // in yet
            //DT->load_game_translation_tables(m_df);
            connect(m_df, SIGNAL(connection_interrupted()),
                    SLOT(lost_df_connection()));

            m_df->load_game_data();
            //Read raws once memory layout is complete
            //m_df->read_raws();
            if(m_view_manager)
                m_view_manager->reload_views();
        }

    } else if (m_df && m_df->find_running_copy() && m_df->is_ok()) {
        m_scanner = new Scanner(m_df, this);
        LOGD << "Connection to DF version" << m_df->memory_layout()->game_version() << "established.";
        m_lbl_status->setText(tr("Connected to %1").arg(m_df->memory_layout()->game_version()));
        m_lbl_status->setToolTip(tr("Currently using layout file: %1").arg(m_df->memory_layout()->filename()));
        set_interface_enabled(true);
        connect(m_df, SIGNAL(progress_message(QString)),
                SLOT(set_progress_message(QString)));
        connect(m_df, SIGNAL(progress_range(int,int)),
                SLOT(set_progress_range(int,int)));
        connect(m_df, SIGNAL(progress_value(int)),
                SLOT(set_progress_value(int)));

        if (m_df->memory_layout()->is_complete()) {
            // if the memory layout is still being mapped don't read all this
            // in yet
            //DT->load_game_translation_tables(m_df);
            connect(m_df, SIGNAL(connection_interrupted()),
                    SLOT(lost_df_connection()));

            m_df->load_game_data();
            //Read raws once memory layout is complete
            //m_df->read_raws();
            if(m_view_manager)
                m_view_manager->reload_views();
            if (DT->user_settings()->value("options/read_on_startup",
                                           true).toBool()) {
                read_dwarves();
            }
        }
    } else {
        m_force_connect = true;
    }
}

void MainWindow::lost_df_connection() {
    LOGW << "lost connection to DF";
    if (m_df) {
        m_model->clear_all();
        delete m_df;
        m_df = 0;
        set_interface_enabled(false);
        m_lbl_status->setText(tr("Not Connected"));
        QMessageBox::information(this, tr("Unable to talk to Dwarf Fortress"),
            tr("Dwarf Fortress has either stopped running, or you unloaded "
               "your game. Please re-connect when a fort is loaded."));
    }
}

void MainWindow::read_dwarves() {
    if (!m_df || !m_df->is_ok()) {
        lost_df_connection();
        return;
    }
    m_model->set_instance(m_df);
    m_model->load_dwarves();

    if (m_model->get_dwarves().size() < 1) {
        lost_df_connection();
        return;
    }

    set_interface_enabled(true);
    new_pending_changes(0);
    // cheap trick to setup the view correctly
    m_view_manager->redraw_current_tab();

    // setup the filter auto-completer
    m_dwarf_names_list.clear();
    foreach(Dwarf *d, m_model->get_dwarves()) {
        m_dwarf_names_list << d->nice_name();
    }
    if (!m_dwarf_name_completer) {
        m_dwarf_name_completer = new QCompleter(m_dwarf_names_list, this);
        m_dwarf_name_completer->setCompletionMode(QCompleter::PopupCompletion);
        m_dwarf_name_completer->setCaseSensitivity(Qt::CaseInsensitive);
        ui->le_filter_text->setCompleter(m_dwarf_name_completer);
    }

    PreferencesDock *dock = qobject_cast<PreferencesDock*>(QObject::findChild<PreferencesDock*>("dock_preferences"));
    if(dock){
        dock->refresh();
    }
}

void MainWindow::set_interface_enabled(bool enabled) {
    ui->act_connect_to_DF->setEnabled(!enabled);
    ui->act_read_dwarves->setEnabled(enabled);
    //ui->act_scan_memory->setEnabled(enabled);
    ui->act_expand_all->setEnabled(enabled);
    ui->act_collapse_all->setEnabled(enabled);
    ui->cb_group_by->setEnabled(enabled);
    ui->act_import_existing_professions->setEnabled(enabled);
    ui->act_print->setEnabled(enabled);
}

void MainWindow::check_latest_version(bool show_result_on_equal) {
    return;
    m_show_result_on_equal = show_result_on_equal;
    Version our_v(DT_VERSION_MAJOR, DT_VERSION_MINOR, DT_VERSION_PATCH);

    QHttpRequestHeader header("GET", "/version");
    header.setValue("Host", "dt-tracker.appspot.com");
    //header.setValue("Host", "localhost");
    header.setValue("User-Agent", QString("DwarfTherapist %1").arg(our_v.to_string()));
    if (m_http) {
        m_http->deleteLater();
    }
    m_http = new QHttp(this);
    m_http->setHost("dt-tracker.appspot.com");
    //m_http->setHost("localhost", 8080);
    disconnect(m_http, SIGNAL(done(bool)));
    connect(m_http, SIGNAL(done(bool)), this, SLOT(version_check_finished(bool)));
    m_http->request(header);
}

void MainWindow::version_check_finished(bool error) {
    if (error) {
        qWarning() << m_http->errorString();
    }
    QString data = QString(m_http->readAll());
    QRegExp rx("(\\d+)\\.(\\d+)\\.(\\d+)");
    int pos = rx.indexIn(data);
    if (pos != -1) {
        Version our_v(DT_VERSION_MAJOR, DT_VERSION_MINOR, DT_VERSION_PATCH);
        QString major = rx.cap(1);
        QString minor = rx.cap(2);
        QString patch = rx.cap(3);
        Version newest_v(major.toInt(), minor.toInt(), patch.toInt());
        LOGI << "RUNNING VERSION         :" << our_v.to_string();
        LOGI << "LATEST AVAILABLE VERSION:" << newest_v.to_string();
        if (our_v < newest_v) {
            LOGI << "LATEST VERSION IS NEWER!";
            QMessageBox *mb = new QMessageBox(this);
            mb->setIcon(QMessageBox::Information);
            mb->setWindowTitle(tr("Update Available"));
            mb->setText(tr("A newer version of this application is available."));
            QString link = tr("<br><a href=\"%1\">Click Here to Download v%2"
                              "</a>")
                           .arg(URL_DOWNLOAD_LIST)
                           .arg(newest_v.to_string());
            mb->setInformativeText(tr("You are currently running v%1. %2")
                                   .arg(our_v.to_string()).arg(link));
            mb->exec();
        } else if (m_show_result_on_equal) {
            QMessageBox *mb = new QMessageBox(this);
            mb->setWindowTitle(tr("Up to Date"));
            mb->setText(tr("You are running the most recent version of Dwarf "
                           "Therapist."));
            mb->exec();
        }
        m_about_dialog->set_latest_version(newest_v);
    } else {
        m_about_dialog->version_check_failed();
    }
}

void MainWindow::check_for_layout(const QString & checksum) {
    if(m_try_download &&
            (m_settings->value("options/check_for_updates_on_startup", true).toBool())) {
        m_try_download = false;

        LOGI << "Checking for layout for checksum: " << checksum;
        m_tmp_checksum = checksum;

        Version our_v(DT_VERSION_MAJOR, DT_VERSION_MINOR, DT_VERSION_PATCH);

        QString request = QString("/memory_layouts/checksum/%1").arg(checksum);
        QHttpRequestHeader header("GET", request);
        header.setValue("Host", "www.dwarftherapist.com");
        header.setValue("User-Agent", QString("DwarfTherapist %1").arg(our_v.to_string()));
        if (m_http) {
            m_http->deleteLater();
        }
        m_http = new QHttp(this);
        m_http->setHost("www.dwarftherapist.com");

        disconnect(m_http, SIGNAL(done(bool)));
        connect(m_http, SIGNAL(done(bool)), this, SLOT(layout_check_finished(bool)));
        m_http->request(header);
    } else if (!m_force_connect) {
        m_df->layout_not_found(checksum);
    }
}

void MainWindow::layout_check_finished(bool error) {
    int status = m_http->lastResponse().statusCode();
    LOGD << "Status: " << status;

    error = error || (status != 200);
    if(!error) {
        QTemporaryFile outFile("layout.ini");
        if (!outFile.open())
         return;

        QString fileName = outFile.fileName();
        QTextStream out(&outFile);
        out << m_http->readAll();
        outFile.close();

        QString version;

        {
            QSettings layout(fileName, QSettings::IniFormat);
            version = layout.value("info/version_name", "").toString();
        }

        LOGD << "Found version" << version;

        if(m_df->add_new_layout(version, outFile)) {
            QMessageBox *mb = new QMessageBox(this);
            mb->setIcon(QMessageBox::Information);
            mb->setWindowTitle(tr("New Memory Layout Added"));
            mb->setText(tr("A new memory layout has been downloaded for this version of dwarf fortress!"));
            mb->setInformativeText(tr("New layout for version %1 of Dwarf Fortress.").arg(version));
            mb->exec();

            LOGD << "Reconnecting to Dwarf Fortress!";
            m_force_connect = false;
            connect_to_df();
        } else {
            error = true;
        }
    }

    LOGD << "Error: " << error << " Force Connect: " << m_force_connect;

    if(error) {
        m_df->layout_not_found(m_tmp_checksum);
    }
}

void MainWindow::scan_memory() {
    if (m_scanner) {
        m_scanner->show();
    }
}

void MainWindow::set_group_by(int group_by) {
    m_view_manager->set_group_by(group_by);
}

void MainWindow::show_about() {
    m_about_dialog->show();
}

void MainWindow::new_pending_changes(int cnt) {
    bool on_off = cnt > 0;
    ui->lbl_pending_changes->setNum(cnt);
    ui->btn_clear->setEnabled(on_off);
    ui->btn_commit->setEnabled(on_off);
    ui->act_clear_pending_changes->setEnabled(on_off);
    ui->act_commit_pending_changes->setEnabled(on_off);
    list_pending();
}

void MainWindow::list_pending() {
    disconnect(ui->tree_pending, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
        0, 0);
    ui->tree_pending->clear();
    foreach(Dwarf *d, m_model->get_dirty_dwarves()) {
        ui->tree_pending->addTopLevelItem(d->get_pending_changes_tree());
    }
    ui->tree_pending->expandAll();
    connect(ui->tree_pending, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
        m_view_manager, SLOT(jump_to_dwarf(QTreeWidgetItem *, QTreeWidgetItem *)));
}

void MainWindow::new_creatures_count(int adults, int children, int babies, QString race_name) {
    ui->lbl_dwarf_total->setText(tr("%1/%2/%3").arg(adults).arg(children).arg(babies));
    ui->lbl_dwarf_total->setToolTip(tr("%1 Adult%2<br>%3 Child%4<br>%5 Bab%6<br>%7 Total Population")
                                    .arg(adults).arg(adults == 1 ? "" : "s")
                                    .arg(children).arg(children == 1 ? "" : "ren")
                                    .arg(babies).arg(babies == 1 ? "y" : "ies")
                                    .arg(adults+children+babies));
    ui->lbl_dwarfs->setText(race_name);
    ui->lbl_filter->setText("Filter " + race_name);
    ui->act_read_dwarves->setText("Read " + race_name);
    //TODO: update other interface stuff for the race name when using a custom race
}

void MainWindow::draw_professions() {
    disconnect(ui->list_custom_professions, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)),
        0, 0);
    ui->list_custom_professions->clear();
    QVector<CustomProfession*> profs = DT->get_custom_professions();
    foreach(CustomProfession *cp, profs) {
        new QListWidgetItem(cp->get_name(), ui->list_custom_professions);
    }
    connect(ui->list_custom_professions, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)),
        m_view_manager, SLOT(jump_to_profession(QListWidgetItem *, QListWidgetItem *)));
    // allow exports only when there are profs to export
    ui->act_export_custom_professions->setEnabled(profs.size());
}

void MainWindow::draw_custom_profession_context_menu(const QPoint &p) {
    QModelIndex idx = ui->list_custom_professions->indexAt(p);
    if (!idx.isValid())
        return;

    QString cp_name = idx.data().toString();

    QMenu m(this);
    m.setTitle(tr("Custom Profession"));
    QAction *a = m.addAction(tr("Edit..."), DT, SLOT(edit_custom_profession()));
    a->setData(cp_name);
    a = m.addAction(tr("Delete..."), DT, SLOT(delete_custom_profession()));
    a->setData(cp_name);
    m.exec(ui->list_custom_professions->viewport()->mapToGlobal(p));
}

// web addresses
void MainWindow::go_to_forums() {
    QDesktopServices::openUrl(QUrl("http://udpviper.com/forums/viewforum.php?f=36"));
}
void MainWindow::go_to_donate() {
    QDesktopServices::openUrl(QUrl("http://code.google.com/p/dwarftherapist/wiki/Donations"));
}
void MainWindow::go_to_project_home() {
    QDesktopServices::openUrl(QUrl("http://code.google.com/p/dwarftherapist"));
}
void MainWindow::go_to_new_issue() {
    QDesktopServices::openUrl(QUrl("http://code.google.com/p/dwarftherapist/issues/entry"));
}

QToolBar *MainWindow::get_toolbar() {
    return ui->main_toolbar;
}

void MainWindow::export_custom_professions() {
    ImportExportDialog d(this);
    d.setup_for_profession_export();
    d.exec();
}

void MainWindow::import_custom_professions() {
    ImportExportDialog d(this);
    d.setup_for_profession_import();
    d.exec();
}

void MainWindow::save_gridview()
{
    m_model->save_rows();
}

void MainWindow::export_gridviews() {
    ImportExportDialog d(this);
    d.setup_for_gridview_export();
    d.exec();
}

void MainWindow::import_gridviews() {
    ImportExportDialog d(this);
    d.setup_for_gridview_import();
    if (d.exec()) {
        GridViewDock *dock = qobject_cast<GridViewDock*>(QObject::findChild<GridViewDock*>("GridViewDock"));
        if (dock)
            dock->draw_views();        
    }
}

void MainWindow::clear_user_settings() {
    QMessageBox *mb = new QMessageBox(qApp->activeWindow());
    mb->setIcon(QMessageBox::Warning);
    mb->setWindowTitle(tr("Clear User Settings"));
    mb->setText(tr("Warning: This will delete all of your user settings and exit Dwarf Therapist!"));
    mb->addButton(QMessageBox::Ok);
    mb->addButton(QMessageBox::Cancel);
    if(QMessageBox::Ok == mb->exec()) {
        //Delete data
        m_settings->clear();
        m_settings->sync();

        QFile file(m_settings->fileName());
        LOGI << "Removing file:" << m_settings->fileName();

        delete m_settings;
        m_settings = 0;

        if(!file.remove()) {
            LOGW << "Error removing file!";
            delete mb;
            mb = new QMessageBox(qApp->activeWindow());
            mb->setIcon(QMessageBox::Critical);
            mb->setWindowTitle("Clear User Settings");
            mb->setText(tr("Unable to delete settings file."));
            mb->exec();
            return;
        }

        m_deleting_settings = true;
        close();
    }
}


void MainWindow::show_dwarf_details_dock(Dwarf *d) {
    DwarfDetailsDock *dock = qobject_cast<DwarfDetailsDock*>(QObject::findChild<DwarfDetailsDock*>("dock_dwarf_details"));
    if (dock && d) {
        dock->show_dwarf(d);
        dock->show();        
    }
}

void MainWindow::add_new_filter_script() {
    m_script_dialog->clear_script();
    m_script_dialog->show();
}

void MainWindow::edit_filter_script() {
    QAction *a = qobject_cast<QAction*>(QObject::sender());
    QStringList script = a->data().toStringList();
    m_script_dialog->load_script(script.at(0),script.at(1));
    m_script_dialog->show();
}

void MainWindow::remove_filter_script(){
    QAction *a = qobject_cast<QAction*>(QObject::sender());
    QString name =a->data().toString();

    int answer = QMessageBox::question(0,"Confirm Remove",tr("Are you sure you want to remove script: <b>%1</b>?").arg(name),QMessageBox::Yes,QMessageBox::No);
    if(answer == QMessageBox::Yes){
        QSettings *s = DT->user_settings();
        s->remove(QString("filter_scripts/%1").arg(name));
    }
    reload_filter_scripts();
}

void MainWindow::reload_filter_scripts() {
    ui->cb_filter_script->clear();
    ui->cb_filter_script->addItem(tr("None"));

    ui->menu_edit_filters->clear();
    ui->menu_remove_script->clear();
    QSettings *s = DT->user_settings();

    QMap<QString,QString> m_scripts;
    s->beginGroup("filter_scripts");
    foreach(QString script_name, s->childKeys()){
        m_scripts.insert(script_name,s->value(script_name).toString());
    }
    s->endGroup();

    QMap<QString, QString>::const_iterator i = m_scripts.constBegin();
    while(i != m_scripts.constEnd()){
        QStringList data;
        data.append(i.key());
        data.append(i.value());
        QAction *a = ui->menu_edit_filters->addAction(i.key(),this,SLOT(edit_filter_script()) );
        a->setData(data);

        QAction *r = ui->menu_remove_script->addAction(i.key(),this,SLOT(remove_filter_script()) );
        r->setData(i.key());
        i++;
    }
    ui->cb_filter_script->addItems(m_scripts.uniqueKeys());


//    s->beginGroup("filter_scripts");

//    ui->cb_filter_script->addItems(s->childKeys());

//    foreach(QString script_name, s->childKeys()){
//        QStringList data;
//        data.append(script_name);
//        data.append(s->value(script_name).toString());
//        QAction *a = ui->menu_edit_filters->addAction(script_name,this,SLOT(edit_filter_script()) );
//        a->setData(data);

//        QAction *r = ui->menu_remove_script->addAction(script_name,this,SLOT(remove_filter_script()) );
//        r->setData(script_name);
//    }
//    s->endGroup();


//    m_ordered_roles.clear();
//    QStringList role_names;
//    foreach(Role *r, m_dwarf_roles) {
//        role_names << r->name.toUpper();
//    }
//    qSort(role_names);
//    foreach(QString name, role_names) {
//        foreach(Role *r, m_dwarf_roles) {
//            if (r->name.toUpper() == name.toUpper()) {
//                m_ordered_roles << QPair<QString, Role*>(r->name, r);
//                break;
//            }
//        }
//    }
}

void MainWindow::add_new_custom_role() {
    roleDialog *edit = new roleDialog(this,"");

    if(edit->exec() == QDialog::Accepted){
        write_custom_roles();
        refresh_roles_data();
    }
}

void MainWindow::edit_custom_role() {
    QAction *a = qobject_cast<QAction*>(QObject::sender());
    QString name = a->data().toString();
    roleDialog *edit = new roleDialog(this,name);
    if(edit->exec() == QDialog::Accepted){
        write_custom_roles();
        refresh_roles_data();
    }

}

void MainWindow::remove_custom_role(){
    QAction *a = qobject_cast<QAction*>(QObject::sender());
    QString name = a->data().toString();
    int answer = QMessageBox::question(0,"Confirm Remove",tr("Are you sure you want to remove role: <b>%1</b>?").arg(name),QMessageBox::Yes,QMessageBox::No);
    if(answer == QMessageBox::Yes){
        GameDataReader::ptr()->get_roles().remove(name);

        //prompt and remove columns??
        answer = QMessageBox::question(0,"Clean Views",tr("Would you also like to remove role <b>%1</b> from all custom views?").arg(name),QMessageBox::Yes,QMessageBox::No);
        if(answer == QMessageBox::Yes){
            ViewManager *vm = m_view_manager;
            foreach(GridView *gv, vm->views()){
                if(gv->is_custom() && gv->is_active()){ //only remove from custom views which are active
                    foreach(ViewColumnSet *vcs, gv->sets()){
                        foreach(ViewColumn *vc, vcs->columns()){
                            if(vc->type()==CT_ROLE){
                                RoleColumn *rc = ((RoleColumn*)vc);
                                if(rc->get_role() && rc->get_role()->name==name){
                                    vcs->remove_column(vc);
                                }
                            }
                        }
                    }
                }
            }
        }
        //first write our custom roles
        write_custom_roles();
        //re-read roles from the ini to replace any default roles that may have been replaced by a custom role which was just removed
        //this will also rebuild our sorted role list
        GameDataReader::ptr()->load_roles();
        //update our current roles/ui elements
        DT->emit_roles_changed();
        refresh_role_menus();
        m_view_manager->update();
        m_view_manager->redraw_current_tab();
    }
}

void MainWindow::refresh_roles_data(){
    //GameDataReader::ptr()->load_roles();

    DT->emit_roles_changed();
    GameDataReader::ptr()->load_role_mappings();

    refresh_role_menus();
    m_view_manager->update();
    m_view_manager->redraw_current_tab();
}

void MainWindow::refresh_role_menus() {
    ui->menu_edit_roles->clear();
    ui->menu_remove_roles->clear();

    QList<QPair<QString, Role*> > roles = GameDataReader::ptr()->get_ordered_roles();
    QPair<QString, Role*> role_pair;
    foreach(role_pair, roles){
        if(role_pair.second->is_custom){
            QAction *edit = ui->menu_edit_roles->addAction(role_pair.first,this,SLOT(edit_custom_role()) );
            edit->setData(role_pair.first);

            QAction *rem = ui->menu_remove_roles->addAction(role_pair.first,this,SLOT(remove_custom_role()) );
            rem->setData(role_pair.first);
        }
    }
}

void MainWindow::write_custom_roles(){
    //re-write custom roles, ugly but doesn't seem that replacing only one works properly
    QSettings *s = DT->user_settings();
    s->remove("custom_roles");

    //read defaults before we start writing
    float default_attributes_weight = s->value("options/default_attributes_weight",1.0).toFloat();
    float default_skills_weight = s->value("options/default_skills_weight",1.0).toFloat();
    float default_traits_weight = s->value("options/default_traits_weight",1.0).toFloat();

    s->beginWriteArray("custom_roles");
    int count = 0;
    foreach(Role *r, GameDataReader::ptr()->get_roles()){
        if(r->is_custom){
            s->setArrayIndex(count);
            r->write_to_ini(*s, default_attributes_weight, default_traits_weight, default_skills_weight);
            count++;
        }
    }
    s->endArray();
}




void MainWindow::new_filter_script_chosen(const QString &script_name) {
    m_proxy->apply_script(DT->user_settings()->value(QString("filter_scripts/%1").arg(script_name), QString()).toString());
}

void MainWindow::print_gridview() {
    QWidget *curr = get_view_manager()->currentWidget();
    if(!curr)
        return;
    StateTableView *s;
    s = qobject_cast<StateTableView*>(curr);

    if(!s)
        return;

    if(!m_view_manager || !m_view_manager->get_active_view())
        return;

    QString path = QFileDialog::getSaveFileName(this,tr("Save Snapshot"),m_view_manager->get_active_view()->name(),tr("PNG (*.png);;All Files (*)"));
    if(path.isEmpty())
        return;

    int w = 0;
    int h = 0;
    QSize currSize = this->size();
    QSize currMax = this->maximumSize();
    QSize currMin = this->minimumSize();
    int cell_size = DT->user_settings()->value("options/grid/cell_size", DEFAULT_CELL_SIZE).toInt();

    //currently this is just a hack to resize the form to ensure all rows/columns are showing
    //then rendering to the painter and resizing back to the previous size
    //it may be possible to avoid this by using the opengl libs and accessing the frame buffer

    //calculate the width
    int first_col_width = s->columnWidth(0);
    w = first_col_width;
    w += (this->width() - s->width());
    foreach(ViewColumnSet *vcs, m_view_manager->get_active_view()->sets()){
        foreach(ViewColumn *vc, vcs->columns()){
            if(vc->type() != CT_SPACER)
                w += cell_size;
            else
                w += DEFAULT_SPACER_WIDTH;
        }
    }
    w += 2;

    //calculate the height
    h = (s->get_model()->total_row_count * cell_size) + s->get_header()->height();
    h += (this->height() - s->height());
    h += 2; //small buffer for the edge

    if(this->maximumHeight() < h)
        this->setMaximumHeight(h);
    if(this->maximumWidth() < w)
        this->setMaximumWidth(w);

    this->setMinimumSize(100,100);
    this->resize(w,h);

    QImage img(QSize(s->width(),s->height()),QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&img);
    painter.setRenderHints(QPainter::SmoothPixmapTransform);
    s->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    s->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    s->render(&painter,QPoint(0,0),QRegion(0,0,s->width(),s->height()),QWidget::DrawChildren | QWidget::DrawWindowBackground);
    painter.end();
    img.save(path,"PNG");
    s->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    s->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    this->setMaximumSize(currMax);
    this->setMinimumSize(currMin);
    this->resize(currSize);
}

///////////////////////////////////////////////////////////////////////////////
//! Progress Stuff
void MainWindow::set_progress_message(const QString &msg) {
    m_lbl_message->setText(msg);
}

void MainWindow::set_progress_range(int min, int max) {
    m_progress->setVisible(true);
    m_progress->setRange(min, max);
    m_progress->setValue(min);    
    statusBar()->insertPermanentWidget(1, m_progress, 1);
}

void MainWindow::set_progress_value(int value) {
    m_progress->setValue(value);
    if (value >= m_progress->maximum()) {
        statusBar()->removeWidget(m_progress);
        m_progress->setVisible(false);
        set_progress_message("");
    }
}

void MainWindow::display_group(const int group_by){
    //this is a signal sent from the view manager when we change tabs and update grouping
    //we also want to change the combobox's index, but not refresh the grid again
    ui->cb_group_by->blockSignals(true);
    ui->cb_group_by->setCurrentIndex(group_by);
    //write_settings();
    ui->cb_group_by->blockSignals(false);
}

void MainWindow::preference_selected(QStringList names){
    QString filter = "";
    if(!names.empty()){
        foreach(QString pref, names){
            filter.append(QString("d.has_preference('%1') && ").arg(pref.toLower()));
        }
        filter.chop(4);

        m_proxy->set_pref_script(filter);
    }
    else
        m_proxy->set_pref_script("");

    m_proxy->refresh_script();

//    QString filter = name;
//    if(!name.isEmpty()){
//        filter = m_df->get_preference_stats().value(name).names.join("' || d.nice_name()=='");
//        filter.prepend("(d.nice_name()=='").append("')");
//        m_proxy->set_pref_script(filter);
//    }
//    else
//        m_proxy->set_pref_script("");

//    m_proxy->refresh_script();
}
