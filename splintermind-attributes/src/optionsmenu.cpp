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

#include "optionsmenu.h"
#include "customcolor.h"
#include "dwarf.h"
#include "dwarftherapist.h"
#include "utils.h"
#include "defines.h"
#include "truncatingfilelogger.h"
#include "uberdelegate.h"
#include "mainwindow.h"
#include "fortressentity.h"
#include "dfinstance.h"

OptionsMenu::OptionsMenu(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::OptionsMenu)
    , m_reading_settings(false)
{
    ui->setupUi(this);

    m_general_colors
            << new CustomColor(tr("Skill"), tr("The color of the growing skill indicator box "
                                               "inside a cell. Is not used when auto-contrast is enabled."), "skill", from_hex("0xAAAAAAFF"), this)
            << new CustomColor(tr("Active Labor Cell"),
                               tr("Color shown for a cell when the labor is active for a dwarf."),
                               "active_labor", from_hex("0x7878B3FF"), this)
            << new CustomColor(tr("Active Group Cell"),
                               tr("Color shown on an aggregate cell if <b>all</b> dwarves have this labor enabled."),
                               "active_group", from_hex("0x33FF33FF"), this)
            << new CustomColor(tr("Inactive Group Cell"),
                               tr("Color shown on an aggregate cell if <b>none</b> of the dwarves have this labor enabled."),
                               "inactive_group", from_hex("0x00000020"), this)
            << new CustomColor(tr("Partial Group Cell"),
                               tr("Color shown on an aggregate cell if <b>some</b> of the dwarves have this labor enabled."),
                               "partial_group", from_hex("0x00000060"), this)
            << new CustomColor(tr("Selection Guides"),
                               tr("Color of the lines around cells when a row and/or column are selected."),
                               "guides", QColor(0x0099FF), this)
            << new CustomColor(tr("Main Border"),
                               tr("Color of cell borders"),
                               "border", QColor(0xd9d9d9), this)
            << new CustomColor(tr("Dirty Cell Indicator"),
                               tr("Border color of a cell that has pending changes. Set to main border color to disable this."),
                               "dirty_border", QColor(0xFF6600), this);

    m_happiness_colors
            << new CustomColor(tr("Ecstatic"), tr("Color shown in happiness columns when a dwarf is <b>ecstatic</b>."),
                               QString("happiness/%1").arg(static_cast<int>(Dwarf::DH_ECSTATIC)), QColor(0x00FF00), this)
            << new CustomColor(tr("Happy"), tr("Color shown in happiness columns when a dwarf is <b>happy</b>."),
                               QString("happiness/%1").arg(static_cast<int>(Dwarf::DH_HAPPY)), QColor(0x71cc09), this)
            << new CustomColor(tr("Content"), tr("Color shown in happiness columns when a dwarf is <b>quite content</b>."),
                               QString("happiness/%1").arg(static_cast<int>(Dwarf::DH_CONTENT)), QColor(0xDDDD00), this)
            << new CustomColor(tr("Fine"), tr("Color shown in happiness columns when a dwarf is <b>fine</b>."),
                               QString("happiness/%1").arg(static_cast<int>(Dwarf::DH_FINE)), QColor(0xe7e2ab), this)
            << new CustomColor(tr("Unhappy"), tr("Color shown in happiness columns when a dwarf is <b>unhappy</b>."),
                               QString("happiness/%1").arg(static_cast<int>(Dwarf::DH_UNHAPPY)), QColor(0xffaa00), this)
            << new CustomColor(tr("Very Unhappy"), tr("Color shown in happiness columns when a dwarf is <b>very unhappy</b>."),
                               QString("happiness/%1").arg(static_cast<int>(Dwarf::DH_VERY_UNHAPPY)), QColor(0xCC0000), this)
            << new CustomColor(tr("Miserable"), tr("Color shown in happiness columns when a dwarf is <b>miserable.</b>"),
                               QString("happiness/%1").arg(static_cast<int>(Dwarf::DH_MISERABLE)), QColor(0xFF0000), this);

    QColor m_noble_default = QColor(255,153,0);
    m_noble_colors
            << new CustomColor(tr("Bookkeeper"),tr("Highlight color for the bookkeeper."),
                               QString("nobles/%1").arg(static_cast<int>(FortressEntity::BOOKKEEPER)), m_noble_default, this)
    << new CustomColor(tr("Broker"),tr("Highlight color for the broker."),
                       QString("nobles/%1").arg(static_cast<int>(FortressEntity::BROKER)), m_noble_default, this)
    << new CustomColor(tr("Champions"),tr("Highlight color for champions."),
                       QString("nobles/%1").arg(static_cast<int>(FortressEntity::CHAMPION)), m_noble_default, this)
    << new CustomColor(tr("Chief Medical Dwarf"),tr("Highlight color for the chief medical dwarf."),
                       QString("nobles/%1").arg(static_cast<int>(FortressEntity::CHIEF_MEDICAL_DWARF)), m_noble_default, this)
    << new CustomColor(tr("Hammerer"),tr("Highlight color for the hammerer."),
                       QString("nobles/%1").arg(static_cast<int>(FortressEntity::HAMMERER)), m_noble_default, this)
    << new CustomColor(tr("Law"),tr("Highlight color for the captain of the guard and sherrif."),
                       QString("nobles/%1").arg(static_cast<int>(FortressEntity::LAW)), m_noble_default, this)
    << new CustomColor(tr("Leader/Mayor"),tr("Highlight color for the expedition leaders and mayors."),
                       QString("nobles/%1").arg(static_cast<int>(FortressEntity::LEADER)), m_noble_default, this)
    << new CustomColor(tr("Manager"),tr("Highlight color for the managers."),
                       QString("nobles/%1").arg(static_cast<int>(FortressEntity::MANAGER)), m_noble_default, this)
    << new CustomColor(tr("Militia"),tr("Highlight color for the militia commander, militia captains, lieutenants and generals."),
                       QString("nobles/%1").arg(static_cast<int>(FortressEntity::MILITIA)), m_noble_default, this)
    << new CustomColor(tr("Monarch"),tr("Highlight color for kings and queens."),
                       QString("nobles/%1").arg(static_cast<int>(FortressEntity::MONARCH)), m_noble_default, this)
    << new CustomColor(tr("Royalty"),tr("Highlight color for barons, baronesses, dukes, duchesses, counts, countesses, lords and ladies."),
                       QString("nobles/%1").arg(static_cast<int>(FortressEntity::ROYALTY)), m_noble_default, this)
    << new CustomColor(tr("Multiple"),tr("Highlight color when holding multiple positions, or unknown positions."),
                       QString("nobles/%1").arg(static_cast<int>(FortressEntity::MULTIPLE)), m_noble_default, this);

    m_curse_color = new CustomColor(tr("Cursed"),tr("Cursed creatures will be highlighted with this color."),
                                    "cursed",QColor(125,97,186),this);

    QVBoxLayout *main_layout = new QVBoxLayout;
    foreach(CustomColor *cc, m_general_colors) {
        main_layout->addWidget(cc);
    }
    main_layout->setSpacing(0);
    ui->tab_grid_colors->setLayout(main_layout);

    QVBoxLayout *happiness_layout = new QVBoxLayout;
    happiness_layout->addWidget(ui->cb_happiness_icons);
    foreach(CustomColor *cc, m_happiness_colors) {
        happiness_layout->addWidget(cc);
    }
    happiness_layout->setSpacing(0);
    ui->tab_happiness_colors->setLayout(happiness_layout);

    QVBoxLayout *nobles_layout = new QVBoxLayout;
    nobles_layout->addWidget(ui->cb_noble_highlight);
    foreach(CustomColor *cc, m_noble_colors) {
        nobles_layout->addWidget(cc);
    }
    nobles_layout->setSpacing(4);
    ui->tab_noble_colors->setLayout(nobles_layout);

    ui->horizontal_curse_layout->addWidget(m_curse_color);

    ui->cb_skill_drawing_method->addItem("Growing Center Box", UberDelegate::SDM_GROWING_CENTRAL_BOX);
    ui->cb_skill_drawing_method->addItem("Line Glyphs", UberDelegate::SDM_GLYPH_LINES);
    ui->cb_skill_drawing_method->addItem("Growing Fill", UberDelegate::SDM_GROWING_FILL);
    ui->cb_skill_drawing_method->addItem("Numbers", UberDelegate::SDM_NUMERIC);

    connect(ui->btn_restore_defaults, SIGNAL(pressed()), this, SLOT(restore_defaults()));
    connect(ui->btn_change_font, SIGNAL(pressed()), this, SLOT(show_font_chooser()));
    connect(ui->btn_change_header_font, SIGNAL(pressed()), this, SLOT(show_header_font_chooser()));
    connect(ui->cb_auto_contrast, SIGNAL(toggled(bool)), m_general_colors[0], SLOT(setDisabled(bool)));

    connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tab_index_changed(int)));

    connect(ui->chk_roles_in_labor, SIGNAL(stateChanged(int)), this, SLOT(roles_in_labor_changed(int)));
    connect(ui->chk_roles_in_skills, SIGNAL(stateChanged(int)), this, SLOT(roles_in_skills_changed(int)));

    read_settings();
}

