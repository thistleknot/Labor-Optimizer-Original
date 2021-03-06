#ifndef GLOBAL_ENUMS_H
#define GLOBAL_ENUMS_H

#include "qstring.h"

typedef enum {
    triple_negative=-3,
    double_negative=-2,
    negative=-1,
    average=0,
    positive=1,
    double_positive=2,
    triple_positive=3
} ASPECT_TYPE;

typedef enum{
    none=-1, //custom
    semi_wild=0,
    trained=1,
    well_trained=2,
    skillfully_trained=3,
    expertly_trained=4,
    exceptionally_trained=5,
    masterfully_trained=6,
    domesticated=7,
    unknown_trained=8,
    wild_untamed=9,
    hostile=10 //custom
} ANIMAL_TYPE;

typedef enum {
    NONE=-1,
    BAR=0,
    SMALLGEM=1,
    BLOCKS=2,
    ROUGH=3,
    BOULDER=4,
    WOOD=5,
    DOOR=6,
    FLOODGATE=7,
    BED=8,
    CHAIR=9,
    CHAIN=10,
    FLASK=11,
    GOBLET=12,
    INSTRUMENT=13,
    TOY=14,
    WINDOW=15,
    CAGE=16,
    BARREL=17,
    BUCKET=18,
    ANIMALTRAP=19,
    TABLE=20,
    COFFIN=21,
    STATUE=22,
    CORPSE=23,
    WEAPON=24,
    ARMOR=25,
    SHOES=26,
    SHIELD=27,
    HELM=28,
    GLOVES=29,
    BOX=30,
    BIN=31,
    ARMORSTAND=32,
    WEAPONRACK=33,
    CABINET=34,
    FIGURINE=35,
    AMULET=36,
    SCEPTER=37,
    AMMO=38,
    CROWN=39,
    RING=40,
    EARRING=41,
    BRACELET=42,
    GEM=43,
    ANVIL=44,
    CORPSEPIECE=45,
    REMAINS=46,
    MEAT=47,
    FISH=48,
    FISH_RAW=49,
    VERMIN=50,
    PET=51,
    SEEDS=52,
    PLANT=53,
    SKIN_TANNED=54,
    LEAVES=55,
    THREAD=56,
    CLOTH=57,
    TOTEM=58,
    PANTS=59,
    BACKPACK=60,
    QUIVER=61,
    CATAPULTPARTS=62,
    BALLISTAPARTS=63,
    SIEGEAMMO=64,
    BALLISTAARROWHEAD=65,
    TRAPPARTS=66,
    TRAPCOMP=67,
    DRINK=68,
    POWDER_MISC=69,
    CHEESE=70,
    FOOD=71,
    LIQUID_MISC=72,
    COIN=73,
    GLOB=74,
    ROCK=75,
    PIPE_SECTION=76,
    HATCH_COVER=77,
    GRATE=78,
    QUERN=79,
    MILLSTONE=80,
    SPLINT=81,
    CRUTCH=82,
    TRACTION_BENCH=83,
    ORTHOPEDIC_CAST=84,
    TOOL=85,
    SLAB=86,
    EGG=87,
    BOOK=88,
    NUM_OF_TYPES=89
} ITEM_TYPE;

typedef enum {
    SOLID,
    LIQUID,
    GAS,
    POWDER,
    PASTE,
    PRESSED,
    GENERIC
} MATERIAL_STATES;

typedef enum {
    LIKE_MATERIAL=0,
    LIKE_CREATURE=1,
    LIKE_FOOD=2,
    HATE_CREATURE=3,
    LIKE_ITEM=4,
    LIKE_PLANT=5,
    LIKE_TREE=6,
    LIKE_COLOR=7,
    LIKE_SHAPE=8
} PREF_TYPES;

#endif // GLOBAL_ENUMS_H
