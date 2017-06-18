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
#include <QtDebug>
#include "defines.h"
#include "dfinstance.h"
#include "dwarf.h"
#include "squad.h"
#include "word.h"
#include "gamedatareader.h"
#include "memorylayout.h"
#include "cp437codec.h"
#include "dwarftherapist.h"
#include "memorysegment.h"
#include "truncatingfilelogger.h"
#include "mainwindow.h"
#include "limits"
#include "dwarfstats.h"
#include "QThreadPool"
#include "rolecalc.h"
#include "viewmanager.h"
#include "languages.h"
#include "reaction.h"
#include "races.h"
#include "weapon.h"
#include "fortressentity.h"
#include "material.h"
#include "plant.h"

#ifdef Q_WS_WIN
#define LAYOUT_SUBDIR "windows"
#else
#ifdef Q_WS_X11
#define LAYOUT_SUBDIR "linux"
#else
#ifdef _OSX
#define LAYOUT_SUBDIR "osx"
#endif
#endif
#endif


DFInstance::DFInstance(QObject* parent)
    : QObject(parent)
    , m_pid(0)
    , m_memory_correction(0)
    , m_stop_scan(false)
    , m_is_ok(true)
    , m_bytes_scanned(0)
    , m_layout(0)
    , m_attach_count(0)
    , m_heartbeat_timer(new QTimer(this))
    , m_memory_remap_timer(new QTimer(this))
    , m_scan_speed_timer(new QTimer(this))
    , m_dwarf_race_id(0)
    , m_dwarf_civ_id(0)
{
    connect(m_scan_speed_timer, SIGNAL(timeout()),
            SLOT(calculate_scan_rate()));
    connect(m_memory_remap_timer, SIGNAL(timeout()),
            SLOT(map_virtual_memory()));
    m_memory_remap_timer->start(20000); // 20 seconds
    // let subclasses start the heartbeat timer, since we don't want to be
    // checking before we're connected
    connect(m_heartbeat_timer, SIGNAL(timeout()), SLOT(heartbeat()));

    // We need to scan for memory layout files to get a list of DF versions this
    // DT version can talk to. Start by building up a list of search paths
    QDir working_dir = QDir::current();
    QStringList search_paths;
    search_paths << working_dir.path();

    QString subdir = LAYOUT_SUBDIR;
    search_paths << QString("etc/memory_layouts/%1").arg(subdir);

    TRACE << "Searching for MemoryLayout ini files in the following directories";
    foreach(QString path, search_paths) {
        TRACE<< path;
        QDir d(path);
        d.setNameFilters(QStringList() << "*.ini");
        d.setFilter(QDir::NoDotAndDotDot | QDir::Readable | QDir::Files);
        d.setSorting(QDir::Name | QDir::Reversed);
        QFileInfoList files = d.entryInfoList();
        foreach(QFileInfo info, files) {
            MemoryLayout *temp = new MemoryLayout(info.absoluteFilePath());
            if (temp && temp->is_valid()) {
                LOGD << "adding valid layout" << temp->game_version()
                        << temp->checksum();
                m_memory_layouts.insert(temp->checksum().toLower(), temp);
            }
        }
    }
    // if no memory layouts were found that's a critical error
    if (m_memory_layouts.size() < 1) {
        LOGE << "No valid memory layouts found in the following directories..."
                << QDir::searchPaths("memory_layouts");
        qApp->exit(ERROR_NO_VALID_LAYOUTS);
    }
}

DFInstance::~DFInstance() {
    LOGD << "DFInstance baseclass virtual dtor!";
    foreach(MemoryLayout *l, m_memory_layouts) {
        delete(l);
    }
    m_memory_layouts.clear();
}

BYTE DFInstance::read_byte(const VIRTADDR &addr) {
    QByteArray out;
    read_raw(addr, sizeof(BYTE), out);
    return out.at(0);
}

WORD DFInstance::read_word(const VIRTADDR &addr) {
    QByteArray out;
    read_raw(addr, sizeof(WORD), out);
    return decode_word(out);
}

VIRTADDR DFInstance::read_addr(const VIRTADDR &addr) {
    QByteArray out;
    read_raw(addr, sizeof(VIRTADDR), out);
    return decode_dword(out);
}

qint16 DFInstance::read_short(const VIRTADDR &addr) {
    QByteArray out;
    read_raw(addr, sizeof(qint16), out);
    return decode_short(out);
}

qint32 DFInstance::read_int(const VIRTADDR &addr) {
    QByteArray out;
    read_raw(addr, sizeof(qint32), out);
    return decode_int(out);
}

QVector<VIRTADDR> DFInstance::scan_mem(const QByteArray &needle, const uint start_addr, const uint end_addr) {
    // progress reporting
    m_scan_speed_timer->start(500);
    m_memory_remap_timer->stop(); // don't remap segments while scanning
    int total_bytes = 0;
    m_bytes_scanned = 0; // for global timings
    int bytes_scanned = 0; // for progress calcs
    foreach(MemorySegment *seg, m_regions) {
        total_bytes += seg->size;
    }
    int report_every_n_bytes = total_bytes / 1000;
    emit scan_total_steps(1000);
    emit scan_progress(0);


    m_stop_scan = false;
    QVector<VIRTADDR> addresses; //! return value
    QByteArrayMatcher matcher(needle);

    int step_size = 0x1000;
    QByteArray buffer(step_size, 0);
    QByteArray back_buffer(step_size * 2, 0);

    QTime timer;
    timer.start();
    attach();
    foreach(MemorySegment *seg, m_regions) {
        int step = step_size;
        int steps = seg->size / step;
        if (seg->size % step)
            steps++;

        if( seg->end_addr < start_addr ) {
            continue;
        }

        if( seg->start_addr > end_addr ) {
            break;
        }

        for(VIRTADDR ptr = seg->start_addr; ptr < seg->end_addr; ptr += step) {

            if( ptr < start_addr ) {
                continue;
            }
            if( ptr > end_addr ) {
                m_stop_scan = true;
                break;
            }

            step = step_size;
            if (ptr + step > seg->end_addr) {
                step = seg->end_addr - ptr;
            }

            // move the last thing we read to the front of the back_buffer
            back_buffer.replace(0, step, buffer);

            // fill the main read buffer
            int bytes_read = read_raw(ptr, step, buffer);
            if (bytes_read < step && !seg->is_guarded) {
                if (m_layout->is_complete()) {
                    LOGW << "tried to read" << step << "bytes starting at" <<
                            hexify(ptr) << "but only got" << dec << bytes_read;
                }
                continue;
            }
            bytes_scanned += bytes_read;
            m_bytes_scanned += bytes_read;

            // put the main buffer on the end of the back_buffer
            back_buffer.replace(step, step, buffer);

            int idx = -1;
            forever {
                idx = matcher.indexIn(back_buffer, idx+1);
                if (idx == -1) {
                    break;
                } else {
                    VIRTADDR hit = ptr + idx - step;
                    if (!addresses.contains(hit)) {
                        // backbuffer may cause duplicate hits
                        addresses << hit;
                    }
                }
            }

            if (m_stop_scan)
                break;
            emit scan_progress(bytes_scanned / report_every_n_bytes);

        }
        DT->processEvents();
        if (m_stop_scan)
            break;
    }
    detach();
    m_memory_remap_timer->start(20000); // start the remapper again
    LOGD << QString("Scanned %L1MB in %L2ms").arg(bytes_scanned / 1024 * 1024)
            .arg(timer.elapsed());
    return addresses;
}

