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
#include "rotatedheader.h"
#include "dwarfmodel.h"
#include "dwarfmodelproxy.h"
#include "dwarftherapist.h"
#include "defines.h"

RotatedHeader::RotatedHeader(Qt::Orientation orientation, QWidget *parent)
    : QHeaderView(orientation, parent)
    , m_hovered_column(-1)
{
    setClickable(true);
    setSortIndicatorShown(true);
    setMouseTracking(true);

    read_settings();
    connect(DT, SIGNAL(settings_changed()), this, SLOT(read_settings()));
}

void RotatedHeader::column_hover(int col) {
    updateSection(m_hovered_column);
    m_hovered_column = col;
    updateSection(col);
}

void RotatedHeader::read_settings() {
    QSettings *s = DT->user_settings();
    int cell_size = s->value("options/grid/cell_size", DEFAULT_CELL_SIZE).toInt();
    for(int i=1; i < count(); ++i) {
        if (!m_spacer_indexes.contains(i)) {
            resizeSection(i, cell_size);
        }
    }
    m_shade_column_headers = s->value("options/grid/shade_column_headers", true).toBool();
    m_header_text_bottom = s->value("options/grid/header_text_bottom", false).toBool();
    m_font = s->value("options/grid/header_font", QFont("Segoe UI", 8)).value<QFont>();
}


void RotatedHeader::paintSection(QPainter *p, const QRect &rect, int idx) const {
    QColor bg = model()->headerData(idx, Qt::Horizontal,
                                    Qt::BackgroundColorRole).value<QColor>();
    if (m_spacer_indexes.contains(idx)) {
        p->save();
        p->fillRect(rect, QBrush(bg));
        p->restore();
        return;
    }

    QStyleOptionHeader opt;
    opt.rect = rect;
    opt.orientation = Qt::Horizontal;
    opt.section = idx;
    opt.sortIndicator = QStyleOptionHeader::None;

    QStyle::State state = QStyle::State_None;
    if (isEnabled())
        state |= QStyle::State_Enabled;
    if (window()->isActiveWindow())
        state |= QStyle::State_Active;
    if (rect.contains(m_p))
        state |= QStyle::State_MouseOver;
    if (sortIndicatorSection() == idx) {
        //state |= QStyle::State_Sunken;
        if (sortIndicatorOrder() == Qt::AscendingOrder) {
            opt.sortIndicator = QStyleOptionHeader::SortDown;
        } else {
            opt.sortIndicator = QStyleOptionHeader::SortUp;
        }
    }
    if (m_hovered_column == idx) {
        state |= QStyle::State_MouseOver;
    }

    opt.state = state;
    style()->drawControl(QStyle::CE_HeaderSection, &opt, p);

    QBrush brush = QBrush(bg);
    if (m_shade_column_headers) {
        QLinearGradient g(rect.topLeft(), rect.bottomLeft());
        g.setColorAt(0.25, QColor(255, 255, 255, 10));
        g.setColorAt(1.0, bg);
        brush = QBrush(g);
    }
    if (idx > 0)
        p->fillRect(rect.adjusted(1,8,-1,-2), brush);

    if (sortIndicatorSection() == idx) {
        opt.rect = QRect(opt.rect.x() + opt.rect.width()/2 - 5, opt.rect.y(), 10, 8);
        style()->drawPrimitive(QStyle::PE_IndicatorHeaderArrow, &opt, p);
    }

    /* Draw a border around header if column has guides applied
    DwarfModelProxy *prox = static_cast<DwarfModelProxy*>(model());
    DwarfModel *dm = prox->get_dwarf_model();
    int col = dm->selected_col();
    if (dm->selected_col() == idx) {
        p->save();
        p->setPen(Qt::red);
        p->setBrush(Qt::NoBrush);
        p->drawRect(rect);
        p->restore();
    }
    */

    QString data = this->model()->headerData(idx, Qt::Horizontal).toString();    
    p->save();
    p->setPen(Qt::black);
    p->setRenderHint(QPainter::TextAntialiasing);
    //p->setFont(QFont("Trebuchet", 9, QFont::Normal));
    p->setFont(m_font);

    QFontMetrics fm = p->fontMetrics();

//    if(data.length() > 20){
//        data = data.mid(0,20);
//        data += "...";
//    }
    if (m_header_text_bottom)
    {
        //flip column header text to read from bottom to top (supposedly this is more readable...)
        p->translate(rect.x() + rect.width(), rect.height());
        p->rotate(270);                        
        p->drawText(4,-rect.width() + ((rect.width()-fm.height()) / 2),rect.height()-10,rect.width(),1,data);
    }
    else
    {
        p->translate(rect.x(), rect.y());
        p->rotate(90);        
        p->drawText(12, -((rect.width()-fm.height()) / 2) - (fm.height()/4), data); //wtf.. i have no idea but it's centered so i'll take it
    }
    p->restore();
}