OptionsMenu::~OptionsMenu() {}


bool OptionsMenu::event(QEvent *evt) {
    if (evt->type() == QEvent::StatusTip) {
        ui->text_status_tip->setHtml(static_cast<QStatusTipEvent*>(evt)->tip());
        return true; // we've handled it, don't pass it
    }
    return QWidget::event(evt); // pass the event along the chain
}

void OptionsMenu::read_settings() {
    m_reading_settings = true;
    QSettings *s = DT->user_settings();
    s->beginGroup("options");
    s->beginGroup("colors");
    QColor c;
    foreach(CustomColor *cc, m_general_colors) {
        c = s->value(cc->get_config_key(), cc->get_default()).value<QColor>();
        cc->set_color(c);
    }
    foreach(CustomColor *cc, m_happiness_colors) {
        c = s->value(cc->get_config_key(), cc->get_default()).value<QColor>();
        cc->set_color(c);
    }
    foreach(CustomColor *cc, m_noble_colors) {
        c = s->value(cc->get_config_key(), cc->get_default()).value<QColor>();
        cc->set_color(c);
    }
    c = s->value(m_curse_color->get_config_key(), m_curse_color->get_default()).value<QColor>();
    m_curse_color->set_color(c);

    s->endGroup();
    s->beginGroup("grid");
    UberDelegate::SKILL_DRAWING_METHOD m = static_cast<UberDelegate::SKILL_DRAWING_METHOD>(s->value("skill_drawing_method", UberDelegate::SDM_GROWING_CENTRAL_BOX).toInt());
    for(int i=0; i < ui->cb_skill_drawing_method->count(); ++i) {
        if (ui->cb_skill_drawing_method->itemData(i) == m) {
            ui->cb_skill_drawing_method->setCurrentIndex(i);
            break;
        }
    }
    ui->sb_cell_size->setValue(s->value("cell_size", DEFAULT_CELL_SIZE).toInt());
    ui->sb_cell_padding->setValue(s->value("cell_padding", 0).toInt());
    ui->cb_shade_column_headers->setChecked(s->value("shade_column_headers", true).toBool());
    ui->cb_shade_cells->setChecked(s->value("shade_cells", true).toBool());
    ui->cb_header_text_direction->setChecked(s->value("header_text_bottom", false).toBool());

    m_font = s->value("font", QFont("Segoe UI", 8)).value<QFont>();
    m_dirty_font = m_font;
    ui->lbl_current_font->setText(m_font.family() + " [" + QString::number(m_font.pointSize()) + "pt]");

    m_header_font = s->value("header_font", QFont("Segoe UI", 9)).value<QFont>();
    m_dirty_header_font = m_header_font;
    ui->lbl_header_font->setText(m_header_font.family() + " [" + QString::number(m_header_font.pointSize()) + "pt]");

    ui->cb_happiness_icons->setChecked(s->value("happiness_icons",true).toBool());
    ui->cb_labor_counts->setChecked(s->value("show_labor_counts",false).toBool());

    ui->cb_sync_grouping->setChecked(s->value("group_all_views",true).toBool());
    ui->cb_sync_scrolling->setChecked(s->value("scroll_all_views",false).toBool());

    s->endGroup();

    ui->cb_read_dwarves_on_startup->setChecked(s->value("read_on_startup", true).toBool());
    ui->cb_auto_contrast->setChecked(s->value("auto_contrast", true).toBool());
    ui->cb_show_aggregates->setChecked(s->value("show_aggregates", true).toBool());
    ui->cb_single_click_labor_changes->setChecked(s->value("single_click_labor_changes", true).toBool());
    ui->cb_show_toolbar_text->setChecked(s->value("show_toolbutton_text", true).toBool());
    ui->cb_auto_expand->setChecked(s->value("auto_expand_groups", true).toBool());
    ui->cb_show_full_dwarf_names->setChecked(s->value("show_full_dwarf_names", false).toBool());
    ui->cb_show_dabbling_in_tooltip->setChecked(s->value("show_dabbling_in_tooltips", true).toBool());
    ui->cb_check_for_updates_on_startup->setChecked(s->value("check_for_updates_on_startup", true).toBool());
    ui->cb_alert_on_lost_connection->setChecked(s->value("alert_on_lost_connection", true).toBool());
    ui->cb_labor_cheats->setChecked(s->value("allow_labor_cheats", false).toBool());
    ui->cb_hide_children->setChecked(s->value("hide_children_and_babies", false).toBool());
    ui->cb_generic_names->setChecked(s->value("use_generic_names", false).toBool());
    ui->cb_curse_highlight->setChecked(s->value("highlight_cursed", false).toBool());
    ui->cb_noble_highlight->setChecked(s->value("highlight_nobles", false).toBool());
    ui->cb_labor_exclusions->setChecked(s->value("labor_exclusions", true).toBool());

    ui->dsb_attribute_weight->setValue(s->value("default_attributes_weight",1.0).toDouble());
    ui->dsb_skill_weight->setValue(s->value("default_skills_weight",1.0).toDouble());
    ui->dsb_trait_weight->setValue(s->value("default_traits_weight",1.0).toDouble());

    ui->sb_roles_tooltip->setValue(s->value("role_count_tooltip",3).toInt());
    ui->sb_roles_pane->setValue(s->value("role_count_pane",10).toInt());

    ui->chk_roles_in_labor->setChecked(s->value("show_roles_in_labor",true).toBool());
    ui->chk_roles_in_skills->setChecked(s->value("show_roles_in_skills",true).toBool());
    ui->chk_labor_sort_by_roles->setChecked(s->value("sort_roles_in_labor",true).toBool());
    ui->chk_skills_sort_by_roles->setChecked(s->value("sort_roles_in_skills",true).toBool());

    s->endGroup();

    m_reading_settings = false;
}