bool DFInstance::looks_like_vector_of_pointers(const VIRTADDR &addr) {
    int start = read_int(addr + 0x4);
    int end = read_int(addr + 0x8);
    int entries = (end - start) / sizeof(int);
    LOGD << "LOOKS LIKE VECTOR? unverified entries:" << entries;

    return start >=0 &&
           end >=0 &&
           end >= start &&
           (end-start) % 4 == 0 &&
           start % 4 == 0 &&
           end % 4 == 0 &&
           entries < 10000;

}

//void DFInstance::read_raws() {
//    emit progress_message(tr("Reading raws"));

//    LOGI << "Reading some game raws...";
//    GameDataReader::ptr()->read_raws(m_df_dir);
//}


void DFInstance::load_game_data()
{
    map_virtual_memory();
    emit progress_message(tr("Loading languages"));
    m_languages = Languages::get_languages(this);

    emit progress_message(tr("Loading reactions"));
    m_reactions.clear();
    QFuture<void> f = QtConcurrent::run(this,&DFInstance::load_reactions);
    f.waitForFinished();

    emit progress_message(tr("Loading races and castes"));
    m_races.clear();
    f = QtConcurrent::run(this,&DFInstance::load_races_castes);
    f.waitForFinished();

    f = QtConcurrent::run(this,&DFInstance::load_main_vectors);
    f.waitForFinished();

    emit progress_message(tr("Loading weapons"));
    f = QtConcurrent::run(this,&DFInstance::load_weapons);
    f.waitForFinished();
}

QString DFInstance::get_language_word(VIRTADDR addr){
    return m_languages->language_word(addr);
}

QString DFInstance::get_translated_word(VIRTADDR addr){
    return m_languages->english_word(addr);
}

QVector<Dwarf*> DFInstance::load_dwarves() {
    map_virtual_memory();
    QVector<Dwarf*> dwarves;
    if (!m_is_ok) {
        LOGW << "not connected";
        detach();
        return dwarves;
    }

    //load the fortress now as we need this data before loading squads and dwarfs
    m_fortress = FortressEntity::get_entity(this,read_addr(m_memory_correction + m_layout->address("fortress_entity")));

    // we're connected, make sure we have good addresses
    VIRTADDR creature_vector = m_layout->address("creature_vector");
    creature_vector += m_memory_correction;
    VIRTADDR active_creature_vector = m_layout->address("active_creature_vector");
    active_creature_vector += m_memory_correction;

    VIRTADDR dwarf_race_index = m_layout->address("dwarf_race_index");
    dwarf_race_index += m_memory_correction;
    VIRTADDR current_year = m_layout->address("current_year");
    current_year += m_memory_correction;
    VIRTADDR current_year_tick = m_layout->address("cur_year_tick");
    current_year_tick += m_memory_correction;
    m_cur_year_tick = read_int(current_year_tick);
    VIRTADDR dwarf_civ_index = m_layout->address("dwarf_civ_index");
    dwarf_civ_index += m_memory_correction;

    if (!is_valid_address(creature_vector) || !is_valid_address(active_creature_vector)) {
        LOGW << "Active Memory Layout" << m_layout->filename() << "("
                << m_layout->game_version() << ")" << "contains an invalid"
                << "creature_vector address. Either you are scanning a new "
                << "DF version or your config files are corrupted.";
        return dwarves;
    }
    if (!is_valid_address(dwarf_race_index)) {
        LOGW << "Active Memory Layout" << m_layout->filename() << "("
                << m_layout->game_version() << ")" << "contains an invalid"
                << "dwarf_race_index address. Either you are scanning a new "
                << "DF version or your config files are corrupted.";
        return dwarves;
    }

    // both necessary addresses are valid, so let's try to read the creatures
    LOGD << "loading creatures from " << hexify(creature_vector) <<
            hexify(creature_vector - m_memory_correction) << "(UNCORRECTED)";
    LOGD << "dwarf race index" << hexify(dwarf_race_index) <<
            hexify(dwarf_race_index - m_memory_correction) << "(UNCORRECTED)";
    LOGD << "current year" << hexify(current_year) <<
            hexify(current_year - m_memory_correction) << "(UNCORRECTED)";
    emit progress_message(tr("Loading Dwarves"));

    attach();
    m_dwarf_civ_id = read_word(dwarf_civ_index);
    LOGD << "civilization id:" << hexify(m_dwarf_civ_id);

    // which race id is dwarven?
    m_dwarf_race_id = read_word(dwarf_race_index);
    LOGD << "dwarf race:" << hexify(m_dwarf_race_id);

    m_current_year = read_word(current_year);
    LOGD << "current year:" << m_current_year;

    QVector<VIRTADDR> entries = get_creatures();

    emit progress_range(0, entries.size()-1);
    TRACE << "FOUND" << entries.size() << "creatures";
    bool hide_non_adults = DT->user_settings()->value("options/hide_children_and_babies",true).toBool();
    if (!entries.empty()) {
        Dwarf *d = 0;
        int i = 0;
        actual_dwarves.clear();
        foreach(VIRTADDR creature_addr, entries) {
            d = Dwarf::get_dwarf(this, creature_addr);
            if (d != 0) {
                dwarves.append(d); //add animals as well so we can show them
                if(d->is_animal() == false){
                    LOGD << "FOUND DWARF" << hexify(creature_addr)
                         << d->nice_name();
                    //never calculate roles for babies
                    if(d->profession() != "Baby"){
                        //only calculate roles for children if they're set to be able to assign labours
                        if(!hide_non_adults){
                            actual_dwarves.append(d);
                        }else if(d->profession() != "Child" ){
                            actual_dwarves.append(d);
                        }
                    }

                } else {
                    TRACE << "FOUND OTHER CREATURE" << hexify(creature_addr);
                }
            }
            emit progress_value(i++);
        }

        //load up role stuff now
        //load_roles();
        QFuture<void> f = QtConcurrent::run(this,&DFInstance::load_population_data);
        f.waitForFinished();
        f = QtConcurrent::run(this,&DFInstance::calc_done);
        f.waitForFinished();
        //calc_done();


        DT->emit_labor_counts_updated();

    } else {
        // we lost the fort!
        m_is_ok = false;
    }
    detach();

    LOGI << "found" << dwarves.size() << "dwarves out of" << entries.size()
            << "creatures";

    return dwarves;
}

