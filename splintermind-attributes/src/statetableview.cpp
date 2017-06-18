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
#include "qmath.h"

#include "mainwindow.h"
#include "statetableview.h"
#include "dwarfmodel.h"
#include "dwarfmodelproxy.h"
#include "uberdelegate.h"
#include "rotatedheader.h"
#include "dwarf.h"
#include "defines.h"
#include "columntypes.h"
#include "gridview.h"
#include "viewcolumnset.h"
#include "laborcolumn.h"
#include "happinesscolumn.h"
#include "dwarftherapist.h"
#include "customprofession.h"
#include "viewmanager.h"
#include "squad.h"

StateTableView::StateTableView(QWidget *parent)
    : QTreeView(parent)
    , m_model(0)
    , m_proxy(0)
    , m_delegate(new UberDelegate(this))
    , m_header(new RotatedHeader(Qt::Horizontal, this))
    , m_expanded_rows(QList<int>())
    , m_last_sorted_col(0)
    , m_last_sort_order(Qt::AscendingOrder)
    , m_vscroll(0)
    , m_hscroll(0)
    , m_last_group_by(DwarfModel::GB_NOTHING)
{
    read_settings();

    setMouseTracking(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setUniformRowHeights(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setIndentation(8);
    //setFocusPolicy(Qt::NoFocus); // keep the dotted border off of things
    setFocusPolicy(Qt::ClickFocus);

    setItemDelegate(m_delegate);
    setHeader(m_header);

    verticalScrollBar()->setFocusPolicy(Qt::StrongFocus);
    horizontalScrollBar()->setFocusPolicy(Qt::StrongFocus);

    // Set StaticContents to enable minimal repaints on resizes.
    viewport()->setAttribute(Qt::WA_StaticContents);

    connect(DT, SIGNAL(settings_changed()), this, SLOT(read_settings()));
    connect(this, SIGNAL(expanded(const QModelIndex &)),
            SLOT(index_expanded(const QModelIndex &)));
    connect(this, SIGNAL(collapsed(const QModelIndex &)),
            SLOT(index_collapsed(const QModelIndex &)));
    connect(this, SIGNAL(clicked(const QModelIndex &)),
            SLOT(clicked(const QModelIndex &)));
    connect(m_header, SIGNAL(sectionPressed(int)), SLOT(header_pressed(int)));
    connect(m_header, SIGNAL(sectionClicked(int)), SLOT(header_clicked(int)));

    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(hscroll_value_changed(int)));
    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(vscroll_value_changed(int)));
}

StateTableView::~StateTableView()
{}

void StateTableView::read_settings() {
    QSettings *s = DT->user_settings();

    //font
    QFont fnt = s->value("options/grid/font", QFont("Segoe UI", 8)).value<QFont>();
    setFont(fnt);

    //cell size
    m_grid_size = s->value("options/grid/cell_size", DEFAULT_CELL_SIZE).toInt();
    int pad = s->value("options/grid/cell_padding", 0).toInt();
    setIconSize(QSize(m_grid_size - 2 - pad * 2, m_grid_size - 2 - pad * 2));

    set_single_click_labor_changes(s->value("options/single_click_labor_changes", true).toBool());
    m_auto_expand_groups = s->value("options/auto_expand_groups", false).toBool();
}

void StateTableView::set_model(DwarfModel *model, DwarfModelProxy *proxy) {
    QTreeView::setModel(proxy);
    m_model = model;
    m_proxy = proxy;

    m_delegate->set_model(model);
    m_delegate->set_proxy(proxy);

    connect(m_header, SIGNAL(section_right_clicked(int)), m_model,
            SLOT(section_right_clicked(int)));
    connect(m_header, SIGNAL(sort(int, DwarfModelProxy::DWARF_SORT_ROLE)),
            m_proxy, SLOT(sort(int, DwarfModelProxy::DWARF_SORT_ROLE)));

    connect(this, SIGNAL(activated(const QModelIndex&)), proxy, SLOT(cell_activated(const QModelIndex&)));
    connect(m_model, SIGNAL(preferred_header_size(int, int)), m_header, SLOT(resizeSection(int, int)));
    connect(m_model, SIGNAL(set_index_as_spacer(int)), m_header, SLOT(set_index_as_spacer(int)));
    connect(m_model, SIGNAL(clear_spacers()), m_header, SLOT(clear_spacers()));
    set_single_click_labor_changes(DT->user_settings()->value("options/single_click_labor_changes", true).toBool());        
}