void OptionsMenu::write_settings() {
    if (!m_reading_settings) {
        QSettings *s = DT->user_settings();
        s->beginGroup("options");

        s->beginGroup("colors");
        foreach(CustomColor *cc, m_general_colors) {
            s->setValue(cc->get_config_key(), cc->get_color());
        }
        foreach(CustomColor *cc, m_happiness_colors) {
            s->setValue(cc->get_config_key(), cc->get_color());
        }
        foreach(CustomColor *cc, m_noble_colors) {
            s->setValue(cc->get_config_key(), cc->get_color());
        }
        s->setValue(m_curse_color->get_config_key(), m_curse_color->get_color());
        s->endGroup();

        s->beginGroup("grid");
        s->setValue("skill_drawing_method", ui->cb_skill_drawing_method->itemData(ui->cb_skill_drawing_method->currentIndex()).toInt());
        s->setValue("cell_size", ui->sb_cell_size->value());
        s->setValue("cell_padding", ui->sb_cell_padding->value());
        s->setValue("shade_column_headers", ui->cb_shade_column_headers->isChecked());
        s->setValue("shade_cells", ui->cb_shade_cells->isChecked());
        s->setValue("header_text_bottom", ui->cb_header_text_direction->isChecked());
        s->setValue("font", m_font);
        s->setValue("header_font", m_header_font);
        s->setValue("happiness_icons",ui->cb_happiness_icons->isChecked());
        s->setValue("show_labor_counts",ui->cb_labor_counts->isChecked());
        s->setValue("group_all_views",ui->cb_sync_grouping->isChecked());
        s->setValue("scroll_all_views",ui->cb_sync_scrolling->isChecked());
        s->endGroup();

        s->setValue("read_on_startup", ui->cb_read_dwarves_on_startup->isChecked());
        s->setValue("auto_contrast", ui->cb_auto_contrast->isChecked());
        s->setValue("show_aggregates", ui->cb_show_aggregates->isChecked());
        s->setValue("single_click_labor_changes", ui->cb_single_click_labor_changes->isChecked());
        s->setValue("show_toolbutton_text", ui->cb_show_toolbar_text->isChecked());
        s->setValue("auto_expand_groups", ui->cb_auto_expand->isChecked());
        s->setValue("show_full_dwarf_names", ui->cb_show_full_dwarf_names->isChecked());
        s->setValue("show_dabbling_in_tooltips", ui->cb_show_dabbling_in_tooltip->isChecked());
        s->setValue("check_for_updates_on_startup", ui->cb_check_for_updates_on_startup->isChecked());
        s->setValue("alert_on_lost_connection", ui->cb_alert_on_lost_connection->isChecked());
        s->setValue("allow_labor_cheats", ui->cb_labor_cheats->isChecked());
        s->setValue("hide_children_and_babies", ui->cb_hide_children->isChecked());
        s->setValue("use_generic_names", ui->cb_generic_names->isChecked());
        s->setValue("highlight_cursed", ui->cb_curse_highlight->isChecked());
        s->setValue("highlight_nobles", ui->cb_noble_highlight->isChecked());
        s->setValue("labor_exclusions", ui->cb_labor_exclusions->isChecked());

        s->setValue("default_attributes_weight",ui->dsb_attribute_weight->value());
        s->setValue("default_skills_weight",ui->dsb_skill_weight->value());
        s->setValue("default_traits_weight",ui->dsb_trait_weight->value());
        s->setValue("role_count_tooltip",ui->sb_roles_tooltip->value());
        s->setValue("role_count_pane",ui->sb_roles_pane->value());
        s->setValue("show_roles_in_labor",ui->chk_roles_in_labor->isChecked());
        s->setValue("show_roles_in_skills",ui->chk_roles_in_skills->isChecked());
        s->setValue("sort_roles_in_labor",ui->chk_labor_sort_by_roles->isChecked());
        s->setValue("sort_roles_in_skills",ui->chk_skills_sort_by_roles->isChecked());

        s->endGroup();
    }
}