void DFInstance::load_reactions(){
    attach();
    //LOGI << "Reading reactions names...";
    VIRTADDR reactions_vector = m_layout->address("reactions_vector");
    reactions_vector += m_memory_correction;
    QVector<VIRTADDR> reactions = enumerate_vector(reactions_vector);
    //TRACE << "FOUND" << reactions.size() << "reactions";
    //emit progress_range(0, reactions.size()-1);
    if (!reactions.empty()) {
        foreach(VIRTADDR reaction_addr, reactions) {
            Reaction* r = Reaction::get_reaction(this, reaction_addr);
            m_reactions.insert(r->tag(), r);
            //emit progress_value(i++);
        }
    }
    detach();
}

void DFInstance::load_main_vectors(){
    //world.raws.itemdefs.
    QVector<VIRTADDR> weapons = enumerate_vector(m_memory_correction + m_layout->address("weapons_vector"));
    m_item_vectors.insert(WEAPON,weapons);
    QVector<VIRTADDR> traps = enumerate_vector(m_memory_correction + m_layout->address("trap_vector"));
    m_item_vectors.insert(TRAPCOMP,traps);
    QVector<VIRTADDR> toys = enumerate_vector(m_memory_correction + m_layout->address("toy_vector"));
    m_item_vectors.insert(TOY,toys);
    QVector<VIRTADDR> tools = enumerate_vector(m_memory_correction + m_layout->address("tool_vector"));
    m_item_vectors.insert(TOOL,tools);
    QVector<VIRTADDR> instruments = enumerate_vector(m_memory_correction + m_layout->address("instrument_vector"));
    m_item_vectors.insert(INSTRUMENT,instruments);
    QVector<VIRTADDR> armor = enumerate_vector(m_memory_correction + m_layout->address("armor_vector"));
    m_item_vectors.insert(ARMOR,armor);
    QVector<VIRTADDR> ammo = enumerate_vector(m_memory_correction + m_layout->address("ammo_vector"));
    m_item_vectors.insert(AMMO,ammo);
    QVector<VIRTADDR> siege_ammo = enumerate_vector(m_memory_correction + m_layout->address("siegeammo_vector"));
    m_item_vectors.insert(SIEGEAMMO,siege_ammo);
    QVector<VIRTADDR> gloves = enumerate_vector(m_memory_correction + m_layout->address("glove_vector"));
    m_item_vectors.insert(GLOVES,gloves);
    QVector<VIRTADDR> shoes = enumerate_vector(m_memory_correction + m_layout->address("shoe_vector"));
    m_item_vectors.insert(SHOES,shoes);
    QVector<VIRTADDR> shields = enumerate_vector(m_memory_correction + m_layout->address("shield_vector"));
    m_item_vectors.insert(SHIELD,shields);
    QVector<VIRTADDR> helms = enumerate_vector(m_memory_correction + m_layout->address("helm_vector"));
    m_item_vectors.insert(HELM,helms);
    QVector<VIRTADDR> pants = enumerate_vector(m_memory_correction + m_layout->address("pant_vector"));
    m_item_vectors.insert(PANTS,pants);
    QVector<VIRTADDR> food = enumerate_vector(m_memory_correction + m_layout->address("food_vector"));
    m_item_vectors.insert(FOOD,food);

    m_color_vector = enumerate_vector(m_memory_correction + m_layout->address("colors_vector"));
    m_shape_vector = enumerate_vector(m_memory_correction + m_layout->address("shapes_vector"));

    VIRTADDR addr = m_memory_correction + m_layout->address("base_materials");
    int i = 0;
    for(i = 0; i < 256; i++){
        Material* m = Material::get_material(this, read_addr(addr), i);
        m_base_materials.insert(i,m);
        addr += 0x4;
    }

    //inorganics
    addr = m_memory_correction + m_layout->address("inorganics_vector");
    foreach(VIRTADDR mat, enumerate_vector(addr)){
        //inorganic_raw.material
        Material* m = Material::get_material(this, mat + m_layout->material_offset("inorganic_materials_vector"), 0);
        m_inorganics_vector.append(m);
    }

    //plants
    addr = m_memory_correction + m_layout->address("plants_vector");
    QVector<VIRTADDR> vec = enumerate_vector(addr);
    foreach(VIRTADDR plant, vec){
        Plant* p = Plant::get_plant(this, plant, 0);
        m_plants_vector.append(p);
    }
}

void DFInstance::load_weapons(){
    attach();
    QVector<VIRTADDR> weapons = m_item_vectors.value(WEAPON); //enumerate_vector(weapons_vector);
    m_weapons.clear();
    if (!weapons.empty()) {
        foreach(VIRTADDR weapon_addr, weapons) {
            Weapon* w = Weapon::get_weapon(this, weapon_addr);
            m_weapons.insert(w->name_plural(), w);
        }
    }

    m_ordered_weapons.clear();

    QStringList weapon_names;
    foreach(QString key, m_weapons.uniqueKeys()) {
        weapon_names << key;
    }

    qSort(weapon_names);
    foreach(QString name, weapon_names) {
        m_ordered_weapons << QPair<QString, Weapon *>(name, m_weapons.value(name));
    }

    detach();
}

void DFInstance::load_races_castes(){
    attach();
    //LOGI << "Reading races and castes...";
    VIRTADDR races_vector = m_layout->address("races_vector");
    races_vector += m_memory_correction;
    QVector<VIRTADDR> races = enumerate_vector(races_vector);
    //TRACE << "FOUND" << races.size() << "races";
    //emit progress_range(0, races.size()-1);
    int i = 0;
    if (!races.empty()) {
        foreach(VIRTADDR race_addr, races) {
            m_races << Race::get_race(this, race_addr);
            //emit progress_value(i++);
            LOGD << "race " << m_races.at(i)->name() << " index " << i;
            i++;
        }
    }
    detach();
}