void RotatedHeader::resizeSection(int logicalIndex, int size) {
    QHeaderView::resizeSection(logicalIndex, size);
}

void RotatedHeader::set_index_as_spacer(int idx) {
    m_spacer_indexes << idx;
}

void RotatedHeader::clear_spacers() {
    m_spacer_indexes.clear();
}

QSize RotatedHeader::sizeHint() const {
    return QSize(32, 150);
}

void RotatedHeader::mouseMoveEvent(QMouseEvent *e) {
    m_p = e->pos();
    QHeaderView::mouseMoveEvent(e);
}

void RotatedHeader::mousePressEvent(QMouseEvent *e) {
    m_p = e->pos();
    int idx = logicalIndexAt(e->pos());
    if (idx > 0 && idx < count() && e->button() == Qt::RightButton) {
        emit section_right_clicked(idx);
    }
    QHeaderView::mousePressEvent(e);
}

void RotatedHeader::leaveEvent(QEvent *e) {
    m_p = QPoint(-1, -1);
    QHeaderView::leaveEvent(e);
}

void RotatedHeader::contextMenuEvent(QContextMenuEvent *evt) {
    int idx = logicalIndexAt(evt->pos());
    if (idx == 0) { //name header
        QMenu *m = new QMenu(this);
        QAction *a = m->addAction(tr("Sort Alphabetically Ascending"), this, SLOT(sort_action()));        
        a->setData(DwarfModelProxy::DSR_NAME_ASC);
        a = m->addAction(tr("Sort Alphabetically Descending"), this, SLOT(sort_action()));
        a->setData(DwarfModelProxy::DSR_NAME_DESC);
        a = m->addAction(tr("Sort by ID Ascending"), this, SLOT(sort_action()));
        a->setData(DwarfModelProxy::DSR_ID_ASC);
        a = m->addAction(tr("Sort by ID Descending"), this, SLOT(sort_action()));
        a->setData(DwarfModelProxy::DSR_ID_DESC);
        a = m->addAction(tr("Sort in Game Order"), this, SLOT(sort_action()));
        a->setData(DwarfModelProxy::DSR_GAME_ORDER);
        m->exec(viewport()->mapToGlobal(evt->pos()));
    } else {
        /* Don't do this yet
        // find out what set this is...
        QString set_name = model()->headerData(idx, Qt::Horizontal, Qt::UserRole).toString();
        QMenu *m = new QMenu(this);
        QAction *a = m->addAction(tr("Toggle Set %1").arg(set_name),
                                  this, SLOT(toggle_set_action()));
        a->setData(set_name);
        m->exec(viewport()->mapToGlobal(evt->pos()));
        */
    }
}

void RotatedHeader::sort_action() {
    QAction *sender = qobject_cast<QAction*>(QObject::sender());
    DwarfModelProxy::DWARF_SORT_ROLE role = static_cast<DwarfModelProxy::DWARF_SORT_ROLE>(sender->data().toInt());
    emit sort(0, role);
}

void RotatedHeader::toggle_set_action() {
    QAction *sender = qobject_cast<QAction*>(QObject::sender());
    QString set_name = sender->data().toString();
    for(int i = 1; i < count(); ++i) {
        QString col_set_name = model()->headerData(i, Qt::Horizontal,
                                                   Qt::UserRole).toString();
        if (col_set_name == set_name) {
            hideSection(i);
        }
    }
}
