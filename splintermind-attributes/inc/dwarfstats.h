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
#ifndef DWARFSTATS_H
#define DWARFSTATS_H

#include "dwarf.h"

class DwarfStats
{

public:
    static float calc_cdf(float mean, float stdev, float rawValue);    

    struct bin{
        int min;
        int max;
        double density;
        double probability;
    };

    static void load_attribute_bins(ASPECT_TYPE, QList<int>);
    static float get_attribute_role_rating(ASPECT_TYPE, int);
    static float get_trait_role_rating(ASPECT_TYPE, int);
    //static float get_skill_role_rating(int skill_id, int value);

    static QHash<ASPECT_TYPE, QList<bin> > m_trait_bins;
    static void load_trait_bins(ASPECT_TYPE, QList<int>);

    //static QHash<int, QVector<int>* > m_dwarf_skills;
    //static void load_skills(QVector<Dwarf *> dwarves);

private:
//    static QVector<float> m_dwarf_skill_mean;
//    static QVector<float> m_dwarf_skill_stdDev;

    static QHash<ASPECT_TYPE, QList<bin> > m_attribute_bins;

    static float get_aspect_role_rating(float value, QList<bin> m_bins);
};

#endif // DWARFSTATS_H