void DFInstance::load_population_data(){
    //    t.start();
    //    calc_progress = 0;

    //    foreach(Dwarf *d, actual_dwarves){
    //        rolecalc *rc = new rolecalc(d);
    //        connect(rc,SIGNAL(done()),this,SLOT(calc_done()),Qt::QueuedConnection);
    //        QThreadPool::globalInstance()->start(rc);
    //    }
    //    foreach(Dwarf *d, actual_dwarves){
    //        d->calc_role_ratings();
    //    }

    m_enabled_labor_count.clear();
    m_pref_counts.clear();
    int cnt = 0;
    foreach(Dwarf *d, actual_dwarves){
        d->calc_role_ratings();
        foreach(int key, d->get_labors().uniqueKeys()){
            if(d->labor_enabled(key)){
                if(m_enabled_labor_count.contains(key))
                    cnt = m_enabled_labor_count.value(key)+1;
                else
                    cnt = 1;
                m_enabled_labor_count.insert(key,cnt);
            }
        }

        foreach(QString key, d->get_grouped_preferences().uniqueKeys()){
            for(int i = 0; i < d->get_grouped_preferences().value(key)->count(); i++){

                QString val = d->get_grouped_preferences().value(key)->at(i);
                QString cat_name = key;
                pref_stat p;
                if(m_pref_counts.contains(val))
                    p = m_pref_counts.value(val);
                else{
                    p.likes = 0;
                    p.dislikes = 0;
                }

                if(key.toLower()=="dislikes"){
                    cat_name = "Creature";
                    p.dislikes++;
                    p.names_dislikes.append(d->nice_name());
                }else{
                    p.likes++;
                    p.names_likes.append(d->nice_name());
                }

                p.pref_category = cat_name;

                m_pref_counts.insert(val,p);
            }
        }

    }
}

void DFInstance::calc_done(){    
    calc_progress ++;
    //emit progress_value(calc_progress);
    //emit progress_message(tr("Calculating roles...%1%").arg(QString::number(((float)calc_progress/(float)actual_dwarves.count())*100,'f',2)));
    //if(calc_progress == actual_dwarves.count()){
        foreach(Role *r, GameDataReader::ptr()->get_roles()){
            float mean = 0.0;
            float stdev = 0.0;
            int count = 0;
            foreach(Dwarf *d, actual_dwarves){
                count ++;
                mean += d->get_role_rating(r->name);
            }
            mean = mean / count;
            foreach(Dwarf *d, actual_dwarves){
                stdev += pow(d->get_role_rating(r->name) - mean,2);
            }
            stdev = sqrt(stdev / (count-1));
            foreach(Dwarf *d, actual_dwarves){
                d->set_role_rating(r->name, DwarfStats::calc_cdf(mean,stdev,d->get_role_rating(r->name))*100);
                //used to disable cdf calculations
                //d->set_role_rating(r->name, d->get_role_rating(r->name));
            }

        }
        foreach(Dwarf *d, actual_dwarves){
            d->update_rating_list();
        }
        actual_dwarves.clear();        
        //emit progress_value(calc_progress+1);
        //progress_message(QString("All roles loaded in %1 seconds.").arg(QString::number((float)t.elapsed()/1000,'f',2)));

        //update role columns
        //DT->emit_roles_changed();
        //DT->get_main_window()->get_view_manager()->redraw_current_tab();
    //}

}

QVector<Squad*> DFInstance::load_squads() {

    QVector<Squad*> squads;
    if (!m_is_ok) {
        LOGW << "not connected";
        detach();
        return squads;
    }

    // we're connected, make sure we have good addresses
    VIRTADDR squad_vector = m_layout->address("squad_vector");
    if(squad_vector == 0xFFFFFFFF) {
        LOGI << "Squads not supported for this version of Dwarf Fortress";
        return squads;
    }
    squad_vector += m_memory_correction;

    if (!is_valid_address(squad_vector)) {
        LOGW << "Active Memory Layout" << m_layout->filename() << "("
                << m_layout->game_version() << ")" << "contains an invalid"
                << "squad_vector address. Either you are scanning a new "
                << "DF version or your config files are corrupted.";
        return squads;
    }

    // both necessary addresses are valid, so let's try to read the creatures
    LOGD << "loading squads from " << hexify(squad_vector) <<
            hexify(squad_vector - m_memory_correction) << "(UNCORRECTED)";

    emit progress_message(tr("Loading Squads"));

    attach();

    QVector<VIRTADDR> entries = enumerate_vector(squad_vector);
    TRACE << "FOUND" << entries.size() << "squads";

    if (!entries.empty()) {
        emit progress_range(0, entries.size()-1);
        Squad *s = NULL;
        int i = 0;
        foreach(VIRTADDR squad_addr, entries) {
            s = Squad::get_squad(this, squad_addr);
            if (s) {
                LOGD << "FOUND SQUAD" << hexify(squad_addr) << s->name() << " member count: " << s->assigned_count() << " id: " << s->id();
                if(m_fortress->squad_is_active(s->id()))
                    squads.push_front(s);
            }
            emit progress_value(i++);
        }
    }

    detach();
    //LOGI << "Found" << squads.size() << "squads out of" << entries.size();
    return squads;
}

void DFInstance::heartbeat() {
    // simple read attempt that will fail if the DF game isn't running a fort,
    // or isn't running at all
    QVector<VIRTADDR> trans = enumerate_vector(m_layout->address("translation_vector") + m_memory_correction);
    if (trans.size() < 1) {
        // no game loaded, or process is gone
        emit connection_interrupted();
    }
}

QVector<VIRTADDR> DFInstance::get_creatures(){
    VIRTADDR active_units = m_layout->address("active_creature_vector");
    active_units += m_memory_correction;
    VIRTADDR all_units = m_layout->address("creature_vector");
    all_units += m_memory_correction;

    //first try the active unit list
    QVector<VIRTADDR> entries = enumerate_vector(active_units);
    if(entries.isEmpty()){
        LOGD << "no active units (embark) using full unit list";
        entries = enumerate_vector(all_units);
    }else{
        //there are active units, but are they ours?
        foreach(VIRTADDR entry, entries){
            if(read_word(entry + m_layout->dwarf_offset("civ"))==m_dwarf_civ_id){
                LOGD << "using active units";
                return entries;
            }
        }
        LOGD << "no active units with our civ (reclaim), using full unit list";
        entries = enumerate_vector(all_units);
    }
    return entries;
}