void StateTableView::new_custom_profession() {
    QModelIndex idx = currentIndex();
    if (idx.isValid()) {
        int id = idx.data(DwarfModel::DR_ID).toInt();
        Dwarf *d = m_model->get_dwarf_by_id(id);
        if (d)
            emit new_custom_profession(d);
    }
}

void StateTableView::filter_dwarves(QString text) {
    m_proxy->setFilterFixedString(text);
    m_proxy->setFilterKeyColumn(0);
    m_proxy->setFilterRole(Qt::DisplayRole);
}

void StateTableView::jump_to_dwarf(QTreeWidgetItem* current, QTreeWidgetItem*) {
    if (!current)
        return;
    int dwarf_id = current->data(0, Qt::UserRole).toInt();

    Dwarf *d = m_model->get_dwarf_by_id(dwarf_id);
    if (d && d->m_name_idx.isValid()) {
        QModelIndex proxy_idx = m_proxy->mapFromSource(d->m_name_idx);
        if (proxy_idx.isValid()) {
            //scrollTo(proxy_idx);
            selectionModel()->select(proxy_idx, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
        }
    }
}

void StateTableView::jump_to_profession(QListWidgetItem* current, QListWidgetItem*) {
    if (!current)
        return;
    QString prof_name = current->text();
    QModelIndexList matches = m_proxy->match(m_proxy->index(0,0), Qt::DisplayRole, prof_name);
    if (matches.size() > 0) {
        QModelIndex group_header = matches.at(0);
        //scrollTo(group_header);
        expand(group_header);
        selectionModel()->select(group_header, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
    }
}

void StateTableView::contextMenuEvent(QContextMenuEvent *event) {
    QModelIndex idx = indexAt(event->pos());
    if (!idx.isValid())
        return;

    if (idx.column() == 0 && !idx.data(DwarfModel::DR_IS_AGGREGATE).toBool()) {
        // we're on top of a dwarf's name
        QMenu m(this); // this will be the popup menu        
        int id = idx.data(DwarfModel::DR_ID).toInt();
        Dwarf *d = m_model->get_dwarf_by_id(id);
        m.addActions(d->get_actions());
        m.addSeparator();
        m.addAction(tr("Set Nickname..."), this, SLOT(set_nickname()));
        m.addSeparator();

        QMenu sub(&m);
        sub.setTitle(tr("Custom Professions"));
        QAction *a = sub.addAction(tr("Set custom profession name..."), this, SLOT(set_custom_profession_text()));        
        a->setData(id);
        a = sub.addAction(tr("New custom profession from this dwarf..."), this, SLOT(custom_profession_from_dwarf()));
        a->setData(id);
        sub.addAction(tr("Reset to default profession"), this, SLOT(reset_custom_profession()));
        sub.addSeparator();

        foreach(CustomProfession *cp, DT->get_custom_professions()) {
            sub.addAction(cp->get_name(), this, SLOT(apply_custom_profession()));
        }
        m.addMenu(&sub);

        //SQUAD MENU ITEMS
        m.addSeparator();
        QMenu assign(&m);
        if(m_model->squads().count() > 0 && d->profession() != "Child" && d->profession() != "Baby"){
            assign.setTitle(tr("Assign to squad..."));
            QIcon icon;
            icon.addFile(QString::fromUtf8(":/img/add.png"), QSize(), QIcon::Normal, QIcon::Off);
            foreach(int key, m_model->squads().uniqueKeys()){
                Squad *s = m_model->squads().value(key);
                if(d->squad_id() != s->id()){
                    if(s->assigned_count() < 10){
                        QAction *add=assign.addAction(tr("%1 (%2 members)").arg(s->name()).arg(s->assigned_count()), this, SLOT(assign_to_squad()));
                        add->setData(s->id());
                        add->setIcon(icon);
                    }
                }
            }
            if(assign.actions().count()>0)
                m.addMenu(&assign);

            //squad removal (can't remove captain if there are subordinates)
            if(d->squad_id()!=-1){
                icon.addFile(QString::fromUtf8(":/img/delete.png"), QSize(), QIcon::Normal, QIcon::Off);
                if((d->squad_position()==0 && m_model->squads().value(d->squad_id())->assigned_count()==1) || d->squad_position() != 0){
                    m.addAction(icon,tr("Remove from squad"),this,SLOT(remove_squad()));
                }else{
                    m.addAction(icon,tr("Remove subordinates first!"));
                }
            }
        }

        m.exec(viewport()->mapToGlobal(event->pos()));
    } else if (idx.data(DwarfModel::DR_COL_TYPE).toInt() == CT_LABOR) {
        // labor column
        QMenu m(this); // this will be the popup menu
        QString set_name = idx.data(DwarfModel::DR_SET_NAME).toString();
        ViewColumnSet *set = DT->get_main_window()->get_view_manager()->get_active_view()->get_set(set_name);
        QString labor_id = QString("%1").arg(idx.data(DwarfModel::DR_LABOR_ID).toInt());
        if (idx.data(DwarfModel::DR_IS_AGGREGATE).toBool()) { //aggregate labor            
            QString group_name = idx.data(DwarfModel::DR_GROUP_NAME).toString();
            QAction *a = m.addAction(tr("Toggle %1 for %2").arg(set_name).arg(group_name));
            a->setData(group_name);
            connect(a, SIGNAL(triggered()), set, SLOT(toggle_for_dwarf_group()));
            QAction *assign = m.addAction(tr("Assign first x dwarves to this labor"));
            assign->setData(labor_id);
            connect(assign, SIGNAL(triggered()), this, SLOT(assign_x_dwarves_to_labor()));
        } else { // single dwarf labor
            // find the dwarf...            
            int dwarf_id = idx.data(DwarfModel::DR_ID).toInt();
            Dwarf *d = m_model->get_dwarf_by_id(dwarf_id);
            QAction *a = m.addAction(tr("Toggle %1 for %2").arg(set_name).arg(d->nice_name()));            
            a->setData(dwarf_id);
            connect(a, SIGNAL(triggered()), set, SLOT(toggle_for_dwarf()));
        }
        m.exec(viewport()->mapToGlobal(event->pos()));
    } else if (idx.data(DwarfModel::DR_IS_AGGREGATE).toBool() && m_model->current_grouping()==DwarfModel::GB_SQUAD){
        QMenu m(this);
        QAction *a = m.addAction("Change Squad Name",this,SLOT(set_squad_name()));
        a->setData(idx.data(DwarfModel::DR_ID));
        m.exec(viewport()->mapToGlobal(event->pos()));
    }
}



void StateTableView::set_squad_name(){
    QAction *a = qobject_cast<QAction*>(QObject::sender());
    Squad *s = m_model->squads().value(a->data().toInt());
    if(s){
        bool ok;
        QString alias = QInputDialog::getText(this, tr("New Squad Name"),
                                              tr("Enter a new squad name for %1, or leave blank to reset "
                                                 "to their default name.").arg(s->name()), QLineEdit::Normal, "", &ok);
        if(ok) {
            int limit = 16;
            if (alias.length() > limit) {
                QMessageBox::warning(this, tr("Squad name too long"),
                                     tr("Squad names must be under %1 characters "
                                        "long.").arg(limit));
                return;
            }
            s->rename_squad(alias);
            m_model->load_dwarves();
        }
    }
}

void StateTableView::assign_x_dwarves_to_labor()
{
    bool ok;
    QString how_many = QInputDialog::getText(0, tr("Select number of dwarf"),
         tr("How many dwarves you want to assign to this labor?"), QLineEdit::Normal,"", &ok);
    if (!ok)
        return;
    int howmany = how_many.toInt();
    QAction *a = qobject_cast<QAction*>(QObject::sender());
    int l_id = a->data().toInt();
    DwarfModel *dm = DT->get_main_window()->get_model();
    int assigned = 0;
    for(int top = 0; top < m_proxy->rowCount(); ++top) {
        QModelIndex idx = m_proxy->index(top, 0);
        if (m_proxy->rowCount(idx)) { // has children
            for (int sub = 0; sub < m_proxy->rowCount(idx); ++sub) {
                QModelIndex sub_idx = m_proxy->index(sub, 0, idx);
                int id = sub_idx.data(DwarfModel::DR_ID).toInt();
                Dwarf *d = dm->get_dwarf_by_id(id);
                d->set_labor(l_id, true);
                assigned++;
                if(assigned==howmany) break;
            }
        }
        if(assigned==howmany) break;
    }
DT->get_main_window()->get_model()->calculate_pending();
}

void StateTableView::assign_to_squad(){
    QAction *a = qobject_cast<QAction*>(QObject::sender());
    Squad *new_squad = m_model->squads().value(a->data().toInt());
    int id = -1;
    const QItemSelection sel = selectionModel()->selection();
    foreach(QModelIndex i, sel.indexes()) {
        if (i.column() == 0 && !i.data(DwarfModel::DR_IS_AGGREGATE).toBool()){
            id = i.data(DwarfModel::DR_ID).toInt();
            Dwarf *d = m_model->get_dwarf_by_id(id);
            if (d) {
                if(d->squad_id() != new_squad->id()){ //don't add to squad if they're already in it..
                    if(d->squad_id() != -1){ //remove from old squad first
                        if(d->squad_position()==0)
                            emit squad_leader_changed();
                        Squad *old_squad = m_model->squads().value(d->squad_id());
                        if(old_squad)
                            old_squad->remove_from_squad(d);
                    }

                    int new_pos = new_squad->assign_to_squad(d);
                    if(new_pos==0)
                        emit squad_leader_changed();
                }
            }
        }
    }
    if(m_model->current_grouping()==DwarfModel::GB_SQUAD)
        DT->get_main_window()->get_view_manager()->redraw_current_tab();

}
void StateTableView::remove_squad(){
    const QItemSelection sel = selectionModel()->selection();
    foreach(QModelIndex i, sel.indexes()) {
        if (i.column() == 0 && !i.data(DwarfModel::DR_IS_AGGREGATE).toBool()){
            int id = i.data(DwarfModel::DR_ID).toInt();
            Dwarf *d = m_model->get_dwarf_by_id(id);
            if (d) {
                if(d->squad_position()==0)
                    emit squad_leader_changed();
                Squad *s = m_model->squads().value(d->squad_id());
                if(s)
                    s->remove_from_squad(d);
            }
        }
    }
    if(m_model->current_grouping()==DwarfModel::GB_SQUAD)
        DT->get_main_window()->get_view_manager()->redraw_current_tab();
}

void StateTableView::set_nickname() {
    const QItemSelection sel = selectionModel()->selection();
    if(sel.count() <= 0)
        return;
    bool ok;
    QString new_nick = QInputDialog::getText(this, tr("New Nickname"),
                                             tr("Enter a new nickname for the selected dwarves. Or leave blank to reset "
                                                "to their default name."), QLineEdit::Normal,
                                             m_model->get_dwarf_by_id(sel.indexes().at(0).data(DwarfModel::DR_ID).toInt())->nickname(), &ok);
    if(ok) {
        int limit = 16;
        if (new_nick.length() > limit) {
            QMessageBox::warning(this, tr("Nickname too long"),
                                 tr("Nicknames must be under %1 characters "
                                    "long.").arg(limit));
            return;
        }

        foreach(QModelIndex i, sel.indexes()) {
            if (i.column() == 0 && !i.data(DwarfModel::DR_IS_AGGREGATE).toBool()){
                int id = i.data(DwarfModel::DR_ID).toInt();
                Dwarf *d = m_model->get_dwarf_by_id(id);
                if (d) {
                    d->set_nickname(new_nick);
                    m_model->setData(i, d->nice_name(), Qt::DisplayRole);
                }
            }
        }
        m_model->calculate_pending();
    }
}

void StateTableView::custom_profession_from_dwarf() {
    QAction *a = qobject_cast<QAction*>(QObject::sender());
    int id = a->data().toInt();
    Dwarf *d = m_model->get_dwarf_by_id(id);

    DT->custom_profession_from_dwarf(d);
}

void StateTableView::apply_custom_profession() {
    QAction *a = qobject_cast<QAction*>(QObject::sender());
    CustomProfession *cp = DT->get_custom_profession(a->text());
    if (!cp)
        return;

    const QItemSelection sel = selectionModel()->selection();
    foreach(const QModelIndex idx, sel.indexes()) {
        if (idx.column() == 0 && !idx.data(DwarfModel::DR_IS_AGGREGATE).toBool()) {
            Dwarf *d = m_model->get_dwarf_by_id(idx.data(DwarfModel::DR_ID).toInt());
            if (d)
                d->apply_custom_profession(cp);
        }
    }
    m_model->calculate_pending();
    DT->emit_labor_counts_updated();
}

void StateTableView::reset_custom_profession() {
    const QItemSelection sel = selectionModel()->selection();
    foreach(const QModelIndex idx, sel.indexes()) {
        if (idx.column() == 0 && !idx.data(DwarfModel::DR_IS_AGGREGATE).toBool()) {
            Dwarf *d = m_model->get_dwarf_by_id(idx.data(DwarfModel::DR_ID).toInt());
            if (d)
                d->reset_custom_profession();
        }
    }
    m_model->calculate_pending();
}

void StateTableView::set_custom_profession_text() {
    QString prof_name;
    bool warn = false;
    do {
        bool ok;
        if (warn)
            QMessageBox::warning(this, tr("Name too long!"), tr("Profession names must be 15 characters or shorter!"));
        prof_name = QInputDialog::getText(this, tr("New Custom Profession Name"),
            tr("Custom Profession"), QLineEdit::Normal, QString(), &ok);
        if (!ok)
            return;
        warn = prof_name.length() > 15;
    } while(warn);

    const QItemSelection sel = selectionModel()->selection();
    foreach(const QModelIndex idx, sel.indexes()) {
        if (idx.column() == 0 && !idx.data(DwarfModel::DR_IS_AGGREGATE).toBool()) {
            Dwarf *d = m_model->get_dwarf_by_id(idx.data(DwarfModel::DR_ID).toInt());
            if (d)
                d->set_custom_profession_text(prof_name);
        }
    }
    m_model->calculate_pending();

}

void StateTableView::currentChanged(const QModelIndex &cur, const QModelIndex &) {
    // current item changed, so find out what dwarf the current item is for...
    if (!m_proxy) // special case tables (like skill legend, don't have a proxy)
        return;
    int id = m_proxy->data(cur, DwarfModel::DR_ID).toInt();
    Dwarf *d = m_model->get_dwarf_by_id(id);
    if (d) {
        //LOGD << "focus changed to" << d->nice_name();
        emit dwarf_focus_changed(d);
    }
}

void StateTableView::select_dwarf(Dwarf *d) {
    for(int top = 0; top < m_proxy->rowCount(); ++top) {
        QModelIndex idx = m_proxy->index(top, 0);
        if (idx.data(DwarfModel::DR_ID).toInt() == d->id()) {
            this->selectionModel()->select(idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
            break;
        } else if (m_proxy->rowCount(idx)) { // has children
            for (int sub = 0; sub < m_proxy->rowCount(idx); ++sub) {
                QModelIndex sub_idx = m_proxy->index(sub, 0, idx);
                if (sub_idx.data(DwarfModel::DR_ID).toInt() == d->id()) {
                    this->selectionModel()->select(sub_idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                }
            }
        }
    }
    m_selected = selectionModel()->selection();
}
/************************************************************************/
/* Hey look, our own mouse handling (/rolleyes)                         */
/************************************************************************/
void StateTableView::mousePressEvent(QMouseEvent *event) {
    m_last_button = event->button();
    QTreeView::mousePressEvent(event);
}
void StateTableView::mouseMoveEvent(QMouseEvent *event) {
    QModelIndex idx = indexAt(event->pos());
    if (idx.isValid())
        m_header->column_hover(idx.column());
    QTreeView::mouseMoveEvent(event);
}

void StateTableView::mouseReleaseEvent(QMouseEvent *event) {
    m_last_button = event->button();
    QTreeView::mouseReleaseEvent(event);
}

void StateTableView::clicked(const QModelIndex &idx) {
    if (m_last_button == Qt::LeftButton && // only activate on left-clicks
            m_single_click_labor_changes && // only activated on single-click if the user set it that way
            m_proxy && // we only do this for views that have proxies (dwarf views)
            idx.column() != 0) // don't single-click activate the name column
    {
        if(m_selected.count() > 1){
            for(int i = 0; i<m_selected.count();i++){
                const QModelIndex idx_sel = m_selected.at(i).indexes().at(idx.column());
                m_proxy->cell_activated(idx_sel);
                selectionModel()->select(
                            QItemSelection(
                                this->model()->index(idx_sel.row(),0),
                                this->model()->index(idx_sel.row(),this->model()->columnCount()-1)),
                            QItemSelectionModel::Select);
            }
        }else{
            m_proxy->cell_activated(idx);
        }
    }else{
        if(m_proxy && idx.column() == 0)
            m_selected = selectionModel()->selection();
    }
}

void StateTableView::header_clicked(int index) {
    if (!m_column_already_sorted && index > 0) {
        m_header->setSortIndicator(index, Qt::DescendingOrder);
        m_last_sorted_col = index;
    }
    m_last_sort_order = m_header->sortIndicatorOrder();
}

void StateTableView::header_pressed(int index) {
    m_column_already_sorted = (m_header->sortIndicatorSection() == index);
}

/************************************************************************/
/* Handlers for expand/collapse persistence                             */
/************************************************************************/
void StateTableView::expandAll() {
    m_expanded_rows.clear();
    for(int i = 0; i < m_proxy->rowCount(); ++i) {
        m_expanded_rows << i;
    }
    QTreeView::expandAll();
}

void StateTableView::collapseAll() {
    m_expanded_rows.clear();
    QTreeView::collapseAll();
}

void StateTableView::index_expanded(const QModelIndex &idx) {
    m_expanded_rows << idx.row();
}

void StateTableView::index_collapsed(const QModelIndex &idx) {
    int i = m_expanded_rows.indexOf(idx.row());
    if (i != -1)
        m_expanded_rows.removeAt(i);
}

void StateTableView::restore_expanded_items() {
    if (m_auto_expand_groups) {
        expandAll();
        return;
    }
    disconnect(this, SIGNAL(expanded(const QModelIndex &)), 0, 0);
    foreach(int row, m_expanded_rows) {
        expand(m_proxy->index(row, 0));
    }
    connect(this, SIGNAL(expanded(const QModelIndex &)), SLOT(index_expanded(const QModelIndex &)));
}

void StateTableView::keyPressEvent(QKeyEvent *event ){
    switch(event->key()){
    case Qt::Key_Escape:
        selectionModel()->clear();
        m_selected.clear();
        break;
    case Qt::Key_PageDown:
        verticalScrollBar()->setValue(verticalScrollBar()->value() + verticalScrollBar()->pageStep());
        break;
    case Qt::Key_PageUp:
        verticalScrollBar()->setValue(verticalScrollBar()->value() - verticalScrollBar()->pageStep());
        break;
    case Qt::Key_Up:
        verticalScrollBar()->setValue(verticalScrollBar()->value() - verticalScrollBar()->singleStep());
        break;
    case Qt::Key_Down:
        verticalScrollBar()->setValue(verticalScrollBar()->value() + verticalScrollBar()->singleStep());
        break;
    case Qt::Key_Right:
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() + horizontalScrollBar()->singleStep());
        break;
    case Qt::Key_Left:
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - horizontalScrollBar()->singleStep());
        break;
    default:
        break;
    }
}

void StateTableView::restore_scroll_positions(){
    if(m_vscroll > verticalScrollBar()->maximum())
        m_vscroll = verticalScrollBar()->maximum();
    else if(m_vscroll < verticalScrollBar()->minimum())
        m_vscroll = verticalScrollBar()->minimum();
    verticalScrollBar()->setValue(m_vscroll);

    if(m_hscroll > horizontalScrollBar()->maximum())
        m_hscroll = horizontalScrollBar()->maximum();
    else if(m_hscroll < verticalScrollBar()->minimum())
        m_hscroll = horizontalScrollBar()->minimum();
    horizontalScrollBar()->setValue(m_hscroll);
}

void StateTableView::vscroll_value_changed(int value){
    //when loading rows, the slider will move back to the top
    //on linux, changing tabs also resets the slider of the previous tab,
    //so don't record the position of the scroll if it moves on an inactive view
    if(!is_loading_rows && is_active)
        m_vscroll = value;
}
void StateTableView::hscroll_value_changed(int value){
    if(!is_loading_rows && is_active)
        m_hscroll = value;
}

void StateTableView::set_scroll_positions(int v_value, int h_value){
    m_vscroll = v_value;
    m_hscroll = h_value;
}