void OptionsMenu::accept() {
    m_font = m_dirty_font;
    m_header_font = m_dirty_header_font;
    write_settings();
    emit settings_changed();
    QDialog::accept();
    int answer = QMessageBox::question(
            0, tr("Read Dwarves"),
            tr("Would you like to apply the new options now (Read Dwarves)?"),
            QMessageBox::Yes | QMessageBox::No);
    if (answer == QMessageBox::Yes)
        DT->get_main_window()->read_dwarves();
}

void OptionsMenu::reject() {
    m_dirty_font = m_font;
    m_dirty_header_font = m_header_font;
    read_settings();
    QDialog::reject();
}

void OptionsMenu::restore_defaults() {
    foreach(CustomColor *cc, m_general_colors) {
        cc->reset_to_default();
    }
    foreach(CustomColor *cc, m_happiness_colors) {
        cc->reset_to_default();
    }
    foreach(CustomColor *cc, m_noble_colors) {
        cc->reset_to_default();
    }
    ui->cb_read_dwarves_on_startup->setChecked(true);
    ui->cb_auto_contrast->setChecked(true);
    ui->cb_show_aggregates->setChecked(true);
    ui->cb_single_click_labor_changes->setChecked(false);
    ui->cb_show_toolbar_text->setChecked(true);
    ui->cb_auto_expand->setChecked(false);
    ui->cb_show_full_dwarf_names->setChecked(false);
    ui->cb_show_dabbling_in_tooltip->setChecked(true);
    ui->cb_check_for_updates_on_startup->setChecked(true);
    ui->cb_alert_on_lost_connection->setChecked(true);
    ui->cb_labor_cheats->setChecked(false);
    ui->cb_header_text_direction->setChecked(false);
    ui->cb_curse_highlight->setChecked(false);
    ui->cb_happiness_icons->setChecked(false);
    ui->cb_noble_highlight->setChecked(false);
    ui->sb_roles_tooltip->setValue(3);
    ui->sb_roles_pane->setValue(10);
    ui->chk_roles_in_labor->setChecked(false);
    ui->cb_labor_counts->setChecked(false);
    ui->cb_sync_grouping->setChecked(true);
    ui->cb_sync_scrolling->setChecked(false);
    ui->chk_labor_sort_by_roles->setChecked(true);
    ui->chk_skills_sort_by_roles->setChecked(true);

    m_font = QFont("Segoe UI", 8);
    m_dirty_font = m_font;
    ui->lbl_current_font->setText(m_font.family() + " [" + QString::number(m_font.pointSize()) + "pt]");

    m_header_font = QFont("Segoe UI", 8);
    m_dirty_header_font = m_header_font;
    ui->lbl_header_font->setText(m_header_font.family() + " [" + QString::number(m_header_font.pointSize()) + "pt]");

    ui->sb_cell_size->setValue(DEFAULT_CELL_SIZE);
    ui->sb_cell_padding->setValue(0);
}