bool DFInstance::is_valid_address(const VIRTADDR &addr) {
    bool valid = false;
    foreach(MemorySegment *seg, m_regions) {
        if (seg->contains(addr)) {
            valid = true;
            break;
        }
    }
    return valid;
}

QByteArray DFInstance::get_data(const VIRTADDR &addr, int size) {
    QByteArray ret_val(size, 0); // 0 filled to proper length
    int bytes_read = read_raw(addr, size, ret_val);
    if (bytes_read != size) {
        ret_val.clear();
    }
    return ret_val;
}

//! ahhh convenience
QString DFInstance::pprint(const VIRTADDR &addr, int size) {
    return pprint(get_data(addr, size), addr);
}

QString DFInstance::pprint(const QByteArray &ba, const VIRTADDR &start_addr) {
    QString out = "    ADDR   | 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F | TEXT\n";
    out.append("------------------------------------------------------------------------\n");
    int lines = ba.size() / 16;
    if (ba.size() % 16)
        lines++;
    if (lines < 1)
        lines = 0;

    for(int i = 0; i < lines; ++i) {
        VIRTADDR offset = start_addr + i * 16;
        out.append(hexify(offset));
        out.append(" | ");
        for (int c = 0; c < 16; ++c) {
            out.append(ba.mid(i*16 + c, 1).toHex());
            out.append(" ");
        }
        out.append("| ");
        for (int c = 0; c < 16; ++c) {
            QByteArray tmp = ba.mid(i*16 + c, 1);
            if (tmp.at(0) == 0)
                out.append(".");
            else if (tmp.at(0) <= 126 && tmp.at(0) >= 32)
                out.append(tmp);
            else
                out.append(tmp.toHex());
        }
        //out.append(ba.mid(i*16, 16).toPercentEncoding());
        out.append("\n");
    }
    return out;
}

Word * DFInstance::read_dwarf_word(const VIRTADDR &addr) {
    Word * result = NULL;
    uint word_id = read_int(addr);
    if(word_id != 0xFFFFFFFF) {
        result = DT->get_word(word_id);
    }
    return result;
}

QString DFInstance::read_dwarf_name(const VIRTADDR &addr) {
    QString result = "The";

    //7 parts e.g.  ffffffff ffffffff 000006d4
    //      ffffffff ffffffff 000002b1 ffffffff

    //Unknown
    Word * word = read_dwarf_word(addr);
    if(word)
        result.append(" " + capitalize(word->base()));

    //Unknown
    word = read_dwarf_word(addr + 0x04);
    if(word)
        result.append(" " + capitalize(word->base()));

    //Verb
    word = read_dwarf_word(addr + 0x08);
    if(word) {
        result.append(" " + capitalize(word->adjective()));
    }

    //Unknown
    word = read_dwarf_word(addr + 0x0C);
    if(word)
        result.append(" " + capitalize(word->base()));

    //Unknown
    word = read_dwarf_word(addr + 0x10);
    if(word)
        result.append(" " + capitalize(word->base()));

    //Noun
    word = read_dwarf_word(addr + 0x14);
    bool singular = false;
    if(word) {
        if(word->plural_noun().isEmpty()) {
            result.append(" " + capitalize(word->noun()));
            singular = true;
        } else {
            result.append(" " + capitalize(word->plural_noun()));
        }
    }

    //of verb(noun)
    word = read_dwarf_word(addr + 0x18);
    if(word) {
        if( !word->verb().isEmpty() ) {
            if(singular) {
                result.append(" of " + capitalize(word->verb()));
            } else {
                result.append(" of " + capitalize(word->present_participle_verb()));
            }
        } else {
            if(singular) {
                result.append(" of " + capitalize(word->noun()));
            } else {
                result.append(" of " + capitalize(word->plural_noun()));
            }
        }
    }

    return result.trimmed();
}


QVector<VIRTADDR> DFInstance::find_vectors_in_range(const int &max_entries,
                                                const VIRTADDR &start_address,
                                                const int &range_length) {
    QByteArray data = get_data(start_address, range_length);
    QVector<VIRTADDR> vectors;
    VIRTADDR int1 = 0; // holds the start val
    VIRTADDR int2 = 0; // holds the end val

    for (int i = 0; i < range_length; i += 4) {
        memcpy(&int1, data.data() + i, 4);
        memcpy(&int2, data.data() + i + 4, 4);
        if (int2 >= int1 && is_valid_address(int1) && is_valid_address(int2)) {
            int bytes = int2 - int1;
            int entries = bytes / 4;
            if (entries > 0 && entries <= max_entries) {
                VIRTADDR vector_addr = start_address + i - VECTOR_POINTER_OFFSET;
                QVector<VIRTADDR> addrs = enumerate_vector(vector_addr);
                bool all_valid = true;
                foreach(VIRTADDR vec_entry, addrs) {
                    if (!is_valid_address(vec_entry)) {
                        all_valid = false;
                        break;
                    }
                }
                if (all_valid) {
                    vectors << vector_addr;
                }
            }
        }
    }
    return vectors;
}

