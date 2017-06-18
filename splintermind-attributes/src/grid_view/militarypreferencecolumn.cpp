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

#include "militarypreferencecolumn.h"
#include "militarypreference.h"
#include "gamedatareader.h"
#include "columntypes.h"
#include "dwarfmodel.h"
#include "dwarf.h"
#include "viewcolumnset.h"

MilitaryPreferenceColumn::MilitaryPreferenceColumn(const QString &title, const int &labor_id, const int &skill_id, ViewColumnSet *set, QObject *parent) 
	: ViewColumn(title, CT_MILITARY_PREFERENCE, set, parent)
	, m_labor_id(labor_id)
	, m_skill_id(skill_id)
{}

MilitaryPreferenceColumn::MilitaryPreferenceColumn(QSettings &s, ViewColumnSet *set, QObject *parent) 
	: ViewColumn(s, set, parent)
	, m_labor_id(s.value("labor_id", -1).toInt())
	, m_skill_id(s.value("skill_id", -1).toInt())
{}

MilitaryPreferenceColumn::MilitaryPreferenceColumn(const MilitaryPreferenceColumn &to_copy) 
    : ViewColumn(to_copy)
    , m_labor_id(to_copy.m_labor_id)
    , m_skill_id(to_copy.m_skill_id)
{}

QStandardItem *MilitaryPreferenceColumn::build_cell(Dwarf *d) {
	GameDataReader *gdr = GameDataReader::ptr();
	QStandardItem *item = init_cell(d);

	item->setData(CT_MILITARY_PREFERENCE, DwarfModel::DR_COL_TYPE);
    short rating = d->skill_rating(m_skill_id);
    short val = d->pref_value(m_labor_id);
    QString val_name = gdr->get_military_preference(m_labor_id)->value_name(val);

    item->setData(rating * (val + 1), DwarfModel::DR_SORT_VALUE); // push assigned labors above no exp in sort order
	item->setData(rating, DwarfModel::DR_RATING);
	item->setData(m_labor_id, DwarfModel::DR_LABOR_ID);
	item->setData(m_set->name(), DwarfModel::DR_SET_NAME);
	
	QString skill_str;
	if (m_skill_id != -1 && rating > -1) {
		QString adjusted_rating = QString::number(rating);
		if (rating > 15)
			adjusted_rating = QString("15 +%1").arg(rating - 15);
        skill_str = tr("<b>%1</b> %2 %3<br/>[RAW LEVEL: <b><font color=blue>%4</font></b>]<br/><b>Experience:</b><br/>%5")
                .arg(d->get_skill(m_skill_id).rust_rating())
                .arg(gdr->get_skill_level_name(rating))
                .arg(gdr->get_skill_name(m_skill_id))
                .arg(adjusted_rating)
                .arg(d->get_skill(m_skill_id).exp_summary());
    } else {
		// either the skill isn't a valid id, or they have 0 experience in it
		skill_str = "0 experience";
	}
    item->setToolTip(QString("<h3>%1</h3><b>USING: %2</b><br/>%3<h4>%4</h4>")
        .arg(m_title)
        .arg(val_name)
        .arg(skill_str)
        .arg(d->nice_name())
    );
	return item;
}

QStandardItem *MilitaryPreferenceColumn::build_aggregate(const QString &group_name, const QVector<Dwarf*> &) {
	Q_UNUSED(group_name);
	QStandardItem *item = new QStandardItem;
	item->setData(CT_MILITARY_PREFERENCE, DwarfModel::DR_COL_TYPE);
	
	QColor bg;
	if (m_override_set_colors) {
		bg = m_bg_color;
	} else {
		bg = set()->bg_color();
	}
	item->setData(bg, Qt::BackgroundColorRole);
	return item;
}

void MilitaryPreferenceColumn::write_to_ini(QSettings &s) {
	ViewColumn::write_to_ini(s); 
	s.setValue("skill_id", m_skill_id); 
	s.setValue("labor_id", m_labor_id);
}