void OptionsMenu::show_font_chooser() {
    bool ok;
    QFont tmp = QFontDialog::getFont(&ok, m_font, this, tr("Font used in main table"));
    if (ok) {
        ui->lbl_current_font->setText(tmp.family() + " [" + QString::number(tmp.pointSize()) + "pt]");
        m_dirty_font = tmp;
    }
}

void OptionsMenu::show_header_font_chooser() {
    bool ok;
    QFont tmp = QFontDialog::getFont(&ok, m_header_font, this, tr("Font used in main table"));
    if (ok) {
        ui->lbl_header_font->setText(tmp.family() + " [" + QString::number(tmp.pointSize()) + "pt]");
        m_dirty_header_font = tmp;
    }
}

void OptionsMenu::set_skill_drawing_method(const UberDelegate::SKILL_DRAWING_METHOD &sdm) {
    LOGD << "Setting SDM to" << UberDelegate::name_for_method(sdm);
    QSettings *s = DT->user_settings();
    s->setValue("options/grid/skill_drawing_method", static_cast<int>(sdm));
    read_settings(); // to set the combo-box correctly
    emit settings_changed();
}

void OptionsMenu::tab_index_changed(int index){
    if(index == ui->tabWidget->indexOf(ui->tab_roles)){
        int max_roles = GameDataReader::ptr()->get_roles().count();
        QString max_text = tr(" (max %1)").arg(max_roles);

        ui->lbl_pane_roles_max->setText(max_text);
        ui->sb_roles_pane->setMaximum(max_roles);

        ui->lbl_tooltip_roles_max->setText(max_text);
        ui->sb_roles_tooltip->setMaximum(max_roles);
    }
}

void OptionsMenu::roles_in_labor_changed(int state){
    if(state==2)
        ui->chk_labor_sort_by_roles->setEnabled(true);
    else
        ui->chk_labor_sort_by_roles->setEnabled(false);
}
void OptionsMenu::roles_in_skills_changed(int state){
    if(state==2)
        ui->chk_skills_sort_by_roles->setEnabled(true);
    else
        ui->chk_skills_sort_by_roles->setEnabled(false);
}