QVector<VIRTADDR> DFInstance::find_vectors(int num_entries, int fuzz/* =0 */,
                                           int entry_size/* =4 */) {
    /*
    glibc++ does vectors like so...
    |4bytes      | 4bytes    | 4bytes
    START_ADDRESS|END_ADDRESS|END_ALLOCATOR

    MSVC++ does vectors like so...
    | 4bytes     | 4bytes      | 4 bytes   | 4bytes
    ALLOCATOR    |START_ADDRESS|END_ADDRESS|END_ALLOCATOR
    */
    m_stop_scan = false; //! if ever set true, bail from the inner loop
    QVector<VIRTADDR> vectors; //! return value collection of vectors found
    VIRTADDR int1 = 0; // holds the start val
    VIRTADDR int2 = 0; // holds the end val

    // progress reporting
    m_scan_speed_timer->start(500);
    m_memory_remap_timer->stop(); // don't remap segments while scanning
    int total_bytes = 0;
    m_bytes_scanned = 0; // for global timings
    int bytes_scanned = 0; // for progress calcs
    foreach(MemorySegment *seg, m_regions) {
        total_bytes += seg->size;
    }
    int report_every_n_bytes = total_bytes / 1000;
    emit scan_total_steps(1000);
    emit scan_progress(0);

    int scan_step_size = 0x10000;
    QByteArray buffer(scan_step_size, '\0');
    QTime timer;
    timer.start();
    attach();
    foreach(MemorySegment *seg, m_regions) {
        //TRACE << "SCANNING REGION" << hex << seg->start_addr << "-"
        //        << seg->end_addr << "BYTES:" << dec << seg->size;
        if ((int)seg->size <= scan_step_size) {
            scan_step_size = seg->size;
        }
        int scan_steps = seg->size / scan_step_size;
        if (seg->size % scan_step_size) {
            scan_steps++;
        }
        VIRTADDR addr = 0; // the ptr we will read from
        for(int step = 0; step < scan_steps; ++step) {
            addr = seg->start_addr + (scan_step_size * step);
            //LOGD << "starting scan for vectors at" << hex << addr << "step"
            //        << dec << step << "of" << scan_steps;
            int bytes_read = read_raw(addr, scan_step_size, buffer);
            if (bytes_read < scan_step_size) {
                continue;
            }
            for(int offset = 0; offset < scan_step_size; offset += entry_size) {
                int1 = decode_int(buffer.mid(offset, entry_size));
                int2 = decode_int(buffer.mid(offset + entry_size, entry_size));
                if (int1 && int2 && int2 >= int1
                    && int1 % 4 == 0
                    && int2 % 4 == 0
                    //&& is_valid_address(int1)
                    //&& is_valid_address(int2)
                    ) {
                    int bytes = int2 - int1;
                    int entries = bytes / entry_size;
                    int diff = entries - num_entries;
                    if (qAbs(diff) <= fuzz) {
                        VIRTADDR vector_addr = addr + offset -
                                               VECTOR_POINTER_OFFSET;
                        QVector<VIRTADDR> addrs = enumerate_vector(vector_addr);
                        diff = addrs.size() - num_entries;
                        if (qAbs(diff) <= fuzz) {
                            vectors << vector_addr;
                        }
                    }
                }
                m_bytes_scanned += entry_size;
                bytes_scanned += entry_size;
                if (m_stop_scan)
                    break;
            }
            emit scan_progress(bytes_scanned / report_every_n_bytes);
            DT->processEvents();
            if (m_stop_scan)
                break;
        }
    }
    detach();
    m_memory_remap_timer->start(20000); // start the remapper again
    m_scan_speed_timer->stop();
    LOGD << QString("Scanned %L1MB in %L2ms").arg(bytes_scanned / 1024 * 1024)
            .arg(timer.elapsed());
    emit scan_progress(100);
    return vectors;
}

QVector<VIRTADDR> DFInstance::find_vectors_ext(int num_entries, const char op,
                              const uint start_addr, const uint end_addr, int entry_size/* =4 */) {
    /*
    glibc++ does vectors like so...
    |4bytes      | 4bytes    | 4bytes
    START_ADDRESS|END_ADDRESS|END_ALLOCATOR

    MSVC++ does vectors like so...
    | 4bytes     | 4bytes      | 4 bytes   | 4bytes
    ALLOCATOR    |START_ADDRESS|END_ADDRESS|END_ALLOCATOR
    */
    m_stop_scan = false; //! if ever set true, bail from the inner loop
    QVector<VIRTADDR> vectors; //! return value collection of vectors found
    VIRTADDR int1 = 0; // holds the start val
    VIRTADDR int2 = 0; // holds the end val

    // progress reporting
    m_scan_speed_timer->start(500);
    m_memory_remap_timer->stop(); // don't remap segments while scanning
    int total_bytes = 0;
    m_bytes_scanned = 0; // for global timings
    int bytes_scanned = 0; // for progress calcs
    foreach(MemorySegment *seg, m_regions) {
        total_bytes += seg->size;
    }
    int report_every_n_bytes = total_bytes / 1000;
    emit scan_total_steps(1000);
    emit scan_progress(0);

    int scan_step_size = 0x10000;
    QByteArray buffer(scan_step_size, '\0');
    QTime timer;
    timer.start();
    attach();
    foreach(MemorySegment *seg, m_regions) {
        //TRACE << "SCANNING REGION" << hex << seg->start_addr << "-"
        //        << seg->end_addr << "BYTES:" << dec << seg->size;
        if ((int)seg->size <= scan_step_size) {
            scan_step_size = seg->size;
        }
        int scan_steps = seg->size / scan_step_size;
        if (seg->size % scan_step_size) {
            scan_steps++;
        }

        if( seg->end_addr < start_addr ) {
            continue;
        }

        if( seg->start_addr > end_addr ) {
            break;
        }

        VIRTADDR addr = 0; // the ptr we will read from
        for(int step = 0; step < scan_steps; ++step) {
            addr = seg->start_addr + (scan_step_size * step);
            //LOGD << "starting scan for vectors at" << hex << addr << "step"
            //        << dec << step << "of" << scan_steps;
            int bytes_read = read_raw(addr, scan_step_size, buffer);
            if (bytes_read < scan_step_size) {
                continue;
            }

            for(int offset = 0; offset < scan_step_size; offset += entry_size) {
                VIRTADDR vector_addr = addr + offset - VECTOR_POINTER_OFFSET;

                if( vector_addr < start_addr ) {
                    continue;
                }

                if( vector_addr > end_addr ) {
                    m_stop_scan = true;
                    break;
                }

                int1 = decode_int(buffer.mid(offset, entry_size));
                int2 = decode_int(buffer.mid(offset + entry_size, entry_size));
                if (int1 && int2 && int2 >= int1
                    && int1 % 4 == 0
                    && int2 % 4 == 0
                    //&& is_valid_address(int1)
                    //&& is_valid_address(int2)
                    ) {
                    int bytes = int2 - int1;
                    int entries = bytes / entry_size;
                    if (entries > 0 && entries < 1000) {
                        QVector<VIRTADDR> addrs = enumerate_vector(vector_addr);
                        if( (op == '=' && addrs.size() == num_entries)
                                || (op == '<' && addrs.size() < num_entries)
                                || (op == '>' && addrs.size() > num_entries) ) {
                            vectors << vector_addr;
                        }
                    }
                }
                m_bytes_scanned += entry_size;
                bytes_scanned += entry_size;
                if (m_stop_scan)
                    break;
            }
            emit scan_progress(bytes_scanned / report_every_n_bytes);
            DT->processEvents();
            if (m_stop_scan)
                break;
        }
    }
    detach();
    m_memory_remap_timer->start(20000); // start the remapper again
    m_scan_speed_timer->stop();
    LOGD << QString("Scanned %L1MB in %L2ms").arg(bytes_scanned / 1024 * 1024)
            .arg(timer.elapsed());
    emit scan_progress(100);
    return vectors;
}

QVector<VIRTADDR> DFInstance::find_vectors(int num_entries, const QVector<VIRTADDR> & search_set,
                               int fuzz/* =0 */, int entry_size/* =4 */) {

    m_stop_scan = false; //! if ever set true, bail from the inner loop
    QVector<VIRTADDR> vectors; //! return value collection of vectors found

    // progress reporting
    m_scan_speed_timer->start(500);
    m_memory_remap_timer->stop(); // don't remap segments while scanning

    int total_vectors = vectors.size();
    m_bytes_scanned = 0; // for global timings
    int vectors_scanned = 0; // for progress calcs

    emit scan_total_steps(total_vectors);
    emit scan_progress(0);

    QTime timer;
    timer.start();
    attach();

    int vector_size = 8 + VECTOR_POINTER_OFFSET;
    QByteArray buffer(vector_size, '\0');

    foreach(VIRTADDR addr, search_set) {
        int bytes_read = read_raw(addr, vector_size, buffer);
        if (bytes_read < vector_size) {
            continue;
        }

        VIRTADDR int1 = 0; // holds the start val
        VIRTADDR int2 = 0; // holds the end val
        int1 = decode_int(buffer.mid(VECTOR_POINTER_OFFSET, sizeof(VIRTADDR)));
        int2 = decode_int(buffer.mid(VECTOR_POINTER_OFFSET+ sizeof(VIRTADDR), sizeof(VIRTADDR)));

        if (int1 && int2 && int2 >= int1
                && int1 % 4 == 0
                && int2 % 4 == 0) {

            int bytes = int2 - int1;
            int entries = bytes / entry_size;
            int diff = entries - num_entries;
            if (qAbs(diff) <= fuzz) {
                QVector<VIRTADDR> addrs = enumerate_vector(addr);
                diff = addrs.size() - num_entries;
                if (qAbs(diff) <= fuzz) {
                    vectors << addr;
                }
            }
        }

        vectors_scanned++;

        if(vectors_scanned % 100 == 0) {
            emit scan_progress(vectors_scanned);
            DT->processEvents();
        }

        if (m_stop_scan)
            break;
    }


    detach();
    m_memory_remap_timer->start(20000); // start the remapper again
    m_scan_speed_timer->stop();
    LOGD << QString("Scanned %L1 vectors in %L2ms").arg(vectors_scanned)
            .arg(timer.elapsed());
    emit scan_progress(100);
    return vectors;
}


MemoryLayout *DFInstance::get_memory_layout(QString checksum, bool) {
    checksum = checksum.toLower();
    LOGD << "DF's checksum is:" << checksum;

    MemoryLayout *ret_val = NULL;
    ret_val = m_memory_layouts.value(checksum, NULL);
    m_is_ok = ret_val != NULL && ret_val->is_valid();

    if(!m_is_ok) {
        LOGD << "Could not find layout for checksum" << checksum;
        DT->get_main_window()->check_for_layout(checksum);
    }

    if (m_is_ok) {
        LOGI << "Detected Dwarf Fortress version"
                << ret_val->game_version() << "using MemoryLayout from"
                << ret_val->filename();
    }

    return ret_val;
}

bool DFInstance::add_new_layout(const QString & version, QFile & file) {
    QString newFileName = version;
    newFileName.replace("(", "").replace(")", "").replace(" ", "_");
    newFileName +=  ".ini";

    QFileInfo newFile(QDir(QString("etc/memory_layouts/%1").arg(LAYOUT_SUBDIR)), newFileName);
    newFileName = newFile.absoluteFilePath();

    if(!file.exists()) {
        LOGW << "Layout file" << file.fileName() << "does not exist!";
        return false;
    }

    LOGD << "Copying: " << file.fileName() << " to " << newFileName;
    if(!file.copy(newFileName)) {
        LOGW << "Error renaming layout file!";
        return false;
    }

    MemoryLayout *temp = new MemoryLayout(newFileName);
    if (temp && temp->is_valid()) {
        LOGD << "adding valid layout" << temp->game_version() << temp->checksum();
        m_memory_layouts.insert(temp->checksum().toLower(), temp);
    }
    return true;
}

void DFInstance::layout_not_found(const QString & checksum) {
    QString supported_vers;

    // TODO: Replace this with a rich dialog at some point that
    // is also accessible from the help menu. For now, remove the
    // extra path information as the dialog is getting way too big.
    // And make a half-ass attempt at sorting
    QList<MemoryLayout *> layouts = m_memory_layouts.values();
    qSort(layouts);

    foreach(MemoryLayout * l, layouts) {
        supported_vers.append(
                QString("<li><b>%1</b>(<font color=\"#444444\">%2"
                        "</font>)</li>")
                .arg(l->game_version())
                .arg(l->checksum()));
    }

    QMessageBox *mb = new QMessageBox(qApp->activeWindow());
    mb->setIcon(QMessageBox::Critical);
    mb->setWindowTitle(tr("Unidentified Game Version"));
    mb->setText(tr("I'm sorry but I don't know how to talk to this "
        "version of Dwarf Fortress! (checksum:%1)<br><br> <b>Supported "
        "Versions:</b><ul>%2</ul>").arg(checksum).arg(supported_vers));
    mb->setInformativeText(tr("<a href=\"%1\">Click Here to find out "
                              "more online</a>.")
                           .arg(URL_SUPPORTED_GAME_VERSIONS));

    /*
    mb->setDetailedText(tr("Failed to locate a memory layout file for "
        "Dwarf Fortress exectutable with checksum '%1'").arg(checksum));
    */
    mb->exec();
    LOGE << tr("unable to identify version from checksum:") << checksum;
}

void DFInstance::calculate_scan_rate() {
    float rate = (m_bytes_scanned / 1024.0f / 1024.0f) /
                 (m_scan_speed_timer->interval() / 1000.0f);
    QString msg = QString("%L1MB/s").arg(rate);
    emit scan_message(msg);
    m_bytes_scanned = 0;
}

VIRTADDR DFInstance::find_historical_figure(int hist_id){
    if(m_hist_figures.count() <= 0)
        load_hist_figures();

    return m_hist_figures.value(hist_id);
}

void DFInstance::load_hist_figures(){
    QVector<VIRTADDR> hist_figs = enumerate_vector(m_memory_correction + m_layout->address("historical_figures_vector"));
    int hist_id = 0;
    //it may be possible to filter this list by only the current race.
    //need to test whether or not vampires will steal names from other races
    //this may also break nicknames on kings/queens if they belong to a different race...
    foreach(VIRTADDR fig, hist_figs){
        //if(read_int(fig + 0x0002) == dwarf_race_id() || read_int(fig + 0x00a0) == dwarf_civ_id()){
            hist_id = read_int(fig + m_layout->hist_figure_offset("id"));
            m_hist_figures.insert(hist_id,fig);
        //}
    }
}

VIRTADDR DFInstance::find_fake_identity(int hist_id){
    VIRTADDR fig = find_historical_figure(hist_id);
    if(fig){
        VIRTADDR fig_info = read_addr(fig + m_layout->hist_figure_offset("hist_fig_info"));
        VIRTADDR rep_info = read_addr(fig_info + m_layout->hist_figure_offset("reputation"));
        int cur_ident = read_int(rep_info + m_layout->hist_figure_offset("current_ident"));
        if(m_fake_identities.count() == 0)
            m_fake_identities = enumerate_vector(m_memory_correction + m_layout->address("fake_identities_vector"));
        foreach(VIRTADDR ident, m_fake_identities){
            int fake_id = read_int(ident);
            if(fake_id==cur_ident){
                return ident;
            }
        }
    }
    return 0;
}

QVector<VIRTADDR> DFInstance::get_item_vector(ITEM_TYPE i){
    if(m_item_vectors.contains(i))
        return m_item_vectors.value(i);
    else
        return m_item_vectors.value(NONE);
}

QString DFInstance::get_item(int index, int subtype){
    ITEM_TYPE itype = static_cast<ITEM_TYPE>(index);

    QVector<VIRTADDR> items = get_item_vector(itype);
    if(!items.empty() && subtype < items.count()){
        QString name = read_string(items.at(subtype) + m_layout->item_offset("name_plural"));
        if(itype==TRAPCOMP || itype==WEAPON){
            name.prepend(" ").prepend(read_string(items.at(subtype) + m_layout->item_offset("adjective")));
        }else if(itype==ARMOR || itype==PANTS){
            name.prepend(" ").prepend(read_string(items.at(subtype) + m_layout->item_offset("mat_name")));
        }
        return name.trimmed();
    }
    else{
        return Item::get_item_desc(itype);
    }
}

QString DFInstance::get_color(int index){
    if(index < m_color_vector.count())
        return read_string(m_color_vector.at(index) + m_layout->descriptor_offset("color_name"));
    else
        return "unknown color";
}

QString DFInstance::get_shape(int index){
    if(index < m_shape_vector.count())
        return read_string(m_shape_vector.at(index) + m_layout->descriptor_offset("shape_name_plural"));
    else
        return "unknown shape";
}

Material *DFInstance::get_inorganic_material(int index){
    if(index < m_inorganics_vector.count())
        return m_inorganics_vector.at(index);
    else
        return new Material();
}

Plant *DFInstance::get_plant(int index){
    if(index < m_plants_vector.count())
        return m_plants_vector.at(index);
    else
        return new Plant();
}

Material *DFInstance::get_raw_material(int index){
    return m_base_materials.value(index);
}

QString DFInstance::find_material_name(int mat_index, short mat_type, ITEM_TYPE itype){
    Material *m = find_material(mat_index, mat_type);
    QString name = "unknown";

    if(mat_index < 0){
        name = m->get_material_name(SOLID);
    }
    else if(mat_type == 0){
        name = m->get_material_name(SOLID);
    }
    else if(mat_type < 19){
       name = m->get_material_name(SOLID);
    }
    else if(mat_type < 219){
        Race* r = get_race(mat_index);
        if(r)
        {
            if(itype == DRINK || itype == LIQUID_MISC)
                name = m->get_material_name(LIQUID);
            else if(itype == CHEESE)
                name = m->get_material_name(SOLID);
            else
            {
                name = r->name().toLower();
                name.append(" ");
                name.append(m->get_material_name(SOLID));
            }
        }
    }
    else if(mat_type < 419)
    {
        VIRTADDR hist_figure = find_historical_figure(mat_index);
        if(hist_figure){
            Race *r = get_race(read_int(hist_figure + m_layout->hist_figure_offset("hist_race")));
            QString fig_name = read_string(hist_figure + m_layout->hist_figure_offset("hist_name"));
            if(r){
                name = fig_name.append("'s ");
                name.append(m->get_material_name(LIQUID));
            }
        }
    }
    else if(mat_type < 619){
        Plant *p = get_plant(mat_index);
        if(p){
            name = p->name();

            if(itype==LEAVES)
                name = p->leaf_plural();
            else if(itype==SEEDS)
                name = p->seed_plural();
            else if(itype==PLANT)
                name = p->name_plural();

            //specific plant material
            if(m){
                if(itype == NONE){
                    QString sub_name = m->get_material_name(GENERIC);
                    if(sub_name.toLower()=="thread")
                        sub_name = m->get_material_name(SOLID).toLower().append(" fabric");
                    name.append(" ").append(sub_name);
                }
                else if(itype == DRINK || itype == LIQUID_MISC)
                    name = m->get_material_name(LIQUID);
                else if(itype == POWDER_MISC || itype == CHEESE)
                    name = m->get_material_name(POWDER);
            }
        }
    }
    return name.toLower();
}

Material *DFInstance::find_material(int mat_index, short mat_type){
    int index = 0;
    Material *m = new Material();

    if(mat_index < 0){
        m = get_raw_material(mat_type);
    }
    else if(mat_type == 0){
        m = get_inorganic_material(mat_index);
    }
    else if(mat_type < 19){
       m = get_raw_material(mat_type);
    }
    else if(mat_type < 219){
        Race* r = get_race(mat_index);
        if(r)
        {
            index = mat_type - 19; //base material types
            m = r->get_creature_material(index);
        }
    }
    else if(mat_type < 419)
    {
        VIRTADDR hist_figure = find_historical_figure(mat_index);
        if(hist_figure){
            Race *r = get_race(read_int(hist_figure + m_layout->hist_figure_offset("hist_race")));
            if(r)
                m = r->get_creature_material(mat_type-219);
        }
    }
    else if(mat_type < 619){
        Plant *p = get_plant(mat_index);
        index = mat_type -419;
        if(p)
            if(index < p->material_count()){
                m = p->get_plant_material(index);
            }
    }

    return m;
}


